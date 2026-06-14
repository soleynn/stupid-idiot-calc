#pragma once

namespace calc {

// reserved seam for variables / memory / `ans` later. empty for now, but
// threaded through evaluate() from the start so adding that stuff is additive
// instead of a signature-breaking refactor.
class Environment {};

} // namespace calc
