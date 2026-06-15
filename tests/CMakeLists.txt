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
  test_logger.cpp
  test_format.cpp
  test_diagnostic.cpp)

# tests link the engine directly (no cli, no stdin/stdout). they get the
# warning flags but NOT -Werror, since catch2 macros expand in these TUs. fmt is
# here for test_logger, which exercises the logger header's macros.
target_link_libraries(calc_tests PRIVATE calc_core calc_warnings
                                         Catch2::Catch2WithMain fmt::fmt)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(calc_tests)

# a few black-box checks of the actual binary: the scriptable one-shot contract
# (a result and nothing else on stdout), --version, --help, and the error exit
# code. these are the only tests that run the cli end to end.
add_test(NAME cli_oneshot COMMAND calc "2 + 3 * 4")
set_tests_properties(cli_oneshot PROPERTIES
  PASS_REGULAR_EXPRESSION "^14\n"
  FAIL_REGULAR_EXPRESSION "stupid idiot calc|tokens:|\\[debug\\]")

add_test(NAME cli_trace COMMAND calc --trace "2 + 3 * 4")
set_tests_properties(cli_trace PROPERTIES PASS_REGULAR_EXPRESSION "tokens:")

add_test(NAME cli_log_level COMMAND calc --log-level debug "2 + 2")
set_tests_properties(cli_log_level PROPERTIES PASS_REGULAR_EXPRESSION "\\[debug\\]")

add_test(NAME cli_log_level_bad COMMAND calc --log-level nope "2 + 2")
set_tests_properties(cli_log_level_bad PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_version COMMAND calc --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "^calc [0-9]")

add_test(NAME cli_help COMMAND calc --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "[Uu]sage")

add_test(NAME cli_oneshot_error COMMAND calc "1 / 0")
set_tests_properties(cli_oneshot_error PROPERTIES WILL_FAIL TRUE)

# the repl meta-commands need input on stdin, so these pipe through a shell.
# the ci runner has bash; if its ever missing the tests are just skipped.
find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
  add_test(NAME cli_vars COMMAND ${BASH_EXECUTABLE} -c
    "printf 'let x = 5\\nlet y = 7\\n:vars\\n:quit\\n' | $<TARGET_FILE:calc>")
  set_tests_properties(cli_vars PROPERTIES PASS_REGULAR_EXPRESSION "x = 5")

  add_test(NAME cli_history COMMAND ${BASH_EXECUTABLE} -c
    "printf '2+2\\n:history\\n:quit\\n' | $<TARGET_FILE:calc>")
  set_tests_properties(cli_history PROPERTIES PASS_REGULAR_EXPRESSION "1  2\\+2")

  add_test(NAME cli_help_lists_commands COMMAND ${BASH_EXECUTABLE} -c
    "printf ':help\\n:quit\\n' | $<TARGET_FILE:calc>")
  set_tests_properties(cli_help_lists_commands PROPERTIES
    PASS_REGULAR_EXPRESSION ":vars")
endif()
