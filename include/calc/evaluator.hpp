#pragma once

#include <string_view>

#include "calc/environment.hpp"
#include "calc/number.hpp"
#include "calc/result.hpp"

namespace calc {

// the one public entry point. string in, Result out. never throws across this
// boundary, never touches stdin/stdout.
//
// still a skeleton: right now it only evaluates a single number and reports
// anything else as NotImplemented. the real parser comes later.
Result<Number> evaluate(std::string_view input, Environment &env);

} // namespace calc
