// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QStringList>

class CommandRegistry;
class MainWindow;
class QMenu;
class QMenuBar;
class QTabBar;
class QWidget;
class Ribbon;

// The declarative description of the design workspace: the command table and
// how commands are arranged in the ribbon, top bar, menu bar, navigation bar
// and marking menu. Handlers live on MainWindow; this unit only binds them.
class Workbench final {
    Q_DECLARE_TR_FUNCTIONS(Workbench)

public:
    Workbench() = delete;

    static void registerCommands(CommandRegistry& commands, MainWindow& window);

    static Ribbon* buildRibbon(CommandRegistry& commands, QWidget* parent);
    // `docTabs` receives the document tab bar embedded in the top bar.
    static QWidget* buildTopBar(CommandRegistry& commands, QTabBar*& docTabs, QWidget* parent);
    static void buildMenuBar(CommandRegistry& commands, QMenuBar& menuBar);
    static QMenu* buildDisplayMenu(CommandRegistry& commands, QWidget* parent);

    static QStringList navigationBar(); // ids for NavigationBar
    static QStringList markingMenuRing(const QString& repeatCommand);
    static QStringList markingMenuList();

private:
    static void addFileCommands(CommandRegistry& commands, QMenu& menu);
};
