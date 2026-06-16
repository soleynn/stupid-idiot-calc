# the libFuzzer harness for the parse pipeline. clang-only: -fsanitize=fuzzer
# brings its own main here. the engine under test is instrumented separately -
# calc_core links calc_fuzzer (-fsanitize=fuzzer-no-link) so libFuzzer steers on
# coverage from the grammar, not just this wrapper. address+undefined come from
# calc_core (PUBLIC calc_sanitizers) when CALC_ENABLE_SANITIZERS is also on, so
# the advisory fuzz ci job sets both. built apart from the gcc `build` gate.
add_executable(fuzz_parse fuzz_parse.cpp)
target_link_libraries(fuzz_parse PRIVATE calc_core)
target_compile_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
