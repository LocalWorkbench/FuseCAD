// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "CanvasOverlays.h"
#include "Commands.h"
#include "FeatureDialogs.h"
#include "Icons.h"
#include "ParametersPanel.h"
#include "Ribbon.h"
#include "TimelineBar.h"
#include "ViewCube.h"
#include "Viewport.h"

#include "oc/core/Error.h"
#include "oc/model/Builders.h"
#include "oc/model/Features.h"
#include "oc/model/Samples.h"
#include "oc/occt/OcctKernel.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr double kDisplayDeflection = 0.05; // mm
const char* const kDocFilter = "OpenCircuit documents (*.ocd)";

QString errorText(const std::exception& e) { return QString::fromStdString(e.what()); }

QToolButton* barButton(QWidget* parent, QAction* action, int iconSize = 18) {
    auto* b = new QToolButton(parent);
    b->setDefaultAction(action);
    b->setIconSize(QSize(iconSize, iconSize));
    b->setAutoRaise(true);
    return b;
}

QToolButton* iconButton(QWidget* parent, const QString& icon, const QString& tip, int iconSize = 18) {
    auto* b = new QToolButton(parent);
    b->setIcon(icons::get(icon));
    b->setIconSize(QSize(iconSize, iconSize));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    return b;
}

// Sketch geometry as world-space polylines for display.
std::vector<std::vector<float>> sketchPolylines(const oc::SketchFeature& f) {
    using oc::sketch::EntityType;
    const oc::sketch::Sketch& s = f.sketch;
    const oc::Plane& pl = f.resolvedPlane;
    std::vector<std::vector<float>> out;
    auto push = [&](std::vector<float>& strip, oc::Vec2 p) {
        const oc::Vec3 w = pl.toWorld(p);
        strip.insert(strip.end(), {static_cast<float>(w.x), static_cast<float>(w.y), static_cast<float>(w.z)});
    };
    auto arc = [&](oc::Vec2 c, double r, double a0, double sweep) {
        std::vector<float> strip;
        const int n = std::max(8, static_cast<int>(std::ceil(std::fabs(sweep) / (2 * oc::kPi) * 72)));
        for (int i = 0; i <= n; ++i) {
            const double a = a0 + sweep * i / n;
            push(strip, {c.x + r * std::cos(a), c.y + r * std::sin(a)});
        }
        out.push_back(std::move(strip));
    };
    for (const auto& e : s.entities()) {
        if (e.construction) continue;
        switch (e.type) {
        case EntityType::Point: break;
        case EntityType::Line: {
            std::vector<float> strip;
            push(strip, s.point(e.p1));
            push(strip, s.point(e.p2));
            out.push_back(std::move(strip));
            break;
        }
        case EntityType::Circle: arc(s.point(e.p1), s.variables()[static_cast<size_t>(e.var)], 0, 2 * oc::kPi); break;
        case EntityType::Arc: {
            const oc::Vec2 c = s.point(e.p1), a = s.point(e.p2), b = s.point(e.p3);
            const double a0 = std::atan2(a.y - c.y, a.x - c.x);
            double sweep = std::atan2(b.y - c.y, b.x - c.x) - a0;
            if (sweep <= 0) sweep += 2 * oc::kPi;
            arc(c, (a - c).length(), a0, sweep);
            break;
        }
        }
    }
    return out;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), kernel_(oc::occt::createKernel()), commands_(new CommandRegistry(this)) {
    viewport_ = new Viewport;
    browser_ = new BrowserPanel;
    timeline_ = new TimelineBar(this);
    registerCommands();

    auto* nav = new NavigationBar(*commands_,
                                  {"nav.orbit", "nav.lookat", "|", "nav.pan", "nav.zoom", "view.fit", "|",
                                   "view.grid", "nav.viewports"},
                                  buildDisplayMenu(), nullptr);
    canvas_ = new Canvas(viewport_, browser_, nav, this);
    connect(commands_, &CommandRegistry::notImplemented, this, [this](const QString& text) {
        notify(tr("<b>%1</b> is coming soon &mdash; OpenCircuit is under active development.").arg(text));
    });

    buildRibbon();
    auto* central = new QWidget(this);
    central->setObjectName("Central");
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildTopBar());
    layout->addWidget(ribbon_);
    layout->addWidget(canvas_, 1);
    layout->addWidget(timeline_);
    setCentralWidget(central);
    buildMenuBar();

    connect(viewport_, &Viewport::contextMenuRequested, this, &MainWindow::showMarkingMenu);
    connect(browser_, &BrowserPanel::visibilityToggled, this, &MainWindow::onBrowserVisibility);
    connect(browser_, &BrowserPanel::activated, this, &MainWindow::onBrowserActivated);
    connect(browser_, &BrowserPanel::contextMenuRequested, this, &MainWindow::onBrowserContextMenu);
    connect(timeline_, &TimelineBar::markerRequested, this, &MainWindow::setMarker);
    connect(timeline_, &TimelineBar::contextMenuRequested, this, &MainWindow::onTimelineContextMenu);
    connect(timeline_, &TimelineBar::editRequested, this, [this](int) {
        notify(tr("<b>Edit Feature</b> is coming soon &mdash; for now, change its parameters."));
    });

    resize(1500, 940);
    setDocument(oc::samples::mountingPlate(), true);
    setCurrentFile({});
    viewport_->homeView();
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Commands

