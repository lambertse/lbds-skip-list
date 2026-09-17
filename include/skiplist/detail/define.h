#pragma once

namespace lbds::skip_list::internal {

using Level = int;

// P is the probability that a node promoted to level k is also promoted to
// k+1, so levels are geometrically distributed and a node reaches level k with
// probability P^k. It drives the space/time trade-off: a larger P builds taller
// towers, which means more pointers per node but fewer steps per search.
// 1/4 is the usual choice.
constexpr double P = 0.25;

// MAX_LEVEL is the *number* of levels the list can have. A node's `level` is
// the 0-based index of its topmost link, so valid levels are 0..MAX_LEVEL-1
// and a node's `next` vector always holds `level + 1` pointers.
//
// The cap should sit near log(N) / log(1/P) for the largest N you expect;
// beyond that a taller head sentinel buys nothing. At P = 1/4, 17 levels cover
// ~4^16 (~4e9) elements. Note the two constants are coupled: raising P shortens
// the reach of each level, so P = 1/2 would cover only ~2^16 (~65k) elements at
// this height. Revisit MAX_LEVEL whenever P changes.
constexpr Level MAX_LEVEL = 17;

}  // namespace lbds::skip_list::internal
