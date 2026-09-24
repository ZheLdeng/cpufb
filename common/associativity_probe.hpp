#ifndef CPUFB_ASSOCIATIVITY_PROBE_HPP
#define CPUFB_ASSOCIATIVITY_PROBE_HPP

#include <cstddef>

namespace cpufb {

// Empirical L1 data-cache associativity, or 0 when no conflict transition was
// observed.  The probe uses no OS-reported cache parameter except the line
// size (pass the effective value; 64 is assumed for values <= 0).
//
// Method: a pointer ring of n lines at multiples of 64 KiB maps
// every line to the same L1 set, so it starts missing once n exceeds the
// number of ways.  Widely spaced lines also live on n different pages, and on
// many cores those pages alias one DTLB set, which would show up as an earlier
// transition at the DTLB associativity.  Each n is therefore timed against a
// control ring that touches the same pages but a different L1 set per line;
// translation cost cancels in the test/control ratio and only the cache
// conflict remains.  Address-hashed way predictors can make particular placements
// conflict early, so several placements are tried and the largest result is
// returned.
int probe_l1_associativity(int cacheline_bytes);

// Bytes one L1 way spans (sets x line size), or 0 when no conflict was seen.
// Lines that far apart share a set, so twice `ways` lines at that stride
// thrash one set while at half the stride they split evenly over two and
// fit.  The smallest power-of-two stride that conflicts is the way size, and
// ways x way size is the L1 capacity -- measured without a working-set sweep,
// so a prefetcher that keeps a pointer chase at L1 latency past the real
// capacity cannot stretch it.  Requires the set index to come from virtual
// address bits, which holds on every core measured so far (VIPT L1).
size_t probe_l1_way_bytes(int cacheline_bytes, int ways);

// Granule of the L1 set index, or 0 when it could not be told.  Of twice
// `ways` lines that share one set, every other one is moved by d bytes: while
// d is inside the granule they stay in the set and still conflict, and once
// d reaches it they land in the next set and the conflict goes.  That
// granule is the line size unless the cache keeps its lines in smaller
// indexed sectors: an Apple M4 Pro has 128 B lines and reads 64 B here.  It
// is therefore a lower bound on the line, as the reuse probe, which a
// prefetcher of neighbours can widen, is an upper one.
int probe_l1_line_from_sets(int ways, size_t way_bytes);

// Empirical L2 associativity, or 0 when it cannot be measured.  Lines 2 MiB
// apart in 2 MiB huge pages share every physical address bit below 21, so
// they compete for one set in L1 and in L2 alike: a ring of n of them stays
// in L1 up to the L1 ways, then misses to L2, and misses L2 once n passes the
// L2 ways.  The second transition is the answer.
//
// Returns the ways, or kL2NoHugePages when no 2 MiB huge page could be had
// (physical placement is then random above the page size and no set can be
// targeted: 4 KiB pages with transparent huge pages off, and macOS), or 0
// when huge pages were there but the lines never conflicted in L2.  That
// last case is an L2 whose set index folds in address bits above 21: a
// Kunpeng 920F keeps 32 such lines at L2 latency against 12 ways.
const int kL2NoHugePages = -1;
int probe_l2_associativity(int cacheline_bytes, int l1_ways);

// L2 line size from set indexing, by the method of probe_l1_line_from_sets
// applied to one L2 set in 2 MiB huge pages.  Takes the result of
// probe_l2_associativity and passes its kL2NoHugePages or 0 straight on.  A last level
// shared across a chip is address-hashed over slices on every machine tried,
// so neither its ways nor its line can be targeted this way.
int probe_l2_line_from_sets(int l2_ways);

} // namespace cpufb

#endif