void MainWindow::registerCommands() {
    CommandRegistry& c = *commands_;
    const std::function<void()> todo; // placeholder: roadmap feature

    // File / edit / view.
    c.add(
        "file.new", tr("New Design"), "file",
        [this] {
            if (!maybeSave()) return;
            setDocument(oc::Document{}, true);
            setCurrentFile({});
        },
        QKeySequence::New);
    c.add(
        "file.open", tr("Open..."), "file",
        [this] {
            if (!maybeSave()) return;
            const QString path = QFileDialog::getOpenFileName(this, tr("Open Design"), {}, tr(kDocFilter));
            if (!path.isEmpty()) openFile(path);
        },
        QKeySequence::Open);
    c.add("file.save", tr("Save"), "save", [this] { save(); }, QKeySequence::Save);
    c.add("file.saveas", tr("Save As..."), "save", [this] { saveAs(); }, QKeySequence::SaveAs);
    c.add("file.export-step", tr("Export STEP..."), "export",
          [this] { exportBodies(tr("STEP files (*.step *.stp)"), "step"); });
    c.add("file.export-stl", tr("Export STL..."), "export", [this] { exportBodies(tr("STL files (*.stl)"), "stl"); });
    c.add("sample.plate", tr("Mounting Plate"), "body", [this] { openSample(&oc::samples::mountingPlate); });
    c.add("sample.flange", tr("Flange"), "revolve", [this] { openSample(&oc::samples::flange); });
    c.add(
        "edit.undo", tr("Undo"), "undo",
        [this] {
            if (undo_->canUndo()) setDocument(undo_->undo(), false);
        },
        QKeySequence::Undo);
    c.add(
        "edit.redo", tr("Redo"), "redo",
        [this] {
            if (undo_->canRedo()) setDocument(undo_->redo(), false);
        },
        QKeySequence::Redo);

    c.add("view.fit", tr("Fit"), "fit", [this] { viewport_->fitAll(); }, QKeySequence(Qt::Key_F6));
    c.add("view.home", tr("Home"), "named-view", [this] { viewport_->homeView(); }, QKeySequence(Qt::Key_Home));
    auto standardView = [this](ViewCube::Target t) {
        return [this, t] {
            const auto o = ViewCube::orientation(t);
            viewport_->setView(o.yawDeg, o.pitchDeg);
        };
    };
    c.add("view.top", tr("Top"), "named-view", standardView(ViewCube::Top), QKeySequence(Qt::Key_1));
    c.add("view.front", tr("Front"), "named-view", standardView(ViewCube::Front), QKeySequence(Qt::Key_2));
    c.add("view.right", tr("Right"), "named-view", standardView(ViewCube::Right), QKeySequence(Qt::Key_3));
    QAction* grid = c.add("view.grid", tr("Layout Grid"), "grid", [this] {
        view_.showGrid = commands_->get("view.grid")->isChecked();
        refreshViews();
    });
    grid->setCheckable(true);
    grid->setChecked(view_.showGrid);

    c.add("nav.orbit", tr("Orbit"), "orbit", [this] { notify(tr("<b>Orbit</b>: drag with the left mouse button")); });
    c.add("nav.pan", tr("Pan"), "pan", [this] { notify(tr("<b>Pan</b>: drag with the middle or right mouse button")); });
    c.add("nav.zoom", tr("Zoom"), "zoom", [this] { notify(tr("<b>Zoom</b>: scroll the mouse wheel")); });
    c.add("nav.lookat", tr("Look At"), "lookat", todo);
    c.add("nav.viewports", tr("Multiple Views"), "viewports", todo);

    // Solid / create.
    c.add("create.sketch", tr("Create Sketch"), "sketch", todo);
    c.add("create.extrude", tr("Extrude"), "extrude", todo, QKeySequence(Qt::Key_E));
    c.add("create.revolve", tr("Revolve"), "revolve", todo);
    c.add("create.sweep", tr("Sweep"), "sweep", todo);
    c.add("create.loft", tr("Loft"), "loft", todo);
    c.add("create.rib", tr("Rib"), "rib", todo);
    c.add("create.emboss", tr("Emboss"), "emboss", todo);
    c.add("create.hole", tr("Hole"), "hole", todo, QKeySequence(Qt::Key_H));
    c.add("create.thread", tr("Thread"), "thread", todo);
    c.add(
        "create.box", tr("Box"), "box",
        [this] {
            BoxDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) runCommand(tr("Box"), [&] { oc::build::addBox(doc_, dlg.spec()); });
        },
        QKeySequence(Qt::Key_B));
    c.add("create.cylinder", tr("Cylinder"), "cylinder", [this] {
        CylinderDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            runCommand(tr("Cylinder"), [&] { oc::build::addCylinder(doc_, dlg.spec()); });
    });
    c.add("create.sphere", tr("Sphere"), "sphere", todo);
    c.add("create.torus", tr("Torus"), "torus", todo);
    c.add("create.coil", tr("Coil"), "coil", todo);
    c.add("create.pipe", tr("Pipe"), "sweep", todo);
    c.add("create.pattern", tr("Rectangular Pattern"), "pattern", todo);
    c.add("create.pattern-circular", tr("Circular Pattern"), "pattern", todo);
    c.add("create.mirror", tr("Mirror"), "mirror", todo);

    // Solid / modify.
    c.add("modify.presspull", tr("Press Pull"), "presspull", todo, QKeySequence(Qt::Key_Q));
    c.add(
        "modify.fillet", tr("Fillet"), "fillet",
        [this] {
            EdgeFeatureDialog dlg(tr("Fillet"), tr("Radius"), this);
            if (dlg.exec() == QDialog::Accepted)
                runCommand(tr("Fillet"), [&] { oc::build::addFillet(doc_, dlg.edgeQuery(), dlg.size()); });
        },
        QKeySequence(Qt::Key_F));
    c.add("modify.chamfer", tr("Chamfer"), "chamfer", [this] {
        EdgeFeatureDialog dlg(tr("Chamfer"), tr("Distance"), this);
        if (dlg.exec() == QDialog::Accepted)
            runCommand(tr("Chamfer"), [&] { oc::build::addChamfer(doc_, dlg.edgeQuery(), dlg.size()); });
    });
    c.add("modify.shell", tr("Shell"), "shell", todo);
    c.add("modify.combine", tr("Combine"), "combine", todo);
    c.add("modify.move", tr("Move/Copy"), "move", todo, QKeySequence(Qt::Key_M));
    c.add("modify.align", tr("Align"), "align", todo);
    c.add("modify.scale", tr("Scale"), "scale", todo);
    c.add("modify.split", tr("Split Body"), "split", todo);
    c.add(
        "modify.delete", tr("Delete"), "delete",
        [this] {
            const int id = timeline_->selectedFeatureId();
            if (id < 0) notify(tr("Select a feature in the timeline to delete it"));
            else deleteFeature(id);
        },
        QKeySequence::Delete);
    c.add("modify.appearance", tr("Appearance"), "appearance", todo, QKeySequence(Qt::Key_A));
    c.add("modify.material", tr("Physical Material"), "material", todo);
    c.add("modify.parameters", tr("Change Parameters"), "parameters", [this] { showParameters(); });
    c.add("modify.compute", tr("Compute All"), "compute", [this] { recompute(); }, QKeySequence(Qt::CTRL | Qt::Key_B));

    // Assemble / construct / inspect / insert / select.
    c.add("assemble.component", tr("New Component"), "component", todo);
    c.add("assemble.joint", tr("Joint"), "joint", todo, QKeySequence(Qt::Key_J));
    c.add("assemble.motion", tr("Motion Link"), "motion", todo);
    c.add("construct.offset-plane", tr("Offset Plane"), "plane-offset", todo);
    c.add("construct.angle-plane", tr("Plane at Angle"), "plane-angle", todo);
    c.add("construct.midplane", tr("Midplane"), "midplane", todo);
    c.add("construct.axis", tr("Axis Through Cylinder"), "axis", todo);
    c.add("construct.point", tr("Point at Vertex"), "point", todo);
    c.add("inspect.measure", tr("Measure"), "measure", [this] { showMeasure(); }, QKeySequence(Qt::Key_I));
    c.add("inspect.interference", tr("Interference"), "interference", todo);
    c.add("inspect.section", tr("Section Analysis"), "section", todo);
    c.add("inspect.zebra", tr("Zebra Analysis"), "zebra", todo);
    c.add("insert.step", tr("Insert STEP File..."), "import", [this] { insertStep(); });
    c.add("insert.mesh", tr("Insert Mesh"), "insert-mesh", todo);
    c.add("insert.decal", tr("Decal"), "decal", todo);
    c.add("insert.canvas", tr("Canvas"), "canvas", todo);
    c.add("select.select", tr("Select"), "select", todo);

    // Other tabs.
    c.add("surface.patch", tr("Patch"), "patch", todo);
    c.add("surface.trim", tr("Trim"), "trim", todo);
    c.add("surface.stitch", tr("Stitch"), "stitch", todo);
    c.add("mesh.repair", tr("Repair"), "mesh", todo);
    c.add("sheet.flange", tr("Flange"), "flange-sm", todo);
    c.add("plastic.boss", tr("Boss"), "cylinder", todo);
    c.add("util.print3d", tr("3D Print"), "print3d", todo);
    c.add("util.scripts", tr("Scripts and Add-Ins"), "scripts", todo);
    c.add("util.preferences", tr("Preferences"), "settings", todo);
}

