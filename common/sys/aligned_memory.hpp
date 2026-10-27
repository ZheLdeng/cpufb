#ifndef CPUFB_ALIGNED_MEMORY_HPP
#define CPUFB_ALIGNED_MEMORY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace cpufb {

inline int aligned_allocate(void **allocation, std::size_t alignment,
    std::size_t bytes)
{
#ifdef _WIN32
    *allocation = _aligned_malloc(bytes, alignment);
    return *allocation == nullptr ? ENOMEM : 0;
#else
    return posix_memalign(allocation, alignment, bytes);
#endif
}

inline void aligned_deallocate(void *allocation)
{
#ifdef _WIN32
    _aligned_free(allocation);
#else
    std::free(allocation);
#endif
}

} // namespace cpufb

#endif // CPUFB_ALIGNED_MEMORY_HPP
