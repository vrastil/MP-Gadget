/*Tests for the cooling module. These just check that we get the same answer as Gadget-2.*/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libgadget/config.h>
#include <libgadget/physconst.h>
#include <libgadget/cosmology.h>
#include <libgadget/cooling.h>
#include <libgadget/cooling_rates.h>
#include <libgadget/allvars.h>
#include <libgadget/utils/peano.h>
#include <libgadget/partmanager.h>
#include <libgadget/utils/endrun.h>
#include "stub.h"

#define NSTEP 20
/*Pre-computed table of DoCooling outputs*/
static double unew_table[NSTEP * NSTEP] = {
200.487 , 259.669 , 336.445 , 436.021 , 565.153 , 732.596 , 949.707 , 1231.21 ,
1596.18 , 2069.37 , 2682.87 , 3478.26 , 4509.47 , 5846.43 , 7579.77 , 9826.99 ,
12740.5 , 16517.8 , 21414.9 , 27763.9 , 201.117 , 260.171 , 336.837 , 436.32 ,
565.37 , 732.742 , 949.79 , 1231.23 , 1596.15 , 2069.3 , 2682.76 , 3478.11 ,
4509.28,
5846.21 , 7579.51 , 9826.72 , 12740.2 , 16517.4 , 21414.5 , 27763.5 , 202.514 ,
261.287 , 337.711 , 436.984 , 565.851 , 733.06 , 949.96 , 1231.27 ,
1596.07 , 2069.11, 2682.45 , 3477.7 , 4508.79 , 5845.63 , 7578.87 , 9826.02 ,
12739.4 , 16516.6 , 21413.7 , 27762.6 , 205.578 , 263.751 , 339.648 , 438.456 ,
566.905 , 733.729 , 950.27 , 1231.25 , 1595.74 , 2068.47 , 2681.52 , 3476.49 ,
4507.34 , 5844.02 , 7577.16 , 9824.25 , 12737.6 , 16514.8 , 21411.7 , 27760.5 ,
212.143 , 269.099 , 343.883 , 441.669 , 569.145 , 735.011 , 950.604 , 1230.67 ,
1594.29 , 2066.16 , 2678.34 , 3472.5 , 4502.81 , 5839.29 , 7572.47 , 9819.69 ,
12733.1 , 16510.3 , 21407.2 , 27755.8 , 225.615 , 280.325 , 352.891 , 448.45 ,
573.564 , 736.826 , 949.637 , 1226.97 , 1588.02 , 2057.32 , 2666.78 , 3458.75 ,
4488.37 , 5825.62 , 7560.07 , 9808.32 , 12722.4 , 16499.8 , 21396.7 , 27745.1 ,
251.373 , 302.473 , 370.911 , 461.561 , 580.484 , 735.954 , 939.92 , 1208.83 ,
1562.68 , 2024.56 , 2625.65 , 3412.84 , 4445.4 , 5789.29 , 7529.44 , 9781.39 ,
12697.5 , 16475.9 , 21372.9 , 27720.8 , 295.781 , 341.395 , 401.923 , 480.928 ,
582.908 , 715.357 , 892.24 , 1136.68 , 1473.36 , 1915.65 , 2489.81 , 3269.95 ,
4327.93 , 5699.49 , 7457.46 , 9719.58 , 12641.1 , 16421.9 , 21319.4 , 27666.4 ,
357.205 , 391.458 , 434.545 , 488.028 , 554.342 , 638.34 , 750.975 , 919.709 ,
1204.87 , 1616.65 , 2105.83 , 2824.91 , 4004.01 , 5480.35 , 7290.38 , 9578.92 ,
12513.9 , 16300.7 , 21199.5 , 27544.5 , 392.961 , 409.313 , 429.35 , 453.763 ,
483.438 , 519.698 , 564.88 , 623.704 , 708.275 , 868.722 , 1383.69 , 1824.56 ,
2497.55 , 4878.49 , 6889.78 , 9255.59 , 12226.2 , 16028.4 , 20930.9 , 27272 ,
372.506 , 378.46 , 385.874 , 395.055 , 406.298 , 419.979 , 436.601 , 456.735 ,
481.285 , 511.718 , 550.876 , 605.771 , 705.971 , 1558.81 , 5729.6 , 8476.84 ,
11565.7 , 15414.5 , 20329.9 , 26663.8 , 330.933 , 333.094 , 335.841 , 339.294 ,
343.621 , 348.951 , 355.516 , 363.531 , 373.177 , 384.77 , 398.597 , 415.093 ,
434.9 , 459.087 , 489.694 , 531.724 , 9950.23 , 14007 , 18981.8 , 25310.8 ,
291.807 , 292.639 , 293.701 , 295.036 , 296.746 , 298.91 , 301.603 , 304.937 ,
309.049 , 314.077 , 320.146 , 327.413 , 336.004 , 346.122 , 357.946 , 371.775 ,
388.043 , 10375.6 , 15897.3 , 22310.5 , 259.84 , 260.164 , 260.584 , 261.128 ,
261.793 , 262.661 , 263.778 , 265.165 , 266.921 , 269.117 , 271.827 , 275.144 ,
279.183 , 284.025 , 289.74 , 296.442 , 304.247 , 313.244 , 323.586 , 15355 ,
232.655 , 232.783 , 232.95 , 233.166 , 233.446 , 233.684 , 234.286 , 234.86 ,
235.606 , 236.579 , 237.76 , 239.271 , 241.152 , 243.453 , 246.245 , 249.616 ,
253.634 , 258.36 , 263.845 , 270.19 , 207.222 , 207.279 , 207.353 , 207.449 ,
207.573 , 207.734 , 207.943 , 208.214 , 208.55 , 208.972 , 209.522 , 210.239 ,
211.11 , 212.235 , 213.605 , 215.314 , 217.4 , 219.921 , 222.946 , 226.47 ,
182.401 , 182.43 , 182.467 , 182.516 , 182.579 , 182.66 , 182.757 , 182.883 ,
183.046 , 183.272 , 183.507 , 183.891 , 184.357 , 184.92 , 185.642 , 186.558 ,
187.685 , 189.072 , 190.771 , 192.797 , 159.07 , 159.086 , 159.106 , 159.132 ,
159.164 , 159.207 , 159.17 , 159.336 , 159.43 , 159.551 , 159.71 , 159.916 ,
160.182 , 160.498 , 160.908 , 161.395 , 162.105 , 162.929 , 163.95 , 165.213 ,
138.815 , 138.825 , 138.837 , 138.854 , 138.875 , 138.902 , 138.938 , 138.984 ,
139.044 , 139.122 , 139.225 , 139.354 , 139.518 , 139.737 , 139.976 , 140.315 ,
140.751 , 141.276 , 141.947 , 142.768 , 122.453 , 122.46 , 122.468 , 122.479 ,
122.494 , 122.512 , 122.536 , 122.568 , 122.608 , 122.661 , 122.729 , 122.824 ,
122.932 , 123.081 , 123.269 , 123.496 , 123.789 , 124.171 , 124.645 , 125.227
};

