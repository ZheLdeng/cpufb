// Tables of the generated issue-probe kernels, built from the architecture's
// issue_classes.def with the same grid macros the assembly uses, so the two
// cannot drift apart: a ratio present on one side and not the other is a
// link error.
//
// Included once by <arch>/kernel/issue_kernels.cpp, which first defines
//
//     namespace { bool issue_feature_available(const char *feature); }
//
// and has the kernel directory on its include path for issue_classes.def.

#include "issue_kernels.hpp"
#include "issue_layout.h"

// Declarations of every generated kernel.
extern "C"
{
#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature)        \
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

const cpufb::IssueClass kIssueClasses[] = {
#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature)        \
    {#name, instruction, unit, ISSUE_BANK_##bank, ISSUE_MODE_##mode, feature,  \
        issue_##name##_pure_h, issue_##name##_pure_f},
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
};

// One kernel array per mix.
#define ISSUE_PAIR_ENTRY(A, B, a, b)                                           \
    {                                                                          \
        {a, b, 0}, issue_##A##_##B##_##a##x##b##_il,                           \
            issue_##A##_##B##_##a##x##b##_bl                                   \
    }
#define ISSUE_TRIPLE_ENTRY(A, B, C, a, b, c)                                   \
    {                                                                          \
        {a, b, c}, issue_##A##_##B##_##C##_##a##x##b##x##c##_il, nullptr       \
    }
#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature)
#define ISSUE_PAIR(A, B, label)                                                \
    const cpufb::IssueMixKernel kMix_##A##_##B[] = {                           \
        ISSUE_RATIO_GRID(ISSUE_PAIR_ENTRY, A, B, ISSUE_COMMA)};
#define ISSUE_TRIPLE(A, B, C, label)                                           \
    const cpufb::IssueMixKernel kMix_##A##_##B##_##C[] = {                     \
        ISSUE_TRIPLE_GRID(ISSUE_TRIPLE_ENTRY, A, B, C, ISSUE_COMMA)};
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_PAIR_ENTRY
#undef ISSUE_TRIPLE_ENTRY

const cpufb::IssueMix kIssueMixes[] = {
#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature)
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

} // namespace

const cpufb::IssueProbeInput &cpufb::issue_probe_input()
{
    static const IssueProbeInput input = {kIssueClasses,
        sizeof(kIssueClasses) / sizeof(kIssueClasses[0]), kIssueMixes,
        sizeof(kIssueMixes) / sizeof(kIssueMixes[0]), ISSUE_PURE_BODY,
        issue_feature_available};
    return input;
}
