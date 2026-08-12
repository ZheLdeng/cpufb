#include "cache_topology.hpp"

#include <cstdint>
#include <iostream>

namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;

bool expect_equal(const char *name, std::uint64_t actual, std::uint64_t expected)
{
    if (actual == expected) return true;
    std::cerr << name << ": got " << actual << ", expected " << expected << '\n';
    return false;
}

} // namespace

int main()
{
    cpufb::LastLevelCacheInfo unavailable;
    if (!expect_equal("fallback", cpufb::recommended_stream_workset_bytes(unavailable),
            256 * kMiB))
        return 1;

    cpufb::LastLevelCacheInfo small_cache;
    small_cache.bytes = 32 * kMiB;
    if (!expect_equal("minimum floor",
            cpufb::recommended_stream_workset_bytes(small_cache), 256 * kMiB))
        return 1;

    cpufb::LastLevelCacheInfo large_cache;
    large_cache.bytes = 128 * kMiB;
    if (!expect_equal("four-times cache",
            cpufb::recommended_stream_workset_bytes(large_cache), 512 * kMiB))
        return 1;

    if (cpufb::format_cache_capacity(16 * kMiB) != "16 MiB") {
        std::cerr << "capacity formatting failed\n";
        return 1;
    }
    return 0;
}
