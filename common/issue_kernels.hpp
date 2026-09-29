#ifndef CPUFB_ISSUE_KERNELS_HPP
#define CPUFB_ISSUE_KERNELS_HPP

#include "issue_probe.hpp"

namespace cpufb {

// The issue-probe kernels and mixes of the architecture this binary was
// built for, generated from <arch>/kernel/issue_classes.def; see
// issue_probe.hpp for the method and issue_kernel_tables.inc for the tables.
const IssueProbeInput &issue_probe_input();

} // namespace cpufb

#endif
