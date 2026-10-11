// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimelineBar.h"

#include "Icons.h"
#include "Theme.h"

#include "oc/model/Document.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QToolTip>

namespace {

constexpr int kCell = 30;
constexpr int kIcon = 22;
constexpr int kPad = 10;

QString iconFor(oc::FeatureType t) {
    switch (t) {
    case oc::FeatureType::Sketch: return "sketch";
    case oc::FeatureType::Extrude: return "extrude";
    case oc::FeatureType::Revolve: return "revolve";
    case oc::FeatureType::Fillet: return "fillet";
    case oc::FeatureType::Chamfer: return "chamfer";
    case oc::FeatureType::ImportStep: return "import";
    }
    return {};
}

QToolButton* transport(QWidget* parent, const QString& icon, const QString& tip) {
    auto* b = new QToolButton(parent);
    b->setIcon(icons::get(icon));
    b->setIconSize(QSize(16, 16));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    return b;
}

} // namespace

// --- TimelineStrip ------------------------------------------------------------

TimelineStrip::TimelineStrip(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize TimelineStrip::sizeHint() const { return {kPad * 2 + count() * kCell + 8, 40}; }

void TimelineStrip::setItems(std::vector<Item> items, int marker) {
    items_ = std::move(items);
    marker_ = std::clamp(marker, 0, count());
    if (std::none_of(items_.begin(), items_.end(), [this](const Item& i) { return i.id == selected_; }))
        selected_ = -1;
    updateGeometry();
    update();
}

QRect TimelineStrip::cellRect(int index) const {
    return {kPad + index * kCell + (index >= marker_ ? 8 : 0), (height() - kCell) / 2, kCell, kCell};
}

// x of the marker when placed after `position` features. The rolled-back cells
// are shifted right by 8 px to open a gap for the marker at marker_.
int TimelineStrip::markerX(int position) const { return kPad + position * kCell + (position > marker_ ? 8 : 4); }

int TimelineStrip::hitItem(QPoint p) const {
    for (int i = 0; i < count(); ++i)
        if (cellRect(i).contains(p)) return i;
    return -1;
}

int TimelineStrip::nearestGap(int x) const { return std::clamp((x - kPad + kCell / 2) / kCell, 0, count()); }

void TimelineStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Track line.
    const int midY = height() / 2;
    p.setPen(QPen(theme::kBorder, 1));
    p.drawLine(kPad, midY + kCell / 2 + 2, std::max(width() - kPad, kPad + count() * kCell + 8), midY + kCell / 2 + 2);

    for (int i = 0; i < count(); ++i) {
        const Item& it = items_[static_cast<size_t>(i)];
        const QRect cell = cellRect(i);
        if (it.id == selected_) {
            p.setPen(QPen(theme::kAccent, 1.2));
            p.setBrush(theme::kAccentSoft);
            p.drawRoundedRect(cell.adjusted(1, 1, -1, -1), 4, 4);
        }
        const bool rolledBack = i >= marker_;
        p.setOpacity(rolledBack || it.suppressed ? 0.35 : 1.0);
        icons::get(it.icon).paint(&p, cell.adjusted((kCell - kIcon) / 2, (kCell - kIcon) / 2, -(kCell - kIcon) / 2,
                                                     -(kCell - kIcon) / 2));
        p.setOpacity(1.0);
        if (!it.healthy) {
            p.setPen(Qt::NoPen);
            p.setBrush(theme::kError);
            p.drawEllipse(QPointF(cell.right() - 4, cell.top() + 5), 3.5, 3.5);
        }
        if (it.suppressed) {
            p.setPen(QPen(theme::kTextMuted, 1.6));
            p.drawLine(cell.topLeft() + QPoint(6, 6), cell.bottomRight() - QPoint(6, 6));
        }
    }

    // History marker: a vertical bar with a grab handle.
    const int mx = markerX(draggingMarker_ ? dragPosition_ : marker_);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x3d, 0x44, 0x4f));
    p.drawRoundedRect(QRectF(mx - 1.5, midY - kCell / 2 - 2, 3, kCell + 6), 1.5, 1.5);
    p.drawRoundedRect(QRectF(mx - 4, midY - kCell / 2 - 5, 8, 8), 2, 2);
}

