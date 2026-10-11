// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <vector>

namespace fuse {
class Document;
}
class CommandRegistry;
class TimelineStrip;

// The parametric history along the bottom of the window: transport controls
// plus one icon per feature and a draggable history marker. Features after
// the marker are rolled back (not computed) and drawn faded. "Play" is the
// registered timeline.play command.
class TimelineBar final : public QWidget {
    Q_OBJECT

public:
    TimelineBar(CommandRegistry& commands, QWidget* parent);
    void refresh(const fuse::Document& doc);
    int selectedFeatureId() const;

signals:
    void markerRequested(int position); // number of active features
    void contextMenuRequested(int featureId, const QPoint& globalPos);
    void editRequested(int featureId);

private:
    TimelineStrip* strip_ = nullptr;
};

// Implementation detail of TimelineBar; exposed for moc.
class TimelineStrip final : public QWidget {
    Q_OBJECT

public:
    struct Item {
        int id;
        QString name;
        QString icon;
        bool healthy;
        bool suppressed;
    };

    explicit TimelineStrip(QWidget* parent);
    void setItems(std::vector<Item> items, int marker);
    int marker() const { return marker_; }
    int count() const { return static_cast<int>(items_.size()); }
    int selectedId() const { return selected_; }

    QSize sizeHint() const override;

signals:
    void markerMoved(int position);
    void contextMenuRequested(int featureId, const QPoint& globalPos);
    void activated(int featureId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool event(QEvent* event) override; // tooltips

private:
    QRect cellRect(int index) const;
    int markerX(int position) const;
    int hitItem(QPoint p) const;
    int nearestGap(int x) const;

    std::vector<Item> items_;
    int marker_ = 0;
    int selected_ = -1;
    bool draggingMarker_ = false;
    int dragPosition_ = 0;
};
