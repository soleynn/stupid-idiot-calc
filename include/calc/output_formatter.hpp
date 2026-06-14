#pragma once

#include <string>

#include "calc/number.hpp"
#include "calc/result.hpp"

namespace calc {

// turn a number into clean display text: trims trailing zeros, handles
// inf/nan. the one place numbers become strings. returns a string, never
// prints, so it stays usable by any front-end.
std::string format_number(Number value);

// turn a whole Result into one line for the user: the number, or "error: ...".
std::string format_result(const Result<Number> &result);

} // namespace calc
