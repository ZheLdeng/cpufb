#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <string>
#include <vector>
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
    void load_ldr_kernel(float*, int64_t);
    void load_neon_ld1b_kernel(float*, int, int64_t);
    void load_neon_ld1h_kernel(float*, int, int64_t);
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
    int test_L1 = 0;
    int test_L2 = 0;
    int theory_way = 0;
    int test_way = 0;
    int theory_cacheline = 0;
    int test_cacheline = 0;
};

struct CacheLatencyPoint {
    uint64_t working_set_bytes = 0;
    double latency_ns = 0;
};

struct CacheLevelEstimate {
    std::string level;
    uint64_t capacity_bytes = 0;
    double latency_ns = 0;
    double jump_ratio = 0;
};

struct CacheCurveResult {
    std::vector<CacheLatencyPoint> points;
    std::vector<CacheLevelEstimate> levels;
};

void get_reported_cache_info(struct CacheData *cache_size, int cpu_id);
CacheCurveResult measure_cache_hierarchy(struct CacheData *cache_size, int cpu_id);
void get_cachesize(struct CacheData *cache_size, int cpu_id);
void get_multiway(struct CacheData *cache_size,int cpu_id);
void get_cacheline(struct CacheData *cache_size, int cpu_id);
double get_bandwith(uint64_t looptime, double data_size, std::string type, void* bench);
#endif
