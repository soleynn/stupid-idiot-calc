#include "crash_handler.hpp"

#include <cstdlib>
#include <exception>

#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <unistd.h>
#define CALC_HAVE_POSIX 1
#else
#include <cstdio>
#define CALC_HAVE_POSIX 0
#endif

namespace calc::cli {

namespace {

// exit code for an internal bug (the one-shot path uses 0 ok / 2 user error).
constexpr int kInternalError = 70;

constexpr char kMessage[] =
    "\ninternal error: this is a bug in calc, not your input.\n"
    "please report it and include what you typed.\n";

// async-signal-safe: write() / fputs only, no streams, no allocation.
void emit() {
#if CALC_HAVE_POSIX
  const ssize_t written =
      ::write(STDERR_FILENO, kMessage, sizeof(kMessage) - 1);
  (void)written; // nothing useful to do if even this fails
#else
  std::fputs(kMessage, stderr);
#endif
}

#if CALC_HAVE_POSIX
extern "C" void on_signal(int) {
  emit();
  std::_Exit(kInternalError);
}
#endif

[[noreturn]] void on_terminate() {
  emit();
  std::_Exit(kInternalError);
}

} // namespace

void install_crash_handler() {
  std::set_terminate(&on_terminate);
#if CALC_HAVE_POSIX
  std::signal(SIGSEGV, &on_signal);
  std::signal(SIGABRT, &on_signal);
#endif
}

} // namespace calc::cli
