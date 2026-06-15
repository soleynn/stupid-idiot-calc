#pragma once

#include <string_view>

namespace calc {

// the sink for --trace / :trace output. the engine builds the trace text and
// pushes it here; the front-end decides where it lands (stderr). keeping it
// abstract is what lets the engine narrate itself without ever owning a stream
// or breaking the no-i/o rule.
class Tracer {
public:
  virtual ~Tracer() = default;
  virtual void write(std::string_view text) = 0;
};

} // namespace calc
