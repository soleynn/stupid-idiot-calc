#include <cstdio>
#include <string_view>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include "engine_bridge.hpp"

// the gui front-end entry point. it does what the repl's main does, minus the
// terminal: spin up Qt, load the qml, run the event loop. all the calculator
// lives in calc_core (linked unchanged) and the one Engine bridge.
int main(int argc, char *argv[]) {
  // --version prints and exits before any Qt, so it works with no display (e.g.
  // `flatpak run ... --version` in a terminal). CALC_VERSION is stamped at
  // build time - the project version, or the release tag a build was cut from.
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--version") {
      std::printf("stupid idiot calc %s\n", CALC_VERSION);
      return 0;
    }
  }

  QGuiApplication app(argc, argv);

  // a headless wiring check for ci: build the bridge, evaluate a known
  // expression and exit by its result - no window, no event loop. run it with
  // QT_QPA_PLATFORM=offscreen so it needs no display. this is how ci proves the
  // gui actually computes without anyone clicking buttons.
  if (app.arguments().contains(QStringLiteral("--selftest"))) {
    calc::gui::Engine engine;
    const bool ok =
        engine.evaluate(QStringLiteral("1 + 2 * 3")) == QStringLiteral("7");
    return ok ? 0 : 1;
  }

  QQmlApplicationEngine engine;
  // bail with a non-zero exit if the qml root fails to load, instead of running
  // a windowless event loop forever.
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("calc.gui", "Main");

  return app.exec();
}
