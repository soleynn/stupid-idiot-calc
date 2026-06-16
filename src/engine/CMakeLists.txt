# fmt for number + diagnostic formatting, pinned and fetched at configure time
# (same as catch2). marked system below so its headers skip our -Werror.
include(FetchContent)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.0.2)
FetchContent_MakeAvailable(fmt)

get_target_property(fmt_includes fmt INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(fmt PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${fmt_includes}")

# fast_float for parsing number literals. header-only, MIT. the float overload
# of std::from_chars only reached libc++ in llvm 20, so apple clang and the
# android ndk wont compile the lexer against std; fast_float is a drop-in (its
# what libstdc++ uses under the hood) and works on every toolchain. system below
# so its headers skip our -Werror.
FetchContent_Declare(
  fast_float
  GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
  GIT_TAG v8.2.10)
FetchContent_MakeAvailable(fast_float)

get_target_property(fast_float_includes fast_float INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(fast_float PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${fast_float_includes}")

# the calculator engine. pure logic, knows nothing about stdin/stdout. the cli
# and the tests both link this.
add_library(calc_core STATIC
  lexer.cpp
  parser.cpp
  evaluator.cpp
  output_formatter.cpp
  diagnostic.cpp)

target_include_directories(calc_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(calc_core PUBLIC calc_sanitizers calc_coverage calc_fuzzer
                                PRIVATE calc_warnings calc_strict fmt::fmt
                                        FastFloat::fast_float)
