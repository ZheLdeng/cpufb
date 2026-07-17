#ifndef MULTIPLE_ISSUE_HPP
#define MULTIPLE_ISSUE_HPP
#include <cstdint>
extern "C"
{
    void multiple_issue(float*,int, int64_t);
#ifdef _SVE_
    void sve_multiple_issue(float*,int, int64_t);
#endif
    void sme_multiple_issue(float*,int, int64_t);
}
#endif
