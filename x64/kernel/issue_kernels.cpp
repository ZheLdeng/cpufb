// Tables of the x86-64 issue-probe kernels, built from issue_classes.def with
// the same grid macros the assembly uses.

#include "multiple_issue.hpp"

#include <cstring>

#include "issue_layout.h"
#include "../runtime_features.h"

using cpufb::IssueClass;
using cpufb::IssueMix;
using cpufb::IssueMixKernel;
using cpufb::IssueProbeInput;

extern "C"
{
#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature)       \
    void issue_##name##_pure_h(const void *, int64_t);                         \
    void issue_##name##_pure_f(const void *, int64_t);
#define ISSUE_PAIR_DECL(A, B, a, b)                                            \
    void issue_##A##_##B##_##a##x##b##_il(const void *, int64_t);              \
    void issue_##A##_##B##_##a##x##b##_bl(const void *, int64_t)
#define ISSUE_TRIPLE_DECL(A, B, C, a, b, c)                                    \
    void issue_##A##_##B##_##C##_##a##x##b##x##c##_il(const void *, int64_t)
#define ISSUE_PAIR(A, B, label) ISSUE_RATIO_GRID(ISSUE_PAIR_DECL, A, B, ;);
#define ISSUE_TRIPLE(A, B, C, label)                                           \
    ISSUE_TRIPLE_GRID(ISSUE_TRIPLE_DECL, A, B, C, ;);
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_PAIR_DECL
#undef ISSUE_TRIPLE_DECL
}

namespace {

const IssueClass kClasses[] = {
#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature)       \
    {#name, instruction, unit, ISSUE_BANK_##bank, ISSUE_MODE_##mode, feature,  \
        issue_##name##_pure_h, issue_##name##_pure_f},
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
};

#define ISSUE_PAIR_ENTRY(A, B, a, b)                                           \
    {                                                                          \
        {a, b, 0}, issue_##A##_##B##_##a##x##b##_il,                           \
            issue_##A##_##B##_##a##x##b##_bl                                   \
    }
#define ISSUE_TRIPLE_ENTRY(A, B, C, a, b, c)                                   \
    {                                                                          \
        {a, b, c}, issue_##A##_##B##_##C##_##a##x##b##x##c##_il, nullptr       \
    }
#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature)
#define ISSUE_PAIR(A, B, label)                                                \
    const IssueMixKernel kMix_##A##_##B[] = {                                  \
        ISSUE_RATIO_GRID(ISSUE_PAIR_ENTRY, A, B, ISSUE_COMMA)};
#define ISSUE_TRIPLE(A, B, C, label)                                           \
    const IssueMixKernel kMix_##A##_##B##_##C[] = {                            \
        ISSUE_TRIPLE_GRID(ISSUE_TRIPLE_ENTRY, A, B, C, ISSUE_COMMA)};
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_PAIR_ENTRY
#undef ISSUE_TRIPLE_ENTRY

const IssueMix kMixes[] = {
#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature)
#define ISSUE_PAIR(A, B, label)                                                \
    {label, {#A, #B, nullptr}, 2, kMix_##A##_##B,                              \
        sizeof(kMix_##A##_##B) / sizeof(kMix_##A##_##B[0])},
#define ISSUE_TRIPLE(A, B, C, label)                                           \
    {label, {#A, #B, #C}, 3, kMix_##A##_##B##_##C,                             \
        sizeof(kMix_##A##_##B##_##C) / sizeof(kMix_##A##_##B##_##C[0])},
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
};

bool x64_issue_feature_available(const char *feature)
{
    if (feature == nullptr || feature[0] == '\0') return true;
    static const struct cpufb_x86_runtime_features features =
        cpufb_x86_detect_runtime_features();
    if (std::strcmp(feature, "avx") == 0) return features.avx != 0;
    if (std::strcmp(feature, "fma") == 0) return features.fma != 0;
    if (std::strcmp(feature, "avx512f") == 0) return features.avx512f != 0;
    return false;
}

} // namespace

const IssueProbeInput &x64_issue_probe_input()
{
    static const IssueProbeInput input = {kClasses,
        sizeof(kClasses) / sizeof(kClasses[0]), kMixes,
        sizeof(kMixes) / sizeof(kMixes[0]), ISSUE_PURE_BODY,
        x64_issue_feature_available};
    return input;
}
