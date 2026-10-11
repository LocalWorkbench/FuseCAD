// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BrowserPanel.h"
#include "ViewState.h"

#include "oc/kernel/Kernel.h"
#include "oc/model/Document.h"

#include <QMainWindow>

#include <memory>

class Canvas;
class CommandRegistry;
class ParametersPanel;
class QDialog;
class QMenu;
class QTabBar;
class Ribbon;
class TimelineBar;
class Viewport;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    // UI construction.
    void registerCommands();
    QWidget* buildTopBar();
    void buildRibbon();
    void buildMenuBar();
    QMenu* buildDisplayMenu();

    // Document lifecycle.
    void setDocument(oc::Document doc, bool resetHistory);
    void recompute();     // run the timeline and refresh every view
    void refreshViews();  // views only (visibility changes etc.)
    void commit(const QString& label);
    template <class Fn>
    void runCommand(const QString& label, Fn&& fn); // on oc::Error the document is restored

    bool maybeSave();
    bool save();
    bool saveAs();
    void exportBodies(const QString& filter, const QString& suffix);
    void setCurrentFile(const QString& path);
    QString documentName() const;
    void updateUndoActions();
    void notify(const QString& html);

    // Commands.
    void openSample(oc::Document (*make)());
    void showParameters();
    void showMeasure();
    void insertStep();
    void deleteFeature(int id);
    void renameFeature(int id);
    void toggleSuppress(int id);
    void setMarker(int position);

    // Panels.
    void onBrowserVisibility(BrowserPanel::Kind kind, int id, bool visible);
    void onBrowserActivated(BrowserPanel::Kind kind, int id);
    void onBrowserContextMenu(BrowserPanel::Kind kind, int id, const QPoint& pos);
    void onTimelineContextMenu(int featureId, const QPoint& pos);
    void onExpressionEdited(const QString& name, const QString& expression);
    void onAddParameter();
    void onRemoveParameter(const QString& name);
    void showMarkingMenu(const QPoint& globalPos);

    std::unique_ptr<oc::Kernel> kernel_;
    oc::Document doc_;
    std::unique_ptr<oc::UndoStack> undo_;
    QString currentFile_;
    ViewState view_;

    CommandRegistry* commands_ = nullptr;
    Ribbon* ribbon_ = nullptr;
    Viewport* viewport_ = nullptr;
    BrowserPanel* browser_ = nullptr;
    Canvas* canvas_ = nullptr;
    TimelineBar* timeline_ = nullptr;
    QTabBar* docTabs_ = nullptr;
    QDialog* parametersDialog_ = nullptr;
    ParametersPanel* parameters_ = nullptr;
};
