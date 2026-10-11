// SPDX-License-Identifier: GPL-3.0-or-later
#include "Ribbon.h"

#include "Commands.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kButtonIcon = 30;

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setObjectName("RibbonSeparator");
    line->setFixedWidth(1);
    line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return line;
}

} // namespace

Ribbon::Ribbon(CommandRegistry& commands, QWidget* parent) : QWidget(parent), commands_(commands) {
    setObjectName("Ribbon");
    setAttribute(Qt::WA_StyledBackground);

    workspace_ = new QToolButton(this);
    workspace_->setObjectName("Workspace");
    workspace_->setText(tr("DESIGN"));
    workspace_->setPopupMode(QToolButton::InstantPopup);
    workspace_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    workspace_->setMinimumSize(118, 64);

    tabs_ = new QTabBar(this);
    tabs_->setObjectName("RibbonTabs");
    tabs_->setDrawBase(false);
    tabs_->setExpanding(false);
    pages_ = new QStackedWidget(this);
    connect(tabs_, &QTabBar::currentChanged, pages_, &QStackedWidget::setCurrentIndex);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    right->addWidget(tabs_, 0, Qt::AlignLeft);
    right->addWidget(pages_);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(10);
    layout->addWidget(workspace_, 0, Qt::AlignVCenter);
    layout->addLayout(right, 1);
}

void Ribbon::addTab(const QString& title, const std::vector<Group>& groups) {
    auto* page = new QWidget(pages_);
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(6);
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i > 0) row->addWidget(separator(page));
        row->addWidget(buildGroup(groups[i]));
    }
    row->addStretch();
    pages_->addWidget(page);
    tabs_->addTab(title);
}

QWidget* Ribbon::buildGroup(const Group& group) {
    auto* box = new QWidget(this);
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(1);
    for (const QString& id : group.visible) {
        auto* b = new QToolButton(box);
        b->setObjectName("RibbonButton");
        b->setDefaultAction(commands_.get(id));
        b->setIconSize(QSize(kButtonIcon, kButtonIcon));
        b->setAutoRaise(true);
        buttons->addWidget(b);
    }

    auto* menu = new QMenu(box);
    for (const QString& id : group.visible) menu->addAction(commands_.get(id));
    if (!group.overflow.isEmpty()) menu->addSeparator();
    for (const QString& id : group.overflow) {
        if (id == "-") menu->addSeparator();
        else menu->addAction(commands_.get(id));
    }

    auto* label = new QToolButton(box);
    label->setObjectName("GroupLabel");
    label->setText(group.title + QStringLiteral(" ▾"));
    label->setPopupMode(QToolButton::InstantPopup);
    label->setMenu(menu);

    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(buttons);
    column->addWidget(label, 0, Qt::AlignHCenter);
    return box;
}
