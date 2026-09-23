#ifndef CPUFB_ARM64_LOAD_HPP
#define CPUFB_ARM64_LOAD_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "cache_curve.hpp"

struct tpool;
typedef struct tpool tpool_t;
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float *, int, int64_t);
    void load_ldr_kernel(float *, int64_t);
    void load_neon_ld1b_kernel(float *, int, int64_t);
    void load_neon_ld1h_kernel(float *, int, int64_t);
    void load_neon_ld1h_4x1_kernel(float *, int, int64_t);
    void load_ldrq_4x1_offset_kernel(float *, int, int64_t);
    void load_neon_ld1w_kernel(float *, int, int64_t);
    void load_neon_ld1d_kernel(float *, int, int64_t);
#ifdef _SVE_
    uint64_t load_sve_vector_bytes(void);
    void load_sve_ld1b_kernel(float *, int, int64_t);
    void load_sve_ld1h_kernel(float *, int, int64_t);
    void load_ld1w_kernel(float *, int, int64_t);
    void load_sve_ld1d_kernel(float *, int, int64_t);
#endif
#ifdef _SME_
    uint64_t load_sme_vector_bytes(void);
    void sme_ldr_kernel(float *, int, int);
    void sme_ldr2_kernel(float *, int, int);
    void sme_ld1wV_kernel(float *, int, int);
    void sme_ld1wH_kernel(float *, int, int);
#endif
#ifdef _SME2_
    void sme_ld1w_kernel(float *, int, int);
#endif
}
struct CacheData
{
    int theory_L1 = 0;
    int theory_L2 = 0;
    std::string theory_L1_source;
    std::string theory_L2_source;
    int test_L1 = 0;
    int test_L2 = 0;
    // Third level from the latency curve, 0 when there is none, and whether
    // the curve reached memory latency (so the level list is complete).
    int test_L3 = 0;
    double memory_latency_ns = 0;
    bool hierarchy_complete = false;
    // Per level: empty for a clean step, the uncertainty range for a soft
    // one, the reason the capacity is withheld for a gradual one.
    std::string test_L1_note, test_L2_note, test_L3_note;
    int theory_way = 0;
    int test_way = 0;
    // Bytes one L1 way spans, from set conflicts; test_way x this is an L1
    // capacity that does not come from the latency curve.
    size_t test_way_bytes = 0;
    // L1 line size read from set indexing, immune to neighbour prefetch.
    int test_line_from_sets = 0;
    int theory_l2_way = 0;
    int test_l2_way = 0;
    int theory_l2_line = 0;
    int test_l2_line = 0;
    int theory_cacheline = 0;
    int test_cacheline = 0;
    // False when the curve fell or never reached memory; its capacities are
    // then not used to size anything else.
    bool curve_trusted = true;
};

void get_reported_cache_info(struct CacheData *cache_size, int cpu_id);
cpufb::CacheCurveResult measure_cache_hierarchy(
    struct CacheData *cache_size, int cpu_id);
void get_cache_capacities(struct CacheData *cache_size, int cpu_id);
void get_multiway(struct CacheData *cache_size, int cpu_id);
void get_cacheline(struct CacheData *cache_size, int cpu_id);
#endif