static double tcool_table[NSTEP * NSTEP] = {
    0 , 0 , 0 , 0 , 0 , 0 , 3914.21 , 1933.48 , 1506.85 , 1352.06 , 1293.25 ,
1278.84 , 1287.8 , 1309.44 , 1337.47 , 1367.87 , 1398.08 , 1426.53 , 1452.38 ,
1475.29 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2445.45 , 1354.28 , 1099.91 , 1015.79 ,
997.188 , 1012.05 , 1046.36 , 1091.99 , 1143.19 , 1195.77 , 1246.81 , 1294.43 ,
1337.65 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 3904.24 , 998.667 , 721.282 , 643.403 ,
629.351 , 648.465 , 689.762 , 746.868 , 814.328 , 887.145 , 961.162 , 1033.31 ,
1101.53 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2806.16 , 486.453 , 341.96 , 303.947 ,
300.443 , 318.113 , 353.685 , 405.105 , 469.31 , 542.489 , 620.994 , 701.849 ,
782.766 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 367.187 , 151.252 , 117.355 , 108.236 ,
110.257 , 122.292 , 145.38 , 179.814 , 224.482 , 277.506 , 337.04 , 401.622 ,
470.121 , 0 , 0 , 0 , 0 , 0 , 0 , 1474.23 , 55.5711 , 37.7668 , 33.2225 ,
32.3291 , 34.5742 , 41.336 , 53.9285 , 72.8328 , 97.7401 , 127.999 , 163.024 ,
202.464 , 246.169 , 0 , 0 , 0 , 0 , 0 , 0 , 18.2402 , 10.5357 , 9.22123 ,
8.96195 , 9.15556 , 10.508 , 13.9897 , 20.2526 , 29.5519 , 41.845 , 56.9636 ,
74.7786 , 95.2863 , 118.612 , 0 , 0 , 0 , 0 , 0 , 6.74106 , 2.78324 , 2.36006 ,
2.42939 , 2.49955 , 2.65339 , 3.38303 , 5.10226 , 8.06539 , 12.402 , 18.1342 ,
25.2294 , 33.6692 , 43.4962 , 54.8248 , 0 , 0 , 0 , 0 , 5.19556 , 0.79833 ,
0.582898 , 0.622787 , 0.710667 , 0.7279 , 0.816923 , 1.20026 , 2.02061 , 3.38372
, 5.35794 , 7.96628 , 11.2051 , 15.0756 , 19.6072 , 24.8648 , 0 , 0 , 0 , 0 ,
0.291752 , 0.153164 , 0.150644 , 0.193044 , 0.223622 , 0.218621 , 0.275154 ,
0.467464 , 0.847883 , 1.4655 , 2.35479 , 3.52935 , 4.99004 , 6.73939 , 8.79272 ,
11.1821 , 0 , 0 , 0 , 0.171693 , 0.0482873 , 0.0371044 , 0.0471276 , 0.0676174 ,
0.0719382 , 0.0691284 , 0.102491 , 0.194341 , 0.367519 , 0.645239 , 1.04395 ,
1.57048 , 2.22574 , 3.01126 , 3.93435 , 5.00995 , 0 , 0 , 4.48992 , 0.0210546 ,
0.0104752 , 0.0110333 , 0.0172379 , 0.0253594 , 0.0235216 , 0.0239471 , 0.041405
, 0.0838444 , 0.161899 , 0.286327 , 0.464722 , 0.700288 , 0.993536 , 1.34524 ,
1.75875 , 2.24087 , 0 , 0 , 0.0159506 , 0.00396304 , 0.00278074 , 0.00391108 ,
0.00697368 , 0.00987502 , 0.00814066 , 0.00915845 , 0.0175903 , 0.036853 ,
0.0718627 , 0.127519 , 0.207264 , 0.312561 , 0.443662 , 0.600928 , 0.785872 ,
1.00156 , 0 , 0 , 0.00240333 , 0.000897738 , 0.000891014 , 0.00155978 ,
0.00297251 , 0.00398512 , 0.00306937 , 0.00376809 , 0.00767383 , 0.0163414 ,
0.0320089 , 0.0568848 , 0.0925173 , 0.139567 , 0.19815 , 0.26843 , 0.351089 ,
0.447501 , 0 , 0.00305843 , 0.000463326 , 0.000243052 , 0.000332176 ,
0.000662319 , 0.00129813 , 0.00167006 , 0.00124694 , 0.00161718 , 0.00339088 ,
0.0072754 , 0.0142797 , 0.0253944 , 0.0413132 , 0.0623324 , 0.0885047 , 0.119905
, 0.156836 , 0.199915 , 0 , 0.000421393 , 0.000104831 , 7.92311e-05 ,
0.000136582 , 0.000289784 , 0.000573534 , 0.000720504 , 0.000531159 ,
0.000709083 , 0.00150727 , 0.003245 , 0.00637487 , 0.0113402 , 0.0184514 ,
0.0278409 , 0.0395325 , 0.0535597 , 0.0700584 , 0.0893037 , 0.000749025 ,
7.7081e-05 , 2.8637e-05 , 3.01771e-05 , 5.93687e-05 , 0.000128269 , 0.000254858
, 0.000316375 , 0.000232006 , 0.00031407 , 0.000671796 , 0.00144853 , 0.00284683
, 0.0050649 , 0.00824142 , 0.0124357 , 0.0176583 , 0.0239243 , 0.0312944 ,
0.0398915 , 9.08705e-05 , 1.76614e-05 , 9.68947e-06 , 1.26588e-05 , 2.65203e-05
, 5.70792e-05 , 0.000113566 , 0.000140192 , 0.000102576 , 0.000139757 ,
0.000299786 , 0.000646843 , 0.00127149 , 0.00226229 , 0.0036812 , 0.00555474 ,
0.00788764 , 0.0106866 , 0.0139788 , 0.0178191 , 1.7918e-05 , 5.27643e-06 ,
3.80419e-06 , 5.56985e-06 , 1.19249e-05 , 2.54556e-05 , 5.06728e-05 , 6.2393e-05
, 4.56073e-05 , 6.23206e-05 , 0.00013385 , 0.000288896 , 0.000567923 , 0.0010105
, 0.00164431 , 0.0024812 , 0.00352327 , 0.00477353 , 0.0062441 , 0.00795952 ,
4.7893e-06 , 1.93665e-06 , 1.61025e-06 , 2.48894e-06 , 5.3579e-06 , 1.13627e-05
, 2.26237e-05 , 2.78241e-05 , 2.03297e-05 , 2.78163e-05 , 5.97771e-05 ,
0.000129037 , 0.000253676 , 0.00045137 , 0.000734484 , 0.00110831 , 0.00157379 ,
0.00213226 , 0.00278914 , 0.00355539
};

