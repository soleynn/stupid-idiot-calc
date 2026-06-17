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

// catch a result a double cant represent. two finite numbers can multiply/add
// up to +/-inf (an overflow), and a few ops produce nan (an undefined result,
// like a negative base to a fractional power). either way we hand back an error
// instead of letting a silent inf/nan leak out.
Result<Number> checked(Number value) {
  if (std::isnan(value)) {
    return CalcError{ErrorKind::DomainError, "undefined result"};
  }
  if (std::isinf(value)) {
    return CalcError{ErrorKind::Overflow, "result is too large"};
  }
  return value;
}

// stamp a source span onto an error that doesnt already carry one, so a runtime
// failure (divide-by-zero, a domain error, overflow) points a caret at the
// operator or function that produced it, the way a parse error does.
CalcError at(CalcError error, const SourceSpan &span) {
  if (error.span.offset == 0 && error.span.length == 0) {
    error.span = span;
  }
  return error;
}

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

// if a lower-cased name resolves to a value rather than a function, name which
// kind it is, so `pi(2)` / `ans(1)` can say "thats a constant/value, not a
// function" instead of a misleading "unknown function". returns nullptr for a
// name thats genuinely not a known function or value.
const char *value_name_kind(const std::string &lower) {
  Number ignore = 0.0;
  if (lookup_constant(lower, ignore)) {
    return "constant";
  }
  if (lower == "ans" || lower == "m" || lower == "mr") {
    return "value"; // the session result and the memory register
  }
  return nullptr;
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

// degrees -> radians, but reduce the angle mod 360 in degrees first (exactly,
// with fmod) so a large angle doesnt lose its low bits in the multiply before
// std::sin/cos/tan ever range-reduces. small/whole angles are untouched (fmod
// is a no-op below 360), and e.g. 360 folds to exactly 0, so sin(360) is 0
// instead of a tiny residue, and sin(1e15) stays accurate rather than drifting.
Number deg_to_rad(Number degrees) {
  return std::fmod(degrees, 360.0) * kDegToRad;
}

Result<Number> fn_sin(Number x) { return checked(std::sin(deg_to_rad(x))); }
Result<Number> fn_cos(Number x) { return checked(std::cos(deg_to_rad(x))); }
Result<Number> fn_tan(Number x) {
  // tan blows up at 90, 270, ... where the true value is infinite; a double
  // cant land exactly on the pole, so std::tan would hand back a huge finite
  // number instead of erroring. reject those angles (exact for whole degrees).
  if (std::fmod(std::fabs(x), 180.0) == 90.0) {
    return CalcError{ErrorKind::DomainError, "tan is undefined at this angle"};
  }
  return checked(std::tan(deg_to_rad(x)));
}
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
    if (a == 0.0 && c < 0.0) {
      // 0 to a negative power is 1/0^|c|: a pole, not an overflow. report it
      // the same way 1/0 does, instead of std::pow's silent +inf -> "too
      // large".
      return CalcError{ErrorKind::DivideByZero, "cant divide by zero"};
    }
    if (a < 0.0 && std::floor(c) != c) {
      return CalcError{ErrorKind::DomainError,
                       "cant raise a negative number to a fractional power"};
    }
    return checked(std::pow(a, c));
  }
  CALC_ASSERT(false, "every binary op kind is handled above");
  return CalcError{ErrorKind::NotImplemented, "unknown operator"};
}

// the tree-walk evaluator. it runs off an explicit work-stack instead of
// recursing: a long flat expression (1+1+1+...) builds a tree thousands of
// nodes deep, and a recursive walk would overflow the small thread stacks
// android/qt worker threads run on. the output it produces - including the
// post-order --trace step lines and stop-at-the-first-error behavior - matches
// a plain post-order recursion exactly. the env is read-only here: name
// resolution reads it, but binding/memory writes happen up at the statement
// level in evaluate().
struct Evaluator {
  const Environment &env;
  Tracer *tracer;

  void step(const std::string &line) const {
    if (tracer != nullptr) {
      tracer->write("  " + line + "\n");
    }
  }

  // a leaf: a literal value, which never fails.
  Number literal(const NumberLiteral &n) const {
    step(format_number(n.value));
    return n.value;
  }

  // a leaf: resolve a name. everything resolves case-insensitively - the
  // built-in names (ans, the memory register, pi/e) to match is_reserved, and
  // user `let` names because the calc is case-insensitive throughout, so `Rate`
  // and `rate` are the one variable (they're stored lower-cased; see eval_let).
  Result<Number> variable(const Variable &v) const {
    const std::string lower = to_lower(v.name);
    Number value = 0.0;
    bool found = false;
    if (lower == "ans") {
      value = env.answer();
      found = true;
    } else if (lower == "m" || lower == "mr") { // memory recall
      const Result<Number> mem = checked(env.memory());
      if (!mem) {
        return mem.error(); // the register overflowed at some point
      }
      value = mem.value();
      found = true;
    } else if (lookup_constant(lower, value) ||
               env.lookup_variable(lower, value)) {
      found = true; // a built-in constant, else a let-bound variable
    }
    if (!found) {
      return CalcError{ErrorKind::UnknownName, "unknown name '" + v.name + "'"};
    }
    step(v.name + " = " + format_number(value));
    return value;
  }

