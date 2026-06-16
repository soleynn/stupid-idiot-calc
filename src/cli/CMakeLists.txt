# cxxopts for argument parsing: --version, --help, and one-shot mode. pinned and
# fetched at configure time like the other deps, and marked system so its
# headers skip our -Werror.
include(FetchContent)
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.3.1)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(CXXOPTS_BUILD_TESTS OFF CACHE INTERNAL "")
set(CXXOPTS_ENABLE_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(cxxopts)

get_target_property(cxxopts_includes cxxopts INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(cxxopts PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${cxxopts_includes}")

# isocline gives the repl arrow-key history, line editing and tab completion.
# its shipped as a single C amalgamation (src/isocline.c #includes every other
# .c), so we compile just that one TU into a small static lib. SOURCE_SUBDIR
# points at nothing so MakeAvailable only downloads the source and doesnt run
# isoclines own CMakeLists (which would also build its example/test exes).
FetchContent_Declare(
  isocline
  GIT_REPOSITORY https://github.com/daanx/isocline.git
  GIT_TAG v1.1.0
  SOURCE_SUBDIR none)
FetchContent_MakeAvailable(isocline)

# isocline.c is C, not C++; the root project() is CXX-only, so turn on C here.
enable_language(C)

# ONLY the amalgamation TU. dont add the sibling src/*.c (isocline.c includes
# them) and dont glob them - a second compile would be a duplicate-symbol link
# error. it links neither calc_warnings nor calc_strict, so our -Werror never
# touches third-party C; SYSTEM on the include keeps calc's own -Werror happy
# with isocline.h too.
add_library(isocline_lib STATIC ${isocline_SOURCE_DIR}/src/isocline.c)
set_target_properties(isocline_lib PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(isocline_lib SYSTEM PUBLIC
  ${isocline_SOURCE_DIR}/include)

# thin front-end: read a line, hand it to the engine, print the result. this is
# the only part that touches stdin/stdout. the version string is CALC_VERSION,
# which defaults to project(VERSION) but a release build overrides it from the tag.
# fmt is here for the leveled logger's spdlog-shaped macros.
add_executable(calc main.cpp crash_handler.cpp)
target_compile_definitions(calc PRIVATE CALC_VERSION="${CALC_VERSION}")
target_link_libraries(calc PRIVATE calc_core calc_warnings calc_strict
                                   cxxopts::cxxopts fmt::fmt isocline_lib)
