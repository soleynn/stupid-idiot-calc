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
//     1     + -        left          expression := term  (('+'|'-') term )*
//     2     * / %      left          term       := unary (('*'|'/'|'%') unary)*
//     3     unary -    right         unary      := ('+'|'-') unary | power
//     4     ^          right         power      := primary ('^' unary)?
//     5     ( ) num    --            primary    := number | name | call | group
//
//   group := '(' expression ')'
//   call  := name '(' args? ')'
//   args  := expression (',' expression)*
//
// left-associative levels loop (so 8-3-2 groups as (8-3)-2); the unary level
// recurses, which is right-associative and lets --5 and -+5 nest. unary '+' is
// accepted as a no-op so +5 isnt a surprise error.
//
// '^' is right-associative too (2^3^2 is 2^(3^2)), and it binds tighter than
// a unary minus on its left, so -2^2 means -(2^2). its exponent is a full
// unary level, so 2^-3 still parses.
//
// a bare name is a constant like pi or e; a name before '(' is a function call.
// an empty arg list (foo()) parses fine and comes back as a wrong-arg-count
// error at eval time, not here. which names are valid is the evaluator's call.

// turn a token list (from tokenize) into a parse tree, or a CalcError pointing
// at the offending token. never throws across this boundary, never evaluates.
// guards against runaway depth/length so pathological input cant blow the
// stack. it comes back as a CalcError instead.
Result<Expr> parse(const std::vector<Token> &tokens);

} // namespace calc
