#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h> // isatty / fileno, to gate color on a real terminal
#define CALC_HAVE_ISATTY 1
#else
#define CALC_HAVE_ISATTY 0
#endif

#include <cxxopts.hpp>
#include <fmt/color.h>
#include <fmt/format.h>
#include <isocline.h>

#include "crash_handler.hpp" // sibling header, not part of the public api
#include "logger.hpp"        // ditto: a cli-private leveled logger

#include "calc/diagnostic.hpp"
#include "calc/environment.hpp"
#include "calc/error.hpp"
#include "calc/evaluator.hpp"
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

// is this stream a real terminal? color and the :clear escape are gated on it
// so piped/redirected output stays plain.
bool stream_is_tty(std::FILE *stream) {
#if CALC_HAVE_ISATTY
  return isatty(fileno(stream)) != 0;
#else
  (void)stream;
  return false;
#endif
}

// wrap text in a color style, but only when the target stream is a terminal.
std::string paint(bool enabled, const fmt::text_style &style,
                  const std::string &text) {
  return enabled ? fmt::format(style, "{}", text) : text;
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

// cxxopts reads any leading-'-' token as an option, so a one-shot expression
// like "-5" or "-pi" gets rejected as an unknown flag. walk past the flags we
// actually define (and the value --log-level takes) and drop a "--" in front of
// the first token that isnt one, so the rest is taken literally as the
// expression. a user-supplied "--" is left alone.
std::vector<std::string> with_positional_guard(int argc, char **argv) {
  std::vector<std::string> out;
  out.reserve(static_cast<std::size_t>(argc) + 1);
  bool split = false; // have we crossed from flags into the expression yet?
  for (int i = 0; i < argc; ++i) {
    const std::string arg = argv[i];
    if (i == 0 || split) { // the program name, or already past the "--"
      out.push_back(arg);
      continue;
    }
    if (arg == "--") { // the user separated it themselves
      split = true;
      out.push_back(arg);
      continue;
    }
    if (arg == "-h" || arg == "--help" || arg == "-v" || arg == "--version" ||
        arg == "-t" || arg == "--trace" || arg.rfind("--log-level=", 0) == 0) {
      out.push_back(arg);
      continue;
    }
    if (arg == "--log-level") { // takes its value as the next token
      out.push_back(arg);
      if (i + 1 < argc) {
        out.push_back(argv[++i]);
      }
      continue;
    }
    out.push_back("--"); // first non-flag token: the expression starts here
    out.push_back(arg);
    split = true;
  }
  return out;
}

// the shared state a meta-command may read or poke. references so a handler can
// flip `trace`/`quit` and the loop sees it.
struct ReplState {
  calc::Environment &env;
  Stats &stats;
  std::vector<std::string> &history;
  bool &trace;
  bool &quit;
  bool tty_out; // stdout is a terminal: ok to clear the screen
};

// one meta-command. the table below is the single place they live; :help walks
// it and adding a command is one row.
struct ReplCommand {
  std::vector<std::string> names; // what the user types; first is canonical
  std::string usage;              // left column in :help
  std::string help;               // right column in :help
  void (*run)(ReplState &state, const std::string &arg);
};

const std::vector<ReplCommand> &commands(); // defined below; :help walks it

// isocline calls these back as C function pointers, so they need C language
// linkage; static keeps them file-local. they drive tab-completion in the repl.

extern "C" {

// what counts as one "word" for completion. isocline's default treats ':' as a
// separator, which would strip the colon off ':help'; we keep ':' (and '_') in
// the word so the leading-colon commands complete as a unit.
static bool is_calc_word_char(const char *s, long len) {
  if (len != 1) {
    return false;
  }
  const char c = s[0];
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == ':';
}

// the things worth completing. the ':' commands come straight from the one
// command table - so aliases like :h / :q complete too and a new command needs
// no edit here - while the rest is hand-listed in step with evaluator.cpp's
// lookup_function / lookup_constant / match_memory_command. memory ops are
// spelled uppercase to match :help; matching is case-insensitive.
static void add_calc_words(ic_completion_env_t *cenv, const char *prefix) {
  static const char *words[] = {
      "M+",   "M-",  "MR",  "MC",            // memory
      "sqrt", "sin", "cos", "tan",   "abs",  //
      "ln",   "log", "exp", "floor", "ceil", // built-in functions
      "pi",   "e",   "ans", "let",           // constants + session names
      nullptr};
  ic_add_completions(cenv, prefix, words);

  std::vector<const char *> cmds;
  for (const ReplCommand &c : commands()) {
    for (const std::string &name : c.names) {
      cmds.push_back(name.c_str()); // commands() is static, so these stay valid
    }
  }
  cmds.push_back(nullptr);
  ic_add_completions(cenv, prefix, cmds.data());
}

static void repl_completer(ic_completion_env_t *cenv, const char *input) {
  // complete just the token under the cursor, using our word definition.
  ic_complete_word(cenv, input, &add_calc_words, &is_calc_word_char);
}

} // extern "C"

void cmd_help(ReplState &, const std::string &) {
  std::cout << "type an expression and hit enter.\n"
               "  ans            the last result\n"
               "  let x = 5      name a value, then use x in later lines\n"
               "  M+ M- MR MC    memory: add/subtract the last result, "
               "recall, clear\n"
               "\ncommands:\n";
  for (const ReplCommand &c : commands()) {
    std::cout << fmt::format("  {:<14} {}\n", c.usage, c.help);
  }
}

void cmd_clear(ReplState &state, const std::string &) {
  if (state.tty_out) {
    std::cout << "\x1b[2J\x1b[H" << std::flush; // clear screen, cursor home
  }
}

void cmd_history(ReplState &state, const std::string &) {
  if (state.history.empty()) {
    std::cout << "(nothing yet)\n";
    return;
  }
  for (std::size_t i = 0; i < state.history.size(); ++i) {
    std::cout << fmt::format("  {:>3}  {}\n", i + 1, state.history[i]);
  }
}

void cmd_vars(ReplState &state, const std::string &) {
  const auto &vars = state.env.variables();
  if (vars.empty()) {
    std::cout << "(no variables yet - define one with let)\n";
    return;
  }
  std::vector<std::pair<std::string, calc::Number>> sorted(vars.begin(),
                                                           vars.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  for (const auto &v : sorted) {
    std::cout << fmt::format("  {} = {}\n", v.first,
                             calc::format_number(v.second));
  }
}

void cmd_trace(ReplState &state, const std::string &arg) {
  if (arg == "on") {
    state.trace = true;
    std::cout << "trace on\n";
  } else if (arg == "off") {
    state.trace = false;
    std::cout << "trace off\n";
  } else if (arg.empty()) {
    std::cout << (state.trace ? "trace is on\n" : "trace is off\n");
  } else {
    std::cout << "usage: :trace on|off\n";
  }
}

void cmd_stats(ReplState &state, const std::string &) {
  print_stats(state.stats);
}

void cmd_quit(ReplState &state, const std::string &) { state.quit = true; }

const std::vector<ReplCommand> &commands() {
  static const std::vector<ReplCommand> table = {
      {{":help", ":h"}, ":help", "show this", &cmd_help},
      {{":clear"}, ":clear", "clear the screen", &cmd_clear},
      {{":history"}, ":history", "show the lines u've entered", &cmd_history},
      {{":vars"}, ":vars", "show ur defined variables", &cmd_vars},
      {{":trace"},
       ":trace on/off",
       "show tokens, tree and eval on stderr",
       &cmd_trace},
      {{":stats"},
       ":stats",
       "counts and average eval time this session",
       &cmd_stats},
      {{":quit", ":q"}, ":quit", "leave (or just hit ctrl-d)", &cmd_quit},
  };
  return table;
}

const ReplCommand *find_command(const std::string &name) {
  for (const ReplCommand &c : commands()) {
    for (const std::string &n : c.names) {
      if (n == name) {
        return &c;
      }
    }
  }
  return nullptr;
}

// the interactive loop. keeps one Environment alive so ans/memory/let carry
// from line to line, and never exits on a user error. --trace sets the starting
// state; :trace flips it.
int run_repl(bool trace) {
  calc::Environment env;
  StderrTracer tracer;
  Stats stats;
  std::vector<std::string> history;
  bool quit = false;

  // color is gated per-stream so piped/redirected output stays plain.
  const bool color_out = stream_is_tty(stdout);
  const bool color_err = stream_is_tty(stderr);

  // line editor: arrow-key history kept in memory only (NULL = no dotfile),
  // default 200-entry cap; the "> " prompt comes from the marker; tab completes
  // commands/functions/constants. all of this is a no-op when stdin is piped.
  ic_set_history(nullptr, -1);
  ic_set_prompt_marker("> ", nullptr);
  ic_set_default_completer(&repl_completer, nullptr);

  LOG_INFO("repl started");
  // the banner is interactive chrome, not a result, so keep it off stdout: a
  // piped session then gets only the answers.
  std::cerr << "stupid idiot calc. :help for help, :quit to leave.\n";

  while (!quit) {
    char *raw = ic_readline(""); // the marker supplies the "> " prompt
    if (raw == nullptr) {        // ctrl-d / ctrl-c / end of piped input
      if (color_out) {
        std::cout << "\n"; // tidy the cursor line, but only in a terminal
      }
      break;
    }
    const std::string trimmed = trim(raw);
    ic_free(raw); // isocline owns the buffer; always hand it back

    if (trimmed.empty()) {
      continue;
    }
    history.push_back(trimmed);

    if (trimmed[0] == ':') {
      // split "<name> <arg...>": the colon-word, then the rest, trimmed.
      const auto space = trimmed.find_first_of(" \t");
      const std::string name = trimmed.substr(0, space);
      const std::string arg =
          space == std::string::npos ? "" : trim(trimmed.substr(space + 1));
      const ReplCommand *command = find_command(name);
      if (command == nullptr) {
        std::cout << "unknown command: " << name << " (try :help)\n";
        continue;
      }
      ReplState state{env, stats, history, trace, quit, color_out};
      command->run(state, arg);
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
      std::cout << paint(color_out, fmt::fg(fmt::color::green),
                         calc::format_result(result))
                << "\n"; // result -> stdout
    } else {
      ++stats.failed;
      ++stats.by_kind[result.error().kind];
      LOG_DEBUG("error: {}", result.error().message);
      std::cerr << paint(color_err, fmt::fg(fmt::color::red),
                         calc::render_diagnostic(trimmed, result.error()))
                << "\n"; // error + caret -> stderr
    }
  }

  return kExitOk;
}

} // namespace

int main(int argc, char **argv) {
  calc::cli::install_crash_handler();
  // flush each log line as its written: the crash handler exits via _Exit,
  // which skips stream flushing, so anything still buffered would be lost.
  std::clog << std::unitbuf;

  // the whole body is wrapped so a cxxopts parse/lookup error comes back as a
  // clean user error (exit 2) instead of escaping main and tripping the crash
  // handler, which is meant for actual internal bugs.
  try {
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

    const std::vector<std::string> guarded = with_positional_guard(argc, argv);
    std::vector<const char *> gargv;
    gargv.reserve(guarded.size());
    for (const std::string &word : guarded) {
      gargv.push_back(word.c_str());
    }
    const cxxopts::ParseResult args =
        options.parse(static_cast<int>(gargv.size()), gargv.data());

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

    // any leftover words are the one-shot expression. joined with spaces so
    // both `calc "2+2"` and an unquoted `calc 2 + 2` work the same.
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
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return kExitUserError;
  }
}
