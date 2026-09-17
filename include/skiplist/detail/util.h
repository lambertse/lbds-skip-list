#pragma once

#include <cstdint>

#include "define.h"
namespace lbds::skip_list::internal {

class Util {
 public:
  static inline uint32_t fastRand(uint32_t num);
  static inline Level randomLevel(uint32_t rnd);
};

// Fast Linear Congruential Generator (Numerical Recipes parameters)
uint32_t Util::fastRand(uint32_t num) { return num * 1664525u + 1013904223u; }

Level Util::randomLevel(uint32_t rnd) {
  // For P = 1/4, two bits must be zero to climb one level. The bits are
  // consumed from the TOP of the word on purpose: for an LCG modulo 2^32 the
  // low k bits are themselves an LCG modulo 2^k, so `rnd & 3` would repeat with
  // period 4 across successive fastRand() draws and the climb/stop decision
  // would be cyclic rather than random. The high bits have the full period.
  //
  // A 32-bit draw holds exactly 16 two-bit trials, so `level < MAX_LEVEL`
  // (== 17) stops the loop precisely when the entropy runs out; no separate
  // counter is needed, and every level in 1..MAX_LEVEL stays reachable.
  Level level = 1;
  while ((rnd & 0xC0000000u) == 0 && level < MAX_LEVEL) {
    ++level;
    rnd <<= 2;
  }
  return level;
}

}  // namespace lbds::skip_list::internal
