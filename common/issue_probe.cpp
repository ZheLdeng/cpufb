#include "issue_probe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <map>

#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "common.hpp"
#include "issue_layout.h"

namespace cpufb {

namespace {

// Two mixed-kernel samples closer than this are the same measurement: the
// fastest-of-n minimum of an L1-resident stream repeats to well under one
// percent on every core tried, and the golden-section search must not follow
// a difference it cannot trust.
const double kNoiseTolerance = 0.01;
// The near-optimal interval: ratios that still reach this share of the best
// total IPC.  The same 97% knee the pair tool used.
const double kNearOptimal = 0.03;
// Pure peaks: half and full chain counts must agree this closely, otherwise
// the accumulator latency, not the issue width, bounded the kernel.
const double kChainAgreement = 0.02;
// Interleaved against blocked schedule at the optimum: a larger gap says the
// core needs the classes finely interleaved to overlap them.
const double kScheduleNote = 0.03;
// Joint ceiling against the pure peaks (classify_joint_ceiling).
const double kSharedSlack = 1.05;
const double kIndependentSlack = 0.95;
// The peak-proportional mix and how many grid neighbours of it are run.
const size_t kTripleCandidates = 3;
// L1-resident buffer handed to every kernel: the widest load form reads
// eight vectors of up to 256 bytes from it.
const size_t kBufferBytes = 8192;

double elapsed_seconds(const timespec &start, const timespec &end)
{
    return static_cast<double>(end.tv_sec - start.tv_sec) +
        static_cast<double>(end.tv_nsec - start.tv_nsec) * 1e-9;
}

std::string format_double(double value, int precision)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%.*f", precision, value);
    return text;
}

std::string format_percent(double fraction)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.0f%%", fraction * 100.0);
    return text;
}

// Cycles per loop of one kernel on the pinned core.
class IssueTimer
{
public:
    IssueTimer(int cpu, const IssueProbeOptions &options)
        : options_(options), counted_any_(false), estimated_any_(false)
    {
#ifdef __linux__
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
            std::fprintf(
                stderr, "Warning: issue probe could not bind to CPU %d\n", cpu);
        counter_ = new PerfEventCycle(0, false);
#else
        (void)cpu;
#endif
    }

    ~IssueTimer()
    {
#ifdef __linux__
        delete counter_;
#endif
    }

    IssueTimer(const IssueTimer &) = delete;
    IssueTimer &operator=(const IssueTimer &) = delete;

    bool usable() const
    {
#ifdef __linux__
        if (counter_->available()) return true;
#endif
        return options_.clock_hz > 0.0;
    }

    // Returns 0 when nothing could be measured.
    double cycles_per_loop(IssueKernelFn kernel, const void *buffer)
    {
        if (kernel == nullptr) return 0.0;
        // Calibrate the loop count to the sample length, then warm up.
        int64_t loops = 256;
        timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        kernel(buffer, loops);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double probe = elapsed_seconds(start, end);
        if (probe > 0.0) {
            const double wanted = options_.sample_seconds / probe * loops;
            loops = static_cast<int64_t>(
                std::min(std::max(wanted, 256.0), 1.0e8));
        }
        kernel(buffer, loops);

        double best = 0.0;
        for (int sample = 0; sample < options_.samples; ++sample) {
            double cycles = 0.0;
#ifdef __linux__
            if (counter_->available()) {
                counter_->start();
                kernel(buffer, loops);
                counter_->stop();
                cycles = static_cast<double>(counter_->get_cycle());
                if (cycles > 0.0) counted_any_ = true;
            }
#endif
            if (cycles <= 0.0) {
                // The event was denied, or opened but counts nothing (a
                // VM without a virtual PMU): fall back to time x clock.
                if (options_.clock_hz <= 0.0) return 0.0;
                clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                kernel(buffer, loops);
                clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                cycles = elapsed_seconds(start, end) * options_.clock_hz;
                estimated_any_ = true;
            }
            if (cycles > 0.0 && (best == 0.0 || cycles < best)) best = cycles;
        }
        return best > 0.0 ? best / static_cast<double>(loops) : 0.0;
    }

