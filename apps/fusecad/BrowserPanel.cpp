// SPDX-License-Identifier: GPL-3.0-or-later
#include "BrowserPanel.h"

#include "Icons.h"
#include "fuse/model/Document.h"
#include "fuse/model/Features.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// VisibleRole: the row's own eye state; FolderShownRole: false when the folder holding it is hidden.
enum Role { KindRole = Qt::UserRole, IdRole, CommandRole, VisibleRole, FolderShownRole, IconRole };
constexpr int kIcon = 16;
constexpr int kGap = 4;
constexpr int kRowHeight = 22;
constexpr qreal kInheritedHiddenOpacity = 0.45; // eye of a row whose folder is hidden

// Draws [eye][icon] text, and toggles visibility when the eye is clicked.
class BrowserDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<void(const QModelIndex&)> onEyeClicked;

    static QRect eyeRect(const QRect& row) { return {row.left() + 2, row.center().y() - kIcon / 2, kIcon, kIcon}; }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // Background (hover / selection) from the style sheet.
        opt.text.clear();
        opt.icon = QIcon();
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);

        QRect r = option.rect;
        const QVariant visible = index.data(VisibleRole);
        const bool folderShown = index.data(FolderShownRole).toBool();
        if (visible.isValid()) {
            p->setOpacity(folderShown ? 1.0 : kInheritedHiddenOpacity);
            icons::get(visible.toBool() ? "eye" : "eye-off").paint(p, eyeRect(r));
            p->setOpacity(1.0);
        }
        int x = r.left() + 2 + kIcon + kGap;
        icons::get(index.data(IconRole).toString()).paint(p, QRect(x, r.center().y() - kIcon / 2, kIcon, kIcon));
        x += kIcon + kGap + 2;

        QFont f = option.font;
        const auto kind = static_cast<BrowserPanel::Kind>(index.data(KindRole).toInt());
        if (kind == BrowserPanel::Kind::Document) f.setBold(true);
        p->setFont(f);
        const bool dim = !folderShown || (visible.isValid() && !visible.toBool());
        p->setPen(option.palette.color(dim ? QPalette::Disabled : QPalette::Active, QPalette::Text));
        p->drawText(
            QRect(x, r.top(), r.right() - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
            option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, r.right() - x));
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        return {s.width() + 2 * (kIcon + kGap), std::max(s.height(), kRowHeight)};
    }

protected:
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        if (event->type() == QEvent::MouseButtonRelease && index.data(VisibleRole).isValid()) {
            const auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && eyeRect(option.rect).contains(me->position().toPoint())) {
                if (onEyeClicked) onEyeClicked(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

} // namespace

BrowserPanel::BrowserPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("FloatingPanel");

    auto* header = new QWidget(this);
    header->setObjectName("PanelHeader");
    auto* title = new QLabel(tr("BROWSER"), header);
    title->setObjectName("PanelHeader");
    collapse_ = new QToolButton(header);
    collapse_->setText(QStringLiteral("⊖"));
    collapse_->setToolTip(tr("Collapse browser"));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 4, 4, 2);
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(collapse_);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName("Browser");
    tree_->setHeaderHidden(true);
    tree_->setIndentation(14);
    tree_->setRootIsDecorated(true);
    tree_->setFocusPolicy(Qt::NoFocus);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* delegate = new BrowserDelegate(tree_);
    tree_->setItemDelegate(delegate);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 4);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(tree_);

    delegate->onEyeClicked = [this](const QModelIndex& index) {
        if (QTreeWidgetItem* item = tree_->itemFromIndex(index))
            emit visibilityToggled(info(item), !item->data(0, VisibleRole).toBool());
    };
    connect(tree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item) { emit activated(info(item)); });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (QTreeWidgetItem* item = tree_->itemAt(pos))
            emit contextMenuRequested(info(item), tree_->viewport()->mapToGlobal(pos));
    });
    connect(tree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* i) { expanded_.insert(key(i)); });
    connect(tree_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* i) { expanded_.remove(key(i)); });
    connect(collapse_, &QToolButton::clicked, this, [this] {
        tree_->setVisible(!tree_->isVisible());
        collapse_->setText(tree_->isVisible() ? QStringLiteral("⊖") : QStringLiteral("⊕"));
        adjustSize();
    });
}

