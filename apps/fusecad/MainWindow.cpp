// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "CanvasOverlays.h"
#include "Commands.h"
#include "FeatureDialogs.h"
#include "Icons.h"
#include "ParametersPanel.h"
#include "Ribbon.h"
#include "TimelineBar.h"
#include "Viewport.h"
#include "Workbench.h"
#include "fuse/model/Builders.h"
#include "fuse/model/Features.h"
#include "fuse/model/Samples.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace {

const char* const kDocFilter = QT_TRANSLATE_NOOP("MainWindow", "FuseCAD documents (*.fcad)");

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), controller_(new DocumentController(this)), commands_(new CommandRegistry(this)) {
    viewport_ = new Viewport;
    browser_ = new BrowserPanel;
    Workbench::registerCommands(*commands_, *this);
    timeline_ = new TimelineBar(*commands_, this);

    auto* nav = new NavigationBar(*commands_, Workbench::navigationBar(), Workbench::buildDisplayMenu(*commands_, this),
                                  nullptr);
    canvas_ = new Canvas(viewport_, browser_, nav, this);
    connect(commands_, &CommandRegistry::notImplemented, this, [this](const QString& text) {
        notify(tr("<b>%1</b> is coming soon &mdash; FuseCAD is under active development.").arg(text));
    });

    auto* central = new QWidget(this);
    central->setObjectName("Central");
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(Workbench::buildTopBar(*commands_, docTabs_, this));
    layout->addWidget(Workbench::buildRibbon(*commands_, this));
    layout->addWidget(canvas_, 1);
    layout->addWidget(timeline_);
    setCentralWidget(central);
    Workbench::buildMenuBar(*commands_, *menuBar());

    connect(controller_, &DocumentController::documentReset, this, &MainWindow::onDocumentReset);
    connect(controller_, &DocumentController::recomputed, this, &MainWindow::onRecomputed);
    connect(controller_, &DocumentController::historyChanged, this, &MainWindow::updateUndoActions);
    connect(controller_, &DocumentController::fileStateChanged, this, &MainWindow::onFileStateChanged);
    connect(controller_, &DocumentController::failed, this,
            [this](const QString& title, const QString& message, DocumentController::Severity severity) {
                if (severity == DocumentController::Severity::Critical)
                    QMessageBox::critical(this, title, message);
                else
                    QMessageBox::warning(this, title, message);
            });

    connect(viewport_, &Viewport::contextMenuRequested, this, &MainWindow::showMarkingMenu);
    connect(browser_, &BrowserPanel::visibilityToggled, this, &MainWindow::onBrowserVisibility);
    connect(browser_, &BrowserPanel::activated, this, &MainWindow::onBrowserActivated);
    connect(browser_, &BrowserPanel::contextMenuRequested, this, &MainWindow::onBrowserContextMenu);
    connect(timeline_, &TimelineBar::markerRequested, this, &MainWindow::setMarker);
    connect(timeline_, &TimelineBar::contextMenuRequested, this, &MainWindow::onTimelineContextMenu);
    connect(timeline_, &TimelineBar::editRequested, commands_->get("edit.feature"), &QAction::trigger);

    resize(1500, 940);
    onFileStateChanged();
    updateUndoActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::openInitial(const QString& path) {
    if (path.isEmpty() || !openFile(path)) {
        controller_->setDocument(fuse::samples::mountingPlate());
        viewport_->homeView();
    }
}

QImage MainWindow::snapshot() {
    QImage image = grab().toImage();
    QPainter p(&image);
    const qreal dpr = image.devicePixelRatio();
    const QPoint origin = viewport_->mapTo(this, QPoint(0, 0));
    QImage gl = viewport_->grabFramebuffer();
    gl.setDevicePixelRatio(dpr);
    p.drawImage(origin, gl);
    // Panels floating over the canvas.
    for (QWidget* child : canvas_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        if (child == viewport_ || !child->isVisible()) continue;
        p.drawPixmap(child->mapTo(this, QPoint(0, 0)), child->grab());
    }
    return image;
}

void MainWindow::whenViewSettled(std::function<void()> fn) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(viewport_, &Viewport::frameSwapped, this, [this, connection, fn = std::move(fn)] {
        if (viewport_->isAnimating()) return;
        disconnect(*connection);
        // Not from inside the frame-swap notification: the callback may render.
        QTimer::singleShot(0, this, fn);
    });
    viewport_->update();
}

