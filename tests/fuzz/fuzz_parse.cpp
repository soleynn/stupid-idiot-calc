#include <cstddef>
#include <cstdint>
#include <string>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"

// libFuzzer entry point. feed arbitrary bytes through the whole engine pipeline
// (lexer -> parser -> evaluator) and only require that it doesnt crash: the
// engine returns its errors as values, so any throw, UB, or segfault the
// sanitizers catch here is a genuine bug. built clang-only with
// -fsanitize=fuzzer; never part of the gcc `build` gate.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, std::size_t size) {
  if (size == 0) {
    return 0; // libFuzzer may pass a null pointer here; dont build a string off
              // it
  }
  const std::string input(reinterpret_cast<const char *>(data), size);
  calc::Environment env;
  (void)calc::evaluate(input, env);
  return 0;
}
