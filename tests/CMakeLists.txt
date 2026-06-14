# pull catch2 at configure time, pinned to an exact tag. it builds into the
# build/ dir (gitignored), so nothing is vendored into the repo.
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.15.0)
FetchContent_MakeAvailable(Catch2)

add_executable(calc_tests
  test_result.cpp
  test_lexer.cpp
  test_evaluator.cpp
  test_format.cpp)

# tests link the engine directly (no cli, no stdin/stdout). they get the
# warning flags but NOT -Werror, since catch2 macros expand in these TUs.
target_link_libraries(calc_tests PRIVATE calc_core calc_warnings
                                         Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(calc_tests)
