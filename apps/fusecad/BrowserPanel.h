// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ViewState.h"

#include <QFrame>
#include <QSet>

#include <optional>

namespace fuse {
class Document;
}
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;

// The design browser: a tree of the document's contents (settings, named
// views, origin, bodies, sketches) with per-item visibility toggles.
class BrowserPanel final : public QFrame {
    Q_OBJECT

public:
    enum class Kind {
        Document,
        Settings,
        NamedViews,
        NamedView,
        Origin,
        OriginItem,
        Bodies,
        Body,
        Sketches,
        Sketch,
    };

    // What a row refers to. `command` is the command a named view runs.
    struct Item {
        Kind kind;
        int id;
        QString command;
    };

    explicit BrowserPanel(QWidget* parent = nullptr);
    void refresh(const QString& documentName, const fuse::Document& doc, const ViewState& view);

signals:
    void visibilityToggled(const BrowserPanel::Item& item, bool visible);
    void activated(const BrowserPanel::Item& item);
    void contextMenuRequested(const BrowserPanel::Item& item, const QPoint& globalPos);

private:
    // `toggledOn` is the row's own eye state (no eye when unset); `folderShown`
    // dims the row when the folder containing it is hidden.
    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, Kind kind, int id, const QString& text, const QString& icon,
                             std::optional<bool> toggledOn = std::nullopt, bool folderShown = true);
    static Item info(const QTreeWidgetItem* item);
    static QString key(const QTreeWidgetItem* item);

    QTreeWidget* tree_ = nullptr;
    QToolButton* collapse_ = nullptr;
    QSet<QString> expanded_;
    bool firstRefresh_ = true;
};