void MainWindow::buildRibbon() {
    ribbon_ = new Ribbon(*commands_, this);

    auto* workspaces = new QMenu(ribbon_);
    auto* group = new QActionGroup(workspaces);
    const QStringList names{tr("Design"),     tr("Generative Design"), tr("Render"), tr("Animation"),
                            tr("Simulation"), tr("Manufacture"),       tr("Drawing")};
    for (const QString& name : names) {
        QAction* a = workspaces->addAction(name);
        a->setCheckable(true);
        a->setChecked(name == names.front());
        group->addAction(a);
        if (name == names.front()) continue;
        connect(a, &QAction::triggered, this, [this, name, design = workspaces->actions().front()] {
            design->setChecked(true);
            notify(tr("The <b>%1</b> workspace is coming soon.").arg(name));
        });
    }
    ribbon_->workspaceButton()->setMenu(workspaces);

    using G = Ribbon::Group;
    const G construct{tr("CONSTRUCT"),
                      {"construct.offset-plane"},
                      {"construct.angle-plane", "construct.midplane", "-", "construct.axis", "construct.point"}};
    const G inspect{tr("INSPECT"), {"inspect.measure"}, {"inspect.interference", "inspect.section", "inspect.zebra"}};
    const G insert{tr("INSERT"), {"insert.step"}, {"insert.mesh", "insert.decal", "insert.canvas"}};
    const G select{tr("SELECT"), {"select.select"}, {}};

    ribbon_->addTab(tr("SOLID"),
                    {
                        G{tr("CREATE"),
                          {"create.sketch", "create.extrude", "create.revolve", "create.hole", "create.pattern",
                           "create.box"},
                          {"create.sweep", "create.loft", "create.rib", "create.emboss", "create.thread", "-",
                           "create.cylinder", "create.sphere", "create.torus", "create.coil", "create.pipe", "-",
                           "create.pattern-circular", "create.mirror"}},
                        G{tr("MODIFY"),
                          {"modify.presspull", "modify.fillet", "modify.chamfer", "modify.shell", "modify.combine",
                           "modify.move"},
                          {"modify.align", "modify.scale", "modify.split", "modify.delete", "-", "modify.appearance",
                           "modify.material", "-", "modify.parameters", "modify.compute"}},
                        G{tr("ASSEMBLE"), {"assemble.component", "assemble.joint"}, {"assemble.motion"}},
                        construct,
                        inspect,
                        insert,
                        select,
                    });
    ribbon_->addTab(tr("SURFACE"), {G{tr("CREATE"), {"surface.patch", "create.loft"}, {"create.sweep"}},
                                    G{tr("MODIFY"), {"surface.trim", "surface.stitch"}, {}}, construct, inspect,
                                    insert, select});
    ribbon_->addTab(tr("MESH"), {G{tr("PREPARE"), {"mesh.repair", "insert.mesh"}, {}}, inspect, select});
    ribbon_->addTab(tr("SHEET METAL"),
                    {G{tr("CREATE"), {"create.sketch", "sheet.flange"}, {}}, construct, inspect, select});
    ribbon_->addTab(tr("PLASTIC"),
                    {G{tr("CREATE"), {"create.sketch", "plastic.boss"}, {"create.rib"}}, construct, inspect, select});
    ribbon_->addTab(tr("UTILITIES"), {G{tr("MAKE"), {"util.print3d"}, {}}, G{tr("ADD-INS"), {"util.scripts"}, {}},
                                      G{tr("PREFERENCES"), {"util.preferences"}, {}}});
}

