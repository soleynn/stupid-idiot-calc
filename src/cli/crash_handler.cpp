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

// no streams, no allocation. in signal context only the POSIX ::write branch
// runs, and write() is async-signal-safe; the fputs branch is for the non-POSIX
// terminate path, which is not a signal context.
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
extern "C" void on_signal(int sig) {
  emit();
  // re-raise with the default action restored (SA_RESETHAND did the reset on
  // entry, SA_NODEFER left the signal unblocked) so the kernel still drops a
  // core dump and the process exits with the usual 128+signo status instead of
  // swallowing the crash. raise() is async-signal-safe.
  std::raise(sig);
  std::_Exit(kInternalError); // unreachable once the re-raise lands
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
  // sigaction, not signal(): its disposition/masking are well-defined.
  // RESETHAND restores the default action on entry and NODEFER leaves the
  // signal unblocked, so on_signal can re-raise straight into a core dump.
  struct sigaction sa;
  sa.sa_handler = &on_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = static_cast<int>(SA_RESETHAND | SA_NODEFER);
  sigaction(SIGSEGV, &sa, nullptr);
  sigaction(SIGABRT, &sa, nullptr);
#endif
}

} // namespace calc::cli
