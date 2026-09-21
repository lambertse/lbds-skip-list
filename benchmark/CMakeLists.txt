include(FetchContent)

FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Keep the dependency to just the library: its own test suite would pull in a
# second googletest, and BENCHMARK_ENABLE_WERROR defaults ON, which turns any
# warning from the dependency into a build failure of ours.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googlebenchmark)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(
    WARNING
    "SKIPLIST_BUILD_BENCHMARKS is ON but CMAKE_BUILD_TYPE is empty: the "
    "benchmarks will be built unoptimised and the numbers will be worthless. "
    "Configure with -DCMAKE_BUILD_TYPE=Release."
  )
endif()

# Timing. Linked against google/benchmark; no allocator instrumentation, so
# the harness cannot tax the container that allocates most.
add_executable(skip_list_bench skip_list_bench.cpp)
target_include_directories(skip_list_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  skip_list_bench
  PRIVATE skiplist::skiplist benchmark::benchmark
)

# Memory. Standalone: it replaces global operator new, which is why it must
# not share a binary with the timing benchmarks.
add_executable(skip_list_memory skip_list_memory.cpp)
target_include_directories(skip_list_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(skip_list_memory PRIVATE skiplist::skiplist)
