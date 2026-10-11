// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BrowserPanel.h"
#include "DocumentController.h"
#include "SceneBuilder.h"
#include "ViewState.h"

#include <QMainWindow>

#include <functional>

class Canvas;
class CommandRegistry;
class ParametersPanel;
class QDialog;
class QTabBar;
class TimelineBar;
class Viewport;

// The application window: wires the document controller to the views and
// hosts the command handlers the workbench (Workbench.h) binds to commands.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Open `path`, or the sample design when it is empty or fails to open.
    void openInitial(const QString& path);
    bool openFile(const QString& path);
    // Image of the whole window. Unlike QWidget::grab(), this composites the
    // OpenGL viewport through its framebuffer so 3D content and overlays are exact.
    QImage snapshot();
    // Calls `fn` once the viewport has presented a frame with no camera animation running.
    void whenViewSettled(std::function<void()> fn);

    DocumentController& controller() { return *controller_; }
    Viewport& viewport() { return *viewport_; }
    const ViewState& viewState() const { return view_; }

    // Command handlers.
    void newDesign();
    void openDesign();
    void openSample(fuse::Document (*make)());
    bool save();
    bool saveAs();
    void exportVisibleBodies(DocumentController::ExportFormat format);
    void addBox();
    void addCylinder();
    void addFillet();
    void addChamfer();
    void deleteSelectedFeature();
    void showParameters();
    void showMeasure();
    void insertStep();
    void setGridVisible(bool visible);
    void setVisualStyle(ViewState::Style style);
    void showAbout();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    // Controller signals.
    void onDocumentReset();
    void onRecomputed(const fuse::RecomputeReport& report, qint64 elapsedMs);
    void onFileStateChanged();
    void updateUndoActions();
    void refreshViews(); // views only (visibility changes etc.)

    bool maybeSave();
    void exportShapes(const std::vector<fuse::ShapePtr>& shapes, const QString& baseName,
                      DocumentController::ExportFormat format);
    void notify(const QString& html);

    void deleteFeature(int id);
    void renameFeature(int id);
    void toggleSuppress(int id);
    void setMarker(int position);

    // Panels.
    void onBrowserVisibility(const BrowserPanel::Item& item, bool visible);
    void onBrowserActivated(const BrowserPanel::Item& item);
    void onBrowserContextMenu(const BrowserPanel::Item& item, const QPoint& pos);
    void onTimelineContextMenu(int featureId, const QPoint& pos);
    void onExpressionEdited(const QString& name, const QString& expression);
    void onAddParameter();
    void onRemoveParameter(const QString& name);
    void showMarkingMenu(const QPoint& globalPos);

    DocumentController* controller_ = nullptr;
    scene::MeshCache meshes_;
    ViewState view_;

    CommandRegistry* commands_ = nullptr;
    Viewport* viewport_ = nullptr;
    BrowserPanel* browser_ = nullptr;
    Canvas* canvas_ = nullptr;
    TimelineBar* timeline_ = nullptr;
    QTabBar* docTabs_ = nullptr;
    QDialog* parametersDialog_ = nullptr;
    ParametersPanel* parameters_ = nullptr;
};
