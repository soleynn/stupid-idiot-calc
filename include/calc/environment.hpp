#pragma once

#include <string>
#include <unordered_map>

#include "calc/number.hpp"

namespace calc {

// the bit of state that lives across one evaluate() to the next: the last
// answer, the memory register, and any names the user bound with `let`. its
// threaded through evaluate() so all of this is additive instead of a
// signature-breaking change. front-ends just keep one of these alive for the
// session and hand it back every call.
class Environment {
public:
  // the last successful result. starts at 0 so `ans` works before u've computed
  // anything, same as a pocket calculator.
  Number answer() const { return answer_; }
  void set_answer(Number value) { answer_ = value; }

  // the single memory register behind M+ / M- / MR / MC. also starts at 0.
  Number memory() const { return memory_; }
  void memory_add(Number value) { memory_ += value; }
  void memory_subtract(Number value) { memory_ -= value; }
  void memory_clear() { memory_ = 0.0; }

  // names from `let x = ...`. lookup writes into out and returns whether the
  // name was found, so the caller can fall through to "unknown name".
  bool lookup_variable(const std::string &name, Number &out) const {
    const auto it = variables_.find(name);
    if (it == variables_.end()) {
      return false;
    }
    out = it->second;
    return true;
  }
  void set_variable(const std::string &name, Number value) {
    variables_[name] = value;
  }

  // read-only view of the let-bound names, for the cli's :vars command. still
  // i/o-free; it just hands back the data.
  const std::unordered_map<std::string, Number> &variables() const {
    return variables_;
  }

private:
  Number answer_ = 0.0;
  Number memory_ = 0.0;
  std::unordered_map<std::string, Number> variables_;
};

} // namespace calc
