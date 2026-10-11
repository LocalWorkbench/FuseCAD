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

#include <array>
#include <cmath>

namespace {

void addShadow(QWidget* w, int blur = 18) {
    auto* fx = new QGraphicsDropShadowEffect(w);
    fx->setBlurRadius(blur);
    fx->setOffset(0, 2);
    fx->setColor(QColor(0, 0, 0, 45));
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
        if (id == "|") {
            auto* sep = new QFrame(this);
            sep->setFixedSize(1, 18);
            sep->setStyleSheet(QStringLiteral("background: %1;").arg(theme::kBorder.name()));
            layout->addWidget(sep);
            continue;
        }
        auto* b = new QToolButton(this);
        b->setDefaultAction(commands.get(id));
        b->setIconSize(QSize(18, 18));
        layout->addWidget(b);
    }
    auto* display = new QToolButton(this);
    display->setIcon(icons::get("display"));
    display->setIconSize(QSize(18, 18));
    display->setToolTip(tr("Display settings"));
    display->setMenu(displayMenu);
    display->setPopupMode(QToolButton::InstantPopup);
    layout->insertWidget(layout->count() - 2, display);
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
constexpr QPoint kMarkingCenter(300, 230);
} // namespace

MarkingMenu::MarkingMenu(CommandRegistry& commands, const QStringList& ring, const QStringList& list, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), center_(kMarkingCenter) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(kMarkingSize);

    auto trigger = [this](QAction* a) {
        close();
        QTimer::singleShot(0, a, &QAction::trigger);
    };

    // Eight slots clockwise from north, laid out on an ellipse around the cursor.
    for (int i = 0; i < std::min<int>(8, static_cast<int>(ring.size())); ++i) {
        if (ring[i].isEmpty()) continue;
        QAction* a = commands.get(ring[i]);
        auto* b = new QToolButton(this);
        b->setObjectName("MarkButton");
        b->setIcon(a->icon());
        b->setText(a->text());
        b->setIconSize(QSize(18, 18));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->adjustSize();
        b->resize(std::max(b->width(), 120), b->height());
        connect(b, &QToolButton::clicked, this, [trigger, a] { trigger(a); });
        addShadow(b, 10);

        const double angle = i * 45.0 * 3.14159265358979 / 180.0;
        const double dx = std::sin(angle), dy = -std::cos(angle);
        QPoint anchor = center_ + QPoint(int(dx * 150), int(dy * 105));
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
    frame->setFixedWidth(200);
    frame->adjustSize();
    frame->move(center_.x() + 150 - 40, center_.y() + 150);
    addShadow(frame);
}

void MarkingMenu::popupAt(const QPoint& globalCenter) {
    move(globalCenter - center_);
    show();
}

void MarkingMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x3d, 0x44, 0x4f, 160), 2));
    p.setBrush(QColor(255, 255, 255, 200));
    p.drawEllipse(center_, 14, 14);
    p.setBrush(QColor(0x3d, 0x44, 0x4f, 160));
    p.drawEllipse(center_, 3, 3);
}

// --- Canvas -------------------------------------------------------------------

Canvas::Canvas(QWidget* viewport, QWidget* browser, QWidget* navBar, QWidget* parent)
    : QWidget(parent), viewport_(viewport), browser_(browser), navBar_(navBar) {
    viewport_->setParent(this);
    browser_->setParent(this);
    navBar_->setParent(this);
    addShadow(browser_);

    comments_ = new QFrame(this);
    comments_->setObjectName("FloatingPanel");
    auto* cl = new QHBoxLayout(comments_);
    cl->setContentsMargins(10, 5, 6, 5);
    auto* title = new QLabel(tr("COMMENTS"), comments_);
    title->setObjectName("PanelHeader");
    auto* add = new QToolButton(comments_);
    add->setText(QStringLiteral("⊕"));
    add->setAutoRaise(true);
    add->setToolTip(tr("Comments - coming soon"));
    cl->addWidget(title);
    cl->addStretch();
    cl->addWidget(add);
    addShadow(comments_);

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
    constexpr int m = 10;
    const int browserWidth = 270;
    browser_->setGeometry(m, m, browserWidth, std::min(height() - 2 * m - 50, 440));
    comments_->setGeometry(m, height() - m - 32, browserWidth, 32);
    navBar_->adjustSize();
    navBar_->move((width() - navBar_->width()) / 2, height() - navBar_->height() - m);
    info_->move(width() - info_->width() - 16, height() - info_->height() - 16);
    browser_->raise();
    comments_->raise();
    navBar_->raise();
    info_->raise();
    toast_->raise();
}
