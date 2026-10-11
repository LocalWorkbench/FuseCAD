// SPDX-License-Identifier: GPL-3.0-or-later
#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include <utility>

namespace theme {

namespace {

QString css(const QColor& c) {
    if (c.alpha() == 255) return c.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

QColor withAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

QString styleSheet() {
    // Object names used here are set by the widgets that own them. {tokens}
    // are substituted from the palette below.
    QString sheet = QStringLiteral(R"(
        QMainWindow, #Central { background: {chrome}; }
        QToolTip { background: {panel}; color: {text}; border: 1px solid {border}; padding: 4px; }

        /* Top bar */
        #TopBar { background: {chrome}; border-bottom: 1px solid {border}; }
        #TopBar QToolButton { border: none; padding: 3px; border-radius: 4px; }
        #TopBar QToolButton:hover { background: {accent-soft}; }
        #DocTabs::tab { background: {tab}; color: {text}; padding: 5px 14px; margin-right: 2px;
                        border: 1px solid {border}; border-bottom: none; border-top-left-radius: 5px;
                        border-top-right-radius: 5px; min-width: 180px; }
        #DocTabs::tab:selected { background: {panel}; }

        /* Ribbon */
        #Ribbon { background: {chrome}; border-bottom: 1px solid {border}; }
        #Workspace { background: {panel}; border: 1px solid {border}; border-radius: 6px; font-weight: 600;
                     padding: 8px 14px; color: {text}; }
        #Workspace:hover { background: {accent-soft}; }
        #Workspace::menu-indicator { subcontrol-position: right center; right: 6px; }
        #RibbonTabs::tab { background: transparent; color: {text-muted}; padding: 3px 12px; border: none;
                           font-size: 11px; font-weight: 600; }
        #RibbonTabs::tab:selected { color: {accent}; border-bottom: 2px solid {accent}; }
        #RibbonTabs::tab:hover { color: {text}; }
        #RibbonButton { border: 1px solid transparent; border-radius: 4px; padding: 2px; }
        #RibbonButton:hover { background: {accent-soft}; border-color: {accent-border}; }
        #RibbonButton:pressed { background: {accent-pressed}; }
        #GroupLabel { border: none; color: {text}; font-size: 11px; font-weight: 600; padding: 0 4px; }
        #GroupLabel:hover { color: {accent}; }
        #GroupLabel::menu-indicator { image: none; }
        #RibbonSeparator { background: {border}; }

        /* Floating canvas panels */
        #FloatingPanel { background: {panel-translucent}; border: 1px solid {border}; border-radius: 6px; }
        #PanelHeader { color: {text-muted}; font-size: 11px; font-weight: 600; }
        #PanelHeader QToolButton { border: none; }
        QTreeView#Browser { background: transparent; border: none; color: {text}; }
        QTreeView#Browser::item { padding: 2px 0; }
        QTreeView#Browser::item:selected { background: {accent-soft}; color: {text}; }
        QTreeView#Browser::item:hover { background: {hover}; }

        #NavBar { background: {panel-translucent}; border: 1px solid {border}; border-radius: 6px; }
        #NavBar QToolButton { border: none; padding: 3px; border-radius: 4px; }
        #NavBar QToolButton:hover { background: {accent-soft}; }
        #NavBar QToolButton:checked { background: {accent-soft}; }

        #Toast { background: {toast}; color: {text-inverse}; border-radius: 6px; padding: 8px 14px; }
        #InfoLabel { color: {text}; font-weight: 600; }

        /* Timeline */
        #TimelineBar { background: {chrome}; border-top: 1px solid {border}; }
        #TimelineBar QToolButton { border: none; padding: 2px; border-radius: 4px; }
        #TimelineBar QToolButton:hover { background: {accent-soft}; }

        /* Marking menu */
        #MarkButton { background: {panel}; border: 1px solid {border}; border-radius: 6px; padding: 5px 10px;
                      text-align: left; color: {text}; }
        #MarkButton:hover { background: {accent-soft}; border-color: {accent}; }
        #MarkList { background: {panel}; border: 1px solid {border}; border-radius: 6px; }
        #MarkList QToolButton { border: none; padding: 3px 8px; text-align: left; color: {text}; }
        #MarkList QToolButton:hover { background: {accent-soft}; }

        QDialog { background: {panel}; }
        QPushButton { padding: 4px 14px; }
    )");
    const std::pair<const char*, QColor> tokens[] = {
        {"{chrome}", kChrome},
        {"{panel}", kPanel},
        {"{panel-translucent}", withAlpha(kPanel, kPanelAlpha)},
        {"{tab}", kTab},
        {"{border}", kBorder},
        {"{text}", kText},
        {"{text-muted}", kTextMuted},
        {"{text-inverse}", kTextInverse},
        {"{accent}", kAccent},
        {"{accent-soft}", kAccentSoft},
        {"{accent-border}", kAccentBorder},
        {"{accent-pressed}", kAccentPressed},
        {"{hover}", kHover},
        {"{toast}", kToast},
    };
    for (const auto& [token, color] : tokens) sheet.replace(QLatin1String(token), css(color));
    return sheet;
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
    pal.setColor(QPalette::AlternateBase, kPanelAlt);
    pal.setColor(QPalette::Text, kText);
    pal.setColor(QPalette::WindowText, kText);
    pal.setColor(QPalette::ButtonText, kText);
    pal.setColor(QPalette::Button, kButton);
    pal.setColor(QPalette::Highlight, kAccent);
    pal.setColor(QPalette::HighlightedText, kTextInverse);
    pal.setColor(QPalette::ToolTipBase, kPanel);
    pal.setColor(QPalette::ToolTipText, kText);
    pal.setColor(QPalette::Disabled, QPalette::Text, kTextMuted);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, kTextMuted);
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());
}

} // namespace theme
