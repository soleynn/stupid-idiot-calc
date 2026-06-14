#pragma once

#include <memory>
#include <variant>

#include "calc/number.hpp"

namespace calc {

// the parse tree. three node kinds held in a variant, children owned by
// unique_ptr so a tree frees itself with no raw new/delete anywhere. the
// evaluator (later) walks this with std::visit; for now nothing evaluates it.

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

// one tree node: exactly one of the kinds above. the converting constructors
// let the parser write `make_unique<Expr>(BinaryOp{...})` and read cleanly.
struct Expr {
  std::variant<NumberLiteral, UnaryOp, BinaryOp> node;

  Expr(NumberLiteral n) : node(std::move(n)) {}
  Expr(UnaryOp u) : node(std::move(u)) {}
  Expr(BinaryOp b) : node(std::move(b)) {}
};

} // namespace calc
