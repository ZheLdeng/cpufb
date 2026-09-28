// Unit tests for the issue-probe analysis: the unimodal search, the
// near-optimal interval, the sharing verdict and the proportional-mix choice.
// No hardware is involved; the "measurements" are synthetic curves built from
// the resource model the probe assumes.

#include "issue_probe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace cpufb;

namespace {

int failures = 0;

void check(bool condition, const std::string &what)
{
    if (condition) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
}

// The 21 Farey fractions of order 8 plus the two pure endpoints, sorted.
std::vector<double> candidate_fractions()
{
    const int grid[][2] = {{1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {2, 5},
        {1, 2}, {3, 5}, {2, 3}, {3, 4}, {1, 1}, {4, 3}, {3, 2}, {5, 3}, {2, 1},
        {5, 2}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}};
    std::vector<double> x;
    x.push_back(0.0);
    for (const auto &ab : grid) x.push_back(issue_mix_fraction(ab[0], ab[1]));
    x.push_back(1.0);
    for (size_t i = 1; i < x.size(); ++i) check(x[i] > x[i - 1], "grid sorted");
    return x;
}

// Total IPC of a mix with fraction x of class A under a piecewise-linear
// resource model, with an extra "peak" penalty that makes the joint ceiling
// a true maximum rather than a plateau: the probe must find the single best
// ratio, not the whole flat range.
double model_ipc(double x, double peak_a, double peak_b, double joint,
    double best_x, double droop)
{
    const double per_group = std::max(std::max(x / peak_a, (1.0 - x) / peak_b),
        (1.0 + droop * std::fabs(x - best_x)) / joint);
    return 1.0 / per_group;
}

void test_search_finds_the_peak()
{
    const std::vector<double> x = candidate_fractions();
    // Neoverse V3-like: 4 FMLA, 3 loads, five instructions per cycle at
    // best, and the best ratio is 4:3 (x = 0.571), with 2% droop per 0.1.
    const double best_x = 4.0 / 7.0;
    int evaluations = 0;
    auto evaluate = [&](size_t i) {
        ++evaluations;
        return model_ipc(x[i], 4.0, 3.0, 5.0, best_x, 0.2);
    };
    const IssueSearchResult result =
        search_unimodal_maximum(x.size(), evaluate, 0.002, 0.03);
    check(std::fabs(x[result.best] - best_x) < 1e-9,
        "search lands on 4:3, got x=" + std::to_string(x[result.best]));
    check(std::fabs(result.value - 5.0) < 1e-9, "search reports the joint ceiling");
    // Golden section needs about seven runs for the maximum; the two
    // bisections for the interval add a few more.  Well under the grid.
    check(result.evaluations <= 18,
        "search uses at most 18 of 23 candidates, used " +
            std::to_string(result.evaluations));
    // The 97% interval: T >= 4.85 requires droop * |x - best| <= ~0.03,
    // i.e. |x - 4/7| <= 0.155 with the corner constraints inside it.
    check(x[result.low] >= 0.41 && x[result.low] <= 0.45,
        "interval low end near x=0.43, got " + std::to_string(x[result.low]));
    check(x[result.high] >= 0.70 && x[result.high] <= 0.75,
        "interval high end near x=0.71, got " + std::to_string(x[result.high]));
}

void test_search_reports_a_pure_endpoint_maximum()
{
    // Kunpeng 920F, FMOPA against streaming FMLA: every mix is slower than
    // the pure FMLA stream, and the curve dips next to that endpoint before
    // rising again towards 1:2, so the endpoint must be a candidate in its
    // own right and not only reached through the bracket.
    const std::vector<double> x = candidate_fractions();
    auto evaluate = [&](size_t i) {
        if (x[i] == 0.0) return 2.0;
        if (x[i] == 1.0) return 0.5;
        return 1.5 - 0.6 * std::fabs(x[i] - 1.0 / 3.0);
    };
    const IssueSearchResult result =
        search_unimodal_maximum(x.size(), evaluate, 0.01, 0.03);
    check(result.best == 0 && std::fabs(result.value - 2.0) < 1e-9,
        "pure endpoint is the reported maximum");
}

void test_search_handles_endpoint_maximum()
{
    // Shared ports: mixing never beats the pure faster class, so the maximum
    // sits at x = 1 (pure A) and the interval reaches it.
    const std::vector<double> x = candidate_fractions();
    auto evaluate = [&](size_t i) {
        return model_ipc(x[i], 4.0, 2.0, 4.0, 1.0, 0.0);
    };
    const IssueSearchResult result =
        search_unimodal_maximum(x.size(), evaluate, 0.01, 0.03);
    check(result.best == x.size() - 1 || std::fabs(result.value - 4.0) < 1e-9,
        "endpoint maximum found");
    check(result.high == x.size() - 1, "interval includes the pure endpoint");
}

void test_search_ignores_noise_within_tolerance()
{
    // A flat plateau with +-0.5% noise: every plateau point is within the
    // tolerance, so the search must still return a plateau point and the
    // interval must cover the plateau, whatever the noise pattern.
    const std::vector<double> x = candidate_fractions();
    auto evaluate = [&](size_t i) {
        const double base = model_ipc(x[i], 4.0, 3.0, 5.0, 0.5, 0.0);
        // int before the subtraction: size_t would wrap below zero.
        const double noise = (static_cast<int>((i * 7919) % 11) - 5) * 0.001;
        return base * (1.0 + noise);
    };
    const IssueSearchResult result =
        search_unimodal_maximum(x.size(), evaluate, 0.01, 0.03);
    check(result.value > 4.95, "plateau value found under noise");
    check(x[result.low] <= 0.45 && x[result.high] >= 0.70,
        "interval spans the plateau under noise");
}

void test_verdicts()
{
    check(classify_joint_ceiling(4.0, 4.0, 4.05).sharing == ISSUE_SHARED,
        "SVE+NEON on one set of pipes is shared");
    check(classify_joint_ceiling(4.0, 3.0, 6.9).sharing == ISSUE_INDEPENDENT,
        "FSU+ALU adding up is independent");
    const IssueVerdict partial = classify_joint_ceiling(4.0, 3.0, 5.0);
    check(partial.sharing == ISSUE_PARTIAL, "F+L at 5 of 7 is partial");
    check(std::fabs(partial.shared_budget - 2.0) < 1e-9,
        "shared budget is P_A + P_B - T*");
    check(!describe_issue_verdict(partial).empty(), "verdict has text");
}

void test_proportional_mix()
{
    // ALU 4, FSU 4, LSU 3 wants shares 4:4:3 of 11.  1:1:1 and 2:2:2 are the
    // same mix and count once; 2:2:1 is the next nearest; 1:1:4 is far.
    std::vector<std::vector<int>> counts;
    const int grid[][3] = {{1, 1, 1}, {2, 1, 1}, {1, 1, 4}, {2, 2, 1},
        {2, 2, 2}, {3, 2, 1}};
    for (const auto &c : grid) counts.push_back({c[0], c[1], c[2]});
    const std::vector<size_t> picks =
        nearest_proportional_mixes(counts, {4.0, 4.0, 3.0}, 2);
    check(picks.size() == 2, "two picks");
    bool has_equal = false, has_221 = false, has_114 = false;
    for (size_t p : picks) {
        has_equal |= counts[p] == std::vector<int>{1, 1, 1} ||
            counts[p] == std::vector<int>{2, 2, 2};
        has_221 |= counts[p] == std::vector<int>{2, 2, 1};
        has_114 |= counts[p] == std::vector<int>{1, 1, 4};
    }
    check(has_equal && has_221, "the equal mix and 2:2:1 nearest to 4:4:3");
    check(!has_114, "1:1:4 not chosen");
}

} // namespace

int main()
{
    test_search_finds_the_peak();
    test_search_handles_endpoint_maximum();
    test_search_reports_a_pure_endpoint_maximum();
    test_search_ignores_noise_within_tolerance();
    test_verdicts();
    test_proportional_mix();
    if (failures == 0) std::printf("issue probe analysis: all tests passed\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