QWidget* MainWindow::buildTopBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("TopBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 4, 8, 0);
    layout->setSpacing(2);

    auto* apps = iconButton(bar, "apps", tr("Data panel"));
    connect(apps, &QToolButton::clicked, this, [this] { notify(tr("The <b>Data Panel</b> is coming soon.")); });
    auto* file = iconButton(bar, "file", tr("File"));
    auto* fileMenu = new QMenu(file);
    for (const char* id : {"file.new", "file.open"}) fileMenu->addAction(commands_->get(id));
    QMenu* samples = fileMenu->addMenu(icons::get("doc"), tr("New from Sample"));
    samples->addAction(commands_->get("sample.plate"));
    samples->addAction(commands_->get("sample.flange"));
    fileMenu->addSeparator();
    for (const char* id : {"file.save", "file.saveas", "file.export-step", "file.export-stl"})
        fileMenu->addAction(commands_->get(id));
    file->setMenu(fileMenu);
    file->setPopupMode(QToolButton::InstantPopup);

    layout->addWidget(apps);
    layout->addWidget(file);
    layout->addWidget(barButton(bar, commands_->get("file.save")));
    layout->addSpacing(6);
    layout->addWidget(barButton(bar, commands_->get("edit.undo")));
    layout->addWidget(barButton(bar, commands_->get("edit.redo")));
    layout->addSpacing(16);

    docTabs_ = new QTabBar(bar);
    docTabs_->setObjectName("DocTabs");
    docTabs_->setDrawBase(false);
    docTabs_->setExpanding(false);
    docTabs_->addTab(icons::get("doc"), documentName());
    layout->addWidget(docTabs_, 0, Qt::AlignBottom);
    auto* plus = iconButton(bar, "plus", tr("New design tab"), 14);
    connect(plus, &QToolButton::clicked, this, [this] { notify(tr("Multiple design tabs are coming soon.")); });
    layout->addWidget(plus);
    layout->addStretch();

    const std::pair<const char*, QString> tools[] = {
        {"jobs", tr("Job Status")}, {"bell", tr("Notifications")}, {"help", tr("Help")}, {"user", tr("Account")}};
    for (const auto& [icon, tip] : tools) {
        auto* b = iconButton(bar, icon, tip, QString(icon) == "user" ? 22 : 18);
        connect(b, &QToolButton::clicked, this, [this, tip] { notify(tr("<b>%1</b> is coming soon.").arg(tip)); });
        layout->addWidget(b);
    }
    return bar;
}