    std::string source() const
    {
        if (counted_any_ && !estimated_any_) return "perf_event cycles";
        // An estimated clock is the one measured by the frequency probe;
        // a kernel that lowers the core clock (an AVX-512 licence, a wide
        // SME unit) then runs fewer real cycles than charged and reads low.
        const std::string caveat =
            "; kernels that lower the core clock read low";
        if (counted_any_)
            return "perf_event cycles, some kernels time x " +
                options_.clock_source + caveat;
        return "time x " + options_.clock_source + " (no cycle counter)" +
            caveat;
    }

private:
    const IssueProbeOptions &options_;
#ifdef __linux__
    PerfEventCycle *counter_;
#endif
    bool counted_any_;
    bool estimated_any_;
};

struct ClassState
{
    const IssueClass *info;
    bool available;
    double peak_half;
    double peak_full;
    double peak; // the larger of the two
};

// A pair candidate in mixing-fraction order: the pure B kernel, the grid,
// the pure A kernel.
struct PairCandidate
{
    double fraction; // share of class A
    int a, b;        // 0 marks a pure kernel
    const IssueMixKernel *kernel;
};

std::string ratio_text(const PairCandidate &candidate, const char *name_a,
    const char *name_b)
{
    if (candidate.kernel == nullptr)
        return std::string("pure ") + (candidate.a > 0 ? name_a : name_b);
    return std::to_string(candidate.a) + ":" + std::to_string(candidate.b);
}

void add_row(Table &table, const std::string &item, const std::string &classes,
    const std::string &ipc, const std::string &frontier,
    const std::string &verdict)
{
    std::vector<std::string> row(table.getCol());
    row[0] = item;
    row[1] = classes;
    row[2] = ipc;
    row[3] = frontier;
    row[4] = verdict;
    table.addOneItem(row);
}

} // namespace

// ---------------------------------------------------------------------------
// Analysis.
// ---------------------------------------------------------------------------

IssueSearchResult search_unimodal_maximum(size_t count,
    const std::function<double(size_t)> &evaluate, double tolerance,
    double epsilon)
{
    IssueSearchResult result = {0, 0.0, 0, 0, 0};
    if (count == 0) return result;
    std::vector<double> value(count, 0.0);
    std::vector<bool> known(count, false);
    auto get = [&](size_t i) {
        if (!known[i]) {
            value[i] = evaluate(i);
            known[i] = true;
            ++result.evaluations;
        }
        return value[i];
    };

    // The endpoints are the pure streams, whose peaks are already known, so
    // they cost nothing and are always candidates: a pair whose mixes all
    // issue slower than the faster class alone has its maximum there, and a
    // curve that dips next to an endpoint would otherwise hide it from the
    // bracket search below.
    get(0);
    if (count > 1) get(count - 1);

    // Golden-section search on indices.  Each step drops the part of the
    // bracket that cannot hold the maximum of a unimodal function; when the
    // two probes are equal within the noise band the maximum lies between
    // them and both ends are dropped.
    size_t low = 0;
    size_t high = count - 1;
    while (high - low > 2) {
        const double span = static_cast<double>(high - low);
        size_t m1 = low + static_cast<size_t>(std::lround(span * 0.382));
        size_t m2 = low + static_cast<size_t>(std::lround(span * 0.618));
        m1 = std::max(m1, low + 1);
        m2 = std::min(m2, high - 1);
        if (m2 <= m1) m2 = m1 + 1;
        const double f1 = get(m1);
        const double f2 = get(m2);
        if (f1 > f2 * (1.0 + tolerance))
            high = m2;
        else if (f2 > f1 * (1.0 + tolerance))
            low = m1;
        else {
            low = m1;
            high = m2;
        }
    }
    for (size_t i = low; i <= high; ++i) get(i);

    result.best = 0;
    result.value = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < count; ++i)
        if (known[i] && value[i] > result.value) {
            result.value = value[i];
            result.best = i;
        }

    // The near-optimal interval: T is non-decreasing up to the maximum and
    // non-increasing after it, so each boundary is a bisection.
    const double threshold = result.value * (1.0 - epsilon);
    size_t left = 0;
    if (get(0) < threshold) {
        size_t l = 0, r = result.best; // f(l) < threshold <= f(r)
        while (r - l > 1) {
            const size_t m = (l + r) / 2;
            if (get(m) >= threshold)
                r = m;
            else
                l = m;
        }
        left = r;
    }
    size_t right = count - 1;
    if (get(count - 1) < threshold) {
        size_t l = result.best, r = count - 1; // f(l) >= threshold > f(r)
        while (r - l > 1) {
            const size_t m = (l + r) / 2;
            if (get(m) >= threshold)
                l = m;
            else
                r = m;
        }
        right = l;
    }
    result.low = left;
    result.high = right;
    return result;
}

