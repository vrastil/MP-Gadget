#include <mpi.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "utils.h"

#include "allvars.h"
#include "forcetree.h"
#include "treewalk.h"
#include "timestep.h"

#include "gravshort.h"

/*! \file gravtree.c
 *  \brief main driver routines for gravitational (short-range) force computation
 *
 *  This file contains the code for the gravitational force computation by
 *  means of the tree algorithm. To this end, a tree force is computed for all
 *  active local particles, and particles are exported to other processors if
 *  needed, where they can receive additional force contributions. If the
 *  TreePM algorithm is enabled, the force computed will only be the
 *  short-range part.
 */

/* According to upstream P-GADGET3
 * correct workcount slows it down and yields little benefits in load balancing
 *
 * YF: anything we shall do about this?
 * */

int force_treeev_shortrange(TreeWalkQueryGravShort * input,
        TreeWalkResultGravShort * output,
        LocalTreeWalk * lv);

/*! This function computes the gravitational forces for all active particles.
 *  If needed, a new tree is constructed, otherwise the dynamically updated
 *  tree is used.  Particles are only exported to other processors when really
 *  needed, thereby allowing a good use of the communication buffer.
 */
void grav_short_tree(ForceTree * tree)
{
    double timeall = 0;
    double timetree, timewait, timecomm;
    if(!All.TreeGravOn)
        return;

    TreeWalk tw[1] = {{0}};

    tw->ev_label = "FORCETREE_SHORTRANGE";
    tw->visit = (TreeWalkVisitFunction) force_treeev_shortrange;
    /* gravity applies to all particles. Including Tracer particles to enhance numerical stability. */
    tw->haswork = NULL;
    tw->reduce = (TreeWalkReduceResultFunction) grav_short_reduce;
    tw->postprocess = (TreeWalkProcessFunction) grav_short_postprocess;

    tw->query_type_elsize = sizeof(TreeWalkQueryGravShort);
    tw->result_type_elsize = sizeof(TreeWalkResultGravShort);
    tw->fill = (TreeWalkFillQueryFunction) grav_short_copy;
    tw->tree = tree;

    walltime_measure("/Misc");

    /* allocate buffers to arrange communication */
    MPIU_Barrier(MPI_COMM_WORLD);
    message(0, "Begin tree force.  (presently allocated=%g MB)\n", mymalloc_usedbytes() / (1024.0 * 1024.0));

    walltime_measure("/Misc");

    treewalk_run(tw, ActiveParticle, NumActiveParticle);

    /* now add things for comoving integration */

    MPIU_Barrier(MPI_COMM_WORLD);
    message(0, "Short-range gravitational tree force is done.\n");

    /* Now the force computation is finished */

    /*  gather some diagnostic information */

    timetree = tw->timecomp1 + tw->timecomp2 + tw->timecomp3;
    timewait = tw->timewait1 + tw->timewait2;
    timecomm= tw->timecommsumm1 + tw->timecommsumm2;

    walltime_add("/Tree/Walk1", tw->timecomp1);
    walltime_add("/Tree/Walk2", tw->timecomp2);
    walltime_add("/Tree/PostProcess", tw->timecomp3);
    walltime_add("/Tree/Send", tw->timecommsumm1);
    walltime_add("/Tree/Recv", tw->timecommsumm2);
    walltime_add("/Tree/Wait1", tw->timewait1);
    walltime_add("/Tree/Wait2", tw->timewait2);

    timeall = walltime_measure(WALLTIME_IGNORE);

    walltime_add("/Tree/Misc", timeall - (timetree + timewait + timecomm));

}

/*! In the TreePM algorithm, the tree is walked only locally around the
 *  target coordinate.  Tree nodes that fall outside a box of half
 *  side-length Rcut= RCUT*ASMTH*MeshSize can be discarded. The short-range
 *  potential is modified by a complementary error function, multiplied
 *  with the Newtonian form. The resulting short-range suppression compared
 *  to the Newtonian force is tabulated, because looking up from this table
 *  is faster than recomputing the corresponding factor, despite the
 *  memory-access penalty (which reduces cache performance) incurred by the
 *  table.
 */
