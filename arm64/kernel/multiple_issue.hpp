#ifndef CPUFB_ARM64_MULTIPLE_ISSUE_HPP
#define CPUFB_ARM64_MULTIPLE_ISSUE_HPP

#include "issue_probe.hpp"

// The ARM64 issue-probe kernels and mixes, generated from
// issue_classes.def; see common/issue_probe.hpp for the method.
const cpufb::IssueProbeInput &arm64_issue_probe_input();

#endif