// ---------------------------------------------------------------------------
// Controller signals

void MainWindow::onDocumentReset() {
    view_.hiddenBodies.clear();
    view_.visibleSketches.clear();
}

void MainWindow::onRecomputed(const fuse::RecomputeReport& report, qint64 elapsedMs) {
    QElapsedTimer timer;
    timer.start();
    std::vector<std::string> problems = report.messages;
    viewport_->setBodies(meshes_.update(controller_->kernel(), controller_->document(), problems));
    refreshViews();

    if (!problems.empty())
        notify(tr("<b>%1 problem(s)</b>: %2")
                   .arg(problems.size())
                   .arg(QString::fromStdString(problems.front()).toHtmlEscaped()));
    const fuse::Document& doc = controller_->document();
    const int bodies = static_cast<int>(doc.bodies().size());
    const int features = static_cast<int>(doc.features().size());
    canvas_->setInfo(tr("%1  |  %2  |  %3 ms")
                         .arg(bodies == 1 ? tr("1 body") : tr("%1 bodies").arg(bodies))
                         .arg(features == 1 ? tr("1 feature") : tr("%1 features").arg(features))
                         .arg(elapsedMs + timer.elapsed()));
}

void MainWindow::onFileStateChanged() {
    const QString name = controller_->documentName();
    setWindowModified(controller_->isModified());
    setWindowTitle(tr("%1[*] - FuseCAD").arg(name));
    docTabs_->setTabText(0, name);
    browser_->refresh(name, controller_->document(), view_);
}

void MainWindow::refreshViews() {
    const fuse::Document& doc = controller_->document();
    viewport_->setSketchCurves(scene::visibleSketchPolylines(doc, view_));
    viewport_->setViewState(view_);
    browser_->refresh(controller_->documentName(), doc, view_);
    timeline_->refresh(doc);
    if (parameters_) parameters_->refresh(doc.parameters());
}

void MainWindow::updateUndoActions() {
    QAction* undo = commands_->get("edit.undo");
    QAction* redo = commands_->get("edit.redo");
    undo->setEnabled(controller_->canUndo());
    redo->setEnabled(controller_->canRedo());
    undo->setText(controller_->canUndo() ? tr("Undo %1").arg(controller_->undoLabel()) : tr("Undo"));
    redo->setText(controller_->canRedo() ? tr("Redo %1").arg(controller_->redoLabel()) : tr("Redo"));
}

void MainWindow::notify(const QString& html) { canvas_->toast(html); }

// ---------------------------------------------------------------------------
// Files

bool MainWindow::openFile(const QString& path) {
    if (!controller_->open(path)) return false;
    viewport_->homeView();
    return true;
}

void MainWindow::newDesign() {
    if (!maybeSave()) return;
    controller_->setDocument(fuse::Document{});
}

void MainWindow::openDesign() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Design"), {}, tr(kDocFilter));
    if (!path.isEmpty()) openFile(path);
}

void MainWindow::openSample(fuse::Document (*make)()) {
    if (!maybeSave()) return;
    controller_->setDocument(make());
    viewport_->homeView();
}

bool MainWindow::maybeSave() {
    if (!controller_->isModified()) return true;
    const auto choice = QMessageBox::warning(this, tr("Unsaved Changes"), tr("Save changes to the current design?"),
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Save) return save();
    return choice == QMessageBox::Discard;
}

bool MainWindow::save() {
    if (controller_->filePath().isEmpty()) return saveAs();
    if (!controller_->save()) return false;
    notify(tr("Saved <b>%1</b>").arg(QFileInfo(controller_->filePath()).fileName()));
    return true;
}

