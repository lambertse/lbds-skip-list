#pragma once

#include <cstdint>

#include "define.h"
namespace lbds::skip_list::internal {

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

  while ((rnd & 0xC0000000u) == 0 && level < MAX_LEVEL) {
    ++level;
    rnd <<= 2;
  }
  return level;
}

}  // namespace lbds::skip_list::internal
