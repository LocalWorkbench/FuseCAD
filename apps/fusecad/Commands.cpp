// SPDX-License-Identifier: GPL-3.0-or-later
#include "Commands.h"

#include "Icons.h"

QAction* CommandRegistry::add(const QString& id, const QString& text, const QString& icon,
                              std::function<void()> handler, const QKeySequence& shortcut) {
    Q_ASSERT_X(!actions_.contains(id), "CommandRegistry::add", qPrintable("duplicate command " + id));
    auto* action = new QAction(icon.isEmpty() ? QIcon() : icons::get(icon), text, this);
    action->setObjectName(id);
    action->setIconVisibleInMenu(true);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        action->setShortcutContext(Qt::WindowShortcut);
    }
    QString tip = text;
    if (!shortcut.isEmpty()) tip += QStringLiteral("  (%1)").arg(shortcut.toString(QKeySequence::NativeText));
    if (!handler) tip += tr(" - coming soon");
    action->setToolTip(tip);

    if (handler) {
        const bool repeatable = id.startsWith(QLatin1String("create.")) || id.startsWith(QLatin1String("modify."));
        connect(action, &QAction::triggered, this, [this, action, repeatable, handler = std::move(handler)] {
            if (repeatable) last_ = action;
            handler();
        });
    } else {
        connect(action, &QAction::triggered, this, [this, text] { emit notImplemented(text); });
    }
    actions_.insert(id, action);
    return action;
}

QAction* CommandRegistry::get(const QString& id) const {
    QAction* a = actions_.value(id, nullptr);
    Q_ASSERT_X(a, "CommandRegistry::get", qPrintable("unknown command " + id));
    return a;
}
