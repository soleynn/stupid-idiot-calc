#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "calc/number.hpp"

namespace calc {

// the parse tree. node kinds held in a variant, children owned by unique_ptr so
// a tree frees itself with no raw new/delete anywhere. the evaluator walks this
// with std::visit.

enum class UnaryOpKind {
  Negate, // -x
};

enum class BinaryOpKind {
  Add,      // a + b
  Subtract, // a - b
  Multiply, // a * b
  Divide,   // a / b
  Modulo,   // a % b
  Power,    // a ^ b
};

struct Expr; // a node, defined just below
using ExprPtr = std::unique_ptr<Expr>;

// a literal number, a leaf of the tree.
struct NumberLiteral {
  Number value = 0.0;
};

// a bare name: a constant like `pi`, or (later) a user variable. the evaluator
// is what knows which names mean something.
struct Variable {
  std::string name;
};

// a one-operand op (just unary minus for now).
struct UnaryOp {
  UnaryOpKind op = UnaryOpKind::Negate;
  ExprPtr operand;
};

// a two-operand op: + - * /.
struct BinaryOp {
  BinaryOpKind op = BinaryOpKind::Add;
  ExprPtr lhs;
  ExprPtr rhs;
};

// a call like `sqrt(2)` or `f(a, b)`. the parser records the args it sees; the
// evaluator checks the name exists and that the count is right.
struct FunctionCall {
  std::string name;
  std::vector<ExprPtr> args;
};

// one tree node: exactly one of the kinds above. the converting constructors
// let the parser write `make_unique<Expr>(BinaryOp{...})` and read cleanly.
struct Expr {
  std::variant<NumberLiteral, Variable, UnaryOp, BinaryOp, FunctionCall> node;

  Expr(NumberLiteral n) : node(n) {} // trivially copyable, no move to make
  Expr(Variable v) : node(std::move(v)) {}
  Expr(UnaryOp u) : node(std::move(u)) {}
  Expr(BinaryOp b) : node(std::move(b)) {}
  Expr(FunctionCall f) : node(std::move(f)) {}

  // a deep tree is freed iteratively, not by the default recursive unique_ptr
  // chain (see ast.cpp): a long flat 1+1+1+... builds a tree thousands of nodes
  // deep, and recursing that on teardown overflows a small thread stack. the
  // user-declared destructor means move has to be spelled back out; copy stays
  // gone, since the unique_ptr children were never copyable anyway.
  ~Expr();
  Expr(Expr &&) noexcept = default;
  Expr &operator=(Expr &&) noexcept = default;
};

} // namespace calc