QMenu* MainWindow::buildDisplayMenu() {
    auto* menu = new QMenu(this);
    QMenu* style = menu->addMenu(tr("Visual Style"));
    auto* group = new QActionGroup(style);
    const std::pair<QString, ViewState::Style> styles[] = {
        {tr("Shaded"), ViewState::Style::Shaded},
        {tr("Shaded with Visible Edges"), ViewState::Style::ShadedWithEdges},
        {tr("Wireframe"), ViewState::Style::Wireframe},
    };
    for (const auto& [name, s] : styles) {
        QAction* a = style->addAction(name);
        a->setCheckable(true);
        a->setChecked(s == view_.style);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, s] {
            view_.style = s;
            refreshViews();
        });
    }
    menu->addAction(tr("Environment"), this, [this] { notify(tr("<b>Environments</b> are coming soon.")); });
    menu->addAction(commands_->get("view.grid"));
    return menu;
}

void MainWindow::buildMenuBar() {
    // The native menu bar mirrors the ribbon so every command has a keyboard path.
    QMenu* file = menuBar()->addMenu(tr("&File"));
    for (const char* id : {"file.new", "file.open", "file.save", "file.saveas"}) file->addAction(commands_->get(id));
    QMenu* samples = file->addMenu(tr("New from Sample"));
    samples->addAction(commands_->get("sample.plate"));
    samples->addAction(commands_->get("sample.flange"));
    file->addSeparator();
    file->addAction(commands_->get("file.export-step"));
    file->addAction(commands_->get("file.export-stl"));

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(commands_->get("edit.undo"));
    edit->addAction(commands_->get("edit.redo"));
    edit->addSeparator();
    edit->addAction(commands_->get("modify.delete"));
    edit->addAction(commands_->get("modify.parameters"));

    QMenu* create = menuBar()->addMenu(tr("&Create"));
    for (const char* id : {"create.box", "create.cylinder", "modify.fillet", "modify.chamfer", "insert.step"})
        create->addAction(commands_->get(id));

    QMenu* view = menuBar()->addMenu(tr("&View"));
    for (const char* id : {"view.fit", "view.home", "view.top", "view.front", "view.right", "view.grid"})
        view->addAction(commands_->get(id));

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About OpenCircuit"), this, [this] {
        QMessageBox::about(this, tr("About OpenCircuit"),
                           tr("<b>OpenCircuit %1</b><p>Free and open-source parametric CAD.</p>"
                              "<p>Geometry kernel: %2</p><p>Licensed under the GNU GPL v3 or later.</p>")
                               .arg(QApplication::applicationVersion(), QString::fromStdString(kernel_->name())));
    });
}

// ---------------------------------------------------------------------------
// Document lifecycle

bool MainWindow::openFile(const QString& path) {
    try {
        setDocument(oc::Document::load(path.toStdString()), true);
        setCurrentFile(path);
        viewport_->homeView();
        return true;
    } catch (const oc::Error& e) {
        QMessageBox::critical(this, tr("Open Failed"), tr("Could not open %1:\n%2").arg(path, errorText(e)));
        return false;
    }
}

