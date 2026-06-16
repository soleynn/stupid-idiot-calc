#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include "calc/environment.hpp"

namespace calc::gui {

// the whole gui-to-engine boundary: a thin QObject that hands the typed
// expression to calc::evaluate and hands back the formatted line. it owns one
// Environment, so ans / memory / let carry from one tap to the next exactly
// like the repl does. NO calculation logic lives here - thats all calc_core,
// linked unchanged. if u ever feel like adding math in this file, thats the bug
// the engine/front-end split is meant to prevent.
class Engine : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)

public:
  explicit Engine(QObject *parent = nullptr);

  // evaluate one expression: returns the result line (or "error: ...") and
  // pushes "<expr>  =  <result>" onto the front of the history. invokable from
  // qml, called when the user hits `=`.
  Q_INVOKABLE QString evaluate(const QString &expression);

  QStringList history() const;

signals:
  void historyChanged();

private:
  calc::Environment env_;
  QStringList history_;
};

} // namespace calc::gui
