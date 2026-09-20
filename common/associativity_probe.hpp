#ifndef CPUFB_ASSOCIATIVITY_PROBE_HPP
#define CPUFB_ASSOCIATIVITY_PROBE_HPP

namespace cpufb {

// Empirical L1 data-cache associativity, or 0 when no conflict transition was
// observed.  The probe uses no OS-reported cache parameter except the line
// size (pass the effective value; 64 is assumed for values <= 0).
//
// Method: a pointer ring of n lines spaced one power-of-two stride apart maps
// every line to the same L1 set, so it starts missing once n exceeds the
// number of ways.  Widely spaced lines also live on n different pages, and on
// many cores those pages alias one DTLB set, which would show up as an earlier
// transition at the DTLB associativity.  Each n is therefore timed against a
// control ring that touches the same pages but a different L1 set per line;
// translation cost cancels in the test/control ratio and only the cache
// conflict remains.
int probe_l1_associativity(int cacheline_bytes);

} // namespace cpufb

#endif
