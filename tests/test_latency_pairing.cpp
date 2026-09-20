#include "cli.hpp"

#include <iostream>
#include <string>

using namespace cpufb::cli;

namespace {

int failures = 0;

void expect(bool condition, const std::string &message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

BenchmarkInfo benchmark(
    const char *isa, const char *instruction, const char *metric = "FLOPS")
{
    return BenchmarkInfo(isa, instruction, metric);
}

void test_order_independent_pairing()
{
    BenchmarkCatalog catalog;
    catalog.push_back(benchmark("i8mm", "mmla(s32,s8,s8)_latency", "OPS"));
    catalog.push_back(benchmark("asimd", "fmla.vv(f32,f32,f32)"));
    catalog.push_back(benchmark("i8mm", "mmla(s32,s8,s8)", "OPS"));
    catalog.push_back(benchmark("sve", "sve_fmla.vv(f32,f32,f32)_latency"));
    catalog.push_back(benchmark("ASIMD", "fmla.vv(f32,f32,f32)_latency"));
    catalog.push_back(benchmark("SVE", "sve_fmla.vv(f32,f32,f32)"));
    catalog.push_back(benchmark("asimd", "sve_fmla.vv(f32,f32,f32)"));
    catalog.push_back(benchmark("asimd", "orphan_latency"));

    pair_benchmark_latencies(catalog);

    expect(catalog[0].is_latency,
        "a latency registered first must be classified as latency");
    expect(catalog[0].pair_index == 2 && catalog[2].pair_index == 0,
        "a latency registered before throughput must pair symmetrically");
    expect(catalog[1].pair_index == 4 && catalog[4].pair_index == 1,
        "non-adjacent entries must pair independent of registration order");
    expect(catalog[3].pair_index == 5 && catalog[5].pair_index == 3,
        "ISA matching must be case-insensitive");
    expect(catalog[6].pair_index == -1,
        "a latency from another ISA must not be used as a fallback");
    expect(
        catalog[7].pair_index == -1, "an orphan latency must remain unpaired");

    pair_benchmark_latencies(catalog);
    expect(catalog[0].pair_index == 2 && catalog[2].pair_index == 0,
        "rebuilding pairing metadata must be idempotent");
}

void test_ambiguous_and_non_compute_entries()
{
    BenchmarkCatalog duplicate_latency;
    duplicate_latency.push_back(benchmark("asimd", "duplicate"));
    duplicate_latency.push_back(benchmark("asimd", "duplicate_latency"));
    duplicate_latency.push_back(benchmark("ASIMD", "duplicate_latency"));
    pair_benchmark_latencies(duplicate_latency);

    expect(duplicate_latency[0].pair_index == -1,
        "ambiguous duplicate latency entries must not be guessed");
    expect(duplicate_latency[1].pair_index == -1 &&
            duplicate_latency[2].pair_index == -1,
        "ambiguous latency entries must remain unpaired");

    BenchmarkCatalog non_compute;
    non_compute.push_back(benchmark("L1 Cache", "load", "Byte/Cycle"));
    non_compute.push_back(benchmark("L1 Cache", "load_latency", "Byte/Cycle"));
    pair_benchmark_latencies(non_compute);

    expect(non_compute[0].pair_index == -1 && non_compute[1].pair_index == -1,
        "only compute benchmarks may form throughput/latency pairs");
}

} // namespace

int main()
{
    test_order_independent_pairing();
    test_ambiguous_and_non_compute_entries();
    if (failures != 0) return 1;
    std::cout << "latency pairing tests passed\n";
    return 0;
}