BrowserPanel::Item BrowserPanel::info(const QTreeWidgetItem* item) {
    return {static_cast<Kind>(item->data(0, KindRole).toInt()), item->data(0, IdRole).toInt(),
            item->data(0, CommandRole).toString()};
}

QString BrowserPanel::key(const QTreeWidgetItem* item) {
    return QStringLiteral("%1:%2").arg(item->data(0, KindRole).toInt()).arg(item->data(0, IdRole).toInt());
}

QTreeWidgetItem* BrowserPanel::addItem(QTreeWidgetItem* parent, Kind kind, int id, const QString& text,
                                       const QString& icon, std::optional<bool> toggledOn, bool folderShown) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    item->setText(0, text);
    item->setData(0, KindRole, static_cast<int>(kind));
    item->setData(0, IdRole, id);
    item->setData(0, IconRole, icon);
    if (toggledOn) item->setData(0, VisibleRole, *toggledOn);
    item->setData(0, FolderShownRole, folderShown);
    return item;
}

void BrowserPanel::refresh(const QString& documentName, const fuse::Document& doc, const ViewState& view) {
    tree_->clear();
    // The root eye shows and hides the design's contents (bodies and sketches) together.
    auto* root = addItem(nullptr, Kind::Document, 0, documentName, "doc", view.showBodies || view.showSketches);

    addItem(root, Kind::Settings, 0, tr("Document Settings"), "settings");

    auto* views = addItem(root, Kind::NamedViews, 0, tr("Named Views"), "folder");
    const std::pair<QString, const char*> namedViews[] = {
        {tr("Top"), "view.top"}, {tr("Front"), "view.front"}, {tr("Right"), "view.right"}, {tr("Home"), "view.home"}};
    for (const auto& [name, command] : namedViews)
        addItem(views, Kind::NamedView, 0, name, "named-view")->setData(0, CommandRole, QString::fromLatin1(command));

    auto* origin = addItem(root, Kind::Origin, 0, tr("Origin"), "folder", view.showOrigin);
    const std::pair<QString, const char*> originItems[] = {
        {tr("O"), "point"},         {tr("X"), "axis"},          {tr("Y"), "axis"},         {tr("Z"), "axis"},
        {tr("XY"), "origin-plane"}, {tr("XZ"), "origin-plane"}, {tr("YZ"), "origin-plane"}};
    int originId = 0;
    for (const auto& [name, icon] : originItems)
        addItem(origin, Kind::OriginItem, originId++, name, icon, std::nullopt, view.showOrigin);

    auto* bodies = addItem(root, Kind::Bodies, 0, tr("Bodies"), "folder", view.showBodies);
    for (const auto& [id, body] : doc.bodies())
        addItem(bodies, Kind::Body, id, QString::fromStdString(body.name), "body", view.bodyToggledOn(id),
                view.showBodies);

    auto* sketches = addItem(root, Kind::Sketches, 0, tr("Sketches"), "folder", view.showSketches);
    for (size_t i = 0; i < doc.features().size() && i < doc.marker(); ++i) {
        const fuse::Feature& f = *doc.features()[i];
        if (f.type() != fuse::FeatureType::Sketch) continue;
        addItem(sketches, Kind::Sketch, f.id, QString::fromStdString(f.name), f.healthy() ? "sketch" : "warning",
                view.sketchToggledOn(f.id), view.showSketches);
    }

    if (firstRefresh_) {
        expanded_ = {key(root), key(bodies), key(sketches)};
        firstRefresh_ = false;
    }
    std::function<void(QTreeWidgetItem*)> restore = [&](QTreeWidgetItem* item) {
        if (expanded_.contains(key(item))) item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i) restore(item->child(i));
    };
    restore(root);
}