void TimelineStrip::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPoint pt = e->position().toPoint();
    const int hit = hitItem(pt);
    if (std::abs(pt.x() - markerX(marker_)) <= 6) {
        draggingMarker_ = true;
        dragPosition_ = marker_;
        setCursor(Qt::SizeHorCursor);
        return;
    }
    if (hit >= 0) {
        selected_ = items_[static_cast<size_t>(hit)].id;
        emit selected(selected_);
        update();
    }
}

void TimelineStrip::mouseMoveEvent(QMouseEvent* e) {
    if (!draggingMarker_) return;
    dragPosition_ = nearestGap(e->position().toPoint().x());
    update();
}

void TimelineStrip::mouseReleaseEvent(QMouseEvent*) {
    if (!draggingMarker_) return;
    draggingMarker_ = false;
    unsetCursor();
    if (dragPosition_ != marker_) emit markerMoved(dragPosition_);
    update();
}

void TimelineStrip::mouseDoubleClickEvent(QMouseEvent* e) {
    const int hit = hitItem(e->position().toPoint());
    if (hit >= 0) emit activated(items_[static_cast<size_t>(hit)].id);
}

void TimelineStrip::contextMenuEvent(QContextMenuEvent* e) {
    const int hit = hitItem(e->pos());
    if (hit < 0) return;
    selected_ = items_[static_cast<size_t>(hit)].id;
    update();
    emit contextMenuRequested(selected_, e->globalPos());
}

bool TimelineStrip::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int hit = hitItem(he->pos());
        if (hit >= 0) {
            const Item& it = items_[static_cast<size_t>(hit)];
            QString tip = it.name;
            if (it.suppressed) tip += tr(" (suppressed)");
            if (hit >= marker_) tip += tr(" (rolled back)");
            QToolTip::showText(he->globalPos(), tip, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(e);
}

// --- TimelineBar --------------------------------------------------------------

TimelineBar::TimelineBar(QWidget* parent) : QWidget(parent) {
    setObjectName("TimelineBar");
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(46);

    auto* first = transport(this, "tl-first", tr("Go to beginning"));
    auto* prev = transport(this, "tl-prev", tr("Step back"));
    auto* play = transport(this, "tl-play", tr("Play history"));
    auto* next = transport(this, "tl-next", tr("Step forward"));
    auto* last = transport(this, "tl-last", tr("Go to end"));
    strip_ = new TimelineStrip(this);
    auto* gear = transport(this, "settings", tr("Timeline settings"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(2);
    for (auto* b : {first, prev, play, next, last}) layout->addWidget(b);
    layout->addSpacing(8);
    layout->addWidget(strip_, 1);
    layout->addWidget(gear);

    connect(first, &QToolButton::clicked, this, [this] { emit markerRequested(0); });
    connect(prev, &QToolButton::clicked, this, [this] { emit markerRequested(std::max(strip_->marker() - 1, 0)); });
    connect(next, &QToolButton::clicked, this,
            [this] { emit markerRequested(std::min(strip_->marker() + 1, strip_->count())); });
    connect(last, &QToolButton::clicked, this, [this] { emit markerRequested(strip_->count()); });
    connect(play, &QToolButton::clicked, this, [this] { emit markerRequested(strip_->count()); });
    connect(strip_, &TimelineStrip::markerMoved, this, &TimelineBar::markerRequested);
    connect(strip_, &TimelineStrip::selected, this, &TimelineBar::featureSelected);
    connect(strip_, &TimelineStrip::contextMenuRequested, this, &TimelineBar::contextMenuRequested);
    connect(strip_, &TimelineStrip::activated, this, &TimelineBar::editRequested);
}

void TimelineBar::refresh(const oc::Document& doc) {
    std::vector<TimelineStrip::Item> items;
    items.reserve(doc.features().size());
    for (const auto& f : doc.features())
        items.push_back({f->id, QString::fromStdString(f->name), iconFor(f->type()), f->healthy(), f->suppressed});
    strip_->setItems(std::move(items), static_cast<int>(doc.marker()));
}

int TimelineBar::selectedFeatureId() const { return strip_->selectedId(); }
