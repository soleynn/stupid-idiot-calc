#pragma once

#include <string_view>

#include "calc/environment.hpp"
#include "calc/number.hpp"
#include "calc/result.hpp"

namespace calc {

// the one public entry point. string in, Result out. never throws across this
// boundary, never touches stdin/stdout.
//
// runs the whole pipeline: tokenize -> parse -> walk the tree to a number. bad
// input (a stray char, a parse error, divide by zero, overflow) comes back as a
// CalcError value, never an exception and never a silent inf/nan.
Result<Number> evaluate(std::string_view input, Environment &env);

} // namespace calc
