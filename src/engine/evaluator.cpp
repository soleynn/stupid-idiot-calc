#include "calc/evaluator.hpp"

#include <vector>

#include "calc/lexer.hpp"
#include "calc/token.hpp"

namespace calc {

Result<Number> evaluate(std::string_view input, Environment &env) {
  (void)env; // unused for now, reserved seam.

  Result<std::vector<Token>> lexed = tokenize(input);
  if (!lexed) {
    return lexed.error();
  }

  const std::vector<Token> &tokens = lexed.value();

  // an empty / whitespace-only input lexes to just [End].
  if (tokens.size() == 1) {
    return CalcError{ErrorKind::EmptyInput, "type an expression"};
  }

  // skeleton: only a single bare number works so far. the real recursive
  // descent parser lands later; anything else is reported as not implemented.
  if (tokens.size() == 2 && tokens[0].type == TokenType::Num &&
      tokens[1].type == TokenType::End) {
    return tokens[0].value;
  }

  return CalcError{ErrorKind::NotImplemented,
                   "only single numbers work so far"};
}

} // namespace calc
