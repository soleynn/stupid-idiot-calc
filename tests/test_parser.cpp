#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

#include "calc/ast.hpp"
#include "calc/error.hpp"
#include "calc/lexer.hpp"
#include "calc/output_formatter.hpp"
#include "calc/parser.hpp"

using namespace calc;

namespace {

// render a tree as a lisp-ish string so a test can pin the exact shape on one
// line. unary minus shows as `neg` so it never reads like binary `-`.
std::string sexpr(const Expr &e);

const char *op_text(BinaryOpKind op) {
  switch (op) {
  case BinaryOpKind::Add:
    return "+";
  case BinaryOpKind::Subtract:
    return "-";
  case BinaryOpKind::Multiply:
    return "*";
  case BinaryOpKind::Divide:
    return "/";
  case BinaryOpKind::Power:
    return "^";
  }
  return "?";
}

std::string sexpr(const Expr &e) {
  if (const auto *n = std::get_if<NumberLiteral>(&e.node)) {
    return format_number(n->value);
  }
  if (const auto *u = std::get_if<UnaryOp>(&e.node)) {
    return "(neg " + sexpr(*u->operand) + ")";
  }
  const auto &b = std::get<BinaryOp>(e.node);
  return std::string("(") + op_text(b.op) + " " + sexpr(*b.lhs) + " " +
         sexpr(*b.rhs) + ")";
}

// lex then parse, the way the engine will. the lexing must succeed for these.
Result<Expr> parse_str(std::string_view input) {
  auto lexed = tokenize(input);
  REQUIRE(lexed.has_value());
  return parse(lexed.value());
}

// shape of a successful parse, as an sexpr string.
std::string shape_of(std::string_view input) {
  auto tree = parse_str(input);
  REQUIRE(tree.has_value());
  return sexpr(tree.value());
}

} // namespace

TEST_CASE("parser reads a bare number") {
  REQUIRE(shape_of("42") == "42");
  REQUIRE(shape_of("3.5") == "3.5");
}

TEST_CASE("parser builds left-associative + and -") {
  REQUIRE(shape_of("1 + 2") == "(+ 1 2)");
  REQUIRE(shape_of("8 - 3 - 2") == "(- (- 8 3) 2)");
  REQUIRE(shape_of("2 + 3 - 1") == "(- (+ 2 3) 1)");
}

TEST_CASE("parser builds left-associative * and /") {
  REQUIRE(shape_of("2 * 3 * 4") == "(* (* 2 3) 4)");
  REQUIRE(shape_of("10 / 2 / 5") == "(/ (/ 10 2) 5)");
}

TEST_CASE("parser gives * and / higher precedence than + and -") {
  REQUIRE(shape_of("2 + 3 * 4") == "(+ 2 (* 3 4))");
  REQUIRE(shape_of("2 * 3 + 4") == "(+ (* 2 3) 4)");
  REQUIRE(shape_of("1 + 2 * 3 - 4") == "(- (+ 1 (* 2 3)) 4)");
}

TEST_CASE("parens override precedence") {
  REQUIRE(shape_of("(2 + 3) * 4") == "(* (+ 2 3) 4)");
  REQUIRE(shape_of("2 * (3 + 4)") == "(* 2 (+ 3 4))");
}

TEST_CASE("parser handles unary minus and a no-op unary plus") {
  REQUIRE(shape_of("-5") == "(neg 5)");
  REQUIRE(shape_of("--5") == "(neg (neg 5))");
  REQUIRE(shape_of("-2 + 3") == "(+ (neg 2) 3)");
  REQUIRE(shape_of("2 * -3") == "(* 2 (neg 3))");
  REQUIRE(shape_of("+5") == "5");
  REQUIRE(shape_of("-(2 + 3)") == "(neg (+ 2 3))");
}

TEST_CASE("parser builds right-associative exponents") {
  REQUIRE(shape_of("2 ^ 3") == "(^ 2 3)");
  REQUIRE(shape_of("2 ^ 3 ^ 2") == "(^ 2 (^ 3 2))");
}

