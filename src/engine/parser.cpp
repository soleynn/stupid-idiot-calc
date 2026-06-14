#include "calc/parser.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "calc/ast.hpp"
#include "calc/error.hpp"
#include "calc/token.hpp"

namespace calc {

namespace {

// safety limits. recursive descent recurses on nested parens and on chained
// unary minus, so a wall of '(' or of '-' could overflow the stack. cap the
// depth. a very long flat expression cant overflow the parser (the +/- and */
// levels loop) but it does build a deep left-leaning tree, so cap the token
// count too. both come back as a CalcError, not a crash. neither limit is
// reachable by a human typing at the repl.
//
// note: freeing a near-cap flat tree recurses ~kMaxTokens/2 deep in ~Expr, so
// the teardown stack budget scales with kMaxTokens. its comfortable on the
// usual 8mb main-thread stack (where the engine runs today); a front-end that
// ever drives the engine from a small-stack worker should size that stack to
// match, or drop this cap.
constexpr int kMaxDepth = 256;
constexpr std::size_t kMaxTokens = 4096;

// thrown internally for a convenient early-return out of the recursion, and
// caught at the parse() boundary, which is the only place it converts back to a
// Result. it never escapes this file.
struct ParseError {
  CalcError error;
};

class Parser {
public:
  explicit Parser(const std::vector<Token> &tokens) : tokens_(tokens) {}

  // parse a whole expression and insist the input is fully consumed.
  ExprPtr parse_root() {
    if (peek().type == TokenType::End) {
      fail(ErrorKind::EmptyInput, "type an expression");
    }
    ExprPtr expr = parse_expression();
    const Token &rest = peek();
    if (rest.type != TokenType::End) {
      if (rest.type == TokenType::RParen) {
        fail(ErrorKind::UnbalancedParen, "unmatched ')'", rest);
      }
      fail(ErrorKind::UnexpectedToken, "unexpected input after the expression",
           rest);
    }
    return expr;
  }

private:
  // bumps the depth counter on the way in, drops it on the way out, and refuses
  // to go past kMaxDepth. lives at the two recursive entry points (expression
  // and unary) so both paren nesting and unary chains are bounded.
  struct DepthGuard {
    Parser &p;
    explicit DepthGuard(Parser &parser) : p(parser) {
      if (p.depth_ >= kMaxDepth) {
        p.fail(ErrorKind::TooComplex, "expression nests too deep", p.peek());
      }
      ++p.depth_;
    }
    ~DepthGuard() { --p.depth_; }
    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;
  };

  // level 1: + and -, left-associative (the loop is what makes it left-assoc).
  ExprPtr parse_expression() {
    DepthGuard guard(*this);
    ExprPtr left = parse_term();
    while (peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
      const BinaryOpKind op = advance().type == TokenType::Plus
                                  ? BinaryOpKind::Add
                                  : BinaryOpKind::Subtract;
      ExprPtr right = parse_term();
      left = std::make_unique<Expr>(
          BinaryOp{op, std::move(left), std::move(right)});
    }
    return left;
  }

  // level 2: * / and %, all left-associative and the same precedence.
  ExprPtr parse_term() {
    ExprPtr left = parse_unary();
    while (peek().type == TokenType::Star || peek().type == TokenType::Slash ||
           peek().type == TokenType::Percent) {
      BinaryOpKind op;
      switch (advance().type) {
      case TokenType::Slash:
        op = BinaryOpKind::Divide;
        break;
      case TokenType::Percent:
        op = BinaryOpKind::Modulo;
        break;
      default: // the loop only let Star through to here
        op = BinaryOpKind::Multiply;
        break;
      }
      ExprPtr right = parse_unary();
      left = std::make_unique<Expr>(
          BinaryOp{op, std::move(left), std::move(right)});
    }
    return left;
  }