IssueVerdict classify_joint_ceiling(double peak_a, double peak_b, double joint)
{
    IssueVerdict verdict = {ISSUE_PARTIAL, 0.0};
    const double larger = std::max(peak_a, peak_b);
    if (joint <= larger * kSharedSlack) {
        verdict.sharing = ISSUE_SHARED;
    } else if (joint >= (peak_a + peak_b) * kIndependentSlack) {
        verdict.sharing = ISSUE_INDEPENDENT;
    } else {
        verdict.shared_budget = peak_a + peak_b - joint;
    }
    return verdict;
}

std::string describe_issue_verdict(const IssueVerdict &verdict)
{
    switch (verdict.sharing) {
    case ISSUE_SHARED: return "shared issue ports: mixing adds nothing";
    case ISSUE_INDEPENDENT: return "independent: peaks add up";
    default:
        return "partially shared: " + format_double(verdict.shared_budget, 2) +
            " IPC of the two peaks is a shared budget";
    }
}

std::vector<size_t> nearest_proportional_mixes(
    const std::vector<std::vector<int>> &counts,
    const std::vector<double> &peaks, size_t take)
{
    double total = 0.0;
    for (double peak : peaks) total += peak;
    std::vector<std::pair<double, size_t>> ranked;
    for (size_t i = 0; i < counts.size(); ++i) {
        int n = 0;
        for (int c : counts[i]) n += c;
        double distance = 0.0;
        for (size_t k = 0; k < peaks.size() && k < counts[i].size(); ++k) {
            const double share = n > 0 ? counts[i][k] / static_cast<double>(n)
                                       : 0.0;
            const double target = total > 0.0 ? peaks[k] / total : 0.0;
            distance += std::fabs(share - target);
        }
        ranked.push_back(std::make_pair(distance, i));
    }
    std::sort(ranked.begin(), ranked.end());
    // Grid entries with the same shares (1:1:1 and 2:2:2) are one mix.
    std::vector<size_t> chosen;
    std::vector<std::vector<double>> chosen_shares;
    for (size_t i = 0; i < ranked.size() && chosen.size() < take; ++i) {
        const std::vector<int> &entry = counts[ranked[i].second];
        int n = 0;
        for (int c : entry) n += c;
        std::vector<double> shares;
        for (int c : entry) shares.push_back(n > 0 ? c / static_cast<double>(n) : 0.0);
        bool duplicate = false;
        for (const std::vector<double> &seen : chosen_shares) {
            bool same = seen.size() == shares.size();
            for (size_t k = 0; same && k < seen.size(); ++k)
                same = std::fabs(seen[k] - shares[k]) < 1e-9;
            duplicate |= same;
        }
        if (duplicate) continue;
        chosen_shares.push_back(shares);
        chosen.push_back(ranked[i].second);
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// Measurement.
// ---------------------------------------------------------------------------

IssueProbeOptions::IssueProbeOptions()
    : sample_seconds(0.02), samples(5), clock_hz(0.0),
      clock_source("frequency-table clock")
{
}

bool run_issue_probe(const IssueProbeInput &input, int cpu,
    const IssueProbeOptions &options, Table &table)
{
    IssueTimer timer(cpu, options);
    if (!timer.usable()) {
        add_row(table, "Issue probe", "-", "-", "-",
            "not measured: no cycle counter and no clock estimate");
        return true;
    }
    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, kBufferBytes) != 0) return false;
    std::memset(allocation, 0, kBufferBytes);
    const void *buffer = allocation;

    // Pure peaks.
    std::map<std::string, ClassState> classes;
    for (size_t i = 0; i < input.class_count; ++i) {
        const IssueClass &info = input.classes[i];
        ClassState state = {&info, true, 0.0, 0.0, 0.0};
        state.available = input.feature_available == nullptr ||
            info.feature == nullptr || info.feature[0] == '\0' ||
            input.feature_available(info.feature);
        if (state.available) {
            const double half = timer.cycles_per_loop(info.pure_half, buffer);
            const double full = timer.cycles_per_loop(info.pure_full, buffer);
            state.peak_half = half > 0.0 ? input.pure_body / half : 0.0;
            state.peak_full = full > 0.0 ? input.pure_body / full : 0.0;
            state.peak = std::max(state.peak_half, state.peak_full);
        }
        classes[info.name] = state;
        if (!state.available) continue;
        std::string verdict = "chain count sufficient";
        if (state.peak_full > state.peak_half * (1.0 + kChainAgreement))
            verdict = "full chains needed: the half-chain run was bound by "
                      "the accumulator latency";
        else if (state.peak_half > state.peak_full * (1.0 + kChainAgreement))
            verdict = "half chains faster than full; peak taken from the "
                      "half-chain run";
        add_row(table, std::string(info.unit) + " peak",
            std::string(info.name) + " (" + info.instruction + ")",
            state.peak > 0.0 ? format_double(state.peak, 2) : "-",
            "half / full chains: " + format_double(state.peak_half, 2) +
                " / " + format_double(state.peak_full, 2),
            verdict);
    }

    // Mixes.  Rows are collected first and printed at the end, because the
    // front-end issue width is the largest total IPC any mix reached, pairs
    // included: on a Neoverse V3 the ALU+FSU pair sustains 9.7 while the
    // ALU+FSU+LSU stream stops at 8 because a load takes two dispatch slots.
    // A pair whose maximum sits at that width is bounded there, whatever its
    // ports: on a 4-wide x86 core FMA and ALU otherwise read "shared".
    struct DeferredRow
    {
        std::string item, classes, ipc, frontier, verdict;
        double value;
        bool is_pair;
        bool shared;   // verdict was "shared issue ports"
        bool blocking; // verdict was "shared and blocking"
    };
    std::vector<DeferredRow> rows;
    for (size_t m = 0; m < input.mix_count; ++m) {
        const IssueMix &mix = input.mixes[m];
        std::string names;
        const ClassState *state[3] = {nullptr, nullptr, nullptr};
        std::string missing;
        for (int k = 0; k < mix.class_count; ++k) {
            names += (k ? " + " : "") + std::string(mix.classes[k]);
            std::map<std::string, ClassState>::const_iterator found =
                classes.find(mix.classes[k]);
            if (found == classes.end() || !found->second.available ||
                found->second.peak <= 0.0) {
                if (missing.empty()) {
                    missing = std::string(mix.classes[k]);
                    if (found != classes.end() && found->second.info->feature &&
                        found->second.info->feature[0] != '\0')
                        missing += " needs " + std::string(found->second.info->feature);
                }
            } else {
                state[k] = &found->second;
            }
        }
        if (!missing.empty()) {
            DeferredRow row = {mix.label, names, "-", "-",
                "not available: " + missing, 0.0, mix.class_count == 2, false,
                false};
            rows.push_back(row);
            continue;
        }

        if (mix.class_count == 2) {
            const double peak_a = state[0]->peak;
            const double peak_b = state[1]->peak;
            std::vector<PairCandidate> candidates;
            PairCandidate pure_b = {0.0, 0, 1, nullptr};
            candidates.push_back(pure_b);
            for (size_t k = 0; k < mix.kernel_count; ++k) {
                const IssueMixKernel &kernel = mix.kernels[k];
                PairCandidate candidate = {
                    issue_mix_fraction(kernel.counts[0], kernel.counts[1]),
                    kernel.counts[0], kernel.counts[1], &kernel};
                candidates.push_back(candidate);
            }
            PairCandidate pure_a = {1.0, 1, 0, nullptr};
            candidates.push_back(pure_a);
            std::sort(candidates.begin(), candidates.end(),
                [](const PairCandidate &l, const PairCandidate &r) {
                    return l.fraction < r.fraction;
                });

            std::vector<double> schedule_gap(candidates.size(), 0.0);
            auto evaluate = [&](size_t i) {
                const PairCandidate &candidate = candidates[i];
                if (candidate.kernel == nullptr)
                    return candidate.a > 0 ? peak_a : peak_b;
                const int n = candidate.a + candidate.b;
                const double body =
                    static_cast<double>(ISSUE_MIX_REPEATS(n)) * n;
                const double interleaved =
                    timer.cycles_per_loop(candidate.kernel->interleaved, buffer);
                const double blocked =
                    timer.cycles_per_loop(candidate.kernel->blocked, buffer);
                const double ipc_il = interleaved > 0.0 ? body / interleaved : 0.0;
                const double ipc_bl = blocked > 0.0 ? body / blocked : 0.0;
                const double best = std::max(ipc_il, ipc_bl);
                if (best > 0.0) schedule_gap[i] = (ipc_il - ipc_bl) / best;
                return best;
            };
            const IssueSearchResult search = search_unimodal_maximum(
                candidates.size(), evaluate, kNoiseTolerance, kNearOptimal);

            const char *name_a = mix.classes[0];
            const char *name_b = mix.classes[1];
            const PairCandidate &best = candidates[search.best];
            std::string frontier = "max at " + std::string(name_a) + ":" +
                name_b + " = " + ratio_text(best, name_a, name_b) +
                "; >= " + format_percent(1.0 - kNearOptimal) + " from " +
                ratio_text(candidates[search.low], name_a, name_b) + " to " +
                ratio_text(candidates[search.high], name_a, name_b) + " (" +
                std::to_string(search.evaluations) + " of " +
                std::to_string(candidates.size()) + " ratios run)";
            const IssueVerdict sharing =
                classify_joint_ceiling(peak_a, peak_b, search.value);
            std::string verdict = describe_issue_verdict(sharing);
            // The maximum at a pure stream with no mix inside the 97% band
            // means every mix issued slower than that class alone: the other
            // class costs more than the slot it takes (a Kunpeng 920F FMOPA
            // holds the vector pipes for two cycles).  Equal peaks on shared
            // ports also put the maximum at an endpoint, but then the band
            // spans the mixes, and the plain verdict stands.
            const bool blocking =
                best.kernel == nullptr && search.low == search.high;
            if (blocking)
                verdict = std::string("shared and blocking: every mix issues "
                                      "slower than ") +
                    (best.a > 0 ? name_a : name_b) + " alone";
            if (best.kernel != nullptr &&
                std::fabs(schedule_gap[search.best]) > kScheduleNote)
                verdict += schedule_gap[search.best] > 0.0
                    ? "; interleaving beats blocks by " +
                        format_percent(schedule_gap[search.best])
                    : "; blocks beat interleaving by " +
                        format_percent(-schedule_gap[search.best]);
            DeferredRow row = {mix.label, names,
                format_double(search.value, 2), frontier, verdict,
                search.value, true, sharing.sharing == ISSUE_SHARED,
                blocking};
            rows.push_back(row);
        } else {
            std::vector<std::vector<int>> counts;
            for (size_t k = 0; k < mix.kernel_count; ++k)
                counts.push_back(std::vector<int>(mix.kernels[k].counts,
                    mix.kernels[k].counts + mix.class_count));
            std::vector<double> peaks;
            double sum = 0.0;
            for (int k = 0; k < mix.class_count; ++k) {
                peaks.push_back(state[k]->peak);
                sum += state[k]->peak;
            }
            const std::vector<size_t> chosen =
                nearest_proportional_mixes(counts, peaks, kTripleCandidates);
            double best_ipc = 0.0;
            size_t best_index = 0;
            for (size_t chosen_index : chosen) {
                const IssueMixKernel &kernel = mix.kernels[chosen_index];
                int n = 0;
                for (int k = 0; k < mix.class_count; ++k) n += kernel.counts[k];
                const double cycles =
                    timer.cycles_per_loop(kernel.interleaved, buffer);
                const double ipc = cycles > 0.0
                    ? static_cast<double>(ISSUE_MIX_REPEATS(n)) * n / cycles
                    : 0.0;
                if (ipc > best_ipc) {
                    best_ipc = ipc;
                    best_index = chosen_index;
                }
            }
            std::string ratio;
            for (int k = 0; k < mix.class_count; ++k)
                ratio += (k ? ":" : "") +
                    std::to_string(mix.kernels[best_index].counts[k]);
            std::string verdict;
            if (best_ipc >= kIndependentSlack * sum)
                verdict = "front end not the limit: reaches the sum of the "
                          "class peaks (" +
                    format_double(sum, 2) + ")";
            else
                verdict = "the three-class stream sustains " +
                    format_double(best_ipc, 1) +
                    " instructions per cycle, below the class peaks' sum of " +
                    format_double(sum, 2);
            DeferredRow row = {mix.label, names,
                best_ipc > 0.0 ? format_double(best_ipc, 2) : "-",
                "best of " + std::to_string(chosen.size()) +
                    " peak-proportional mixes at " + ratio,
                verdict, best_ipc, false, false, false};
            rows.push_back(row);
        }
    }

    // The front-end issue width is the largest total IPC any mix reached.
    double issue_width = 0.0;
    for (const DeferredRow &row : rows) issue_width = std::max(issue_width, row.value);
    for (DeferredRow &row : rows) {
        if (row.value <= 0.0 || row.value < kIndependentSlack * issue_width)
            continue;
        if (row.is_pair && row.shared && !row.blocking)
            row.verdict = "bounded by the front-end issue width (" +
                format_double(issue_width, 1) + "), not by shared ports";
        else if (row.is_pair)
            row.verdict += "; at the front-end issue width";
        else if (row.verdict.compare(0, 3, "the") == 0)
            row.verdict += ": this is the front-end issue width";
    }
    for (DeferredRow &row : rows)
        if (!row.is_pair && row.value > 0.0 &&
            row.value < kIndependentSlack * issue_width)
            row.verdict += "; a pair reaches " + format_double(issue_width, 1) +
                ", so the front end is at least that wide and this stream is "
                "bound elsewhere";
    // Pairs first, then the three-class rows, in definition order.
    for (const DeferredRow &row : rows)
        if (row.is_pair)
            add_row(table, row.item, row.classes, row.ipc, row.frontier,
                row.verdict);
    for (const DeferredRow &row : rows)
        if (!row.is_pair)
            add_row(table, row.item, row.classes, row.ipc, row.frontier,
                row.verdict);

    add_row(table, "Cycle source", "-", "-", "-", timer.source());
    std::free(allocation);
    return true;
}

void issue_probe_table_header(Table &table)
{
    std::vector<std::string> head(5);
    head[0] = "Item";
    head[1] = "Classes";
    head[2] = "IPC";
    head[3] = "Frontier";
    head[4] = "Verdict";
    table.setColumnNum(static_cast<int>(head.size()));
    table.addOneItem(head);
}

bool run_issue_probe_category(const IssueProbeInput &input, int cpu,
    double clock_ghz, const std::string &clock_source, unsigned loop_scale,
    Table &table)
{
    IssueProbeOptions options;
    if (clock_ghz > 0.0) options.clock_hz = clock_ghz * 1e9;
    if (!clock_source.empty()) options.clock_source = clock_source;
    if (loop_scale > 1) {
        options.sample_seconds =
            std::max(0.0005, options.sample_seconds / loop_scale);
        options.samples = 3;
    }
    return run_issue_probe(input, cpu, options, table);
}

} // namespace cpufb
