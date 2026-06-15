#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/format.h>

#include "crash_handler.hpp" // sibling header, not part of the public api

#include "calc/diagnostic.hpp"
#include "calc/environment.hpp"
#include "calc/error.hpp"
#include "calc/evaluator.hpp"
#include "calc/logger.hpp"
#include "calc/number.hpp"
#include "calc/output_formatter.hpp"
#include "calc/result.hpp"
#include "calc/tracer.hpp"

namespace {

// exit codes for one-shot mode (the repl never exits on a user error). 0 ok,
// 2 a user/syntax error. an internal bug exits 70, but thats the crash
// handler's job; the engine returns its failures as values and never throws.
constexpr int kExitOk = 0;
constexpr int kExitUserError = 2;

// the front-end's trace sink: the engine narrates the pipeline into this, and
// it lands on stderr so the result on stdout stays clean for piping.
class StderrTracer : public calc::Tracer {
public:
  void write(std::string_view text) override { std::cerr << text; }
};

// a short label per error kind, for the :stats breakdown.
const char *kind_name(calc::ErrorKind kind) {
  switch (kind) {
  case calc::ErrorKind::EmptyInput:
    return "empty input";
  case calc::ErrorKind::UnexpectedChar:
    return "unexpected char";
  case calc::ErrorKind::UnexpectedToken:
    return "unexpected token";
  case calc::ErrorKind::UnbalancedParen:
    return "unbalanced paren";
  case calc::ErrorKind::TooComplex:
    return "too complex";
  case calc::ErrorKind::DivideByZero:
    return "divide by zero";
  case calc::ErrorKind::Overflow:
    return "overflow";
  case calc::ErrorKind::DomainError:
    return "domain error";
  case calc::ErrorKind::UnknownName:
    return "unknown name";
  case calc::ErrorKind::WrongArgCount:
    return "wrong arg count";
  case calc::ErrorKind::ReservedName:
    return "reserved name";
  case calc::ErrorKind::NotImplemented:
    return "not implemented";
  }
  return "?";
}

// a few in-memory counters for :stats. session state, lives in the front-end.
struct Stats {
  long ok = 0;
  long failed = 0;
  std::map<calc::ErrorKind, long> by_kind;
  double total_us = 0.0; // summed wall-clock over every evaluate() call
};

void print_stats(const Stats &stats) {
  std::cout << "expressions: " << stats.ok << " ok, " << stats.failed
            << " error" << (stats.failed == 1 ? "" : "s") << "\n";
  if (!stats.by_kind.empty()) {
    std::cout << "errors:";
    for (const auto &entry : stats.by_kind) {
      std::cout << " " << kind_name(entry.first) << " x" << entry.second;
    }
    std::cout << "\n";
  }
  const long total = stats.ok + stats.failed;
  if (total > 0) {
    std::cout << "avg eval: "
              << fmt::format("{:.1f}",
                             stats.total_us / static_cast<double>(total))
              << " us\n";
  }
}

void print_repl_help() {
  std::cout << "type an expression and hit enter.\n"
               "  ans            the last result\n"
               "  let x = 5      name a value, then use x in later lines\n"
               "  M+ M- MR MC    memory: add/subtract the last result, "
               "recall, clear\n"
               "  :trace on/off  show tokens, tree and eval on stderr\n"
               "  :stats         counts and average eval time this session\n"
               "  :help          show this\n"
               "  :quit          leave (or just hit ctrl-d)\n";
}

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

// evaluate one expression, print the result to stdout or the error to stderr,
// and hand back a process exit code. this is the scriptable path: `calc "2+2"`
// prints exactly `4` and nothing else, so it composes in a pipeline.
int run_once(const std::string &expression, bool trace) {
  calc::Environment env;
  StderrTracer tracer;
  LOG_DEBUG("one-shot: {}", expression);
  const calc::Result<calc::Number> result =
      calc::evaluate(expression, env, trace ? &tracer : nullptr);
  if (result) {
    std::cout << calc::format_result(result) << "\n";
    return kExitOk;
  }
  LOG_DEBUG("error: {}", result.error().message);
  std::cerr << calc::render_diagnostic(expression, result.error()) << "\n";
  return kExitUserError;
}

// the interactive loop. keeps one Environment alive so ans/memory/let carry
// from line to line, and never exits on a user error. --trace sets the starting
// state; :trace flips it.
int run_repl(bool trace) {
  calc::Environment env;
  StderrTracer tracer;
  Stats stats;
  std::string line;

  LOG_INFO("repl started");
  std::cout << "stupid idiot calc. :help for help, :quit to leave.\n";

  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) {
      std::cout << "\n"; // tidy newline after ctrl-d
      break;
    }

    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue;
    }

    if (trimmed[0] == ':') {
      if (trimmed == ":quit" || trimmed == ":q") {
        break;
      }
      if (trimmed == ":help" || trimmed == ":h") {
        print_repl_help();
        continue;
      }
      if (trimmed == ":trace on") {
        trace = true;
        std::cout << "trace on\n";
        continue;
      }
      if (trimmed == ":trace off") {
        trace = false;
        std::cout << "trace off\n";
        continue;
      }
      if (trimmed == ":trace") {
        std::cout << (trace ? "trace is on\n" : "trace is off\n");
        continue;
      }
      if (trimmed == ":stats") {
        print_stats(stats);
        continue;
      }
      std::cout << "unknown command: " << trimmed << " (try :help)\n";
      continue;
    }

    LOG_DEBUG("eval: {}", trimmed);
    const auto start = std::chrono::steady_clock::now();
    const calc::Result<calc::Number> result =
        calc::evaluate(trimmed, env, trace ? &tracer : nullptr);
    const auto end = std::chrono::steady_clock::now();
    stats.total_us +=
        std::chrono::duration<double, std::micro>(end - start).count();

    if (result) {
      ++stats.ok;
      std::cout << calc::format_result(result) << "\n"; // result -> stdout
    } else {
      ++stats.failed;
      ++stats.by_kind[result.error().kind];
      LOG_DEBUG("error: {}", result.error().message);
      std::cerr << calc::render_diagnostic(trimmed, result.error())
                << "\n"; // error + caret -> stderr
    }
  }

  return kExitOk;
}

} // namespace