  // walk the tree to a value. each frame tracks how many of its children have
  // been launched; a node is only reduced once its children have all produced
  // values, which keeps the step lines in post-order. the first error returns
  // straight out, leaving the rest of the tree unwalked - same short-circuit a
  // recursive walk gives.
  Result<Number> run(const Expr &root) const {
    struct Frame {
      const Expr *expr;
      int stage;  // children launched so far (0 = none yet)
      Number lhs; // a binary op parks its left value here across the right
    };

    std::vector<Frame> stack;
    stack.push_back({&root, 0, 0.0});
    Number value = 0.0; // the value the just-finished child handed back

    while (!stack.empty()) {
      Frame &f = stack.back();
      const Expr &e = *f.expr;

      if (const auto *n = std::get_if<NumberLiteral>(&e.node)) {
        value = literal(*n);
        stack.pop_back();
      } else if (const auto *v = std::get_if<Variable>(&e.node)) {
        Result<Number> resolved = variable(*v);
        if (!resolved) {
          return at(resolved.error(), v->span);
        }
        value = resolved.value();
        stack.pop_back();
      } else if (const auto *u = std::get_if<UnaryOp>(&e.node)) {
        if (f.stage == 0) {
          f.stage = 1;
          stack.push_back({u->operand.get(), 0, 0.0});
        } else {
          const Number out = -value; // negate cant overflow
          step("-(" + format_number(value) + ") = " + format_number(out));
          value = out;
          stack.pop_back();
        }
      } else if (const auto *b = std::get_if<BinaryOp>(&e.node)) {
        if (f.stage == 0) {
          f.stage = 1;
          stack.push_back({b->lhs.get(), 0, 0.0});
        } else if (f.stage == 1) {
          f.lhs = value; // left is done; park it and go do the right
          f.stage = 2;
          stack.push_back({b->rhs.get(), 0, 0.0});
        } else {
          const Number a = f.lhs;
          const Number c = value;
          Result<Number> out = apply_binary(b->op, a, c);
          if (!out) {
            return at(out.error(), b->span);
          }
          step(format_number(a) + " " + op_symbol(b->op) + " " +
               format_number(c) + " = " + format_number(out.value()));
          value = out.value();
          stack.pop_back();
        }
      } else {
        const auto &call = std::get<FunctionCall>(e.node);
        if (f.stage == 0) {
          // the name and arg count are checked before the argument is walked,
          // so a bad call short-circuits without evaluating (or tracing) it.
          const UnaryFn fn = lookup_function(to_lower(call.name));
          if (fn == nullptr) {
            if (const char *kind = value_name_kind(to_lower(call.name))) {
              return CalcError{ErrorKind::UnknownName,
                               "'" + call.name + "' is a " + kind +
                                   ", not a function",
                               call.span};
            }
            return CalcError{ErrorKind::UnknownName,
                             "unknown function '" + call.name + "'", call.span};
          }
          if (call.args.size() != 1) {
            return CalcError{ErrorKind::WrongArgCount,
                             call.name + " takes 1 argument but got " +
                                 std::to_string(call.args.size()),
                             call.span};
          }
          f.stage = 1;
          stack.push_back({call.args[0].get(), 0, 0.0});
        } else {
          const UnaryFn fn = lookup_function(to_lower(call.name));
          Result<Number> out = fn(value);
          if (!out) {
            return at(out.error(), call.span);
          }
          step(call.name + "(" + format_number(value) +
               ") = " + format_number(out.value()));
          value = out.value();
          stack.pop_back();
        }
      }
    }

    return value;
  }
};

Result<Number> eval(const Expr &expr, const Environment &env, Tracer *tracer) {
  return Evaluator{env, tracer}.run(expr);
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
// parent (the second trace section). like the evaluator it walks off an
// explicit stack so a very deep tree cant overflow the real one. the order a
// recursive pre-order walk gives (a node, then its children left to right) is
// kept by pushing the children in reverse, so they come back off the stack in
// order.
void trace_tree(Tracer &tracer, const Expr &tree) {
  tracer.write("tree:\n");
  std::string out;

  const auto line = [&out](int depth, const std::string &label) {
    out.append(static_cast<std::size_t>(depth) * 2 + 2, ' ');
    out += label;
    out += '\n';
  };

  struct Item {
    const Expr *expr;
    int depth;
  };
  std::vector<Item> stack;
  stack.push_back({&tree, 0});

  while (!stack.empty()) {
    const Item it = stack.back();
    stack.pop_back();
    const Expr &e = *it.expr;
    const int depth = it.depth;

    if (const auto *n = std::get_if<NumberLiteral>(&e.node)) {
      line(depth, format_number(n->value));
    } else if (const auto *v = std::get_if<Variable>(&e.node)) {
      line(depth, v->name);
    } else if (const auto *u = std::get_if<UnaryOp>(&e.node)) {
      line(depth, "- (negate)");
      stack.push_back({u->operand.get(), depth + 1});
    } else if (const auto *b = std::get_if<BinaryOp>(&e.node)) {
      line(depth, op_symbol(b->op));
      stack.push_back({b->rhs.get(), depth + 1});
      stack.push_back({b->lhs.get(), depth + 1});
    } else {
      const auto &call = std::get<FunctionCall>(e.node);
      line(depth, call.args.empty() ? call.name + "()" : call.name + "(...)");
      for (auto arg = call.args.rbegin(); arg != call.args.rend(); ++arg) {
        stack.push_back({arg->get(), depth + 1});
      }
    }
  }

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
  // store under the lower-cased name so `let X = 1` then `x` resolves, matching
  // the case-insensitivity of constants/functions/ans. the reserved-name check
  // above already folds case, so this only affects user names.
  env.set_variable(to_lower(name), value.value());
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
    // show the register; a memory op leaves `ans` alone. checked() so an
    // overflowed register comes back as an error, never a silent inf.
    return checked(env.memory());
  }

  if (tokens[0].type == TokenType::Ident && to_lower(tokens[0].text) == "let") {
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
