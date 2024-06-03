#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <vector>
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
    void load_ldr_kernel(float*, int64_t);
#ifdef _SVE_LD1W_
    void load_ld1w_kernel(float*, int ,int);
#endif
}
struct CacheData {
    int theory_L1 = 0;
    int theory_L2 = 0;
    int test_L1 = 0;
    int test_L2 = 0;
};

void get_cachesize(struct CacheData *cache_size, int cpu_id);
int get_multiway();
double get_bandwith(uint64_t looptime, double data_size, std::string type);
#endif