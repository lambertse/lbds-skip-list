#pragma once

#include <cstdint>
namespace lbds::skip_list::internal {

using Level = int;

// A single 32-bit random draw carries exactly 16 two-bit trials (see
// Util::randomLevel), so levels 1..17 are reachable and 17 is the tail bucket.
// With P = 1/4 that supports ~4^16 (~4e9) elements, so there is nothing to gain
// from a taller head sentinel.
constexpr Level MAX_LEVEL = 17;
constexpr double P = 0.25;

}  // namespace lbds::skip_list::internal
