#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <string>
#include <vector>
extern std::vector<double> freq;
extern "C"
{
    void load_movups_kernel(float*, int64_t);
    void load_vmovups_kernel(float*, int, int64_t);
    void load_movss_stream_kernel(float*, int, int64_t);
    void load_movups_xmm_kernel(float*, int, int64_t);
    void load_vmovups_zmm_kernel(float*, int, int64_t);
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
    // test_* hold raw probe results only; 0 means "not observed".
};

// Cache-resident load bandwidth.  bytes_per_cycle is 0 when no cycle rate is
// available; gb_per_second (1e9 B/s) never depends on the frequency probe.
struct LoadBandwidth {
    double gb_per_second = 0.0;
    double bytes_per_cycle = 0.0;
    uint64_t workset_bytes = 0;
};

// Pins the calling thread; returns false (after a warning) on failure.
bool bind_current_thread(int cpu_id);

void get_cachesize(struct CacheData *cache_size, int cpu_id);
void get_multiway(struct CacheData *cache_size,int cpu_id);
void get_cacheline(struct CacheData *cache_size, int cpu_id);
void get_theory_cache(struct CacheData *cache_size, int cpu_id);
LoadBandwidth get_bandwith(uint64_t looptime, double data_size, std::string type);
#endif
