#ifndef CPUFB_ARM64_MULTIPLE_ISSUE_HPP
#define CPUFB_ARM64_MULTIPLE_ISSUE_HPP
#include <cstdint>
extern "C"
{
    void multiple_issue(float*,int, int64_t);
    void neon_multiple_issue(float*,int, int64_t);
#ifdef _SVE_
    void sve_multiple_issue(float*,int, int64_t);
    void sve_scalar_add_5_1(float*,int, int64_t);
    void sve_scalar_add_5_2(float*,int, int64_t);
    void sve_scalar_add_5_3(float*,int, int64_t);
    void sve_scalar_add_5_4(float*,int, int64_t);
    void sve_scalar_add_5_5(float*,int, int64_t);
    void sve_scalar_add_5_6(float*,int, int64_t);
#endif
    void sme_multiple_issue(float*,int, int64_t);
}
#endif
