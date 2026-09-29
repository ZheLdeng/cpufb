// ARM64 issue-probe kernel tables: only the runtime feature check is
// architecture-specific; the tables come from common/issue_kernel_tables.inc.

#include "issue_kernels.hpp"

#if defined(__linux__) && !defined(__APPLE__)
#include "../runtime_features.hpp"
#endif

namespace {

bool issue_feature_available(const char *feature)
{
    if (feature == nullptr || feature[0] == '\0') return true;
#if defined(__linux__) && !defined(__APPLE__)
    return arm64_runtime_features().supports(feature);
#else
    // macOS: whatever the build detected on this machine is present.
    return true;
#endif
}

} // namespace

#include "issue_kernel_tables.inc"
