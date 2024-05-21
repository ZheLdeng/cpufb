#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <vector>
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
    void load_ldr_kernel(float*, int64_t);
}

void get_cachesize(int *cache_size, int cpu_id);
int get_multiway();
double get_bandwith(uint64_t looptime, int data_size);
#endif