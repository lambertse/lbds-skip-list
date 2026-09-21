#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include <skiplist/detail/util.h>

namespace {

using lbds::skip_list::internal::Level;
using lbds::skip_list::internal::MAX_LEVEL;
using lbds::skip_list::internal::P;
using lbds::skip_list::internal::PROMOTE_THRESHOLD;
using lbds::skip_list::internal::Util;

// ---------------------------------------------------------------------------
// PROMOTE_THRESHOLD: the compile-time encoding of P
// ---------------------------------------------------------------------------

TEST(PromoteThreshold, EncodesPAsACutPointOnA32BitDraw) {
  // A uniform 32-bit draw falls below the threshold with probability ~P.
  EXPECT_EQ(PROMOTE_THRESHOLD, static_cast<uint32_t>(P * 4294967296.0));

  // Scaling a double by 2^32 only shifts its exponent, so the product is exact
  // and can never saturate for a P in (0, 1).
  EXPECT_GT(PROMOTE_THRESHOLD, 0u);
  EXPECT_LT(PROMOTE_THRESHOLD, 0xFFFFFFFFu);
}

TEST(PromoteThreshold, AtOneQuarterMeansTheTopTwoBitsAreZero) {
  // Guarded so the suite stays honest if P is retuned in define.h.
  if (P != 0.25) {
    GTEST_SKIP() << "P has been retuned to " << P
                 << "; the 2^30 identity is specific to 1/4";
  }
  EXPECT_EQ(PROMOTE_THRESHOLD, uint32_t{1} << 30);
  EXPECT_EQ(PROMOTE_THRESHOLD, 0x40000000u);
}

// ---------------------------------------------------------------------------
// fastRand
// ---------------------------------------------------------------------------

TEST(FastRand, HasNoFixedPoint) {
  // A seed that mapped to itself would freeze every node at the same level.
  for (uint32_t seed : {0u, 1u, 7u, 12345u, 0x80000000u, 0xFFFFFFFFu}) {
    EXPECT_NE(Util::fastRand(seed), seed) << "seed " << seed;
  }
}

TEST(FastRand, DoesNotCycleEarly) {
  // The generator is full-period; at minimum it must not repeat quickly.
  std::unordered_set<uint32_t> seen;
  seen.reserve(200000);

  uint32_t value = 1;
  int firstRepeat = -1;
  for (int i = 0; i < 200000; ++i) {
    value = Util::fastRand(value);
    if (!seen.insert(value).second) {
      firstRepeat = i;
      break;
    }
  }
  EXPECT_EQ(firstRepeat, -1) << "sequence repeated after " << firstRepeat
                             << " draws";
}

// ---------------------------------------------------------------------------
// randomLevel
// ---------------------------------------------------------------------------

TEST(RandomLevel, StaysWithinTheLevelCap) {
  // A level of MAX_LEVEL or more would index past every node's next[] array.
  uint32_t rng = 1;
  Level lowest = MAX_LEVEL;
  Level highest = 0;
  for (int i = 0; i < 2000000; ++i) {
    const Level level = Util::randomLevel(rng);
    if (level < lowest) lowest = level;
    if (level > highest) highest = level;
  }

  // Level is unsigned, so "not below 0" asserts nothing; over 2M draws at
  // P = 0.25 an unpromoted draw is certain, so pin the floor at exactly 0.
  EXPECT_EQ(lowest, Level{0});
  EXPECT_LE(highest, MAX_LEVEL - 1);
}

TEST(RandomLevel, ReachesLevelsWellAboveZero) {
  // Guards against a promotion loop that never runs.
  uint32_t rng = 99;
  Level highest = 0;
  for (int i = 0; i < 200000; ++i) {
    const Level level = Util::randomLevel(rng);
    if (level > highest) highest = level;
  }
  // P(level >= 5) = P^5, so over 200k draws this is all but certain.
  EXPECT_GE(highest, 5) << "towers are not being built";
}

TEST(RandomLevel, IsGeometricallyDistributed) {
  constexpr int kDraws = 500000;
  std::vector<int> histogram(static_cast<std::size_t>(MAX_LEVEL), 0);

  uint32_t rng = 20240917;
  for (int i = 0; i < kDraws; ++i) {
    histogram[static_cast<std::size_t>(Util::randomLevel(rng))] += 1;
  }

  // P(level == k) = (1 - P) * P^k, checked while the expected count is large
  // enough for the sampling noise to stay negligible. The tolerance is five
  // standard errors, so a passing run is not a coin flip.
  double expectedShare = 1.0 - P;
  for (Level k = 0; k < MAX_LEVEL; ++k) {
    if (expectedShare * kDraws < 500) break;

    const double observedShare =
        static_cast<double>(histogram[static_cast<std::size_t>(k)]) / kDraws;
    const double tolerance =
        5.0 * std::sqrt(expectedShare * (1.0 - expectedShare) / kDraws);

    EXPECT_NEAR(observedShare, expectedShare, tolerance)
        << "at level " << static_cast<unsigned>(k);
    expectedShare *= P;
  }
}

TEST(RandomLevel, LeavesTheRngOnAPureLcgChain) {
  // Regression: the draw used to be consumed by shifting `rnd` in place, which
  // wrote partially-consumed bits back into the generator state and destroyed
  // the recurrence -- and with it the full period the design relies on.
  uint32_t rng = 7;
  uint32_t shadow = 7;

  for (int call = 0; call < 50000; ++call) {
    const Level level = Util::randomLevel(rng);

    // One draw for the first Bernoulli trial, plus one more per promotion.
    for (Level k = 0; k <= level; ++k) shadow = Util::fastRand(shadow);

    ASSERT_EQ(rng, shadow) << "generator state diverged at call " << call;
  }
}

TEST(RandomLevel, ConsumesTheExpectedNumberOfDraws) {
  // The mean level is P / (1 - P), so a call costs 1 + P/(1-P) = 1/(1-P) draws.
  constexpr int kCalls = 200000;

  uint32_t rng = 31337;
  long long draws = 0;
  for (int i = 0; i < kCalls; ++i)
    draws += static_cast<long long>(Util::randomLevel(rng)) + 1;

  EXPECT_NEAR(static_cast<double>(draws) / kCalls, 1.0 / (1.0 - P), 0.02);
}

}  // namespace
