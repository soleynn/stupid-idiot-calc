#include <iostream>
#include <string>

#include "calc/environment.hpp"
#include "calc/evaluator.hpp"
#include "calc/number.hpp"
#include "calc/output_formatter.hpp"
#include "calc/result.hpp"

namespace {

void print_help() {
  std::cout << "type an expression and hit enter.\n"
               "  :help   show this\n"
               "  :quit   leave (or just hit ctrl-d)\n";
}

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

} // namespace

int main() {
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
        print_help();
        continue;
      }
      std::cout << "unknown command: " << trimmed << " (try :help)\n";
      continue;
    }

    const calc::Result<calc::Number> result = calc::evaluate(trimmed, env);
    if (result) {
      std::cout << calc::format_result(result) << "\n"; // result -> stdout
    } else {
      std::cerr << calc::format_result(result) << "\n"; // error -> stderr
    }
  }

  return 0;
}
