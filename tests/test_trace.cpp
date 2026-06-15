#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"
#include "calc/tracer.hpp"

using namespace calc;

namespace {

// a tracer that just collects everything into a string so a test can assert on
// what the engine narrated.
class StringTracer : public Tracer {
public:
  void write(std::string_view text) override { out.append(text); }
  std::string out;
};

std::string trace_of(std::string_view input) {
  Environment env;
  StringTracer tracer;
  evaluate(input, env, &tracer);
  return tracer.out;
}

constexpr auto npos = std::string::npos;

} // namespace

TEST_CASE("a trace has the three labelled sections") {
  const std::string t = trace_of("2 + 3 * 4");
  REQUIRE(t.find("tokens:") != npos);
  REQUIRE(t.find("tree:") != npos);
  REQUIRE(t.find("eval:") != npos);
}

TEST_CASE("the tokens section lists the tokens") {
  const std::string t = trace_of("2 + 3");
  REQUIRE(t.find("number 2") != npos);
  REQUIRE(t.find("number 3") != npos);
  REQUIRE(t.find("end") != npos);
}

TEST_CASE("the tree section is indented under each parent") {
  // for `2 + 3` the tree is `+` with 2 and 3 as deeper-indented children. that
  // exact block only shows up in the tree section, not the flat token/eval
  // ones.
  const std::string t = trace_of("2 + 3");
  REQUIRE(t.find("  +\n    2\n    3\n") != npos);
}

TEST_CASE("eval steps are post-order with intermediate values") {
  const std::string t = trace_of("2 + 3 * 4");
  const auto mul = t.find("3 * 4 = 12");
  const auto add = t.find("2 + 12 = 14");
  REQUIRE(mul != npos);
  REQUIRE(add != npos);
  REQUIRE(mul < add); // the * is computed before the + that depends on it
  REQUIRE(t.find("= 14 in") != npos); // final value, then timing
}

TEST_CASE("a unary minus and a function call each get a step") {
  REQUIRE(trace_of("-5").find("-(5) = -5") != npos);
  REQUIRE(trace_of("sqrt(9)").find("sqrt(9) = 3") != npos);
}

TEST_CASE("names resolve to their value in the trace") {
  REQUIRE(trace_of("pi").find("pi = 3.14159") != npos);
}

TEST_CASE("a failing eval stops and says so") {
  const std::string t = trace_of("1 / 0");
  REQUIRE(t.find("stopped at an error") != npos);
  REQUIRE(t.find("= 0 in") == npos); // no successful final-value line
}

TEST_CASE("a let traces the right-hand side") {
  Environment env;
  StringTracer tracer;
  evaluate("let x = 6 * 7", env, &tracer);
  REQUIRE(tracer.out.find("tree:") != npos);
  REQUIRE(tracer.out.find("6 * 7 = 42") != npos);
}

TEST_CASE("a null tracer leaves the result unchanged") {
  Environment env;
  const Result<Number> result =
      evaluate("2 + 3 * 4", env); // default: no tracer
  REQUIRE(result.has_value());
  REQUIRE(result.value() == 14.0);
}