struct global_data_all_processes All;
struct part_manager_type PartManager[1];
/* Check that DoCooling and GetCoolingTime both return
 * a stable value over a wide range of internal energies and densities.*/
static void test_DoCooling(void ** state)
{
    int i, j;
    struct cooling_params coolpar;
    coolpar.CMBTemperature = 2.7255;
    coolpar.PhotoIonizeFactor = 1;
    coolpar.SelfShieldingOn = 0;
    coolpar.fBar = 0.17;
    coolpar.PhotoIonizationOn = 1;
    coolpar.recomb = Cen92;
    coolpar.cooling = KWH92;
    coolpar.HeliumHeatOn = 0;
    coolpar.HeliumHeatAmp = 1.;
    coolpar.HeliumHeatExp = 0.;
    coolpar.HeliumHeatThresh = 10;
    coolpar.MinGasTemp = 100;
    coolpar.UVRedshiftThreshold = -1;
    coolpar.HydrogenHeatAmp = 0;
    coolpar.rho_crit_baryon = 0.045 * 3.0 * pow(0.7*HUBBLE,2.0) /(8.0*M_PI*GRAVITY);

    char * TreeCool = GADGET_TESTDATA_ROOT "/examples/TREECOOL_ep_2018p";
    char * MetalCool = "";
    char * UVFluc = "";

    /*unit system*/
    double HubbleParam = 0.7;
    double UnitDensity_in_cgs = 6.76991e-22;
    double UnitTime_in_s = 3.08568e+16;
    double UnitMass_in_g = 1.989e+43;
    double UnitLength_in_cm = 3.08568e+21;
    double UnitEnergy_in_cgs = UnitMass_in_g  * pow(UnitLength_in_cm, 2) / pow(UnitTime_in_s, 2);

    struct cooling_units coolunits;
    coolunits.CoolingOn = 1;
    coolunits.density_in_phys_cgs = UnitDensity_in_cgs * HubbleParam * HubbleParam;
    coolunits.uu_in_cgs = UnitEnergy_in_cgs / UnitMass_in_g;
    coolunits.tt_in_s = UnitTime_in_s / HubbleParam;

    Cosmology CP = {0};
    CP.OmegaCDM = 0.3;
    CP.OmegaBaryon = coolpar.fBar * CP.OmegaCDM;
    CP.HubbleParam = HubbleParam;
    set_coolpar(coolpar);
    init_cooling(TreeCool, MetalCool, UVFluc, coolunits, &CP);
    struct UVBG uvbg = get_global_UVBG(0);
    assert_true(fabs(uvbg.epsH0/3.65296e-25 -1) < 1e-5);
    assert_true(fabs(uvbg.epsHe0/3.98942e-25 -1) < 1e-5);
    assert_true(fabs(uvbg.epsHep/3.33253e-26 -1) < 1e-5);

    double umax = 36000, umin = 200;
    double dmax = 1e-2, dmin = 1e-9;

    double ne= 1.0;
    /*Check two particular values*/
    double tcool = GetCoolingTime(0, 949.755, 7.07946e-06, &uvbg, &ne, 0);
    assert_true(fabs(tcool/ 0.0172379) -1 < 1e-3);
    double unew = DoCooling(0,  9828.44, 7.07946e-06, 0.2, &uvbg, &ne, 0);
    assert_true(fabs(unew/ 531.724) -1 < 1e-3);

    double dt = 0.2;
    for(i=0; i < NSTEP; i++)
    {
        double dens = exp(log(dmin) +  i * (log(dmax) - log(dmin)) / 1. /NSTEP);
        for (j = 0; j<NSTEP; j++)
        {
            double ne=1.0, ne2=1.0;
            double uu = exp(log(umin) +  j * (log(umax) - log(umin)) / 1. /NSTEP);
            double tcool = GetCoolingTime(0, uu, dens, &uvbg, &ne2, 0);
            double unew = DoCooling(0, uu, dens, dt, &uvbg, &ne, 0);
            assert_false(isnan(unew));
//             message(0, "d = %g u = %g tcool = %g tcool_table = %g unew = %g ne_after = %g unew_table = %g\n", dens, uu, tcool, tcool_table[i*NSTEP + j], unew, ne, unew_table[i*NSTEP+j]);
            assert_true(fabs(unew/unew_table[i*NSTEP + j] - 1) < 5e-3);
            assert_true(fabs(1/(1e-20 + tcool) - 1./(1e-20 + tcool_table[i*NSTEP + j])) < 1 || fabs((1e-20 + tcool)/(1e-20 + tcool_table[i*NSTEP + j]) - 1) < 2e-2);
            /*Make the tables*/
            //printf("%g , ", unew);
           //printf("%g , ", tcool);

        }
    }
//    printf("\n");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_DoCooling),

    };
    return cmocka_run_group_tests_mpi(tests, NULL, NULL);
}
