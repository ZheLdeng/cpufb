#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
    void load_ldr_kernel(float*, int64_t);
}

void get_cachesize(int *cache_size, int cpu_id);
#endif