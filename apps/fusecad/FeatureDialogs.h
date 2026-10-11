// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/model/Builders.h"

#include <QDialog>

class QComboBox;
class QFormLayout;
class QLineEdit;

// A form of labelled expression fields. Fields accept anything the expression
// language does, including parameter names ("width / 2").
class ExpressionDialog : public QDialog {
    Q_OBJECT

public:
    ExpressionDialog(const QString& title, QWidget* parent);

protected:
    QLineEdit* addField(const QString& label, const QString& initial);
    QComboBox* addOperationField();
    QComboBox* addEdgeField();
    static std::string text(const QLineEdit* edit);
    static fuse::BodyOperation operation(const QComboBox* combo);
    static fuse::EdgeQuery edges(const QComboBox* combo);

private:
    QFormLayout* form_ = nullptr;
};

class BoxDialog final : public ExpressionDialog {
    Q_OBJECT
public:
    explicit BoxDialog(QWidget* parent);
    fuse::build::BoxSpec spec() const;

private:
    QLineEdit *width_, *depth_, *height_, *x_, *y_, *z_;
    QComboBox* op_;
};

class CylinderDialog final : public ExpressionDialog {
    Q_OBJECT
public:
    explicit CylinderDialog(QWidget* parent);
    fuse::build::CylinderSpec spec() const;

private:
    QLineEdit *diameter_, *height_, *x_, *y_, *z_;
    QComboBox* op_;
};

// Fillet or chamfer: one size plus an edge selection.
class EdgeFeatureDialog final : public ExpressionDialog {
    Q_OBJECT
public:
    EdgeFeatureDialog(const QString& title, const QString& sizeLabel, QWidget* parent);
    std::string sizeExpression() const { return text(size_); }
    fuse::EdgeQuery edgeQuery() const { return edges(edges_); }

private:
    QLineEdit* size_;
    QComboBox* edges_;
};
