// the headline crash from the break-it pass: a legal at-cap flat expression
// (1+1+...+1, exactly kMaxTokens tokens) builds a ~2047-deep tree, and walking
// or freeing it used to recurse one native stack frame per node - fine on the
// 8mb main stack, a segfault on the ~256kb-1mb stacks android/qt worker threads
// run on. this runs that exact input on a deliberately tiny 256kb thread stack;
// if the evaluator, the --trace tree render, or ~Expr ever go back to per-node
// recursion this process dies with a signal and the test fails - in its own
// process, so it doesnt take the rest of the suite down with it.
//
// linux-only: it leans on pthread_attr_setstacksize, so the cmake side only
// builds and registers it there.

#include <pthread.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"
#include "calc/lexer.hpp"
#include "calc/number.hpp"
#include "calc/parser.hpp"
#include "calc/result.hpp"
#include "calc/token.hpp"
#include "calc/tracer.hpp"

namespace {

constexpr std::size_t kStackBytes = 256u * 1024u;

// the largest flat expression parse() accepts: 2048 ones + 2047 '+' + an End
// token = 4096 tokens = kMaxTokens. one more term is rejected as "too long".
std::string at_cap_expression() {
  std::string expr = "1";
  for (int i = 0; i < 2047; ++i) {
    expr += "+1";
  }
  return expr;
}

// drops every trace line: we only care that the deep --trace tree render runs
// to completion without overflowing, not what it narrates.
class NullTracer : public calc::Tracer {
public:
  void write(std::string_view) override {}
};

// the flag the small-stack thread hands back: nullptr means every check passed,
// a non-null pointer means a wrong answer. a stack overflow never gets here -
// it kills the thread, which shows up as a failed join / killed process
// instead.
int failure = 1;

void *run_checks(void * /*unused*/) {
  const std::string expr = at_cap_expression();

  // plain evaluate: parse + iterative eval + iterative ~Expr teardown, all on
  // the small stack.
  {
    calc::Environment env;
    const calc::Result<calc::Number> result = calc::evaluate(expr, env);
    if (!result.has_value() || result.value() != 2048.0) {
      return &failure;
    }
  }

  // the --trace path lays the deep tree render on top of the same walk.
  {
    calc::Environment env;
    NullTracer tracer;
    const calc::Result<calc::Number> result =
        calc::evaluate(expr, env, &tracer);
    if (!result.has_value() || result.value() != 2048.0) {
      return &failure;
    }
  }

  // teardown on its own: the break-it pass showed freeing the tree overflows
  // even without evaluating it. build one and let it drop at the end of scope.
  {
    const calc::Result<std::vector<calc::Token>> tokens = calc::tokenize(expr);
    if (!tokens.has_value()) {
      return &failure;
    }
    const calc::Result<calc::Expr> tree = calc::parse(tokens.value());
    if (!tree.has_value()) {
      return &failure;
    }
  } // ~Expr runs here, on the small stack

  return nullptr;
}

} // namespace

int main() {
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    return 2;
  }
  if (pthread_attr_setstacksize(&attr, kStackBytes) != 0) {
    pthread_attr_destroy(&attr);
    return 2;
  }

  pthread_t thread;
  const int created = pthread_create(&thread, &attr, run_checks, nullptr);
  pthread_attr_destroy(&attr);
  if (created != 0) {
    return 2;
  }

  void *result = nullptr;
  if (pthread_join(thread, &result) != 0) {
    return 2;
  }
  return result == nullptr ? 0 : 1;
}
