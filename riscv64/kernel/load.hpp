#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
#include <vector>
extern std::vector<double> freq;

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

void get_cachesize(struct CacheData *cache_size, int cpu_id);
void get_multiway(struct CacheData *cache_size,int cpu_id);
void get_cacheline(struct CacheData *cache_size, int cpu_id);
double get_bandwith(uint64_t looptime, double data_size, std::string type, void* bench);
#endif