bool MainWindow::saveAs() {
    QString path =
        QFileDialog::getSaveFileName(this, tr("Save Design"), controller_->documentName() + ".fcad", tr(kDocFilter));
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) path += ".fcad";
    if (!controller_->saveAs(path)) return false;
    notify(tr("Saved <b>%1</b>").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::exportVisibleBodies(DocumentController::ExportFormat format) {
    std::vector<fuse::ShapePtr> shapes;
    for (const auto& [id, body] : controller_->document().bodies())
        if (view_.bodyVisible(id)) shapes.push_back(body.shape);
    if (shapes.empty()) {
        notify(tr("There are no visible bodies to export."));
        return;
    }
    exportShapes(shapes, controller_->documentName(), format);
}

void MainWindow::exportShapes(const std::vector<fuse::ShapePtr>& shapes, const QString& baseName,
                              DocumentController::ExportFormat format) {
    const bool stl = format == DocumentController::ExportFormat::Stl;
    const QString filter = stl ? tr("STL files (*.stl)") : tr("STEP files (*.step *.stp)");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), baseName + (stl ? ".stl" : ".step"), filter);
    if (path.isEmpty()) return;
    if (controller_->exportShapes(shapes, path, format))
        notify(tr("Exported <b>%1</b>").arg(QFileInfo(path).fileName()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

// ---------------------------------------------------------------------------
// Commands with UI

void MainWindow::addBox() {
    BoxDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    controller_->runCommand(tr("Box"), [spec = dlg.spec()](fuse::Document& doc) { fuse::build::addBox(doc, spec); });
}

void MainWindow::addCylinder() {
    CylinderDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    controller_->runCommand(tr("Cylinder"),
                            [spec = dlg.spec()](fuse::Document& doc) { fuse::build::addCylinder(doc, spec); });
}

void MainWindow::addFillet() {
    EdgeFeatureDialog dlg(tr("Fillet"), tr("Radius"), this);
    if (dlg.exec() != QDialog::Accepted) return;
    controller_->runCommand(
        tr("Fillet"), [&](fuse::Document& doc) { fuse::build::addFillet(doc, dlg.edgeQuery(), dlg.sizeExpression()); });
}

void MainWindow::addChamfer() {
    EdgeFeatureDialog dlg(tr("Chamfer"), tr("Distance"), this);
    if (dlg.exec() != QDialog::Accepted) return;
    controller_->runCommand(tr("Chamfer"), [&](fuse::Document& doc) {
        fuse::build::addChamfer(doc, dlg.edgeQuery(), dlg.sizeExpression());
    });
}

void MainWindow::deleteSelectedFeature() {
    const int id = timeline_->selectedFeatureId();
    if (id < 0)
        notify(tr("Select a feature in the timeline to delete it"));
    else
        deleteFeature(id);
}

void MainWindow::showParameters() {
    if (!parametersDialog_) {
        parametersDialog_ = new QDialog(this);
        parametersDialog_->setWindowTitle(tr("Parameters"));
        parametersDialog_->resize(640, 360);
        parameters_ = new ParametersPanel(parametersDialog_);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, parametersDialog_);
        connect(buttons, &QDialogButtonBox::rejected, parametersDialog_, &QDialog::hide);
        auto* layout = new QVBoxLayout(parametersDialog_);
        layout->addWidget(parameters_);
        layout->addWidget(buttons);
        // Queued: the edit refreshes (rebuilds) the table, which must not happen
        // while the table is still committing the edited item.
        connect(parameters_, &ParametersPanel::expressionEdited, this, &MainWindow::onExpressionEdited,
                Qt::QueuedConnection);
        connect(parameters_, &ParametersPanel::addRequested, this, &MainWindow::onAddParameter);
        connect(parameters_, &ParametersPanel::removeRequested, this, &MainWindow::onRemoveParameter);
    }
    parameters_->refresh(controller_->document().parameters());
    parametersDialog_->show();
    parametersDialog_->raise();
}

void MainWindow::showMeasure() {
    const fuse::Document& doc = controller_->document();
    if (doc.bodies().empty()) {
        notify(tr("There are no bodies to measure."));
        return;
    }
    fuse::Kernel& kernel = controller_->kernel();
    const QLocale loc;
    QString html = QStringLiteral("<table cellspacing='6'><tr><th align='left'>%1</th><th>%2</th><th>%3</th>"
                                  "<th>%4</th></tr>")
                       .arg(tr("Body"), tr("Volume (mm³)"), tr("Area (mm²)"), tr("Size (mm)"));
    for (const auto& [id, body] : doc.bodies()) {
        const fuse::BoundingBox bb = kernel.boundingBox(body.shape);
        html +=
            QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td align='right'>%3</td>"
                           "<td>%4 × %5 × %6</td></tr>")
                .arg(QString::fromStdString(body.name), loc.toString(kernel.volume(body.shape), 'f', 2),
                     loc.toString(kernel.surfaceArea(body.shape), 'f', 2), loc.toString(bb.max.x - bb.min.x, 'f', 2),
                     loc.toString(bb.max.y - bb.min.y, 'f', 2), loc.toString(bb.max.z - bb.min.z, 'f', 2));
    }
    html += QStringLiteral("</table>");
    QMessageBox box(QMessageBox::NoIcon, tr("Measure"), html, QMessageBox::Close, this);
    box.setIconPixmap(icons::get("measure").pixmap(48, 48));
    box.exec();
}

