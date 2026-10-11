// SPDX-License-Identifier: GPL-3.0-or-later
#include "ParametersPanel.h"

#include "fuse/core/Parameters.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

enum Column { Name, Expression, Value, Comment, ColumnCount };

QTableWidgetItem* readOnlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

ParametersPanel::ParametersPanel(QWidget* parent) : QWidget(parent) {
    table_ = new QTableWidget(0, ColumnCount, this);
    table_->setHorizontalHeaderLabels({tr("Name"), tr("Expression"), tr("Value"), tr("Comment")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto* add = new QPushButton(tr("Add"), this);
    auto* remove = new QPushButton(tr("Delete"), this);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(table_);
    layout->addLayout(buttons);

    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (refreshing_ || item->column() != Expression) return;
        emit expressionEdited(table_->item(item->row(), Name)->text(), item->text());
    });
    connect(add, &QPushButton::clicked, this, &ParametersPanel::addRequested);
    connect(remove, &QPushButton::clicked, this, [this] {
        int row = table_->currentRow();
        if (row >= 0) emit removeRequested(table_->item(row, Name)->text());
    });
}

void ParametersPanel::refresh(const fuse::ParameterTable& params) {
    refreshing_ = true;
    const auto& all = params.all();
    table_->setRowCount(static_cast<int>(all.size()));
    int row = 0;
    for (const fuse::Parameter& p : all) {
        table_->setItem(row, Name, readOnlyItem(QString::fromStdString(p.name)));
        table_->setItem(row, Expression, new QTableWidgetItem(QString::fromStdString(p.expression.source())));
        table_->setItem(row, Value, readOnlyItem(QString::fromStdString(fuse::formatValue(p.value, p.unit))));
        table_->setItem(row, Comment, readOnlyItem(QString::fromStdString(p.comment)));
        ++row;
    }
    table_->resizeColumnsToContents();
    refreshing_ = false;
}
