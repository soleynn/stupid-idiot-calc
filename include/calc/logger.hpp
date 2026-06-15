#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <fmt/format.h>

// a tiny leveled logger for the *front-end's* dev output. it writes to
// std::clog (stderr), so its NOT for the engine: calc_core stays i/o-free and
// never includes this. the call sites are spdlog-shaped on purpose, so if a
// file or async sink is ever wanted, swapping the body for spdlog stays
// mechanical.

namespace calc {

// the levels, low to high. a message shows when its level is >= the current
// threshold; Off sits at the top so it silences everything.
enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

inline const char *level_name(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "trace";
  case LogLevel::Debug:
    return "debug";
  case LogLevel::Info:
    return "info";
  case LogLevel::Warn:
    return "warn";
  case LogLevel::Error:
    return "error";
  case LogLevel::Off:
    return "off";
  }
  return "?";
}

// the threshold lives in one function-local static so the header stays
// self-contained. default Warn keeps an ordinary run quiet.
inline LogLevel &current_log_level() {
  static LogLevel level = LogLevel::Warn;
  return level;
}
inline void set_log_level(LogLevel level) { current_log_level() = level; }
inline LogLevel log_level() { return current_log_level(); }

inline bool log_enabled(LogLevel level) { return level >= log_level(); }

// parse a --log-level value, case-insensitively. false on an unknown name.
inline bool parse_log_level(std::string_view name, LogLevel &out) {
  std::string lower(name);
  for (char &c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  if (lower == "trace") {
    out = LogLevel::Trace;
  } else if (lower == "debug") {
    out = LogLevel::Debug;
  } else if (lower == "info") {
    out = LogLevel::Info;
  } else if (lower == "warn") {
    out = LogLevel::Warn;
  } else if (lower == "error") {
    out = LogLevel::Error;
  } else if (lower == "off") {
    out = LogLevel::Off;
  } else {
    return false;
  }
  return true;
}

// write one already-formatted line to the sink. the macros below are the real
// entry point; this is just the tail they call once the level check passes.
inline void log_write(LogLevel level, std::string_view message) {
  std::clog << "[" << level_name(level) << "] " << message << '\n';
}

} // namespace calc

// early-out on the level BEFORE formatting, so a disabled
// LOG_DEBUG(expensive()) pays for nothing but the comparison.
#define CALC_LOG(level, ...)                                                   \
  do {                                                                         \
    if (::calc::log_enabled(level)) {                                          \
      ::calc::log_write((level), ::fmt::format(__VA_ARGS__));                  \
    }                                                                          \
  } while (0)

#define LOG_TRACE(...) CALC_LOG(::calc::LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...) CALC_LOG(::calc::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) CALC_LOG(::calc::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) CALC_LOG(::calc::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) CALC_LOG(::calc::LogLevel::Error, __VA_ARGS__)