void MainWindow::insertStep() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Insert STEP File"), {}, tr("STEP files (*.step *.stp)"));
    if (path.isEmpty()) return;
    int featureId = -1;
    const bool committed =
        controller_->runCommand(tr("Insert %1").arg(QFileInfo(path).fileName()), [&](fuse::Document& doc) {
            auto& f = doc.add<fuse::ImportStepFeature>();
            f.path = path.toStdString();
            f.name = QFileInfo(path).completeBaseName().toStdString();
            featureId = f.id;
        });
    // The import itself runs during recompute; only frame the result if it succeeded.
    const fuse::Feature* f = controller_->document().findFeature(featureId);
    if (committed && f && f->healthy()) viewport_->fitAll();
}

void MainWindow::setGridVisible(bool visible) {
    view_.showGrid = visible;
    refreshViews();
}

void MainWindow::setVisualStyle(ViewState::Style style) {
    view_.style = style;
    refreshViews();
}

void MainWindow::showAbout() {
    QMessageBox::about(
        this, tr("About FuseCAD"),
        tr("<b>FuseCAD %1</b><p>Free and open-source parametric CAD.</p>"
           "<p>Geometry kernel: %2</p><p>Licensed under the GNU GPL v3 or later.</p>")
            .arg(QApplication::applicationVersion(), QString::fromStdString(controller_->kernel().name())));
}

void MainWindow::deleteFeature(int id) {
    const fuse::Feature* f = controller_->document().findFeature(id);
    if (!f) return;
    controller_->runCommand(tr("Delete %1").arg(QString::fromStdString(f->name)),
                            [id](fuse::Document& doc) { doc.removeFeature(id); });
}

void MainWindow::renameFeature(int id) {
    const fuse::Feature* f = controller_->document().findFeature(id);
    if (!f) return;
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename"), tr("Name:"), QLineEdit::Normal, QString::fromStdString(f->name), &ok)
            .trimmed();
    if (!ok || name.isEmpty()) return;
    controller_->runCommand(tr("Rename %1").arg(name),
                            [id, name = name.toStdString()](fuse::Document& doc) { doc.findFeature(id)->name = name; });
}

void MainWindow::toggleSuppress(int id) {
    const fuse::Feature* f = controller_->document().findFeature(id);
    if (!f) return;
    const QString label =
        (f->suppressed ? tr("Unsuppress %1") : tr("Suppress %1")).arg(QString::fromStdString(f->name));
    controller_->runCommand(label, [id](fuse::Document& doc) {
        fuse::Feature* feature = doc.findFeature(id);
        feature->suppressed = !feature->suppressed;
    });
}

void MainWindow::setMarker(int position) {
    if (position < 0 || static_cast<size_t>(position) == controller_->document().marker()) return;
    controller_->runCommand(tr("Move History Marker"),
                            [position](fuse::Document& doc) { doc.setMarker(static_cast<size_t>(position)); });
}

// ---------------------------------------------------------------------------
// Panels

void MainWindow::onBrowserVisibility(const BrowserPanel::Item& item, bool visible) {
    using K = BrowserPanel::Kind;
    switch (item.kind) {
    case K::Document:
        view_.showBodies = view_.showSketches = visible;
        break;
    case K::Origin:
        view_.showOrigin = visible;
        break;
    case K::Bodies:
        view_.showBodies = visible;
        break;
    case K::Body:
        if (visible)
            view_.hiddenBodies.remove(item.id);
        else
            view_.hiddenBodies.insert(item.id);
        break;
    case K::Sketches:
        view_.showSketches = visible;
        break;
    case K::Sketch:
        if (visible)
            view_.visibleSketches.insert(item.id);
        else
            view_.visibleSketches.remove(item.id);
        break;
    default:
        return;
    }
    refreshViews();
}

void MainWindow::onBrowserActivated(const BrowserPanel::Item& item) {
    if (!item.command.isEmpty()) commands_->get(item.command)->trigger();
}