void MainWindow::openSample(oc::Document (*make)()) {
    if (!maybeSave()) return;
    setDocument(make(), true);
    setCurrentFile({});
    viewport_->homeView();
}

void MainWindow::setDocument(oc::Document doc, bool resetHistory) {
    doc_ = std::move(doc);
    if (resetHistory) {
        undo_ = std::make_unique<oc::UndoStack>(doc_);
        view_.hiddenBodies.clear();
        view_.visibleSketches.clear();
    } else {
        setWindowModified(true);
    }
    recompute();
    updateUndoActions();
}

void MainWindow::recompute() {
    QElapsedTimer timer;
    timer.start();
    oc::RecomputeReport report = doc_.recompute(*kernel_);

    std::vector<Viewport::BodyMesh> meshes;
    for (const auto& [id, body] : doc_.bodies()) {
        try {
            meshes.push_back({id, kernel_->tessellate(body.shape, kDisplayDeflection)});
        } catch (const oc::Error& e) {
            report.messages.push_back(body.name + ": " + e.what());
        }
    }
    viewport_->setBodies(std::move(meshes));
    refreshViews();

    if (!report.messages.empty())
        notify(tr("<b>%1 problem(s)</b>: %2")
                   .arg(report.messages.size())
                   .arg(QString::fromStdString(report.messages.front()).toHtmlEscaped()));
    const int bodies = static_cast<int>(doc_.bodies().size());
    const int features = static_cast<int>(doc_.features().size());
    canvas_->setInfo(tr("%1  |  %2  |  %3 ms")
                         .arg(bodies == 1 ? tr("1 body") : tr("%1 bodies").arg(bodies))
                         .arg(features == 1 ? tr("1 feature") : tr("%1 features").arg(features))
                         .arg(timer.elapsed()));
}

void MainWindow::refreshViews() {
    std::vector<std::vector<float>> curves;
    for (size_t i = 0; i < doc_.features().size() && i < doc_.marker(); ++i) {
        const auto* sk = oc::featureCast<oc::SketchFeature>(doc_.features()[i].get());
        if (!sk || !sk->healthy() || !view_.sketchVisible(sk->id)) continue;
        auto lines = sketchPolylines(*sk);
        curves.insert(curves.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
    }
    viewport_->setSketchCurves(std::move(curves));
    viewport_->setViewState(view_);
    browser_->refresh(documentName(), doc_, view_);
    timeline_->refresh(doc_);
    if (parameters_) parameters_->refresh(doc_.parameters());
}

void MainWindow::commit(const QString& label) {
    undo_->commit(doc_, label.toStdString());
    setWindowModified(true);
    recompute();
    updateUndoActions();
}

template <class Fn>
void MainWindow::runCommand(const QString& label, Fn&& fn) {
    oc::Document before = doc_;
    try {
        fn();
    } catch (const oc::Error& e) {
        doc_ = std::move(before);
        QMessageBox::warning(this, label, errorText(e));
        return;
    }
    commit(label);
}

void MainWindow::updateUndoActions() {
    QAction* undo = commands_->get("edit.undo");
    QAction* redo = commands_->get("edit.redo");
    undo->setEnabled(undo_->canUndo());
    redo->setEnabled(undo_->canRedo());
    undo->setText(undo_->canUndo() ? tr("Undo %1").arg(QString::fromStdString(undo_->undoLabel())) : tr("Undo"));
    redo->setText(undo_->canRedo() ? tr("Redo %1").arg(QString::fromStdString(undo_->redoLabel())) : tr("Redo"));
}

void MainWindow::notify(const QString& html) { canvas_->toast(html); }

QString MainWindow::documentName() const {
    return currentFile_.isEmpty() ? tr("Untitled") : QFileInfo(currentFile_).completeBaseName();
}

void MainWindow::setCurrentFile(const QString& path) {
    currentFile_ = path;
    setWindowModified(false);
    setWindowTitle(tr("%1[*] - OpenCircuit").arg(documentName()));
    if (docTabs_) docTabs_->setTabText(0, documentName());
    browser_->refresh(documentName(), doc_, view_);
}

bool MainWindow::maybeSave() {
    if (!isWindowModified()) return true;
    const auto choice = QMessageBox::warning(this, tr("Unsaved Changes"), tr("Save changes to the current design?"),
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Save) return save();
    return choice == QMessageBox::Discard;
}

bool MainWindow::save() {
    if (currentFile_.isEmpty()) return saveAs();
    try {
        doc_.save(currentFile_.toStdString());
    } catch (const oc::Error& e) {
        QMessageBox::critical(this, tr("Save Failed"), errorText(e));
        return false;
    }
    setCurrentFile(currentFile_);
    notify(tr("Saved <b>%1</b>").arg(QFileInfo(currentFile_).fileName()));
    return true;
}

bool MainWindow::saveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Save Design"), documentName() + ".ocd", tr(kDocFilter));
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) path += ".ocd";
    currentFile_ = path;
    return save();
}

