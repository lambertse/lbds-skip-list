// Timing benchmarks: SkipList vs std::set (red-black tree) vs
// std::unordered_set (hash table).
//
// Deliberately kept free of any global operator new instrumentation -- that
// lives in skip_list_memory.cpp, in a separate binary, so the allocation
// counter's own overhead cannot bias the containers that allocate most.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "containers.h"

namespace {

using bench::Erase;
using bench::HashSet;
using bench::Insert;
using bench::Lookup;
using bench::MixedOp;
using bench::MixedScript;
using bench::RangeSum;
using bench::SumAll;
using bench::EraseOrder;
using bench::MissingKeys;
using bench::Name;
using bench::Op;
using bench::RbSet;
using bench::SequentialKeys;
using bench::ShuffledKeys;
using bench::SkipSet;

// SkipList deletes both copy and move, so a container can never be returned
// from a factory or parked in a vector. unique_ptr moves the pointer, not the
// container, which keeps the generic shape working for all three types.
template <typename C>
std::unique_ptr<C> MakeEmpty() {
  return std::make_unique<C>();
}

template <typename C>
std::unique_ptr<C> MakePopulated(const std::vector<int>& keys) {
  auto c = MakeEmpty<C>();
  for (const int key : keys) {
    benchmark::DoNotOptimize(Insert(*c, key));
  }
  return c;
}

// ---------------------------------------------------------------------------
// 1 & 2. Insert N keys into an empty container.
//
// Construction is timed (it is cheap and part of the cost); destruction is
// not, since teardown is a separate concern from insertion throughput.
// ---------------------------------------------------------------------------

template <typename C>
void BM_InsertRandom(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const std::vector<int>& keys = ShuffledKeys(n);

  for (auto _ : state) {
    auto c = MakeEmpty<C>();
    for (const int key : keys) {
      benchmark::DoNotOptimize(Insert(*c, key));
    }
    state.PauseTiming();
    c.reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

template <typename C>
void BM_InsertSequential(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const std::vector<int>& keys = SequentialKeys(n);

  for (auto _ : state) {
    auto c = MakeEmpty<C>();
    for (const int key : keys) {
      benchmark::DoNotOptimize(Insert(*c, key));
    }
    state.PauseTiming();
    c.reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

// ---------------------------------------------------------------------------
// 3 & 4. Lookup at a fixed size N, hits and misses measured separately.
//
// The container is built once, before the timing loop, so only the lookups
// are measured.
// ---------------------------------------------------------------------------

template <typename C>
void BM_LookupHit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const std::vector<int>& keys = ShuffledKeys(n);
  auto c = MakePopulated<C>(keys);

  for (auto _ : state) {
    for (const int key : keys) {
      benchmark::DoNotOptimize(Lookup(*c, key));
    }
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

template <typename C>
void BM_LookupMiss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto c = MakePopulated<C>(ShuffledKeys(n));
  const std::vector<int>& probes = MissingKeys(n);

  for (auto _ : state) {
    for (const int key : probes) {
      benchmark::DoNotOptimize(Lookup(*c, key));
    }
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

// ---------------------------------------------------------------------------
// 5. Erase every key, in an order unrelated to insertion order.
//
// Population is paused out: including it would fold insert cost into the
// erase number, and insert dominates.
// ---------------------------------------------------------------------------

template <typename C>
void BM_EraseAll(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const std::vector<int>& keys = ShuffledKeys(n);
  const std::vector<int>& order = EraseOrder(n);

  for (auto _ : state) {
    state.PauseTiming();
    auto c = MakePopulated<C>(keys);
    state.ResumeTiming();

    for (const int key : order) {
      benchmark::DoNotOptimize(Erase(*c, key));
    }

    state.PauseTiming();
    c.reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

// ---------------------------------------------------------------------------
// 6. Mixed 70/20/10 lookup/insert/erase against a container already at size N.
// ---------------------------------------------------------------------------

template <typename C>
void BM_Mixed(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  const std::vector<int>& keys = ShuffledKeys(n);
  const std::vector<MixedOp>& script = MixedScript(n);

  for (auto _ : state) {
    state.PauseTiming();
    auto c = MakePopulated<C>(keys);
    state.ResumeTiming();

    for (const MixedOp& op : script) {
      switch (op.op) {
        case Op::kLookup:
          benchmark::DoNotOptimize(Lookup(*c, op.key));
          break;
        case Op::kInsert:
          benchmark::DoNotOptimize(Insert(*c, op.key));
          break;
        case Op::kErase:
          benchmark::DoNotOptimize(Erase(*c, op.key));
          break;
      }
    }

    state.PauseTiming();
    c.reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

// ---------------------------------------------------------------------------
// 7 & 8. Ordered traversal and range query -- the operations an ordered
// container exists for. Both were unmeasurable before the list had iterators.
// ---------------------------------------------------------------------------

template <typename C>
void BM_IterateAll(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto c = MakePopulated<C>(ShuffledKeys(n));

  for (auto _ : state) {
    benchmark::DoNotOptimize(SumAll(*c));
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * n));
}

// Twenty ranges, each spanning 1% of the container, walked end to end: 20%
// of the elements are visited per iteration.
template <typename C>
void BM_RangeQuery(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto c = MakePopulated<C>(ShuffledKeys(n));

  const int span = static_cast<int>(n) / 50;  // keys are even: n/50 spans 1%
  const int stride = static_cast<int>(n) / 10;
  for (auto _ : state) {
    for (int lo = 0; lo < static_cast<int>(2 * n); lo += stride) {
      benchmark::DoNotOptimize(RangeSum(*c, lo, lo + span));
    }
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Registration: every workload x every container x N = 1e3 .. 1e6.
// ---------------------------------------------------------------------------

#define BENCH_SIZES Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)

#define REGISTER_FOR(workload, container, label)         \
  BENCHMARK_TEMPLATE(workload, container)                \
      ->Name(#workload "/" label)                        \
      ->BENCH_SIZES                                      \
      ->Unit(benchmark::kMillisecond)

#define REGISTER_ALL(workload)                              \
  REGISTER_FOR(workload, bench::SkipSet, "SkipList");       \
  REGISTER_FOR(workload, bench::RbSet, "std_set");          \
  REGISTER_FOR(workload, bench::HashSet, "std_unordered_set")

REGISTER_ALL(BM_InsertRandom);
REGISTER_ALL(BM_InsertSequential);
REGISTER_ALL(BM_LookupHit);
REGISTER_ALL(BM_LookupMiss);
REGISTER_ALL(BM_EraseAll);
REGISTER_ALL(BM_Mixed);
REGISTER_ALL(BM_IterateAll);

// No std::unordered_set row: a hash table has no lower_bound, so the range
// query is not slower there -- it is unavailable.
REGISTER_FOR(BM_RangeQuery, bench::SkipSet, "SkipList");
REGISTER_FOR(BM_RangeQuery, bench::RbSet, "std_set");

BENCHMARK_MAIN();