int main(int argc, char **argv) {
  calc::cli::install_crash_handler();

  cxxopts::Options options("calc", "a stupid idiot calculator");
  options.add_options()                                                     //
      ("h,help", "show this help and exit")                                 //
      ("v,version", "show the version and exit")                            //
      ("t,trace", "show the pipeline (tokens, tree, eval steps) on stderr") //
      ("log-level", "log threshold: trace|debug|info|warn|error|off",
       cxxopts::value<std::string>()) //
      ("expression", "evaluate this and exit instead of starting the repl",
       cxxopts::value<std::vector<std::string>>());
  options.positional_help("[expression]");
  options.parse_positional({"expression"});

  cxxopts::ParseResult args;
  try {
    args = options.parse(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return kExitUserError;
  }

  if (args.count("help") != 0) {
    std::cout << options.help();
    return kExitOk;
  }
  if (args.count("version") != 0) {
    std::cout << "calc " << CALC_VERSION << "\n";
    return kExitOk;
  }

  if (args.count("log-level") != 0) {
    calc::LogLevel level = calc::LogLevel::Warn;
    if (!calc::parse_log_level(args["log-level"].as<std::string>(), level)) {
      std::cerr << "error: unknown log level '"
                << args["log-level"].as<std::string>() << "'\n";
      return kExitUserError;
    }
    calc::set_log_level(level);
    LOG_DEBUG("log level set to {}", calc::level_name(level));
  }

  const bool trace = args.count("trace") != 0;

  // any leftover words are the one-shot expression. joined with spaces so both
  // `calc "2+2"` and an unquoted `calc 2 + 2` work the same.
  if (args.count("expression") != 0) {
    const auto &words = args["expression"].as<std::vector<std::string>>();
    std::string expression;
    for (const std::string &word : words) {
      if (!expression.empty()) {
        expression += ' ';
      }
      expression += word;
    }
    return run_once(expression, trace);
  }

  return run_repl(trace);
}
