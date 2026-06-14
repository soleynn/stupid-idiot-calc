#pragma once

#include <vector>

#include "calc/ast.hpp"
#include "calc/result.hpp"
#include "calc/token.hpp"

namespace calc {

// precedence + associativity, loosest-binding at the top. this is the whole
// design of the parser: one function per level, and the call chain
// expression -> term -> unary -> primary *is* the precedence ladder. levels
// further down bind tighter.
//
//   level  operators  associativity  grammar rule
//   -----  ---------  -------------  --------------------------------------
//     1     + -        left           expression := term  (('+'|'-') term )*
//     2     * /        left           term       := unary (('*'|'/') unary)*
//     3     unary -    right          unary      := ('+'|'-') unary | primary
//     4     ( ) num    --             primary    := number | '(' expression ')'
//
// left-associative levels loop (so 8-3-2 groups as (8-3)-2); the unary level
// recurses, which is right-associative and lets --5 and -+5 nest. unary '+' is
// accepted as a no-op so +5 isnt a surprise error.

// turn a token list (from tokenize) into a parse tree, or a CalcError pointing
// at the offending token. never throws across this boundary, never evaluates.
// guards against runaway depth/length so pathological input cant blow the
// stack. it comes back as a CalcError instead.
Result<Expr> parse(const std::vector<Token> &tokens);

} // namespace calc