TEST_CASE("exponent precedence against * and a unary minus") {
  REQUIRE(shape_of("2 * 3 ^ 2") == "(* 2 (^ 3 2))");
  REQUIRE(shape_of("-2 ^ 2") == "(neg (^ 2 2))");
  REQUIRE(shape_of("(-2) ^ 2") == "(^ (neg 2) 2)");
  REQUIRE(shape_of("2 ^ -3") == "(^ 2 (neg 3))");
}

TEST_CASE("parser flags a missing operand and points at the bad token") {
  auto tree = parse_str("2 + * 3");
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::UnexpectedToken);
  REQUIRE(tree.error().span.offset == 4u); // the '*'
  REQUIRE(tree.error().span.length == 1u);
}

TEST_CASE("parser flags input that ends mid-expression") {
  auto tree = parse_str("1 +");
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::UnexpectedToken);
  REQUIRE(tree.error().span.offset == 3u); // the End, past the last char
  REQUIRE(tree.error().span.length == 0u); // End has no width
}

TEST_CASE("parser flags trailing junk after a complete expression") {
  auto tree = parse_str("3 4");
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::UnexpectedToken);
  REQUIRE(tree.error().span.offset == 2u); // the second number
}

TEST_CASE("parser flags unbalanced parentheses") {
  SECTION("missing close") {
    auto tree = parse_str("(1 + 2");
    REQUIRE_FALSE(tree.has_value());
    REQUIRE(tree.error().kind == ErrorKind::UnbalancedParen);
  }
  SECTION("extra close") {
    auto tree = parse_str("1 + 2)");
    REQUIRE_FALSE(tree.has_value());
    REQUIRE(tree.error().kind == ErrorKind::UnbalancedParen);
    REQUIRE(tree.error().span.offset == 5u); // the stray ')'
  }
  SECTION("just a close paren") {
    auto tree = parse_str(")");
    REQUIRE_FALSE(tree.has_value());
    REQUIRE(tree.error().kind == ErrorKind::UnbalancedParen);
    REQUIRE(tree.error().span.offset == 0u);
  }
}

TEST_CASE("balanced but empty parens are a missing operand, not unbalanced") {
  auto tree = parse_str("()");
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::UnexpectedToken);
  REQUIRE(tree.error().span.offset == 1u); // the ')'
}

TEST_CASE("parser reports empty input") {
  auto tree = parse_str("   ");
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::EmptyInput);
  // locationless, so a caret renderer wont point under blank space.
  REQUIRE(tree.error().span.offset == 0u);
  REQUIRE(tree.error().span.length == 0u);
}

TEST_CASE("a reasonable nesting depth still parses") {
  // 100 deep is well under the guard; the parens add no nodes, so it collapses
  // back to the single number inside.
  const int depth = 100;
  std::string input(static_cast<std::size_t>(depth), '(');
  input += "1";
  input.append(static_cast<std::size_t>(depth), ')');
  REQUIRE(shape_of(input) == "1");
}

TEST_CASE("absurd paren nesting comes back as an error, not a crash") {
  // 200 deep (~402 tokens, way under the token cap) blows past the depth guard
  // via the paren path. the message check makes sure it's really the depth
  // guard firing, not the length pre-check.
  const int depth = 200;
  std::string input(static_cast<std::size_t>(depth), '(');
  input += "1";
  input.append(static_cast<std::size_t>(depth), ')');
  auto tree = parse_str(input);
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::TooComplex);
  REQUIRE(tree.error().message == "expression nests too deep");
}

TEST_CASE("a deep chain of unary minus comes back as an error too") {
  // the unary level has its own depth guard. "-----...1" recurses one level per
  // '-', so this hits the guard down the parse_unary path (which the paren test
  // never touches), at ~300 tokens, still well under the token cap.
  const std::string input = std::string(300, '-') + "1";
  auto tree = parse_str(input);
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::TooComplex);
  REQUIRE(tree.error().message == "expression nests too deep");
}

TEST_CASE("an oversized flat expression comes back as an error") {
  // "1+1+1+..." with enough terms to pass the token cap. it isnt deeply
  // recursive, so this exercises the length pre-check, not the depth guard.
  // the message check pins that down.
  std::string input = "1";
  for (int i = 0; i < 2100; ++i) {
    input += "+1";
  }
  auto tree = parse_str(input);
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::TooComplex);
  REQUIRE(tree.error().message == "expression is too long");
}
