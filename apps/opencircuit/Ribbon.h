// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QWidget>

#include <vector>

class CommandRegistry;
class QHBoxLayout;
class QStackedWidget;
class QTabBar;
class QToolButton;

// The command ribbon: a workspace switcher, a row of tabs, and per tab a strip
// of groups. Each group shows its most-used commands as large buttons and its
// full command list under a "NAME ▾" drop-down.
class Ribbon final : public QWidget {
    Q_OBJECT

public:
    struct Group {
        QString title;
        QStringList visible;  // command ids shown as buttons
        QStringList overflow; // additional ids in the drop-down (after the visible ones); "-" = separator
    };

    Ribbon(CommandRegistry& commands, QWidget* parent = nullptr);

    QToolButton* workspaceButton() const { return workspace_; }
    void addTab(const QString& title, const std::vector<Group>& groups);

private:
    QWidget* buildGroup(const Group& group);

    CommandRegistry& commands_;
    QToolButton* workspace_ = nullptr;
    QTabBar* tabs_ = nullptr;
    QStackedWidget* pages_ = nullptr;
};
