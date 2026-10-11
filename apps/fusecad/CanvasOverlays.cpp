// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasOverlays.h"

#include "Commands.h"
#include "Icons.h"
#include "Theme.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr QSize kNavIcon(18, 18);

void addShadow(QWidget* w, int blur = 18) {
    auto* fx = new QGraphicsDropShadowEffect(w);
    fx->setBlurRadius(blur);
    fx->setOffset(0, 2);
    fx->setColor(theme::kShadow);
    w->setGraphicsEffect(fx);
}

} // namespace

// --- NavigationBar ------------------------------------------------------------

NavigationBar::NavigationBar(CommandRegistry& commands, const QStringList& ids, QMenu* displayMenu, QWidget* parent)
    : QFrame(parent) {
    setObjectName("NavBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(2);
    for (const QString& id : ids) {
        if (id == QLatin1String(kDisplayMenu)) {
            auto* display = new QToolButton(this);
            display->setIcon(icons::get("display"));
            display->setIconSize(kNavIcon);
            display->setToolTip(tr("Display settings"));
            display->setMenu(displayMenu);
            display->setPopupMode(QToolButton::InstantPopup);
            layout->addWidget(display);
            continue;
        }
        if (id == "|") {
            auto* sep = new QFrame(this);
            sep->setFixedSize(1, 18);
            sep->setStyleSheet(QStringLiteral("background: %1;").arg(theme::kBorder.name()));
            layout->addWidget(sep);
            continue;
        }
        auto* b = new QToolButton(this);
        b->setDefaultAction(commands.get(id));
        b->setIconSize(kNavIcon);
        layout->addWidget(b);
    }
    addShadow(this);
}

// --- Toast --------------------------------------------------------------------

Toast::Toast(QWidget* parent) : QLabel(parent), timer_(new QTimer(this)) {
    setObjectName("Toast");
    setTextFormat(Qt::RichText);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &QWidget::hide);
    hide();
}

void Toast::showMessage(const QString& html, int ms) {
    setText(html);
    adjustSize();
    if (auto* p = parentWidget()) move((p->width() - width()) / 2, 14);
    show();
    raise();
    timer_->start(ms);
}

// --- MarkingMenu --------------------------------------------------------------

namespace {
constexpr QSize kMarkingSize(600, 640);
constexpr QPoint kMarkingCenter(300, 230); // the cursor, in popup coordinates
constexpr int kRingSlots = 8;
constexpr double kRingRadiusX = 150; // the ring is an ellipse, wider than tall
constexpr double kRingRadiusY = 105;
constexpr int kRingButtonMinWidth = 120;
constexpr int kListWidth = 200;
constexpr QPoint kListOffset(110, 150); // overflow list, from the centre
constexpr int kHubRadius = 14;
constexpr int kHubDotRadius = 3;
constexpr int kHubAlpha = 160;
} // namespace

