#include <catch2/catch_test_macros.hpp>

#include <ios>
#include <iostream>
#include <sstream>
#include <string>

#include "logger.hpp"

using namespace calc;

TEST_CASE("parse_log_level accepts the level names, any case") {
  LogLevel level = LogLevel::Off;
  REQUIRE(parse_log_level("trace", level));
  REQUIRE(level == LogLevel::Trace);
  REQUIRE(parse_log_level("DEBUG", level));
  REQUIRE(level == LogLevel::Debug);
  REQUIRE(parse_log_level("Info", level));
  REQUIRE(level == LogLevel::Info);
  REQUIRE(parse_log_level("warn", level));
  REQUIRE(level == LogLevel::Warn);
  REQUIRE(parse_log_level("ERROR", level));
  REQUIRE(level == LogLevel::Error);
  REQUIRE(parse_log_level("off", level));
  REQUIRE(level == LogLevel::Off);
}

TEST_CASE("parse_log_level rejects an unknown name") {
  LogLevel level = LogLevel::Warn;
  REQUIRE_FALSE(parse_log_level("loud", level));
  REQUIRE_FALSE(parse_log_level("", level));
}

TEST_CASE("the threshold gates which levels are enabled") {
  set_log_level(LogLevel::Warn);
  REQUIRE_FALSE(log_enabled(LogLevel::Debug));
  REQUIRE_FALSE(log_enabled(LogLevel::Info));
  REQUIRE(log_enabled(LogLevel::Warn));
  REQUIRE(log_enabled(LogLevel::Error));

  set_log_level(LogLevel::Off);
  REQUIRE_FALSE(log_enabled(LogLevel::Error));

  set_log_level(LogLevel::Trace);
  REQUIRE(log_enabled(LogLevel::Trace));

  set_log_level(LogLevel::Warn); // back to the default for later tests
}

TEST_CASE("a disabled macro writes nothing, an enabled one formats with fmt") {
  std::ostringstream captured;
  std::streambuf *old = std::clog.rdbuf(captured.rdbuf());

  set_log_level(LogLevel::Warn);
  LOG_DEBUG("debug {}", 1); // below the threshold: dropped before formatting
  REQUIRE(captured.str().empty());

  LOG_WARN("answer is {}", 42);

  std::clog.rdbuf(old); // restore before asserting, so a failure still prints

  REQUIRE(captured.str().find("answer is 42") != std::string::npos);
  REQUIRE(captured.str().find("[warn]") != std::string::npos);
}
