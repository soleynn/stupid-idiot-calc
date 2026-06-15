#include "calc/evaluator.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "calc/assert.hpp"
#include "calc/ast.hpp"
#include "calc/environment.hpp"
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

// the walk recurses and needs the env to resolve names (ans, memory, user
// variables), so it rides along by const ref the whole way down.
Result<Number> eval(const Expr &expr, const Environment &env);

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

std::string to_lower(std::string s) {
  for (char &c : s) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return s;
}

// a name the user cant rebind with `let`: the session names, the constants, and
// every built-in function. checked lower-cased so `let PI = 3` is caught too.
bool is_reserved(const std::string &name) {
  const std::string lower = to_lower(name);
  if (lower == "ans" || lower == "let" || lower == "m" || lower == "mr" ||
      lower == "mc" || lower == "pi" || lower == "e") {
    return true;
  }
  return lookup_function(lower) != nullptr;
}

// one operator() per node kind; std::visit dispatches on the live variant. the
// env is read-only here: name resolution reads it, but binding/memory writes
// happen up at the statement level in evaluate().
struct EvalVisitor {
  const Environment &env;

  Result<Number> operator()(const NumberLiteral &n) const { return n.value; }

  Result<Number> operator()(const Variable &v) const {
    if (v.name == "ans") {
      return env.answer();
    }
    const std::string lower = to_lower(v.name);
    if (lower == "m" || lower == "mr") { // memory recall
      return env.memory();
    }
    Number value = 0.0;
    if (lookup_constant(v.name, value)) {
      return value;
    }
    if (env.lookup_variable(v.name, value)) {
      return value;
    }
    return CalcError{ErrorKind::UnknownName, "unknown name '" + v.name + "'"};
  }

  Result<Number> operator()(const UnaryOp &u) const {
    Result<Number> operand = eval(*u.operand, env);
    if (!operand) {
      return operand.error();
    }
    return -operand.value(); // negate is the only unary op, cant overflow
  }

  Result<Number> operator()(const BinaryOp &b) const {
    Result<Number> lhs = eval(*b.lhs, env);
    if (!lhs) {
      return lhs.error();
    }
    Result<Number> rhs = eval(*b.rhs, env);
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
    const Result<Number> arg = eval(*call.args[0], env);
    if (!arg) {
      return arg.error();
    }
    return fn(arg.value());
  }
};

Result<Number> eval(const Expr &expr, const Environment &env) {
  return std::visit(EvalVisitor{env}, expr.node);
}

// the bare memory "buttons", recognised before the expression parser sees them.
// M+ / M- arrive as [name, +/-, End] (a dangling operator that would otherwise
// be a parse error), MC as [name, End]; all case-insensitive. MR isnt here: its
// just a name the evaluator resolves, so it also works inside expressions.
enum class MemoryCommand { Add, Subtract, Clear };

std::optional<MemoryCommand>
match_memory_command(const std::vector<Token> &tokens) {
  const bool m_op = tokens.size() == 3 && tokens[0].type == TokenType::Ident &&
                    to_lower(tokens[0].text) == "m" &&
                    tokens[2].type == TokenType::End;
  if (m_op && tokens[1].type == TokenType::Plus) {
    return MemoryCommand::Add;
  }
  if (m_op && tokens[1].type == TokenType::Minus) {
    return MemoryCommand::Subtract;
  }
  if (tokens.size() == 2 && tokens[0].type == TokenType::Ident &&
      to_lower(tokens[0].text) == "mc" && tokens[1].type == TokenType::End) {
    return MemoryCommand::Clear;
  }
  return std::nullopt;
}

// `x = 5` without `let` would otherwise die with a confusing "unexpected '='";
// spot it so evaluate() can hand back a pointer to the right syntax instead.
bool looks_like_assignment(const std::vector<Token> &tokens) {
  return tokens.size() >= 2 && tokens[0].type == TokenType::Ident &&
         tokens[1].type == TokenType::Equals;
}

// `let name = expr`: evaluate the right-hand side as an ordinary expression,
// bind the name, hand back the value. the rhs reuses parse()/eval() wholesale
// by slicing the tokens after '='; the original offsets ride along, so a
// mistake in the rhs still points a caret at the right column of the input.
Result<Number> eval_let(const std::vector<Token> &tokens, Environment &env) {
  std::size_t i = 1; // tokens[0] is `let`
  if (tokens[i].type != TokenType::Ident) {
    return CalcError{ErrorKind::UnexpectedToken, "let needs a name",
                     SourceSpan{tokens[i].offset, 1}};
  }
  const std::string name = tokens[i].text;
  if (is_reserved(name)) {
    return CalcError{ErrorKind::ReservedName,
                     "'" + name + "' is a built-in name, pick another",
                     SourceSpan{tokens[i].offset, name.size()}};
  }
  ++i;
  if (tokens[i].type != TokenType::Equals) {
    return CalcError{ErrorKind::UnexpectedToken, "expected '=' after the name",
                     SourceSpan{tokens[i].offset, 1}};
  }
  ++i;
  if (tokens[i].type == TokenType::End) {
    return CalcError{ErrorKind::EmptyInput, "let needs a value after '='",
                     SourceSpan{tokens[i].offset, 0}};
  }

  const std::vector<Token> rhs(tokens.begin() + static_cast<std::ptrdiff_t>(i),
                               tokens.end());
  Result<Expr> tree = parse(rhs);
  if (!tree) {
    return tree.error();
  }
  Result<Number> value = eval(tree.value(), env);
  if (!value) {
    return value;
  }
  env.set_variable(name, value.value());
  env.set_answer(value.value());
  return value.value();
}

} // namespace

Result<Number> evaluate(std::string_view input, Environment &env) {
  Result<std::vector<Token>> lexed = tokenize(input);
  if (!lexed) {
    return lexed.error();
  }
  const std::vector<Token> &tokens = lexed.value();

  // memory buttons and let-bindings are statements, handled here before the
  // expression parser. everything else is a plain expression.
  if (const std::optional<MemoryCommand> cmd = match_memory_command(tokens)) {
    switch (*cmd) {
    case MemoryCommand::Add:
      env.memory_add(env.answer());
      break;
    case MemoryCommand::Subtract:
      env.memory_subtract(env.answer());
      break;
    case MemoryCommand::Clear:
      env.memory_clear();
      break;
    }
    return env.memory(); // show the register; a memory op leaves `ans` alone
  }

  if (tokens[0].type == TokenType::Ident && tokens[0].text == "let") {
    return eval_let(tokens, env);
  }

  if (looks_like_assignment(tokens)) {
    return CalcError{ErrorKind::UnexpectedToken,
                     "to name a value, start with let: let " + tokens[0].text +
                         " = ...",
                     SourceSpan{tokens[1].offset, 1}};
  }

  Result<Expr> tree = parse(tokens);
  if (!tree) {
    return tree.error();
  }

  Result<Number> result = eval(tree.value(), env);
  if (result) {
    env.set_answer(result.value());
  }
  return result;
}

} // namespace calc
