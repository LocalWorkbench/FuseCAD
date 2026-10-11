// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAction>
#include <QHash>
#include <QKeySequence>
#include <QObject>

#include <functional>

// Every user-invokable command is a QAction registered here under a stable id
// ("solid.box", "modify.fillet", ...). Ribbon buttons, menus, the marking menu
// and shortcuts all reference commands by id, so each command has exactly one
// implementation, one label, one icon and one shortcut.
//
// Commands registered without a handler are placeholders for features on the
// roadmap: they stay enabled (so the UI is explorable) and emit
// notImplemented() when triggered.
class CommandRegistry final : public QObject {
    Q_OBJECT

public:
    explicit CommandRegistry(QObject* parent) : QObject(parent) {}

    QAction* add(const QString& id, const QString& text, const QString& icon, std::function<void()> handler = {},
                 const QKeySequence& shortcut = {});
    QAction* get(const QString& id) const; // asserts the id exists
    bool implemented(const QString& id) const { return implemented_.contains(id); }
    // Remember the last real command, for "Repeat" in the marking menu.
    QAction* lastCommand() const { return last_; }

signals:
    void notImplemented(const QString& text);

private:
    QHash<QString, QAction*> actions_;
    QHash<QString, bool> implemented_;
    QAction* last_ = nullptr;
};
