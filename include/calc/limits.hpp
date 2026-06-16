#pragma once

#include <cstddef>

namespace calc {

// the most tokens one expression may have. the lexer stops as soon as it would
// pass this, so an oversized line (a giant paste, a millions-of-chars argv)
// never balloons into a multi-gb token vector before anything checks the size;
// the parser refuses a longer token list too, as a backstop for a hand-built
// one. 4096 is far more than anyone types and still leaves the worst-case parse
// tree small enough to walk. the trailing End token counts toward the cap.
inline constexpr std::size_t kMaxTokens = 4096;

} // namespace calc
