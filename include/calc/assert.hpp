#pragma once

#include <cassert>

// debug-only invariant check, compiled out under NDEBUG. never use it on user
// input; those failures go through CalcError. msg must be a string literal.
#define CALC_ASSERT(cond, msg) assert((cond) && (msg))
