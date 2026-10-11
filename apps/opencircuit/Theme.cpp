// SPDX-License-Identifier: GPL-3.0-or-later
#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace theme {

namespace {

QString hex(const QColor& c) { return c.name(QColor::HexRgb); }

QString styleSheet() {
    // Object names used here are set by the widgets that own them.
    return QStringLiteral(R"(
        QMainWindow, #Central { background: %1; }
        QToolTip { background: #ffffff; color: %4; border: 1px solid %3; padding: 4px; }

        /* Top bar */
        #TopBar { background: %1; border-bottom: 1px solid %3; }
        #TopBar QToolButton { border: none; padding: 3px; border-radius: 4px; }
        #TopBar QToolButton:hover { background: %5; }
        #DocTabs::tab { background: #e9ebee; color: %4; padding: 5px 14px; margin-right: 2px;
                        border: 1px solid %3; border-bottom: none; border-top-left-radius: 5px;
                        border-top-right-radius: 5px; min-width: 180px; }
        #DocTabs::tab:selected { background: #ffffff; }

        /* Ribbon */
        #Ribbon { background: %1; border-bottom: 1px solid %3; }
        #Workspace { background: #ffffff; border: 1px solid %3; border-radius: 6px; font-weight: 600;
                     padding: 8px 14px; color: %4; }
        #Workspace:hover { background: %5; }
        #Workspace::menu-indicator { subcontrol-position: right center; right: 6px; }
        #RibbonTabs::tab { background: transparent; color: %6; padding: 3px 12px; border: none;
                           font-size: 11px; font-weight: 600; }
        #RibbonTabs::tab:selected { color: %2; border-bottom: 2px solid %2; }
        #RibbonTabs::tab:hover { color: %4; }
        #RibbonButton { border: 1px solid transparent; border-radius: 4px; padding: 2px; }
        #RibbonButton:hover { background: %5; border-color: #b8dcf6; }
        #RibbonButton:pressed { background: #bfe0f7; }
        #GroupLabel { border: none; color: %4; font-size: 11px; font-weight: 600; padding: 0 4px; }
        #GroupLabel:hover { color: %2; }
        #GroupLabel::menu-indicator { image: none; }
        #RibbonSeparator { background: %3; }

        /* Floating canvas panels */
        #FloatingPanel { background: rgba(255, 255, 255, 236); border: 1px solid %3; border-radius: 6px; }
        #PanelHeader { color: %6; font-size: 11px; font-weight: 600; }
        #PanelHeader QToolButton { border: none; }
        QTreeView#Browser { background: transparent; border: none; color: %4; }
        QTreeView#Browser::item { padding: 2px 0; }
        QTreeView#Browser::item:selected { background: %5; color: %4; }
        QTreeView#Browser::item:hover { background: #eef6fd; }

        #NavBar { background: rgba(255, 255, 255, 240); border: 1px solid %3; border-radius: 6px; }
        #NavBar QToolButton { border: none; padding: 3px; border-radius: 4px; }
        #NavBar QToolButton:hover { background: %5; }
        #NavBar QToolButton:checked { background: %5; }

        #Toast { background: rgba(43, 47, 54, 230); color: white; border-radius: 6px; padding: 8px 14px; }
        #InfoLabel { color: %4; font-weight: 600; }

        /* Timeline */
        #TimelineBar { background: %1; border-top: 1px solid %3; }
        #TimelineBar QToolButton { border: none; padding: 2px; border-radius: 4px; }
        #TimelineBar QToolButton:hover { background: %5; }

        /* Marking menu */
        #MarkButton { background: #ffffff; border: 1px solid %3; border-radius: 6px; padding: 5px 10px;
                      text-align: left; color: %4; }
        #MarkButton:hover { background: %5; border-color: %2; }
        #MarkList { background: #ffffff; border: 1px solid %3; border-radius: 6px; }
        #MarkList QToolButton { border: none; padding: 3px 8px; text-align: left; color: %4; }
        #MarkList QToolButton:hover { background: %5; }

        QDialog { background: #ffffff; }
        QPushButton { padding: 4px 14px; }
    )")
        .arg(hex(kChrome), hex(kAccent), hex(kBorder), hex(kText), hex(kAccentSoft), hex(kTextMuted));
}

} // namespace

void apply(QApplication& app) {
    // Use Qt's portable built-in style (not a platform-native one): it renders
    // identically everywhere and honours every style-sheet property.
    for (const QString& key : QStyleFactory::keys()) {
        const QString k = key.toLower();
        if (k == "windows" || k.startsWith("mac") || k.startsWith("windows")) continue;
        app.setStyle(QStyleFactory::create(key));
        break;
    }
    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, kChrome);
    pal.setColor(QPalette::Base, kPanel);
    pal.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf8, 0xf9));
    pal.setColor(QPalette::Text, kText);
    pal.setColor(QPalette::WindowText, kText);
    pal.setColor(QPalette::ButtonText, kText);
    pal.setColor(QPalette::Button, QColor(0xfb, 0xfb, 0xfc));
    pal.setColor(QPalette::Highlight, kAccent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, Qt::white);
    pal.setColor(QPalette::ToolTipText, kText);
    pal.setColor(QPalette::Disabled, QPalette::Text, kTextMuted);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, kTextMuted);
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());
}

} // namespace theme
