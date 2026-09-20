#ifndef CPUFB_X64_MULTIPLE_ISSUE_HPP
#define CPUFB_X64_MULTIPLE_ISSUE_HPP
#include <cstdint>
extern "C"
{
    void multiple_issue(float *, int, int64_t);
    void multiple_issue_avx(float *, int, int64_t);
    void multiple_issue_avx512(float *, int, int64_t);
}
#endif
