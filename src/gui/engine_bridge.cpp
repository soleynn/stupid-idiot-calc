#include "engine_bridge.hpp"

#include <string>

#include "calc/evaluator.hpp"
#include "calc/output_formatter.hpp"

namespace calc::gui {

Engine::Engine(QObject *parent) : QObject(parent) {}

QString Engine::evaluate(const QString &expression) {
  // string in, Result out, then format_result turns it into one display line
  // (the number, or "error: ..."). the engine updates env_ (ans etc.) itself.
  const std::string input = expression.toStdString();
  const QString line =
      QString::fromStdString(calc::format_result(calc::evaluate(input, env_)));

  history_.prepend(expression + QStringLiteral("  =  ") + line);
  if (history_.size() > kMaxHistory) {
    history_.removeLast(); // drop the oldest, keep the most recent kMaxHistory
  }
  emit historyChanged();
  // raw expression too, even if it errored - recall lets u up-arrow to a bad
  // line and fix it. stays index-aligned with history_.
  inputs_.prepend(expression);
  if (inputs_.size() > kMaxHistory) {
    inputs_.removeLast();
  }
  emit inputsChanged();
  return line;
}

QStringList Engine::history() const { return history_; }

QStringList Engine::inputs() const { return inputs_; }

} // namespace calc::gui
