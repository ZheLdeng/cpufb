#ifndef CPUFB_X64_MULTIPLE_ISSUE_HPP
#define CPUFB_X64_MULTIPLE_ISSUE_HPP

#include "issue_probe.hpp"

// The x86-64 issue-probe kernels and mixes, generated from
// issue_classes.def; see common/issue_probe.hpp for the method.
const cpufb::IssueProbeInput &x64_issue_probe_input();

#endif