void MainWindow::exportBodies(const QString& filter, const QString& suffix) {
    if (doc_.bodies().empty()) {
        notify(tr("There are no bodies to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), documentName() + "." + suffix, filter);
    if (path.isEmpty()) return;
    std::vector<oc::ShapePtr> shapes;
    for (const auto& [id, body] : doc_.bodies())
        if (view_.bodyVisible(id)) shapes.push_back(body.shape);
    try {
        if (suffix == "stl") kernel_->exportStl(shapes, path.toStdString(), kDisplayDeflection);
        else kernel_->exportStep(shapes, path.toStdString());
        notify(tr("Exported <b>%1</b>").arg(QFileInfo(path).fileName()));
    } catch (const oc::Error& e) {
        QMessageBox::critical(this, tr("Export Failed"), errorText(e));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) event->accept();
    else event->ignore();
}

// ---------------------------------------------------------------------------
// Commands with UI

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
        connect(parameters_, &ParametersPanel::expressionEdited, this, &MainWindow::onExpressionEdited);
        connect(parameters_, &ParametersPanel::addRequested, this, &MainWindow::onAddParameter);
        connect(parameters_, &ParametersPanel::removeRequested, this, &MainWindow::onRemoveParameter);
    }
    parameters_->refresh(doc_.parameters());
    parametersDialog_->show();
    parametersDialog_->raise();
}

void MainWindow::showMeasure() {
    if (doc_.bodies().empty()) {
        notify(tr("There are no bodies to measure."));
        return;
    }
    const QLocale loc;
    QString html = QStringLiteral("<table cellspacing='6'><tr><th align='left'>%1</th><th>%2</th><th>%3</th>"
                                  "<th>%4</th></tr>")
                       .arg(tr("Body"), tr("Volume (mm³)"), tr("Area (mm²)"), tr("Size (mm)"));
    for (const auto& [id, body] : doc_.bodies()) {
        const oc::BoundingBox bb = kernel_->boundingBox(body.shape);
        html += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td align='right'>%3</td>"
                               "<td>%4 × %5 × %6</td></tr>")
                    .arg(QString::fromStdString(body.name), loc.toString(kernel_->volume(body.shape), 'f', 2),
                         loc.toString(kernel_->surfaceArea(body.shape), 'f', 2),
                         loc.toString(bb.max.x - bb.min.x, 'f', 2), loc.toString(bb.max.y - bb.min.y, 'f', 2),
                         loc.toString(bb.max.z - bb.min.z, 'f', 2));
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
    runCommand(tr("Insert %1").arg(QFileInfo(path).fileName()), [&] {
        auto& f = doc_.add<oc::ImportStepFeature>();
        f.path = path.toStdString();
        f.name = QFileInfo(path).completeBaseName().toStdString();
    });
    viewport_->fitAll();
}

void MainWindow::deleteFeature(int id) {
    const oc::Feature* f = doc_.findFeature(id);
    if (!f) return;
    runCommand(tr("Delete %1").arg(QString::fromStdString(f->name)), [&] { doc_.removeFeature(id); });
}

void MainWindow::renameFeature(int id) {
    oc::Feature* f = doc_.findFeature(id);
    if (!f) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("Name:"), QLineEdit::Normal,
                                               QString::fromStdString(f->name), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    f->name = name.trimmed().toStdString();
    commit(tr("Rename %1").arg(name));
}

void MainWindow::toggleSuppress(int id) {
    oc::Feature* f = doc_.findFeature(id);
    if (!f) return;
    f->suppressed = !f->suppressed;
    commit((f->suppressed ? tr("Suppress %1") : tr("Unsuppress %1")).arg(QString::fromStdString(f->name)));
}

void MainWindow::setMarker(int position) {
    if (position < 0 || static_cast<size_t>(position) == doc_.marker()) return;
    doc_.setMarker(static_cast<size_t>(position));
    commit(tr("Move History Marker"));
}

// ---------------------------------------------------------------------------
// Panels

void MainWindow::onBrowserVisibility(BrowserPanel::Kind kind, int id, bool visible) {
    using K = BrowserPanel::Kind;
    switch (kind) {
    case K::Document: view_.showBodies = view_.showSketches = visible; break;
    case K::Origin: view_.showOrigin = visible; break;
    case K::Bodies: view_.showBodies = visible; break;
    case K::Body:
        if (visible) view_.hiddenBodies.remove(id);
        else view_.hiddenBodies.insert(id);
        break;
    case K::Sketches: view_.showSketches = visible; break;
    case K::Sketch:
        if (visible) view_.visibleSketches.insert(id);
        else view_.visibleSketches.remove(id);
        break;
    default: return;
    }
    refreshViews();
}

void MainWindow::onBrowserActivated(BrowserPanel::Kind kind, int id) {
    if (kind != BrowserPanel::Kind::NamedView) return;
    static const char* const views[] = {"view.top", "view.front", "view.right", "view.home"};
    if (id >= 0 && id < 4) commands_->get(views[id])->trigger();
}

void MainWindow::onBrowserContextMenu(BrowserPanel::Kind kind, int id, const QPoint& pos) {
    using K = BrowserPanel::Kind;
    QMenu menu(this);
    if (kind == K::Body) {
        const bool visible = view_.bodyVisible(id);
        menu.addAction(icons::get(visible ? "eye-off" : "eye"), visible ? tr("Hide") : tr("Show"), this,
                       [this, kind, id, visible] { onBrowserVisibility(kind, id, !visible); });
        menu.addAction(commands_->get("inspect.measure"));
        menu.addAction(commands_->get("modify.appearance"));
        menu.addSeparator();
        menu.addAction(commands_->get("file.export-step"));
    } else if (kind == K::Sketch) {
        const bool visible = view_.sketchVisible(id);
        menu.addAction(icons::get(visible ? "eye-off" : "eye"), visible ? tr("Hide") : tr("Show"), this,
                       [this, kind, id, visible] { onBrowserVisibility(kind, id, !visible); });
        menu.addAction(icons::get("sketch"), tr("Edit Sketch"), this,
                       [this] { notify(tr("<b>Edit Sketch</b> is coming soon.")); });
        menu.addSeparator();
        menu.addAction(icons::get("delete"), tr("Delete"), this, [this, id] { deleteFeature(id); });
    } else if (kind == K::Document || kind == K::Settings) {
        menu.addAction(commands_->get("modify.parameters"));
        menu.addAction(commands_->get("file.save"));
    } else if (kind == K::NamedView) {
        menu.addAction(tr("Go to View"), this, [this, kind, id] { onBrowserActivated(kind, id); });
    } else {
        return;
    }
    menu.exec(pos);
}

void MainWindow::onTimelineContextMenu(int featureId, const QPoint& pos) {
    const oc::Feature* f = doc_.findFeature(featureId);
    if (!f) return;
    int index = 0;
    while (doc_.features()[static_cast<size_t>(index)]->id != featureId) ++index;

    QMenu menu(this);
    menu.addAction(icons::get("sketch"), tr("Edit Feature"), this, [this] {
        notify(tr("<b>Edit Feature</b> is coming soon &mdash; for now, change its parameters."));
    });
    menu.addAction(tr("Rename..."), this, [this, featureId] { renameFeature(featureId); });
    menu.addAction(f->suppressed ? tr("Unsuppress Feature") : tr("Suppress Feature"), this,
                   [this, featureId] { toggleSuppress(featureId); });
    menu.addSeparator();
    menu.addAction(tr("Roll History Marker Here"), this, [this, index] { setMarker(index + 1); });
    menu.addAction(tr("Roll History Marker to End"), this,
                   [this] { setMarker(static_cast<int>(doc_.features().size())); });
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
    // Ring: clockwise from north. North repeats the last command.
    QString repeat = "create.box";
    if (QAction* last = commands_->lastCommand()) repeat = last->objectName();
    auto* menu = new MarkingMenu(*commands_,
                                 {repeat, "modify.delete", "modify.fillet", "edit.redo", "create.sketch",
                                  "create.cylinder", "edit.undo", "modify.parameters"},
                                 {"nav.pan", "nav.zoom", "nav.orbit", "view.fit", "-", "create.extrude", "create.box",
                                  "modify.chamfer", "inspect.measure", "modify.appearance", "-", "insert.step",
                                  "modify.compute"},
                                 this);
    menu->popupAt(globalPos);
}

void MainWindow::onExpressionEdited(const QString& name, const QString& expression) {
    try {
        doc_.parameters().setExpression(name.toStdString(), expression.toStdString());
    } catch (const oc::Error& e) {
        QMessageBox::warning(this, tr("Invalid Expression"), errorText(e));
        parameters_->refresh(doc_.parameters());
        return;
    }
    commit(tr("Change %1").arg(name));
}

void MainWindow::onAddParameter() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(parametersDialog_, tr("New Parameter"), tr("Name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.isEmpty()) return;
    const QString expr = QInputDialog::getText(parametersDialog_, tr("New Parameter"), tr("Expression:"),
                                               QLineEdit::Normal, "10 mm", &ok);
    if (!ok) return;
    runCommand(tr("Add %1").arg(name), [&] { doc_.parameters().add(name.toStdString(), expr.toStdString()); });
}

void MainWindow::onRemoveParameter(const QString& name) {
    runCommand(tr("Delete %1").arg(name), [&] { doc_.parameters().remove(name.toStdString()); });
}
