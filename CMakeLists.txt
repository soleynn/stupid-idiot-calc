cmake_minimum_required(VERSION 3.21)
project(stupid_idiot_calc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# drop the binaries straight in the build dir so the run command is short.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

option(CALC_BUILD_TESTS "build the test suite" ON)
option(CALC_WARNINGS_AS_ERRORS "treat warnings as errors (on in ci)" OFF)
option(CALC_ENABLE_SANITIZERS "build with address + undefined sanitizers" OFF)

# one home for the warning flags. every first-party target links this.
add_library(calc_warnings INTERFACE)
target_compile_options(
  calc_warnings
  INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->)

# -Werror only on our own code (not the test TUs, which expand third-party
# macros). turned on in ci via the option.
add_library(calc_strict INTERFACE)
if(CALC_WARNINGS_AS_ERRORS)
  target_compile_options(
    calc_strict INTERFACE $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
                          $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

# optional sanitizers. off by default, flipped on by the `sanitize` preset.
add_library(calc_sanitizers INTERFACE)
if(CALC_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(calc_sanitizers INTERFACE -fsanitize=address,undefined
                                                   -fno-omit-frame-pointer)
  target_link_options(calc_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(src/engine)
add_subdirectory(src/cli)

if(CALC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
