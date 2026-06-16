cmake_minimum_required(VERSION 3.21)
project(stupid_idiot_calc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# always emit compile_commands.json so clang-tidy and editors can find the flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# drop the binaries straight in the build dir so the run command is short.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

option(CALC_BUILD_TESTS "build the test suite" ON)
option(CALC_WARNINGS_AS_ERRORS "treat warnings as errors (on in ci)" OFF)
option(CALC_ENABLE_SANITIZERS "build with address + undefined sanitizers" OFF)
option(CALC_ENABLE_COVERAGE "build with gcov coverage instrumentation" OFF)
option(CALC_ENABLE_FUZZERS "build the libFuzzer harnesses (clang only)" OFF)

# one home for the warning flags. every first-party target links this.
add_library(calc_warnings INTERFACE)
target_compile_options(
  calc_warnings
  INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->)

# -Werror only on our own code (not the test TUs, which expand third-party
# macros). turned on in ci via the option.
add_library(calc_strict INTERFACE)
if(CALC_WARNINGS_AS_ERRORS)
  target_compile_options(
    calc_strict INTERFACE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
                          $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

# optional sanitizers. off by default, flipped on by the `sanitize` preset.
add_library(calc_sanitizers INTERFACE)
if(CALC_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(calc_sanitizers INTERFACE -fsanitize=address,undefined
                                                   -fno-omit-frame-pointer)
  target_link_options(calc_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

# optional gcov coverage. off by default, flipped on by the `coverage` preset and
# the advisory coverage ci job; calc_core carries it PUBLIC so the tests that
# link the engine get instrumented and emit .gcda when they run.
add_library(calc_coverage INTERFACE)
if(CALC_ENABLE_COVERAGE AND NOT MSVC)
  target_compile_options(calc_coverage INTERFACE --coverage -O0 -g)
  target_link_options(calc_coverage INTERFACE --coverage)
endif()

# coverage instrumentation for the libFuzzer harness. the engine UNDER TEST has
# to be built with -fsanitize=fuzzer-no-link so libFuzzer gets the edge-coverage
# feedback it steers mutation with; the harness exe itself adds -fsanitize=fuzzer
# (which brings its own main). calc_core links this, so without it the fuzzer is
# coverage-blind on the grammar and degrades to random input.
add_library(calc_fuzzer INTERFACE)
if(CALC_ENABLE_FUZZERS AND NOT MSVC)
  target_compile_options(calc_fuzzer INTERFACE -fsanitize=fuzzer-no-link)
endif()

add_subdirectory(src/engine)

# the cli front-end (and its cxxopts/isocline fetches) isnt needed for a
# fuzz-only configure, which builds just the harness against calc_core; skip it
# there so that job doesnt clone deps it never links.
if(NOT (CALC_ENABLE_FUZZERS AND NOT CALC_BUILD_TESTS))
  add_subdirectory(src/cli)
endif()

# clang-only libFuzzer harnesses, off by default and built apart from the test
# suite (so the fuzz ci job doesnt drag in catch2/rapidcheck).
if(CALC_ENABLE_FUZZERS)
  add_subdirectory(tests/fuzz)
endif()

if(CALC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