void MainWindow::onBrowserContextMenu(const BrowserPanel::Item& item, const QPoint& pos) {
    using K = BrowserPanel::Kind;
    auto addToggle = [this, &item](QMenu& menu, bool toggledOn) {
        menu.addAction(icons::get(toggledOn ? "eye-off" : "eye"), toggledOn ? tr("Hide") : tr("Show"), this,
                       [this, item, toggledOn] { onBrowserVisibility(item, !toggledOn); });
    };
    QMenu menu(this);
    if (item.kind == K::Body) {
        addToggle(menu, view_.bodyToggledOn(item.id));
        menu.addAction(commands_->get("inspect.measure"));
        menu.addAction(commands_->get("modify.appearance"));
        menu.addSeparator();
        menu.addAction(icons::get("export"), tr("Export STEP..."), this, [this, id = item.id] {
            const auto& bodies = controller_->document().bodies();
            if (auto it = bodies.find(id); it != bodies.end())
                exportShapes({it->second.shape}, QString::fromStdString(it->second.name),
                             DocumentController::ExportFormat::Step);
        });
    } else if (item.kind == K::Sketch) {
        addToggle(menu, view_.sketchToggledOn(item.id));
        menu.addAction(commands_->get("edit.sketch"));
        menu.addSeparator();
        menu.addAction(icons::get("delete"), tr("Delete"), this, [this, id = item.id] { deleteFeature(id); });
    } else if (item.kind == K::Document || item.kind == K::Settings) {
        menu.addAction(commands_->get("modify.parameters"));
        menu.addAction(commands_->get("file.save"));
    } else if (item.kind == K::NamedView) {
        menu.addAction(tr("Go to View"), this, [this, item] { onBrowserActivated(item); });
    } else {
        return;
    }
    menu.exec(pos);
}

void MainWindow::onTimelineContextMenu(int featureId, const QPoint& pos) {
    const fuse::Document& doc = controller_->document();
    const fuse::Feature* f = doc.findFeature(featureId);
    if (!f) return;
    int index = 0;
    while (doc.features()[static_cast<size_t>(index)]->id != featureId) ++index;

    QMenu menu(this);
    menu.addAction(commands_->get("edit.feature"));
    menu.addAction(tr("Rename..."), this, [this, featureId] { renameFeature(featureId); });
    menu.addAction(f->suppressed ? tr("Unsuppress Feature") : tr("Suppress Feature"), this,
                   [this, featureId] { toggleSuppress(featureId); });
    menu.addSeparator();
    menu.addAction(tr("Roll History Marker Here"), this, [this, index] { setMarker(index + 1); });
    menu.addAction(tr("Roll History Marker to End"), this,
                   [this] { setMarker(static_cast<int>(controller_->document().features().size())); });
    menu.addSeparator();
    menu.addAction(icons::get("delete"), tr("Delete"), this, [this, featureId] { deleteFeature(featureId); });
    if (!f->healthy()) {
        menu.addSeparator();
        QAction* err = menu.addAction(icons::get("warning"), QString::fromStdString(f->error));
        err->setEnabled(false);
    }
    menu.exec(pos);
}

void MainWindow::showMarkingMenu(const QPoint& globalPos) {
    QString repeat = "create.box";
    if (QAction* last = commands_->lastCommand()) repeat = last->objectName();
    auto* menu = new MarkingMenu(*commands_, Workbench::markingMenuRing(repeat), Workbench::markingMenuList(), this);
    menu->popupAt(globalPos);
}

void MainWindow::onExpressionEdited(const QString& name, const QString& expression) {
    const bool committed = controller_->runCommand(
        tr("Change %1").arg(name),
        [name = name.toStdString(), expression = expression.toStdString()](fuse::Document& doc) {
            doc.parameters().setExpression(name, expression);
        },
        tr("Invalid Expression"));
    if (!committed) parameters_->refresh(controller_->document().parameters());
}

void MainWindow::onAddParameter() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(parametersDialog_, tr("New Parameter"), tr("Name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.isEmpty()) return;
    const QString expr = QInputDialog::getText(parametersDialog_, tr("New Parameter"), tr("Expression:"),
                                               QLineEdit::Normal, "10 mm", &ok);
    if (!ok) return;
    controller_->runCommand(tr("Add %1").arg(name), [name = name.toStdString(), expr = expr.toStdString()](
                                                        fuse::Document& doc) { doc.parameters().add(name, expr); });
}

void MainWindow::onRemoveParameter(const QString& name) {
    controller_->runCommand(tr("Delete %1").arg(name),
                            [name = name.toStdString()](fuse::Document& doc) { doc.removeParameter(name); });
}