  // level 3: unary - (and a no-op unary +), right-associative via recursion.
  ExprPtr parse_unary() {
    DepthGuard guard(*this);
    if (peek().type == TokenType::Minus) {
      advance();
      ExprPtr operand = parse_unary();
      return std::make_unique<Expr>(
          UnaryOp{UnaryOpKind::Negate, std::move(operand)});
    }
    if (peek().type == TokenType::Plus) {
      advance();
      return parse_unary(); // +x is just x
    }
    return parse_power();
  }

  // level 4: ^, right-associative; the exponent is a full unary, so 2^-3 works.
  ExprPtr parse_power() {
    ExprPtr base = parse_primary();
    if (peek().type == TokenType::Caret) {
      advance();
      ExprPtr exponent = parse_unary();
      return std::make_unique<Expr>(
          BinaryOp{BinaryOpKind::Power, std::move(base), std::move(exponent)});
    }
    return base;
  }

  // level 5: a number, or a parenthesised expression.
  ExprPtr parse_primary() {
    const Token &t = peek();
    if (t.type == TokenType::Num) {
      advance();
      return std::make_unique<Expr>(NumberLiteral{t.value});
    }
    if (t.type == TokenType::LParen) {
      advance();
      ++open_parens_;
      ExprPtr inner = parse_expression();
      expect(TokenType::RParen, ErrorKind::UnbalancedParen, "expected ')'");
      --open_parens_;
      return inner;
    }
    if (t.type == TokenType::RParen) {
      // a ')' with an open '(' waiting on it is a balanced pair missing its
      // operand (`()` or `(1 + )`); a ')' with nothing open is just stray.
      if (open_parens_ > 0) {
        fail(ErrorKind::UnexpectedToken, "expected a number or '('", t);
      }
      fail(ErrorKind::UnbalancedParen, "unmatched ')'", t);
    }
    if (t.type == TokenType::End) {
      fail(ErrorKind::UnexpectedToken,
           "expected a number or '(' but the input ended", t);
    }
    fail(ErrorKind::UnexpectedToken, "expected a number or '('", t);
  }

  const Token &peek() const { return tokens_[pos_]; }

  // hand back the current token and step forward, but never past the trailing
  // End, so peek() stays in bounds no matter how confused the input is.
  const Token &advance() {
    const std::size_t here = pos_;
    if (pos_ + 1 < tokens_.size()) {
      ++pos_;
    }
    return tokens_[here];
  }

  // consume a token of the given type or fail with a pointed error.
  const Token &expect(TokenType type, ErrorKind kind, std::string message) {
    if (peek().type != type) {
      fail(kind, std::move(message), peek());
    }
    return advance();
  }

  // for errors with nothing to point at, like empty input: no span, no caret.
  [[noreturn]] void fail(ErrorKind kind, std::string message) {
    throw ParseError{CalcError{kind, std::move(message)}};
  }

  // bail out of the recursion with an error aimed at one token. operators and
  // parens are a single char; End has no width. a multi-digit number used as an
  // error token (only a trailing/stray one can be, like the "23" in "1 23")
  // under-points to width 1, since Token only carries an offset and no length
  // yet. proper width waits until it does.
  [[noreturn]] void fail(ErrorKind kind, std::string message,
                         const Token &tok) {
    const std::size_t length = tok.type == TokenType::End ? 0u : 1u;
    throw ParseError{
        CalcError{kind, std::move(message), SourceSpan{tok.offset, length}}};
  }

  const std::vector<Token> &tokens_;
  std::size_t pos_ = 0;
  int depth_ = 0;
  int open_parens_ = 0;
};

} // namespace

Result<Expr> parse(const std::vector<Token> &tokens) {
  if (tokens.empty()) {
    return CalcError{ErrorKind::EmptyInput, "type an expression"};
  }
  if (tokens.size() > kMaxTokens) {
    return CalcError{ErrorKind::TooComplex, "expression is too long"};
  }

  try {
    Parser parser(tokens);
    ExprPtr root = parser.parse_root();
    return std::move(*root);
  } catch (const ParseError &e) {
    return e.error;
  }
}

} // namespace calc
