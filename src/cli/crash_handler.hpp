#pragma once

namespace calc::cli {

// install a last-ditch handler for an internal bug (an uncaught exception or a
// segfault): print a short "this is a bug" note and exit 70, instead of a bare
// crash. expected, user-facing failures never get here; they come back as
// CalcError values from the engine.
void install_crash_handler();

} // namespace calc::cli
