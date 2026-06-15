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

# thin front-end: read a line, hand it to the engine, print the result. this is
# the only part that touches stdin/stdout. the version comes from project() so
# theres one source of truth (the release PR will move it into a version.hpp).
# fmt is here for the leveled logger's spdlog-shaped macros.
add_executable(calc main.cpp crash_handler.cpp)
target_compile_definitions(calc PRIVATE CALC_VERSION="${PROJECT_VERSION}")
target_link_libraries(calc PRIVATE calc_core calc_warnings calc_strict
                                   cxxopts::cxxopts fmt::fmt)
