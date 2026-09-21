cmake_minimum_required(VERSION 3.16)

project(
    skiplist
    VERSION 1.0.0
    DESCRIPTION "A header-only C++ skip list"
    LANGUAGES CXX
)

# Generate build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SKIPLIST_BUILD_TESTS "Build skiplist tests" ON)
option(SKIPLIST_BUILD_BENCHMARKS "Build skiplist benchmarks" OFF)

# Header-only skiplist library.
add_library(skiplist INTERFACE)
add_library(skiplist::skiplist ALIAS skiplist)

target_compile_features(
    skiplist
    INTERFACE
        cxx_std_17
)

target_include_directories(
    skiplist
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(SKIPLIST_BUILD_TESTS)
  enable_testing()
  include(CTest)
  add_subdirectory(test)
endif()

if(SKIPLIST_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()
