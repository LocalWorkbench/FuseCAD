// SPDX-License-Identifier: GPL-3.0-or-later
#include "FeatureDialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace {

enum EdgeChoice { AllEdges, VerticalEdges, EdgesAlongX, EdgesAlongY };

} // namespace

ExpressionDialog::ExpressionDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    auto* hint = new QLabel(tr("Values are expressions: <i>25 mm</i>, <i>1 in</i>, <i>width / 2</i>"), this);
    hint->setTextFormat(Qt::RichText);
    form_ = new QFormLayout;
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(form_);
    layout->addWidget(buttons);
}

QLineEdit* ExpressionDialog::addField(const QString& label, const QString& initial) {
    auto* edit = new QLineEdit(initial, this);
    edit->setMinimumWidth(220);
    form_->addRow(label, edit);
    return edit;
}

QComboBox* ExpressionDialog::addOperationField() {
    auto* combo = new QComboBox(this);
    combo->addItem(tr("New Body"), static_cast<int>(fuse::FeatureOperation::NewBody));
    combo->addItem(tr("Join"), static_cast<int>(fuse::FeatureOperation::Join));
    combo->addItem(tr("Cut"), static_cast<int>(fuse::FeatureOperation::Cut));
    combo->addItem(tr("Intersect"), static_cast<int>(fuse::FeatureOperation::Intersect));
    form_->addRow(tr("Operation"), combo);
    return combo;
}

QComboBox* ExpressionDialog::addEdgeField() {
    auto* combo = new QComboBox(this);
    combo->addItem(tr("All edges"), AllEdges);
    combo->addItem(tr("Vertical edges (Z)"), VerticalEdges);
    combo->addItem(tr("Edges along X"), EdgesAlongX);
    combo->addItem(tr("Edges along Y"), EdgesAlongY);
    form_->addRow(tr("Edges"), combo);
    return combo;
}

std::string ExpressionDialog::text(const QLineEdit* edit) { return edit->text().trimmed().toStdString(); }

fuse::BodyOperation ExpressionDialog::operation(const QComboBox* combo) {
    fuse::BodyOperation op;
    op.operation = static_cast<fuse::FeatureOperation>(combo->currentData().toInt());
    return op;
}

fuse::EdgeQuery ExpressionDialog::edges(const QComboBox* combo) {
    fuse::EdgeQuery q;
    switch (combo->currentData().toInt()) {
    case VerticalEdges:
        q.mode = fuse::EdgeQuery::Mode::ParallelTo;
        q.direction = {0, 0, 1};
        break;
    case EdgesAlongX:
        q.mode = fuse::EdgeQuery::Mode::ParallelTo;
        q.direction = {1, 0, 0};
        break;
    case EdgesAlongY:
        q.mode = fuse::EdgeQuery::Mode::ParallelTo;
        q.direction = {0, 1, 0};
        break;
    default:
        q.mode = fuse::EdgeQuery::Mode::All;
        break;
    }
    return q;
}

BoxDialog::BoxDialog(QWidget* parent) : ExpressionDialog(tr("Box"), parent) {
    width_ = addField(tr("Width (X)"), "40 mm");
    depth_ = addField(tr("Depth (Y)"), "30 mm");
    height_ = addField(tr("Height (Z)"), "20 mm");
    x_ = addField(tr("Centre X"), "0 mm");
    y_ = addField(tr("Centre Y"), "0 mm");
    z_ = addField(tr("Base Z"), "0 mm");
    op_ = addOperationField();
}

fuse::build::BoxSpec BoxDialog::spec() const {
    fuse::build::BoxSpec s;
    s.width = text(width_);
    s.depth = text(depth_);
    s.height = text(height_);
    s.at = {text(x_), text(y_), text(z_)};
    s.op = operation(op_);
    return s;
}

CylinderDialog::CylinderDialog(QWidget* parent) : ExpressionDialog(tr("Cylinder"), parent) {
    diameter_ = addField(tr("Diameter"), "20 mm");
    height_ = addField(tr("Height (Z)"), "20 mm");
    x_ = addField(tr("Centre X"), "0 mm");
    y_ = addField(tr("Centre Y"), "0 mm");
    z_ = addField(tr("Base Z"), "0 mm");
    op_ = addOperationField();
}

fuse::build::CylinderSpec CylinderDialog::spec() const {
    fuse::build::CylinderSpec s;
    s.diameter = text(diameter_);
    s.height = text(height_);
    s.at = {text(x_), text(y_), text(z_)};
    s.op = operation(op_);
    return s;
}

EdgeFeatureDialog::EdgeFeatureDialog(const QString& title, const QString& sizeLabel, QWidget* parent)
    : ExpressionDialog(title, parent) {
    size_ = addField(sizeLabel, "2 mm");
    edges_ = addEdgeField();
}
