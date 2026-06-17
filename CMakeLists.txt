cmake_minimum_required(VERSION 3.21)
project(stupid_idiot_calc VERSION 0.2.0 LANGUAGES CXX)

# the version string baked into `--version`. defaults to the project version, but
# a release build overrides it (`-DCALC_VERSION=...`) so the published binaries
# report the git tag they were cut from instead of the hardcoded project version.
set(CALC_VERSION "${PROJECT_VERSION}" CACHE STRING "version string baked into --version")

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
option(CALC_BUILD_GUI "build the Qt Quick gui front-end (needs Qt 6)" OFF)

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
# there so that job doesnt clone deps it never links. its also desktop-only - the
# isocline line-editing, isatty checks and posix signal handler dont belong on
# android, which ships engine + gui only, so its gated off ANDROID too.
if(NOT ANDROID AND NOT (CALC_ENABLE_FUZZERS AND NOT CALC_BUILD_TESTS))
  add_subdirectory(src/cli)
endif()

# the Qt gui front-end, off by default so the plain engine + cli build needs no
# Qt at all. the `gui` preset and the advisory gui ci leg turn it on (Qt comes
# via aqtinstall). it links the same calc_core, so the engine never knows it
# exists - the front-end split, cashed in again.
if(CALC_BUILD_GUI)
  add_subdirectory(src/gui)
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

# package calc-gui into a self-contained archive: a .dmg on mac, a .zip on win.
# version comes from CALC_VERSION (the release injects the git tag via -D). linux
# ships via flatpak, so this is apple/win only and inert everywhere else - the
# engine+cli build and the ubuntu gui leg never see CPack. stays at the very end,
# after add_subdirectory(src/gui), so every install() rule is registered before
# include(CPack) snapshots them.
if(CALC_BUILD_GUI AND (APPLE OR WIN32))
  set(CPACK_PACKAGE_NAME "stupid-idiot-calc")
  set(CPACK_PACKAGE_VERSION "${CALC_VERSION}")
  set(CPACK_PACKAGE_VENDOR "soleynn")
  set(CPACK_VERBATIM_VARIABLES ON)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "stupid-idiot-calc-${CALC_VERSION}-macos")
    set(CPACK_DMG_VOLUME_NAME "stupid idiot calc ${CALC_VERSION}")
  elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "stupid-idiot-calc-${CALC_VERSION}-windows")
  endif()
  include(CPack)
endif()
