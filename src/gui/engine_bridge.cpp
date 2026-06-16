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
  emit historyChanged();
  return line;
}

QStringList Engine::history() const { return history_; }

} // namespace calc::gui
