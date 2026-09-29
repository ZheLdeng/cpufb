// x86-64 issue-probe kernel tables: only the CPUID feature check is
// architecture-specific; the tables come from common/issue_kernel_tables.inc.

#include "issue_kernels.hpp"

#include <cstring>

#include "../runtime_features.h"

namespace {

bool issue_feature_available(const char *feature)
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

#include "issue_kernel_tables.inc"