int force_treeev_shortrange(TreeWalkQueryGravShort * input,
        TreeWalkResultGravShort * output,
        LocalTreeWalk * lv)
{
    /*Counters*/
    int ninteractions = 0;

    /*Added to the particle struct at the end*/
    MyDouble pot = 0;
    MyDouble acc_x = 0;
    MyDouble acc_y = 0;
    MyDouble acc_z = 0;
    const ForceTree * tree = lv->tw->tree;

    /*Hybrid particle neutrinos do not gravitate at early times*/
    const int NeutrinoTracer = All.HybridNeutrinosOn && (All.Time <= All.HybridNuPartTime);
    /*Tree-opening constants*/
    const double rcut = RCUT * All.Asmth * All.BoxSize / All.Nmesh;
    const double rcut2 = rcut * rcut;
    const double aold = All.ErrTolForceAcc * input->OldAcc;

    /*Input particle data*/
    const double pos_x = input->base.Pos[0];
    const double pos_y = input->base.Pos[1];
    const double pos_z = input->base.Pos[2];

    /*Start the tree walk*/
    int no = input->base.NodeList[0];
    int listindex = 1;
    no = tree->Nodes[no].u.d.nextnode;	/* open it */

    while(no >= 0)
    {
        while(no >= 0)
        {
            double mass, r2, h;
            double dx, dy, dz;
            if(node_is_particle(no, tree))
            {
                if(NeutrinoTracer)
                {
                    if(P[no].Type == All.FastParticleType)
                    {
                        no = force_get_next_node(no, tree);
                        continue;
                    }
                }

                dx = NEAREST(P[no].Pos[0] - pos_x);
                dy = NEAREST(P[no].Pos[1] - pos_y);
                dz = NEAREST(P[no].Pos[2] - pos_z);

                r2 = dx * dx + dy * dy + dz * dz;

                mass = P[no].Mass;

                h = input->Soft;
                const double otherh = FORCE_SOFTENING(no);
                if(h < otherh)
                    h = otherh;
                no = force_get_next_node(no, tree);
            }
            else			/* we have an  internal node */
            {
                struct NODE *nop;
                if(node_is_pseudo_particle(no, tree))	/* pseudo particle */
                {
                    if(lv->mode == 0)
                    {
                        if(-1 == treewalk_export_particle(lv, no))
                            return -1;
                    }
                    no = force_get_next_node(no, tree);
                    continue;
                }

                nop = &tree->Nodes[no];

                if(lv->mode == 1)
                {
                    if(nop->f.TopLevel)	/* we reached a top-level node again, which means that we are done with the branch */
                    {
                        no = -1;
                        continue;
                    }
                }

                dx = NEAREST(nop->u.d.s[0] - pos_x);
                dy = NEAREST(nop->u.d.s[1] - pos_y);
                dz = NEAREST(nop->u.d.s[2] - pos_z);

                r2 = dx * dx + dy * dy + dz * dz;

                /*This checks the distance from the node center of mass*/
                if(r2 > rcut2)
                {
                    /* check whether we can stop walking along this branch */
                    const double eff_dist = rcut + 0.5 * nop->len;

                    /*This checks whether we are also outside this region of the oct-tree*/
                    if(fabs(NEAREST(nop->center[0] - pos_x)) > eff_dist ||
                        fabs(NEAREST(nop->center[1] - pos_y)) > eff_dist ||
                            fabs(NEAREST(nop->center[2] - pos_z)) > eff_dist
                      )
                    {
                        no = nop->u.d.sibling;
                        continue;
                    }
                }

                mass = nop->u.d.mass;
                /*Check Barnes-Hut opening angle or relative opening criterion*/
                if(((All.TreeUseBH > 0 && nop->len * nop->len > r2 * All.BHOpeningAngle * All.BHOpeningAngle)) ||
                     (All.TreeUseBH == 0 && (mass * nop->len * nop->len > r2 * r2 * aold)))
                {
                    /* open cell */
                    no = nop->u.d.nextnode;
                    continue;
                }
                /* check in addition whether we lie inside the cell */
                if(fabs(NEAREST(nop->center[0] - pos_x)) < 0.60 * nop->len)
                {
                    if(fabs(NEAREST(nop->center[1] - pos_y)) < 0.60 * nop->len)
                    {
                        if(fabs(NEAREST(nop->center[2] - pos_z)) < 0.60 * nop->len)
                        {
                            no = nop->u.d.nextnode;
                            continue;
                        }
                    }
                }

                h = input->Soft;
                const double otherh = nop->u.d.MaxSoftening;
                if(h < otherh)
                {
                    h = otherh;
                    if(r2 < h * h)
                    {
                        if(nop->f.MixedSofteningsInNode)
                        {
                            no = nop->u.d.nextnode;

                            continue;
                        }
                    }
                }
                no = nop->u.d.sibling;	/* ok, node can be used */

            }

            double facpot, fac;

            const double r = sqrt(r2);

            if(r >= h)
            {
                fac = mass / (r2 * r);
                facpot = -mass / r;
            }
            else
            {
                double wp;
                const double h_inv = 1.0 / h;
                const double h3_inv = h_inv * h_inv * h_inv;
                const double u = r * h_inv;
                if(u < 0.5) {
                    fac = mass * h3_inv * (10.666666666667 + u * u * (32.0 * u - 38.4));
                    wp = -2.8 + u * u * (5.333333333333 + u * u * (6.4 * u - 9.6));
                }
                else {
                    fac =
                        mass * h3_inv * (21.333333333333 - 48.0 * u +
                                38.4 * u * u - 10.666666666667 * u * u * u - 0.066666666667 / (u * u * u));
                    wp =
                        -3.2 + 0.066666666667 / u + u * u * (10.666666666667 +
                                u * (-16.0 + u * (9.6 - 2.133333333333 * u)));
                }

                facpot = mass * h_inv * wp;
            }

            if(0 == grav_apply_short_range_window(r, &fac, &facpot)) {
                acc_x += (dx * fac);
                acc_y += (dy * fac);
                acc_z += (dz * fac);
                pot += facpot;
                ninteractions++;
            }
        }

        if(listindex < NODELISTLENGTH)
        {
            no = input->base.NodeList[listindex];
            if(no >= 0)
            {
                no = tree->Nodes[no].u.d.nextnode;	/* open it */
                listindex++;
            }
        }
    }

    output->Acc[0] = acc_x;
    output->Acc[1] = acc_y;
    output->Acc[2] = acc_z;
    output->Ninteractions = ninteractions;
    output->Potential = pot;

    lv->Ninteractions =+ ninteractions;
    if(lv->mode == 1) {
        lv->Nnodesinlist += listindex;
        lv->Nlist += 1;
    }
    return ninteractions;
}


