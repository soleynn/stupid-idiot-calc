#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <cxxopts.hpp>

#include "calc/diagnostic.hpp"
#include "calc/environment.hpp"
#include "calc/evaluator.hpp"
#include "calc/number.hpp"
#include "calc/output_formatter.hpp"
#include "calc/result.hpp"

namespace {

// exit codes for one-shot mode (the repl never exits on a user error). 0 ok,
// 2 a user/syntax error. there's also a reserved 70 for an internal bug, but
// the engine returns its failures as values and never throws across the
// boundary, so nothing here can hit it yet.
constexpr int kExitOk = 0;
constexpr int kExitUserError = 2;

void print_repl_help() {
  std::cout << "type an expression and hit enter.\n"
               "  ans            the last result\n"
               "  let x = 5      name a value, then use x in later lines\n"
               "  M+ M- MR MC    memory: add/subtract the last result, "
               "recall, clear\n"
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
int run_once(const std::string &expression) {
  calc::Environment env;
  const calc::Result<calc::Number> result = calc::evaluate(expression, env);
  if (result) {
    std::cout << calc::format_result(result) << "\n";
    return kExitOk;
  }
  std::cerr << calc::render_diagnostic(expression, result.error()) << "\n";
  return kExitUserError;
}

// the interactive loop. keeps one Environment alive so ans/memory/let carry
// from line to line, and never exits on a user error.
int run_repl() {
  calc::Environment env;
  std::string line;

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
      std::cout << "unknown command: " << trimmed << " (try :help)\n";
      continue;
    }

    const calc::Result<calc::Number> result = calc::evaluate(trimmed, env);
    if (result) {
      std::cout << calc::format_result(result) << "\n"; // result -> stdout
    } else {
      std::cerr << calc::render_diagnostic(trimmed, result.error())
                << "\n"; // error + caret -> stderr
    }
  }

  return kExitOk;
}

} // namespace

int main(int argc, char **argv) {
  cxxopts::Options options("calc", "a stupid idiot calculator");
  options.add_options()                          //
      ("h,help", "show this help and exit")      //
      ("v,version", "show the version and exit") //
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
    return run_once(expression);
  }

  return run_repl();
}
