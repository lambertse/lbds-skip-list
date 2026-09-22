# skiplist

## Sample code
```cpp
#include <skiplist/skiplist.h>

lbds::skip_list::SkipList<int> numbers;

bool added = numbers.insert(42);      // true  -- inserted
added = numbers.insert(42);           // false -- already present
bool present = numbers.find(42) != numbers.end();  // true
bool removed = numbers.erase(42);     // true

// Ordered traversal, ascending under the comparator.
for (int value : numbers) { /* ... */ }

// Range query: every value in [10, 20]. Hoist the end iterator -- each
// upper_bound() call is its own O(log n) descent.
const auto last = numbers.upper_bound(20);
for (auto it = numbers.lower_bound(10); it != last; ++it) { /* ... */ }
```


## API

| Member | Meaning |
| --- | --- |
| `insert(const T&)` / `insert(T&&)` | `true` if inserted, `false` if an equivalent element was already present |
| `upsert(const T&)` / `upsert(T&&)` | `true` if the value was added, `false` if an equivalent element was already present and has been assigned over |
| `erase(const T&)` | `true` if an element was removed |
| `find(const T&)` | iterator to the element, or `end()`; a membership test is `find(v) != end()` |
| `lower_bound(const T&)` | first element not ordered before the argument |
| `upper_bound(const T&)` | first element ordered strictly after the argument |
| `begin()` / `end()` / `cbegin()` / `cend()` | const forward iteration, in order |
| `size()` / `empty()` / `clear()` | size and reset |
| `display()` | prints every level to stdout; a debugging aid, not an API to build on |

## Requirements

C++17 standalone. The library itself has no dependencies; the test and
benchmark targets fetch GoogleTest and google/benchmark at configure time.

## Building

The library is header-only, so using it is just an include path — or, with
CMake:

```cmake
add_subdirectory(skiplist)
target_link_libraries(your_target PRIVATE skiplist::skiplist)
```

Tests are on by default, benchmarks are off:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Benchmarks are a separate, opt-in target:

```bash
cmake -S . -B build-bench -DSKIPLIST_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j
./build-bench/benchmark/skip_list_bench --benchmark_repetitions=15 \
    --benchmark_report_aggregates_only=true
./build-bench/benchmark/skip_list_memory
```

## Benchmarks

### Method 

**Insert N keys in random order** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.068 | 0.998 | 20.629 | 1207.910 |
| `std::set` | 0.030 | 0.760 | 12.262 | 584.735 |
| `std::unordered_set` | 0.020 | 0.184 | 2.886 | 104.284 |
| **SkipList vs `std::set`** | 2.26x | 1.31x | 1.68x | 2.07x |

**Insert N keys in ascending order** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.058 | 0.639 | 7.205 | 81.992 |
| `std::set` | 0.033 | 0.480 | 6.373 | 133.654 |
| `std::unordered_set` | 0.019 | 0.141 | 1.568 | 15.716 |
| **SkipList vs `std::set`** | 1.74x | 1.33x | 1.13x | 0.61x |

**Lookup, every key present** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.038 | 0.946 | 21.691 | 1428.110 |
| `std::set` | 0.024 | 0.717 | 15.092 | 816.118 |
| `std::unordered_set` | 0.001 | 0.016 | 0.247 | 10.659 |
| **SkipList vs `std::set`** | 1.62x | 1.32x | 1.44x | 1.75x |

**Lookup, every key absent** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.037 | 0.903 | 19.914 | 1209.110 |
| `std::set` | 0.021 | 0.702 | 13.090 | 861.190 |
| `std::unordered_set` | 0.003 | 0.030 | 0.563 | 23.837 |
| **SkipList vs `std::set`** | 1.72x | 1.29x | 1.52x | 1.40x |

**Erase every key** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.065 | 0.992 | 18.446 | 935.361 |
| `std::set` | 0.045 | 0.882 | 14.037 | 631.840 |
| `std::unordered_set` | 0.010 | 0.131 | 1.666 | 109.806 |
| **SkipList vs `std::set`** | 1.44x | 1.13x | 1.31x | 1.48x |

**Mixed workload, 70/20/10 lookup/insert/erase** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.066 | 1.060 | 19.693 | 1063.880 |
| `std::set` | 0.022 | 0.765 | 12.549 | 757.863 |
| `std::unordered_set` | 0.005 | 0.100 | 0.967 | 45.113 |
| **SkipList vs `std::set`** | 3.01x | 1.39x | 1.57x | 1.40x |

**Full traversal** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.001 | 0.048 | 0.721 | 62.515 |
| `std::set` | 0.002 | 0.090 | 1.183 | 75.096 |
| `std::unordered_set` * | 0.001 | 0.025 | 0.302 | 12.134 |
| **SkipList vs `std::set`** | 0.42x | 0.53x | 0.61x | 0.83x |

**Range query: 20 ranges of 1% of the container each** -- milliseconds, median of 15 repetitions

| container | N=10³ | N=10⁴ | N=10⁵ | N=10⁶ |
| --- | ---: | ---: | ---: | ---: |
| SkipList | 0.001 | 0.007 | 0.146 | 10.898 |
| `std::set` | 0.001 | 0.014 | 0.230 | 13.020 |
| `std::unordered_set` | not supported | not supported | not supported | not supported |
| **SkipList vs `std::set`** | 0.75x | 0.48x | 0.63x | 0.84x |

### Memory

Live heap bytes and `operator new` calls per element, measured by a global
allocation counter in a separate binary (`skip_list_memory`) so the counter's
own overhead cannot bias the timing results.

| container | bytes/element | allocations/element |
| --- | ---: | ---: |
| SkipList | **18.67** | **1.00** |
| `std::set` | 40.00 | 1.00 |
| `std::unordered_set` | 24.22 – 29.84 | 1.00 |

Flat across all four sizes. A node is a value, a height, and its links in one
block: `4 + 1 + 8` bytes, rounded to 16 by alignment, plus 8 more per extra
level, and at `P = 0.25` the average tower is 1.33 links tall — which is the
18.67 measured.

## License

MIT — see [LICENSE](LICENSE).
