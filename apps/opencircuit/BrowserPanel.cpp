// SPDX-License-Identifier: GPL-3.0-or-later
#include "BrowserPanel.h"

#include "Icons.h"

#include "oc/model/Document.h"
#include "oc/model/Features.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum Role { KindRole = Qt::UserRole, IdRole, VisibleRole, IconRole };
constexpr int kIcon = 16;
constexpr int kGap = 4;

// Draws [eye][icon] text, and toggles visibility when the eye is clicked.
class BrowserDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<void(const QModelIndex&)> onEyeClicked;

    static QRect eyeRect(const QRect& row) {
        return {row.left() + 2, row.center().y() - kIcon / 2, kIcon, kIcon};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // Background (hover / selection) from the style sheet.
        opt.text.clear();
        opt.icon = QIcon();
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);

        QRect r = option.rect;
        const QVariant visible = index.data(VisibleRole);
        if (visible.isValid()) icons::get(visible.toBool() ? "eye" : "eye-off").paint(p, eyeRect(r));
        int x = r.left() + 2 + kIcon + kGap;
        icons::get(index.data(IconRole).toString()).paint(p, QRect(x, r.center().y() - kIcon / 2, kIcon, kIcon));
        x += kIcon + kGap + 2;

        QFont f = option.font;
        const auto kind = static_cast<BrowserPanel::Kind>(index.data(KindRole).toInt());
        if (kind == BrowserPanel::Kind::Document) f.setBold(true);
        p->setFont(f);
        const bool dim = visible.isValid() && !visible.toBool();
        p->setPen(option.palette.color(dim ? QPalette::Disabled : QPalette::Active, QPalette::Text));
        p->drawText(QRect(x, r.top(), r.right() - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                    option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                  r.right() - x));
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        return {s.width() + 2 * (kIcon + kGap), std::max(s.height(), 22)};
    }

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
    auto* title = new QLabel(tr("◂◂  BROWSER"), header);
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

    auto itemInfo = [](const QTreeWidgetItem* item) {
        return std::pair{static_cast<Kind>(item->data(0, KindRole).toInt()), item->data(0, IdRole).toInt()};
    };
    delegate->onEyeClicked = [this, itemInfo](const QModelIndex& index) {
        QTreeWidgetItem* item = tree_->itemFromIndex(index);
        if (!item) return;
        auto [kind, id] = itemInfo(item);
        emit visibilityToggled(kind, id, !item->data(0, VisibleRole).toBool());
    };
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this, itemInfo](QTreeWidgetItem* item) {
        auto [kind, id] = itemInfo(item);
        emit activated(kind, id);
    });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, [this, itemInfo](const QPoint& pos) {
        if (QTreeWidgetItem* item = tree_->itemAt(pos)) {
            auto [kind, id] = itemInfo(item);
            emit contextMenuRequested(kind, id, tree_->viewport()->mapToGlobal(pos));
        }
    });
    connect(tree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* i) { expanded_.insert(key(i)); });
    connect(tree_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* i) { expanded_.remove(key(i)); });
    connect(collapse_, &QToolButton::clicked, this, [this] {
        tree_->setVisible(!tree_->isVisible());
        collapse_->setText(tree_->isVisible() ? QStringLiteral("⊖") : QStringLiteral("⊕"));
        adjustSize();
    });
}

QString BrowserPanel::key(const QTreeWidgetItem* item) const {
    return QStringLiteral("%1:%2").arg(item->data(0, KindRole).toInt()).arg(item->data(0, IdRole).toInt());
}

QTreeWidgetItem* BrowserPanel::addItem(QTreeWidgetItem* parent, Kind kind, int id, const QString& text,
                                       const QString& icon, int visible) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    item->setText(0, text);
    item->setData(0, KindRole, static_cast<int>(kind));
    item->setData(0, IdRole, id);
    item->setData(0, IconRole, icon);
    if (visible >= 0) item->setData(0, VisibleRole, visible == 1);
    return item;
}

void BrowserPanel::refresh(const QString& documentName, const oc::Document& doc, const ViewState& view) {
    tree_->clear();
    auto* root = addItem(nullptr, Kind::Document, 0, documentName, "doc", 1);

    auto* settings = addItem(root, Kind::Settings, 0, tr("Document Settings"), "settings");
    addItem(settings, Kind::Units, 0, tr("Units: mm"), "measure");

    auto* views = addItem(root, Kind::NamedViews, 0, tr("Named Views"), "folder");
    const QStringList viewNames{tr("Top"), tr("Front"), tr("Right"), tr("Home")};
    for (int i = 0; i < viewNames.size(); ++i) addItem(views, Kind::NamedView, i, viewNames[i], "named-view");

    auto* origin = addItem(root, Kind::Origin, 0, tr("Origin"), "folder", view.showOrigin ? 1 : 0);
    addItem(origin, Kind::OriginItem, 0, tr("O"), "point");
    addItem(origin, Kind::OriginItem, 1, tr("X"), "axis");
    addItem(origin, Kind::OriginItem, 2, tr("Y"), "axis");
    addItem(origin, Kind::OriginItem, 3, tr("Z"), "axis");
    addItem(origin, Kind::OriginItem, 4, tr("XY"), "origin-plane");
    addItem(origin, Kind::OriginItem, 5, tr("XZ"), "origin-plane");
    addItem(origin, Kind::OriginItem, 6, tr("YZ"), "origin-plane");

    auto* bodies = addItem(root, Kind::Bodies, 0, tr("Bodies"), "folder", view.showBodies ? 1 : 0);
    for (const auto& [id, body] : doc.bodies())
        addItem(bodies, Kind::Body, id, QString::fromStdString(body.name), "body", view.bodyVisible(id) ? 1 : 0);

    auto* sketches = addItem(root, Kind::Sketches, 0, tr("Sketches"), "folder", view.showSketches ? 1 : 0);
    for (size_t i = 0; i < doc.features().size() && i < doc.marker(); ++i) {
        const oc::Feature& f = *doc.features()[i];
        if (f.type() != oc::FeatureType::Sketch) continue;
        addItem(sketches, Kind::Sketch, f.id, QString::fromStdString(f.name),
                f.healthy() ? "sketch" : "warning", view.sketchVisible(f.id) ? 1 : 0);
    }
    addItem(root, Kind::Construction, 0, tr("Construction"), "folder");

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
