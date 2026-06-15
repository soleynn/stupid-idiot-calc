#include "calc/evaluator.hpp"

#include <cmath>
#include <string>
#include <unordered_map>
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

// the built-in constants. resolved right here, not in the (still empty)
// Environment; user-defined names come later and will layer on top.
constexpr Number kPi = 3.14159265358979323846;
constexpr Number kE = 2.71828182845904523536;
constexpr Number kDegToRad = kPi / 180.0;

bool lookup_constant(const std::string &name, Number &out) {
  if (name == "pi") {
    out = kPi;
    return true;
  }
  if (name == "e") {
    out = kE;
    return true;
  }
  return false;
}

// the built-in functions, all single-argument. each does its own domain check
// and runs the result through checked(), so an overflow (like exp of a big
// number) comes back as an error, not a silent inf. trig works in degrees.
Result<Number> fn_sqrt(Number x) {
  if (x < 0.0) {
    return CalcError{ErrorKind::DomainError, "sqrt needs a value >= 0"};
  }
  return checked(std::sqrt(x));
}

Result<Number> fn_ln(Number x) {
  if (x <= 0.0) {
    return CalcError{ErrorKind::DomainError, "ln needs a value > 0"};
  }
  return checked(std::log(x));
}

Result<Number> fn_log(Number x) {
  if (x <= 0.0) {
    return CalcError{ErrorKind::DomainError, "log needs a value > 0"};
  }
  return checked(std::log10(x));
}

Result<Number> fn_sin(Number x) { return checked(std::sin(x * kDegToRad)); }
Result<Number> fn_cos(Number x) { return checked(std::cos(x * kDegToRad)); }
Result<Number> fn_tan(Number x) { return checked(std::tan(x * kDegToRad)); }
Result<Number> fn_abs(Number x) { return checked(std::fabs(x)); }
Result<Number> fn_exp(Number x) { return checked(std::exp(x)); }
Result<Number> fn_floor(Number x) { return checked(std::floor(x)); }
Result<Number> fn_ceil(Number x) { return checked(std::ceil(x)); }

using UnaryFn = Result<Number> (*)(Number);

UnaryFn lookup_function(const std::string &name) {
  static const std::unordered_map<std::string, UnaryFn> table = {
      {"sqrt", fn_sqrt},   {"sin", fn_sin},  {"cos", fn_cos}, {"tan", fn_tan},
      {"abs", fn_abs},     {"ln", fn_ln},    {"log", fn_log}, {"exp", fn_exp},
      {"floor", fn_floor}, {"ceil", fn_ceil}};
  const auto it = table.find(name);
  return it == table.end() ? nullptr : it->second;
}

// one operator() per node kind; std::visit dispatches on the live variant.
struct EvalVisitor {
  Result<Number> operator()(const NumberLiteral &n) const { return n.value; }

  Result<Number> operator()(const Variable &v) const {
    Number value = 0.0;
    if (lookup_constant(v.name, value)) {
      return value;
    }
    return CalcError{ErrorKind::UnknownName, "unknown name '" + v.name + "'"};
  }

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

  Result<Number> operator()(const FunctionCall &call) const {
    const UnaryFn fn = lookup_function(call.name);
    if (fn == nullptr) {
      return CalcError{ErrorKind::UnknownName,
                       "unknown function '" + call.name + "'"};
    }
    if (call.args.size() != 1) {
      return CalcError{ErrorKind::WrongArgCount,
                       call.name + " takes 1 argument but got " +
                           std::to_string(call.args.size())};
    }
    const Result<Number> arg = eval(*call.args[0]);
    if (!arg) {
      return arg.error();
    }
    return fn(arg.value());
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
