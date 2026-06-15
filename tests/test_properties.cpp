#include <catch2/catch_test_macros.hpp>

#include <rapidcheck.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"
#include "calc/token.hpp"

using namespace calc;

namespace {

// evaluate a string with a fresh environment.
Result<Number> ev(const std::string &text) {
  Environment env;
  return evaluate(text, env);
}

std::string s(long v) { return std::to_string(v); }

// a small expression tree the generator builds - separate from the engine's AST
// so it can be rendered to text AND evaluated by an independent reference.
struct Node {
  bool leaf = true;
  long value = 0; // operand at a leaf (small int, renders exactly as a double)
  char op = '+';  // + - * / at an internal node
  std::shared_ptr<Node> lhs;
  std::shared_ptr<Node> rhs;
};

Node leaf_node(long v) {
  Node n;
  n.leaf = true;
  n.value = v;
  return n;
}

Node op_node(char op, Node a, Node b) {
  Node n;
  n.leaf = false;
  n.op = op;
  n.lhs = std::make_shared<Node>(std::move(a));
  n.rhs = std::make_shared<Node>(std::move(b));
  return n;
}

// render fully parenthesized, so the engine's precedence cant change the
// meaning vs. the tree the reference evaluates.
std::string render(const Node &n) {
  if (n.leaf) {
    return s(n.value);
  }
  return "(" + render(*n.lhs) + " " + n.op + " " + render(*n.rhs) + ")";
}

// reference evaluation with the same +,-,*,/ semantics; returns false on a
// divide-by-zero or a non-finite result so the property can discard that case.
bool ref_eval(const Node &n, double &out) {
  if (n.leaf) {
    out = static_cast<double>(n.value);
    return true;
  }
  double a = 0.0;
  double b = 0.0;
  if (!ref_eval(*n.lhs, a) || !ref_eval(*n.rhs, b)) {
    return false;
  }
  switch (n.op) {
  case '+':
    out = a + b;
    break;
  case '-':
    out = a - b;
    break;
  case '*':
    out = a * b;
    break;
  case '/':
    if (b == 0.0) {
      return false;
    }
    out = a / b;
    break;
  default:
    return false;
  }
  return std::isfinite(out);
}

// draw a random depth-bounded expression tree. called from inside a property,
// so it can use operator* to draw choices imperatively.
Node gen_tree(int depth) {
  if (depth <= 0 || *rc::gen::element(true, false, false)) {
    return leaf_node(*rc::gen::inRange<long>(1, 101)); // 1..100, never zero
  }
  const char op = *rc::gen::element('+', '-', '*', '/');
  return op_node(op, gen_tree(depth - 1), gen_tree(depth - 1));
}

} // namespace

TEST_CASE("evaluate is total: no input throws or yields a non-finite value") {
  const bool ok =
      rc::check("any string -> a value-or-error, never inf/nan, never a throw",
                [](const std::string &input) {
                  const Result<Number> result = ev(input);
                  if (result) {
                    RC_ASSERT(std::isfinite(result.value()));
                  }
                });
  REQUIRE(ok);
}

TEST_CASE("the parser accepts every well-formed expression") {
  const bool ok =
      rc::check("a rendered tree always tokenizes and parses", []() {
        const std::string text = render(gen_tree(5));
        const Result<std::vector<Token>> toks = tokenize(text);
        RC_ASSERT(toks.has_value());
        const Result<Expr> tree = parse(toks.value());
        RC_ASSERT(tree.has_value());
      });
  REQUIRE(ok);
}

TEST_CASE("evaluate matches an independent reference on random trees") {
  const bool ok = rc::check("rendered tree == reference value", []() {
    const Node tree = gen_tree(4);
    double expected = 0.0;
    RC_PRE(ref_eval(tree, expected)); // skip divide-by-zero / non-finite cases
    const Result<Number> result = ev(render(tree));
    RC_ASSERT(result.has_value());
    RC_ASSERT(result.value() == expected); // same ops, same order -> exact
  });
  REQUIRE(ok);
}

TEST_CASE("multiplication binds tighter than addition") {
  const bool ok = rc::check("a + b * c == a + (b * c)", []() {
    const long a = *rc::gen::inRange<long>(1, 101);
    const long b = *rc::gen::inRange<long>(1, 101);
    const long c = *rc::gen::inRange<long>(1, 101);
    const Result<Number> bare = ev(s(a) + " + " + s(b) + " * " + s(c));
    const Result<Number> paren = ev(s(a) + " + (" + s(b) + " * " + s(c) + ")");
    RC_ASSERT(bare.has_value() && paren.has_value());
    RC_ASSERT(bare.value() == paren.value());
  });
  REQUIRE(ok);
}

TEST_CASE("subtraction is left-associative") {
  const bool ok = rc::check("a - b - c == (a - b) - c", []() {
    const long a = *rc::gen::inRange<long>(1, 1000);
    const long b = *rc::gen::inRange<long>(1, 1000);
    const long c = *rc::gen::inRange<long>(1, 1000);
    const Result<Number> bare = ev(s(a) + " - " + s(b) + " - " + s(c));
    const Result<Number> paren = ev("(" + s(a) + " - " + s(b) + ") - " + s(c));
    RC_ASSERT(bare.has_value() && paren.has_value());
    RC_ASSERT(bare.value() == paren.value());
  });
  REQUIRE(ok);
}

TEST_CASE("exponent is right-associative") {
  const bool ok = rc::check("a ^ b ^ c == a ^ (b ^ c)", []() {
    const long a = *rc::gen::inRange<long>(2, 5); // 2..4
    const long b = *rc::gen::inRange<long>(1, 4); // 1..3
    const long c = *rc::gen::inRange<long>(1, 4); // 1..3, keeps it finite
    const Result<Number> bare = ev(s(a) + " ^ " + s(b) + " ^ " + s(c));
    const Result<Number> paren = ev(s(a) + " ^ (" + s(b) + " ^ " + s(c) + ")");
    RC_ASSERT(bare.has_value() && paren.has_value());
    RC_ASSERT(bare.value() == paren.value());
  });
  REQUIRE(ok);
}

TEST_CASE("addition and multiplication commute") {
  const bool ok = rc::check("a + b == b + a and a * b == b * a", []() {
    const long a = *rc::gen::inRange<long>(-1000, 1001);
    const long b = *rc::gen::inRange<long>(-1000, 1001);
    const Result<Number> add1 = ev(s(a) + " + " + s(b));
    const Result<Number> add2 = ev(s(b) + " + " + s(a));
    const Result<Number> mul1 = ev(s(a) + " * " + s(b));
    const Result<Number> mul2 = ev(s(b) + " * " + s(a));
    RC_ASSERT(add1.has_value() && add2.has_value());
    RC_ASSERT(add1.value() == add2.value());
    RC_ASSERT(mul1.has_value() && mul2.has_value());
    RC_ASSERT(mul1.value() == mul2.value());
  });
  REQUIRE(ok);
}
