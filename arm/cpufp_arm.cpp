#include "cpubm_arm.hpp"

#include <cstring>
#include <cstdint>
#include <vector>
using namespace std;
extern "C"
{
void load_ldp_kernel(float*);
}
#ifdef NEON
#include "asm/cpufp_kernel_armv8.c"
#endif
#ifdef SVE
#include "asm/cpufp_kernel_armv9.c"
#endif
extern "C"
{
    
// #ifdef NEON
// void cpufp_kernel_armv8_fmla_f32(int64_t looptime);
// void cpufp_kernel_armv8_fmla_f64(int64_t looptime);
// #endif
// #ifdef SVE
// void cpufp_kernel_armv9_fmla_f32(int64_t looptime);
// void cpufp_kernel_armv9_fmla_f64(int64_t looptime);
// #endif
}

static void parse_thread_pool(char *sets,
    vector<int> &set_of_threads)
{
    if (sets[0] != '[')
    {
        return;
    }
    int pos = 1;
    int left = 0, right = 0;
    int state = 0;
    while (sets[pos] != ']' && sets[pos] != '\0')
    {
        if (state == 0)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                left *= 10;
                left += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                set_of_threads.push_back(left);
                left = 0;
            }
            else if (sets[pos] == '-')
            {
                right = 0;
                state = 1;
            }
        }
        else if (state == 1)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                right *= 10;
                right += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                int i;
                for (i = left; i <= right; i++)
                {
                    set_of_threads.push_back(i);
                }
                left = 0;
                state = 0;
            }
        }
        pos++;
    }
    if (sets[pos] != ']')
    {
        return;
    }
    if (state == 0)
    {
        set_of_threads.push_back(left);
    }
    else if (state == 1)
    {
        int i;
        for (i = left; i <= right; i++)
        {
            set_of_threads.push_back(i);
        }
    }
}

#include "cpufp_arm_incl.cpp"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s --thread_pool=[xxx]\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        exit(0);
    }

    if (strncmp(argv[1], "--thread_pool=", 14))
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx]\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        exit(0);
    }

    vector<int> set_of_threads;

    parse_thread_pool(argv[1] + 14, set_of_threads);
    //注册汇编
    cpufp_register_isa();
    
    cpubm_do_bench(set_of_threads);

    return 0;
}

