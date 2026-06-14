#pragma once

#include <string>
#include <string_view>

#include "calc/error.hpp"

namespace calc {

// render a CalcError clang style: the "error: <message>" line, and when the
// error has a span, the input line with a 1-based column and a '^' caret under
// the offending token. `input` must be the string the error came from.
std::string render_diagnostic(std::string_view input, const CalcError &error);

} // namespace calc
