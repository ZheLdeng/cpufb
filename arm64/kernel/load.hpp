#ifndef _LOAD_HPP
#define _LOAD_HPP

#include <cstdint>
extern "C"
{
    void load_ldp_kernel(float*, int, int64_t);
}

#endif