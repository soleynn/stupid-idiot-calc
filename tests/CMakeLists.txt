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
  test_parser.cpp
  test_evaluator.cpp
  test_environment.cpp
  test_trace.cpp
  test_format.cpp
  test_diagnostic.cpp)

# tests link the engine directly (no cli, no stdin/stdout). they get the
# warning flags but NOT -Werror, since catch2 macros expand in these TUs.
target_link_libraries(calc_tests PRIVATE calc_core calc_warnings
                                         Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(calc_tests)

# a few black-box checks of the actual binary: the scriptable one-shot contract
# (a result and nothing else on stdout), --version, --help, and the error exit
# code. these are the only tests that run the cli end to end.
add_test(NAME cli_oneshot COMMAND calc "2 + 3 * 4")
set_tests_properties(cli_oneshot PROPERTIES
  PASS_REGULAR_EXPRESSION "^14\n"
  FAIL_REGULAR_EXPRESSION "stupid idiot calc|tokens:")

add_test(NAME cli_trace COMMAND calc --trace "2 + 3 * 4")
set_tests_properties(cli_trace PROPERTIES PASS_REGULAR_EXPRESSION "tokens:")

add_test(NAME cli_version COMMAND calc --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "^calc [0-9]")

add_test(NAME cli_help COMMAND calc --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "[Uu]sage")

add_test(NAME cli_oneshot_error COMMAND calc "1 / 0")
set_tests_properties(cli_oneshot_error PROPERTIES WILL_FAIL TRUE)
