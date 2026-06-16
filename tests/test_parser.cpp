#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

#include "calc/ast.hpp"
#include "calc/error.hpp"
#include "calc/lexer.hpp"
#include "calc/limits.hpp"
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
  case BinaryOpKind::Modulo:
    return "%";
  case BinaryOpKind::Power:
    return "^";
  }
  return "?";
}

std::string sexpr(const Expr &e) {
  if (const auto *n = std::get_if<NumberLiteral>(&e.node)) {
    return format_number(n->value);
  }
  if (const auto *v = std::get_if<Variable>(&e.node)) {
    return v->name;
  }
  if (const auto *u = std::get_if<UnaryOp>(&e.node)) {
    return "(neg " + sexpr(*u->operand) + ")";
  }
  if (const auto *c = std::get_if<FunctionCall>(&e.node)) {
    std::string s = "(" + c->name;
    for (const auto &arg : c->args) {
      s += " " + sexpr(*arg);
    }
    return s + ")";
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

TEST_CASE("modulo shares the * and / level, left-associative") {
  REQUIRE(shape_of("10 % 3") == "(% 10 3)");
  // interleaving with / proves same precedence and left-to-right grouping.
  REQUIRE(shape_of("8 / 3 % 2") == "(% (/ 8 3) 2)");
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

TEST_CASE("parser reads a bare name as a variable") {
  REQUIRE(shape_of("pi") == "pi");
}

TEST_CASE("parser builds function calls") {
  REQUIRE(shape_of("sqrt(2)") == "(sqrt 2)");
  // the call is a primary, and its argument is a full expression.
  REQUIRE(shape_of("2 * sqrt(1 + 3)") == "(* 2 (sqrt (+ 1 3)))");
}

TEST_CASE("parser handles argument lists") {
  // the parser doesnt know or care which names are real; it just shapes the
  // tree. unknown names and bad arg counts are the evaluator's problem.
  REQUIRE(shape_of("foo(3, 4)") == "(foo 3 4)"); // comma-separated args
  REQUIRE(shape_of("bar()") == "(bar)");         // empty arg list still parses
}

TEST_CASE("parser flags a malformed argument list") {
  SECTION("an unclosed call") {
    auto tree = parse_str("sqrt(1");
    REQUIRE_FALSE(tree.has_value());
    REQUIRE(tree.error().kind == ErrorKind::UnbalancedParen);
  }
  SECTION("a trailing comma wants another argument") {
    auto tree = parse_str("sqrt(1,)");
    REQUIRE_FALSE(tree.has_value());
    REQUIRE(tree.error().kind == ErrorKind::UnexpectedToken);
  }
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

TEST_CASE("paren nesting caps at exactly 127, two depth guards per level") {
  // a '(' descends through both the expression guard and the unary guard, so it
  // trips kMaxDepth at half the levels: 127 deep parses (and collapses to the
  // number inside), 128 trips it. the message pins that it's the depth guard
  // firing, not the length cap.
  REQUIRE(shape_of(std::string(127, '(') + "1" + std::string(127, ')')) == "1");

  auto over = parse_str(std::string(128, '(') + "1" + std::string(128, ')'));
  REQUIRE_FALSE(over.has_value());
  REQUIRE(over.error().kind == ErrorKind::TooComplex);
  REQUIRE(over.error().message == "expression nests too deep");
}

TEST_CASE("unary nesting caps at exactly 254, one depth guard per level") {
  // a unary '-' only trips the unary guard, so a chain reaches twice the paren
  // depth before the cap: 254 deep parses, 255 trips it.
  REQUIRE(parse_str(std::string(254, '-') + "1").has_value());

  auto over = parse_str(std::string(255, '-') + "1");
  REQUIRE_FALSE(over.has_value());
  REQUIRE(over.error().kind == ErrorKind::TooComplex);
  REQUIRE(over.error().message == "expression nests too deep");
}

TEST_CASE("parse refuses a token list longer than the cap") {
  // the lexer is the primary length gate now (see test_lexer.cpp), so an
  // oversized string never reaches parse() through tokenize. parse() keeps its
  // own size backstop for a hand-built token list though - feed it one past the
  // cap directly; the size check fires before the contents matter.
  const std::vector<Token> tokens(kMaxTokens + 1);
  const Result<Expr> tree = parse(tokens);
  REQUIRE_FALSE(tree.has_value());
  REQUIRE(tree.error().kind == ErrorKind::TooComplex);
  REQUIRE(tree.error().message == "expression is too long");
}

TEST_CASE("a multi-character error token is spanned in full") {
  // the offending token should be underlined whole, not just at its first char:
  // a stray multi-digit number and a trailing identifier each span their
  // length.
  auto num = parse_str("1 23456789");
  REQUIRE_FALSE(num.has_value());
  REQUIRE(num.error().span.offset == 2u);
  REQUIRE(num.error().span.length == 8u);

  auto id = parse_str("foo bar");
  REQUIRE_FALSE(id.has_value());
  REQUIRE(id.error().span.offset == 4u);
  REQUIRE(id.error().span.length == 3u);
}
