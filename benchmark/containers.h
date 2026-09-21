#pragma once

#include <skiplist/skiplist.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

namespace bench {

using SkipSet = lbds::skip_list::SkipList<int>;
using RbSet = std::set<int>;
using HashSet = std::unordered_set<int>;

// ---------------------------------------------------------------------------
// Adapters
//
// The three containers disagree on return type:
//   - lookup is find() != end() everywhere -- the only lookup this library
//     exposes, and std::set::contains is C++20 while this is C++17.
//   - insert returns bool here but pair<iterator, bool> there; erase returns
//     bool here but size_type there.
// Normalising everything to bool lets each measurement sink its result the
// same way, so no call can be optimised out of one container but not another.
// ---------------------------------------------------------------------------

inline bool Insert(SkipSet& c, int key) { return c.insert(key); }
inline bool Insert(RbSet& c, int key) { return c.insert(key).second; }
inline bool Insert(HashSet& c, int key) { return c.insert(key).second; }

inline bool Lookup(const SkipSet& c, int key) { return c.find(key) != c.end(); }
inline bool Lookup(const RbSet& c, int key) { return c.find(key) != c.end(); }
inline bool Lookup(const HashSet& c, int key) { return c.find(key) != c.end(); }

inline bool Erase(SkipSet& c, int key) { return c.erase(key); }
inline bool Erase(RbSet& c, int key) { return c.erase(key) != 0; }
inline bool Erase(HashSet& c, int key) { return c.erase(key) != 0; }

// Ordered traversal and range queries. All three containers can be walked end
// to end, but only the ordered ones walk in key order -- std::unordered_set
// visits buckets, so its traversal answers a different question.
template <typename C>
inline long SumAll(const C& c) {
  long sum = 0;
  for (const int value : c) sum += value;
  return sum;
}

// A range query needs lower_bound/upper_bound, which a hash table cannot
// provide at all; there is deliberately no HashSet overload.
inline long RangeSum(const SkipSet& c, int lo, int hi) {
  long sum = 0;
  const auto last = c.upper_bound(hi);
  for (auto it = c.lower_bound(lo); it != last; ++it) sum += *it;
  return sum;
}

inline long RangeSum(const RbSet& c, int lo, int hi) {
  long sum = 0;
  const auto last = c.upper_bound(hi);
  for (auto it = c.lower_bound(lo); it != last; ++it) sum += *it;
  return sum;
}

template <typename C>
const char* Name();
template <>
inline const char* Name<SkipSet>() {
  return "SkipList";
}
template <>
inline const char* Name<RbSet>() {
  return "std::set";
}
template <>
inline const char* Name<HashSet>() {
  return "std::unordered_set";
}

// ---------------------------------------------------------------------------
// Key sets
//
// Every container sees byte-identical input: sequences are generated once from
// a fixed seed and cached, so any difference in the numbers can only come from
// the container, never from the data.
//
// Stored keys are the even numbers in [0, 2n) and absent keys are the odd ones.
// Interleaving matters: if the miss probes were simply drawn from above the
// largest stored key, every miss would terminate on the rightmost path and the
// benchmark would measure a degenerate best case rather than a real miss.
// ---------------------------------------------------------------------------

constexpr std::uint32_t SEED = 20250921u;

namespace detail {

inline const std::vector<int>& Cached(
    std::map<std::size_t, std::vector<int>>& cache, std::size_t n, int first,
    std::uint32_t shuffleSeed) {
  auto it = cache.find(n);
  if (it != cache.end()) return it->second;

  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = first + 2 * static_cast<int>(i);
  }
  if (shuffleSeed != 0) {
    std::mt19937 rng(shuffleSeed);
    std::shuffle(keys.begin(), keys.end(), rng);
  }
  return cache.emplace(n, std::move(keys)).first->second;
}

}  // namespace detail

// The n stored keys -- even numbers -- in a fixed shuffled order.
inline const std::vector<int>& ShuffledKeys(std::size_t n) {
  static std::map<std::size_t, std::vector<int>> cache;
  return detail::Cached(cache, n, 0, SEED);
}

// The same n stored keys in ascending order.
inline const std::vector<int>& SequentialKeys(std::size_t n) {
  static std::map<std::size_t, std::vector<int>> cache;
  return detail::Cached(cache, n, 0, 0);
}

// n keys that are never stored -- the odd numbers between the stored ones, so
// each miss terminates in the middle of the structure, not past its end.
inline const std::vector<int>& MissingKeys(std::size_t n) {
  static std::map<std::size_t, std::vector<int>> cache;
  return detail::Cached(cache, n, 1, SEED + 1u);
}

// The stored keys again, shuffled differently, so erase order is unrelated to
// insertion order.
inline const std::vector<int>& EraseOrder(std::size_t n) {
  static std::map<std::size_t, std::vector<int>> cache;
  return detail::Cached(cache, n, 0, SEED + 2u);
}

// A 70/20/10 lookup/insert/erase script of length n, replayed against a
// container already holding ShuffledKeys(n). Lookups and erases draw from the
// stored even keys so they hit; inserts draw from the odd keys so they succeed.
enum class Op { kLookup, kInsert, kErase };

struct MixedOp {
  Op op;
  int key;
};

inline const std::vector<MixedOp>& MixedScript(std::size_t n) {
  static std::map<std::size_t, std::vector<MixedOp>> cache;
  auto it = cache.find(n);
  if (it != cache.end()) return it->second;

  std::mt19937 rng(SEED + 3u);
  std::uniform_int_distribution<int> pick(0, 99);
  std::uniform_int_distribution<int> slot(0, static_cast<int>(n) - 1);

  std::vector<MixedOp> script;
  script.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const int roll = pick(rng);
    if (roll < 70) {
      script.push_back({Op::kLookup, 2 * slot(rng)});
    } else if (roll < 90) {
      script.push_back({Op::kInsert, 2 * slot(rng) + 1});
    } else {
      script.push_back({Op::kErase, 2 * slot(rng)});
    }
  }
  return cache.emplace(n, std::move(script)).first->second;
}

}  // namespace bench
