#include "calc/evaluator.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include "calc/assert.hpp"
#include "calc/ast.hpp"
#include "calc/environment.hpp"
#include "calc/lexer.hpp"
#include "calc/output_formatter.hpp"
#include "calc/parser.hpp"
#include "calc/token.hpp"
#include "calc/tracer.hpp"

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
// variables), so it rides along by const ref the whole way down. the tracer is
// null on the normal path and only non-null under --trace / :trace.
Result<Number> eval(const Expr &expr, const Environment &env, Tracer *tracer);

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

const char *op_symbol(BinaryOpKind op) {
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

// the actual arithmetic for a binary op, pulled out so the evaluator can wrap a
// trace step around it without the switch in the way. divide/modulo by zero and
// overflow come back as errors, never a silent inf/nan.
Result<Number> apply_binary(BinaryOpKind op, Number a, Number c) {
  switch (op) {
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

// one operator() per node kind; std::visit dispatches on the live variant. the
// env is read-only here: name resolution reads it, but binding/memory writes
// happen up at the statement level in evaluate(). when tracer is non-null, each
// node that computes a value emits one post-order line showing the work.
struct EvalVisitor {
  const Environment &env;
  Tracer *tracer;

  void step(const std::string &line) const {
    if (tracer != nullptr) {
      tracer->write("  " + line + "\n");
    }
  }

  Result<Number> operator()(const NumberLiteral &n) const {
    step(format_number(n.value));
    return n.value;
  }

  Result<Number> operator()(const Variable &v) const {
    Number value = 0.0;
    bool found = false;
    if (v.name == "ans") {
      value = env.answer();
      found = true;
    } else {
      const std::string lower = to_lower(v.name);
      if (lower == "m" || lower == "mr") { // memory recall
        value = env.memory();
        found = true;
      } else if (lookup_constant(v.name, value)) {
        found = true;
      } else if (env.lookup_variable(v.name, value)) {
        found = true;
      }
    }
    if (!found) {
      return CalcError{ErrorKind::UnknownName, "unknown name '" + v.name + "'"};
    }
    step(v.name + " = " + format_number(value));
    return value;
  }

  Result<Number> operator()(const UnaryOp &u) const {
    Result<Number> operand = eval(*u.operand, env, tracer);
    if (!operand) {
      return operand.error();
    }
    const Number value = -operand.value(); // negate cant overflow
    step("-(" + format_number(operand.value()) + ") = " + format_number(value));
    return value;
  }

  Result<Number> operator()(const BinaryOp &b) const {
    Result<Number> lhs = eval(*b.lhs, env, tracer);
    if (!lhs) {
      return lhs.error();
    }
    Result<Number> rhs = eval(*b.rhs, env, tracer);
    if (!rhs) {
      return rhs.error();
    }
    const Number a = lhs.value();
    const Number c = rhs.value();
    Result<Number> out = apply_binary(b.op, a, c);
    if (out) {
      step(format_number(a) + " " + op_symbol(b.op) + " " + format_number(c) +
           " = " + format_number(out.value()));
    }
    return out;
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
    const Result<Number> arg = eval(*call.args[0], env, tracer);
    if (!arg) {
      return arg.error();
    }
    Result<Number> out = fn(arg.value());
    if (out) {
      step(call.name + "(" + format_number(arg.value()) +
           ") = " + format_number(out.value()));
    }
    return out;
  }
};

Result<Number> eval(const Expr &expr, const Environment &env, Tracer *tracer) {
  return std::visit(EvalVisitor{env, tracer}, expr.node);
}

// render the token list to the tracer (the first of the three trace sections).
const char *token_label(TokenType type) {
  switch (type) {
  case TokenType::Num:
    return "number";
  case TokenType::Ident:
    return "name";
  case TokenType::Plus:
    return "+";
  case TokenType::Minus:
    return "-";
  case TokenType::Star:
    return "*";
  case TokenType::Slash:
    return "/";
  case TokenType::Percent:
    return "%";
  case TokenType::Caret:
    return "^";
  case TokenType::Equals:
    return "=";
  case TokenType::LParen:
    return "(";
  case TokenType::RParen:
    return ")";
  case TokenType::Comma:
    return ",";
  case TokenType::End:
    return "end";
  }
  return "?";
}

void trace_tokens(Tracer &tracer, const std::vector<Token> &tokens) {
  tracer.write("tokens:\n");
  for (const Token &tok : tokens) {
    std::string line = token_label(tok.type);
    if (tok.type == TokenType::Num) {
      line += " " + format_number(tok.value);
    } else if (tok.type == TokenType::Ident) {
      line += " " + tok.text;
    }
    tracer.write("  " + line + "\n");
  }
}

// render the parse tree, one node per line, children indented under their
// parent (the second trace section).
struct TreeRenderer {
  std::string &out;
  int depth;

  void line(const std::string &label) const {
    out.append(static_cast<std::size_t>(depth) * 2 + 2, ' ');
    out += label;
    out += '\n';
  }
  void recurse(const Expr &child) const {
    std::visit(TreeRenderer{out, depth + 1}, child.node);
  }

  void operator()(const NumberLiteral &n) const {
    line(format_number(n.value));
  }
  void operator()(const Variable &v) const { line(v.name); }
  void operator()(const UnaryOp &u) const {
    line("- (negate)");
    recurse(*u.operand);
  }
  void operator()(const BinaryOp &b) const {
    line(op_symbol(b.op));
    recurse(*b.lhs);
    recurse(*b.rhs);
  }
  void operator()(const FunctionCall &c) const {
    line(c.name + "()");
    for (const ExprPtr &arg : c.args) {
      recurse(*arg);
    }
  }
};

void trace_tree(Tracer &tracer, const Expr &tree) {
  tracer.write("tree:\n");
  std::string out;
  std::visit(TreeRenderer{out, 0}, tree.node);
  tracer.write(out);
}

// the third trace section: header, the post-order step lines (emitted from
// inside eval), then a final value + timing line. without a tracer its just a
// plain eval, no timing overhead.
Result<Number> run_eval(const Expr &tree, const Environment &env,
                        Tracer *tracer) {
  if (tracer == nullptr) {
    return eval(tree, env, nullptr);
  }
  tracer->write("eval:\n");
  const auto start = std::chrono::steady_clock::now();
  Result<Number> result = eval(tree, env, tracer);
  const auto end = std::chrono::steady_clock::now();
  const double us =
      std::chrono::duration<double, std::micro>(end - start).count();
  if (result) {
    tracer->write(fmt::format("  = {} in {:.2f} us\n",
                              format_number(result.value()), us));
  } else {
    tracer->write(fmt::format("  stopped at an error in {:.2f} us\n", us));
  }
  return result;
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
Result<Number> eval_let(const std::vector<Token> &tokens, Environment &env,
                        Tracer *tracer) {
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
  if (tracer != nullptr) {
    trace_tree(*tracer, tree.value());
  }
  Result<Number> value = run_eval(tree.value(), env, tracer);
  if (!value) {
    return value;
  }
  env.set_variable(name, value.value());
  env.set_answer(value.value());
  return value.value();
}

} // namespace

Result<Number> evaluate(std::string_view input, Environment &env,
                        Tracer *tracer) {
  Result<std::vector<Token>> lexed = tokenize(input);
  if (!lexed) {
    return lexed.error();
  }
  const std::vector<Token> &tokens = lexed.value();

  if (tracer != nullptr) {
    trace_tokens(*tracer, tokens);
  }

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
    return eval_let(tokens, env, tracer);
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
  if (tracer != nullptr) {
    trace_tree(*tracer, tree.value());
  }

  Result<Number> result = run_eval(tree.value(), env, tracer);
  if (result) {
    env.set_answer(result.value());
  }
  return result;
}

} // namespace calc
