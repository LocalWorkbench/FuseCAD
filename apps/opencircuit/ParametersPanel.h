// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace oc {
class ParameterTable;
}
class QTableWidget;

// Editable view of the user parameter table (the "Change Parameters" dialog).
class ParametersPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ParametersPanel(QWidget* parent = nullptr);
    void refresh(const oc::ParameterTable& params);

signals:
    void expressionEdited(const QString& name, const QString& expression);
    void addRequested();
    void removeRequested(const QString& name);

private:
    QTableWidget* table_ = nullptr;
    bool refreshing_ = false;
};
