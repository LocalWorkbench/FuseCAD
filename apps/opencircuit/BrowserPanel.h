// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ViewState.h"

#include <QFrame>
#include <QSet>

namespace oc {
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
        Units,
        NamedViews,
        NamedView,
        Origin,
        OriginItem,
        Bodies,
        Body,
        Sketches,
        Sketch,
        Construction,
    };

    explicit BrowserPanel(QWidget* parent = nullptr);
    void refresh(const QString& documentName, const oc::Document& doc, const ViewState& view);

signals:
    void visibilityToggled(BrowserPanel::Kind kind, int id, bool visible);
    void activated(BrowserPanel::Kind kind, int id);
    void contextMenuRequested(BrowserPanel::Kind kind, int id, const QPoint& globalPos);

private:
    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, Kind kind, int id, const QString& text, const QString& icon,
                             int visible = -1); // -1: no eye, 0: hidden, 1: shown
    QString key(const QTreeWidgetItem* item) const;

    QTreeWidget* tree_ = nullptr;
    QToolButton* collapse_ = nullptr;
    QSet<QString> expanded_;
    bool firstRefresh_ = true;
};
