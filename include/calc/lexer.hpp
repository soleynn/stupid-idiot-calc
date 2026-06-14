#pragma once

#include <string_view>
#include <vector>

#include "calc/result.hpp"
#include "calc/token.hpp"

namespace calc {

// turns raw input into a flat list of tokens, ending with an End token. skips
// whitespace, reads numbers, the four operators and parens, and reports the
// first unknown character as an error.
Result<std::vector<Token>> tokenize(std::string_view input);

} // namespace calc
