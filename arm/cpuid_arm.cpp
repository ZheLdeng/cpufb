#include <fstream>
#include <stdlib.h>

using namespace std;

typedef enum
{
    _CPUID_ARM_V8_FMLA         = 0x1,
    _CPUID_ARM_V9_FMLA         = 0x2,
} cpuid_arm_feature_t;


static unsigned int feat = 0;

static unsigned int len = 128;

#define SET_FEAT(feat_mask) { feat |= (feat_mask); }

static void cpuid_arm_init()
{
    if (feat != 0)
    {
        return;
    }

    #ifdef NEON
    SET_FEAT(_CPUID_ARM_V8_FMLA);
    #endif
    #ifdef SVE
    SET_FEAT(_CPUID_ARM_V9_FMLA);
    #endif
}

static int cpuid_arm_support(cpuid_arm_feature_t isa)
{
    return ((feat & isa) != 0);
}

// void gen_build_kernel_sh()
// {
//     ofstream bf("build_kernel_arm.sh");
//     if (cpuid_arm_support(_CPUID_ARM_V8_FMLA))
//     {
//         bf << "g++ -c asm/cpufp_kernel_armv8.c" << endl;
//     }   
//     if (cpuid_arm_support(_CPUID_ARM_V9_FMLA))
//     {
//         bf << "g++ -c asm/cpufp_kernel_armv9.c" << endl;
//     }
//     bf.close();
// }

void gen_cpufp_include_cpp()
{
    ofstream cf("cpufp_arm_incl.cpp");
    cf << "static void cpufp_register_isa()" << endl << "{" << endl;
    if (cpuid_arm_support(_CPUID_ARM_V8_FMLA))
    {
        cf << "    reg_new_isa(\"ARMV8_FMLA\", \"FP32\", \"GFLOPS\"," << endl;
        cf << "        0x80000000LL, 256LL," << endl;
        cf << "        cpufp_kernel_armv8_fmla_f32);" << endl;
        cf << "    reg_new_isa(\"ARMV8_FMLA\", \"FP64\", \"GFLOPS\"," << endl;
        cf << "        0x80000000LL, 128LL," << endl;
        cf << "        cpufp_kernel_armv8_fmla_f64);" << endl;


        cf << "    reg_new_isa(\"LOAD_L1\", \"FP32\", \"Byte/Cycle\"," << endl;
        cf << "        0x186A0LL, 32LL," << endl;
        cf << "        cpufp_kernel_neon_bandwith_l1cache);" << endl;

        cf << "    reg_new_isa(\"LOAD_L2\", \"FP32\", \"Byte/Cycle\"," << endl;
        cf << "        0x186A0LL, 2048LL," << endl;
        cf << "        cpufp_kernel_neon_bandwith_l2cache);" << endl;
    }
    if (cpuid_arm_support(_CPUID_ARM_V9_FMLA))
    {
        cf << "    reg_new_isa(\"ARMV9_FMLA\", \"FP32\", \"GFLOPS\"," << endl;
        cf << "        0x200000LL, "<< len / 32 * 32 * 2<<"," << endl;
        cf << "        cpufp_kernel_armv9_fmla_f64);" << endl;
        cf << "    reg_new_isa(\"ARMV9_FMLA\", \"FP64\", \"GFLOPS\"," << endl;
        cf << "        0x200000LL, "<< len / 64 * 32 * 2<<"," << endl;
        cf << "        cpufp_kernel_armv9_fmla_f64);" << endl;
    }
    cf << "}" << endl;
    cf.close();
}

void gen_link()
{   
    ofstream lf("link_arm.sh");
    lf << "g++ -O3 -o cpufp table.o smtl.o cpubm_arm.o ldp_asm.o cpufp_arm.o -lpthread";
    // if (cpuid_arm_support(_CPUID_ARM_V8_FMLA))
    // {
    //     lf << " cpufp_kernel_armv8.o";
    // }
    // if (cpuid_arm_support(_CPUID_ARM_V9_FMLA))
    // {
    //     lf << " cpufp_kernel_armv9.o";
    // }
    lf << endl;
    lf.close();
}

int main(int argc, char *argv[])
{
    cpuid_arm_init();
    if (argc == 2)
    {
        len = atoi(argv[1]);
    }
    // gen_build_kernel_sh();
    gen_cpufp_include_cpp();
    gen_link();

    return 0;
}

