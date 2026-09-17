#pragma once

#include <cstdint>

#include "define.h"
namespace lbds::skip_list::internal {

static_assert(
    P > 0.0 && P < 1.0,
    "P is the per-level promotion probability and must lie in (0, 1): "
    "P = 0 degenerates the list to a single linked list, P = 1 would "
    "promote every node to MAX_LEVEL.");

constexpr uint32_t PROMOTE_THRESHOLD =
    static_cast<uint32_t>(P * 4294967296.0);  // P * 2^32

static_assert(PROMOTE_THRESHOLD > 0,
              "P is smaller than a 32-bit draw can resolve (< 2^-32), so no "
              "node would ever be promoted. Widen the generator or raise P.");

class Util {
 public:
  static inline uint32_t fastRand(uint32_t num);
  static inline Level randomLevel(uint32_t& rnd);
};

// Fast Linear Congruential Generator (Numerical Recipes parameters)
uint32_t Util::fastRand(uint32_t num) { return num * 1664525u + 1013904223u; }

Level Util::randomLevel(uint32_t& rnd) {
  Level level = 0;

  rnd = fastRand(rnd);
  while (rnd < PROMOTE_THRESHOLD && level < MAX_LEVEL - 1) {
    ++level;
    rnd = fastRand(rnd);
  }
  return level;
}

}  // namespace lbds::skip_list::internal
