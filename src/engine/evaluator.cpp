#include "calc/evaluator.hpp"

#include <cmath>
#include <variant>
#include <vector>

#include "calc/assert.hpp"
#include "calc/ast.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"
#include "calc/token.hpp"

namespace calc {

namespace {

// catch a result that ran past what a double can hold. two finite numbers can
// still multiply/add up to +/-inf, and we hand that back as an error instead of
// letting a silent inf leak out.
Result<Number> checked(Number value) {
  if (!std::isfinite(value)) {
    return CalcError{ErrorKind::Overflow, "result is too large"};
  }
  return value;
}

Result<Number> eval(const Expr &expr); // forward decl, the walk recurses

// one operator() per node kind; std::visit dispatches on the live variant.
struct EvalVisitor {
  Result<Number> operator()(const NumberLiteral &n) const { return n.value; }

  Result<Number> operator()(const UnaryOp &u) const {
    Result<Number> operand = eval(*u.operand);
    if (!operand) {
      return operand.error();
    }
    return -operand.value(); // negate is the only unary op, cant overflow
  }

  Result<Number> operator()(const BinaryOp &b) const {
    Result<Number> lhs = eval(*b.lhs);
    if (!lhs) {
      return lhs.error();
    }
    Result<Number> rhs = eval(*b.rhs);
    if (!rhs) {
      return rhs.error();
    }
    const Number a = lhs.value();
    const Number c = rhs.value();
    switch (b.op) {
    case BinaryOpKind::Add:
      return checked(a + c);
    case BinaryOpKind::Subtract:
      return checked(a - c);
    case BinaryOpKind::Multiply:
      return checked(a * c);
    case BinaryOpKind::Divide:
      if (c == 0.0) {
        return CalcError{ErrorKind::DivideByZero, "cant divide by zero"};
      }
      return checked(a / c);
    case BinaryOpKind::Modulo:
      if (c == 0.0) {
        return CalcError{ErrorKind::DivideByZero, "cant modulo by zero"};
      }
      return checked(std::fmod(a, c));
    case BinaryOpKind::Power:
      return checked(std::pow(a, c));
    }
    CALC_ASSERT(false, "every binary op kind is handled above");
    return CalcError{ErrorKind::NotImplemented, "unknown operator"};
  }
};

Result<Number> eval(const Expr &expr) {
  return std::visit(EvalVisitor{}, expr.node);
}

} // namespace

Result<Number> evaluate(std::string_view input, Environment &env) {
  (void)env; // still unused, reserved seam for variables / ans later.

  Result<std::vector<Token>> lexed = tokenize(input);
  if (!lexed) {
    return lexed.error();
  }

  Result<Expr> tree = parse(lexed.value());
  if (!tree) {
    return tree.error();
  }

  return eval(tree.value());
}

} // namespace calc