MarkingMenu::MarkingMenu(CommandRegistry& commands, const QStringList& ring, const QStringList& list, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(kMarkingSize);

    auto trigger = [this](QAction* a) {
        close();
        QTimer::singleShot(0, a, &QAction::trigger);
    };

    // Eight slots clockwise from north, laid out on an ellipse around the cursor.
    for (int i = 0; i < std::min(kRingSlots, static_cast<int>(ring.size())); ++i) {
        if (ring[i].isEmpty()) continue;
        QAction* a = commands.get(ring[i]);
        auto* b = new QToolButton(this);
        b->setObjectName("MarkButton");
        b->setIcon(a->icon());
        b->setText(a->text());
        b->setIconSize(QSize(18, 18));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->adjustSize();
        b->resize(std::max(b->width(), kRingButtonMinWidth), b->height());
        connect(b, &QToolButton::clicked, this, [trigger, a] { trigger(a); });
        addShadow(b, 10);

        const double angle = qDegreesToRadians(i * 360.0 / kRingSlots);
        const double dx = std::sin(angle), dy = -std::cos(angle);
        const QPoint anchor =
            kMarkingCenter + QPoint(static_cast<int>(dx * kRingRadiusX), static_cast<int>(dy * kRingRadiusY));
        QPoint topLeft;
        if (std::abs(dx) < 0.1)
            topLeft = anchor - QPoint(b->width() / 2, b->height() / 2);
        else if (dx > 0)
            topLeft = anchor - QPoint(0, b->height() / 2);
        else
            topLeft = anchor - QPoint(b->width(), b->height() / 2);
        b->move(topLeft);
    }

    auto* frame = new QFrame(this);
    frame->setObjectName("MarkList");
    auto* col = new QVBoxLayout(frame);
    col->setContentsMargins(4, 4, 4, 4);
    col->setSpacing(0);
    for (const QString& id : list) {
        if (id == "-") {
            auto* sep = new QFrame(frame);
            sep->setFixedHeight(1);
            sep->setStyleSheet(QStringLiteral("background: %1; margin: 3px 4px;").arg(theme::kBorder.name()));
            col->addWidget(sep);
            continue;
        }
        QAction* a = commands.get(id);
        auto* b = new QToolButton(frame);
        b->setIcon(a->icon());
        QString text = a->text();
        if (!a->shortcut().isEmpty()) text += QStringLiteral("    ") + a->shortcut().toString(QKeySequence::NativeText);
        b->setText(text);
        b->setIconSize(QSize(16, 16));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QToolButton::clicked, this, [trigger, a] { trigger(a); });
        col->addWidget(b);
    }
    frame->setFixedWidth(kListWidth);
    frame->adjustSize();
    frame->move(kMarkingCenter + kListOffset);
    addShadow(frame);
}

void MarkingMenu::popupAt(const QPoint& globalCenter) {
    move(globalCenter - kMarkingCenter);
    show();
}

void MarkingMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor ink = theme::kInk;
    ink.setAlpha(kHubAlpha);
    p.setPen(QPen(ink, 2));
    p.setBrush(theme::kOverlayFill);
    p.drawEllipse(kMarkingCenter, kHubRadius, kHubRadius);
    p.setBrush(ink);
    p.drawEllipse(kMarkingCenter, kHubDotRadius, kHubDotRadius);
}

// --- Canvas -------------------------------------------------------------------

Canvas::Canvas(QWidget* viewport, QWidget* browser, QWidget* navBar, QWidget* parent)
    : QWidget(parent), viewport_(viewport), browser_(browser), navBar_(navBar) {
    viewport_->setParent(this);
    browser_->setParent(this);
    navBar_->setParent(this);
    addShadow(browser_);

    toast_ = new Toast(this);
    info_ = new QLabel(this);
    info_->setObjectName("InfoLabel");
    info_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layoutOverlays();
}

void Canvas::setInfo(const QString& text) {
    info_->setText(text);
    info_->adjustSize();
    layoutOverlays();
}

void Canvas::resizeEvent(QResizeEvent*) { layoutOverlays(); }

void Canvas::layoutOverlays() {
    viewport_->setGeometry(rect());
    // The browser hugs the top-left corner and stops short of the navigation bar.
    constexpr int kMargin = 10;
    constexpr int kBrowserWidth = 270;
    constexpr int kBrowserMaxHeight = 440;
    constexpr int kNavBarClearance = 50;
    const int browserHeight = std::clamp(height() - 2 * kMargin - kNavBarClearance, 0, kBrowserMaxHeight);
    browser_->setGeometry(kMargin, kMargin, kBrowserWidth, browserHeight);
    navBar_->adjustSize();
    navBar_->move((width() - navBar_->width()) / 2, height() - navBar_->height() - kMargin);
    constexpr int kInfoMargin = 16;
    info_->move(width() - info_->width() - kInfoMargin, height() - info_->height() - kInfoMargin);
    browser_->raise();
    navBar_->raise();
    info_->raise();
    toast_->raise();
}
