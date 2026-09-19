#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "cache_curve.hpp"

struct tpool;
typedef struct tpool tpool_t;
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
    void load_ldr_kernel(float*, int64_t);
    void load_neon_ld1b_kernel(float*, int, int64_t);
    void load_neon_ld1h_kernel(float*, int, int64_t);
    void load_neon_ld1h_4x1_kernel(float*, int, int64_t);
    void load_ldrq_4x1_offset_kernel(float*, int, int64_t);
    void load_neon_ld1w_kernel(float*, int, int64_t);
    void load_neon_ld1d_kernel(float*, int, int64_t);
#ifdef _SVE_
    uint64_t load_sve_vector_bytes(void);
    void load_sve_ld1b_kernel(float*, int, int64_t);
    void load_sve_ld1h_kernel(float*, int, int64_t);
    void load_ld1w_kernel(float*, int, int64_t);
    void load_sve_ld1d_kernel(float*, int, int64_t);
#endif
#ifdef _SME_
    uint64_t load_sme_vector_bytes(void);
    void sme_ldr_kernel(float*, int ,int);
    void sme_ldr2_kernel(float*, int ,int);
    void sme_ld1wV_kernel(float*, int ,int);
    void sme_ld1wH_kernel(float*, int ,int);
#endif
#ifdef _SME2_
    void sme_ld1w_kernel(float*, int ,int);
#endif
}
struct CacheData {
    int theory_L1 = 0;
    int theory_L2 = 0;
    std::string theory_L1_source;
    std::string theory_L2_source;
    int test_L1 = 0;
    int test_L2 = 0;
    int theory_way = 0;
    int test_way = 0;
    int theory_cacheline = 0;
    int test_cacheline = 0;
};

void get_reported_cache_info(struct CacheData *cache_size, int cpu_id);
CacheCurveResult measure_cache_hierarchy(struct CacheData *cache_size, int cpu_id);
void get_cache_capacities(struct CacheData *cache_size, int cpu_id);
void get_multiway(struct CacheData *cache_size,int cpu_id);
void get_cacheline(struct CacheData *cache_size, int cpu_id);
// Cache-resident load bandwidth.  bytes_per_cycle is the per-core mean and is
// 0 when no cycle count or frequency is available; gb_per_second (1e9 B/s) is
// the aggregate over thread_num workers and never depends on the clock probe.
struct LoadBandwidth {
    double gb_per_second = 0.0;
    double bytes_per_cycle = 0.0;
    uint64_t workset_bytes = 0;
    size_t thread_num = 1;
    std::string cycle_source;
};
LoadBandwidth get_bandwith(uint64_t looptime, double data_size, std::string type, void* bench, tpool_t* tm);
#endif
