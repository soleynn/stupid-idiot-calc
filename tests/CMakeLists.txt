# pull catch2 at configure time, pinned to an exact tag. it builds into the
# build/ dir (gitignored), so nothing is vendored into the repo.
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.15.0)
FetchContent_MakeAvailable(Catch2)

# rapidcheck for property-based parser tests (BSD-2). it has no releases, so its
# pinned to a commit. we DONT set RC_ENABLE_CATCH: that makes rapidcheck vendor
# its own Catch2 target, which collides with ours. its catch binding is also
# catch2-v2-shaped, so instead we use the standalone rc::check() and wrap it in a
# catch2 REQUIRE (see test_properties.cpp) - works cleanly against catch2 v3.
FetchContent_Declare(
  rapidcheck
  GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
  GIT_TAG b2d9ed2dddefc4b84318d664b4f221eb792d89c7)
FetchContent_MakeAvailable(rapidcheck)

# rapidcheck headers arent clean under our warning flags; mark them system.
get_target_property(rapidcheck_includes rapidcheck INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(rapidcheck PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${rapidcheck_includes}")

add_executable(calc_tests
  test_result.cpp
  test_lexer.cpp
  test_parser.cpp
  test_evaluator.cpp
  test_environment.cpp
  test_trace.cpp
  test_logger.cpp
  test_properties.cpp
  test_format.cpp
  test_diagnostic.cpp)

# tests link the engine directly (no cli, no stdin/stdout). they get the
# warning flags but NOT -Werror, since catch2 macros expand in these TUs. fmt is
# here for test_logger; rapidcheck drives the property tests.
target_link_libraries(calc_tests PRIVATE calc_core calc_warnings
                                         Catch2::Catch2WithMain fmt::fmt
                                         rapidcheck)

# test_properties.cpp fuses catch2 v3 and rapidcheck into one TU, which can
# overflow the COFF section limit on msvc; /bigobj lifts it (no-op elsewhere).
target_compile_options(calc_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

# test_logger.cpp covers the cli-private logger, which lives under src/cli (not
# the engine's public include dir), so give the test target that path.
target_include_directories(calc_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/cli)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(calc_tests)

# a standalone small-stack stress test for the bounded-eval fix: it evaluates
# the largest flat expression the parser accepts on a deliberately tiny 256kb
# thread stack, so any slip back to per-node recursion in eval / --trace render
# / ~Expr overflows and fails THIS exe - in its own process, so a regression is
# one red test instead of a segfault that takes the whole suite down. linux-only
# (it needs pthread_attr_setstacksize); the other os legs skip it and stay
# green. it runs in the linux ctest leg, so a regression rightly fails ci.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Threads REQUIRED)
  add_executable(stress_small_stack stress_small_stack.cpp)
  target_link_libraries(stress_small_stack PRIVATE calc_core calc_warnings
                                                   Threads::Threads)
  add_test(NAME stress_small_stack COMMAND stress_small_stack)
endif()

# a few black-box checks of the actual binary: the scriptable one-shot contract
# (a result and nothing else on stdout), --version, --help, and the error exit
# code. these are the only tests that run the cli end to end. line-anchored
# matches allow an optional \r since msvc's text-mode stdout writes \r\n.
add_test(NAME cli_oneshot COMMAND calc "2 + 3 * 4")
set_tests_properties(cli_oneshot PROPERTIES
  PASS_REGULAR_EXPRESSION "^14\r?\n"
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

# a leading-minus expression must reach the engine, not get read as a flag.
add_test(NAME cli_oneshot_negative COMMAND calc "-5")
set_tests_properties(cli_oneshot_negative PROPERTIES PASS_REGULAR_EXPRESSION "^-5\r?\n")

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

  # pin the one-shot contract exactly: `calc 2+2` is the literal `4`, nothing
  # else. the regex tests above cant prove "and nothing else"; an equality check
  # can. tr drops the \r msvc adds to each line so the compare holds on windows.
  add_test(NAME cli_oneshot_exact COMMAND ${BASH_EXECUTABLE} -c
    "[ \"$($<TARGET_FILE:calc> '2+2' | tr -d '\\r')\" = '4' ]")

  # --trace narrates to stderr only: with stderr dropped, stdout is still just
  # the result.
  add_test(NAME cli_trace_stdout_clean COMMAND ${BASH_EXECUTABLE} -c
    "[ \"$($<TARGET_FILE:calc> --trace '2 + 2' 2>/dev/null | tr -d '\\r')\" = '4' ]")

  add_test(NAME cli_stats COMMAND ${BASH_EXECUTABLE} -c
    "printf '2 + 2\\n:stats\\n:quit\\n' | $<TARGET_FILE:calc>")
  set_tests_properties(cli_stats PROPERTIES PASS_REGULAR_EXPRESSION "1 ok")

  # a long session must not grow :history without bound. feed more lines than the
  # cap (kMaxHistory = 1000 in main.cpp), then :history, and count the numbered
  # entries it prints: capped it's exactly 1000, unbounded it would be 1051. the
  # numbered ":history" lines lead with spaces; the bare eval results dont, so
  # the grep counts only the history rows.
  add_test(NAME cli_history_capped COMMAND ${BASH_EXECUTABLE} -c
    "[ \"$({ seq 1050; printf ':history\\n:quit\\n'; } | $<TARGET_FILE:calc> 2>/dev/null | grep -cE '^ +[0-9]+  ')\" = '1000' ]")
endif()
