// Memory accounting: bytes and allocation count per element, for SkipList vs
// std::set vs std::unordered_set.
//
// This lives in its own binary on purpose. The global operator new below adds
// a header word and a counter update to every allocation in the process; in
// the timing binary that would tax the container that allocates most, which is
// exactly the thing under measurement. Here nothing is timed, so the overhead
// is harmless.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include "containers.h"

namespace {

struct Counters {
  std::size_t liveBytes = 0;   // outstanding bytes, new minus delete
  std::size_t totalCount = 0;  // every allocation ever made
};

Counters g_counters;

// Each block carries its size in a header so deallocation can subtract the
// right amount. sized-delete is not reliable enough on its own: it is not
// called for every deallocation path.
constexpr std::size_t kHeader = alignof(std::max_align_t);

}  // namespace

void* operator new(std::size_t size) {
  void* raw = std::malloc(size + kHeader);
  if (raw == nullptr) throw std::bad_alloc();

  *static_cast<std::size_t*>(raw) = size;
  g_counters.liveBytes += size;
  g_counters.totalCount += 1;
  return static_cast<char*>(raw) + kHeader;
}

void operator delete(void* ptr) noexcept {
  if (ptr == nullptr) return;
  char* raw = static_cast<char*>(ptr) - kHeader;
  g_counters.liveBytes -= *reinterpret_cast<std::size_t*>(raw);
  std::free(raw);
}

// C++14 sized delete: must free the same way, and must not double-count.
void operator delete(void* ptr, std::size_t) noexcept { operator delete(ptr); }

void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* ptr) noexcept { operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept {
  operator delete(ptr);
}

namespace {

struct Measurement {
  const char* name;
  double bytesPerElement;
  double allocationsPerElement;
};

// Build a container of n elements and report what it cost. The container is
// allocated inside the measured window and destroyed after the snapshot, so
// the numbers describe a populated container and nothing else.
template <typename C>
Measurement Measure(std::size_t n) {
  const std::vector<int>& keys = bench::ShuffledKeys(n);

  const Counters before = g_counters;
  auto c = std::make_unique<C>();
  for (const int key : keys) {
    if (!bench::Insert(*c, key)) {
      std::fprintf(stderr, "duplicate key %d -- key generation is broken\n",
                   key);
      std::exit(1);
    }
  }
  const Counters after = g_counters;

  if (c->size() != n) {
    std::fprintf(stderr, "%s holds %zu elements, expected %zu\n",
                 bench::Name<C>(), c->size(), n);
    std::exit(1);
  }

  const auto elements = static_cast<double>(n);
  return Measurement{
      bench::Name<C>(),
      static_cast<double>(after.liveBytes - before.liveBytes) / elements,
      static_cast<double>(after.totalCount - before.totalCount) / elements,
  };
}

void ReportSize(std::size_t n) {
  // Measured one at a time: only one container is alive per call, so the
  // live-bytes delta cannot be contaminated by another container.
  const Measurement results[] = {
      Measure<bench::SkipSet>(n),
      Measure<bench::RbSet>(n),
      Measure<bench::HashSet>(n),
  };

  std::printf("\nN = %zu\n", n);
  std::printf("%-22s %14s %18s\n", "container", "bytes/element",
              "allocations/element");
  std::printf("%-22s %14s %18s\n", "----------------------", "--------------",
              "------------------");
  for (const Measurement& m : results) {
    std::printf("%-22s %14.2f %18.2f\n", m.name, m.bytesPerElement,
                m.allocationsPerElement);
  }
}

}  // namespace

int main() {
  std::printf("Memory footprint per element (int keys)\n");
  std::printf(
      "bytes/element is live heap at peak population; allocations/element "
      "counts\nevery call to operator new made while building, including "
      "transient ones.\n");

  for (const std::size_t n : {std::size_t{1000}, std::size_t{10000},
                              std::size_t{100000}, std::size_t{1000000}}) {
    ReportSize(n);
  }
  return 0;
}
