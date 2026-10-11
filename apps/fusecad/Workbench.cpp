// SPDX-License-Identifier: GPL-3.0-or-later
#include "Workbench.h"

#include "CanvasOverlays.h"
#include "Commands.h"
#include "Icons.h"
#include "MainWindow.h"
#include "Ribbon.h"
#include "ViewCube.h"
#include "Viewport.h"
#include "fuse/model/Samples.h"

#include <QActionGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QTabBar>
#include <QToolButton>

#include <utility>

namespace {

QToolButton* barButton(QWidget* parent, QAction* action, int iconSize = 18) {
    auto* b = new QToolButton(parent);
    b->setDefaultAction(action);
    b->setIconSize(QSize(iconSize, iconSize));
    b->setAutoRaise(true);
    return b;
}

} // namespace

void Workbench::registerCommands(CommandRegistry& c, MainWindow& w) {
    const std::function<void()> todo; // placeholder: roadmap feature
    using Format = DocumentController::ExportFormat;
    DocumentController& doc = w.controller();
    Viewport& view = w.viewport();

    // File / edit / view.
    c.add("file.new", tr("New Design"), "file", [&w] { w.newDesign(); }, QKeySequence::New);
    c.add("file.open", tr("Open..."), "file", [&w] { w.openDesign(); }, QKeySequence::Open);
    c.add("file.save", tr("Save"), "save", [&w] { w.save(); }, QKeySequence::Save);
    c.add("file.saveas", tr("Save As..."), "save", [&w] { w.saveAs(); }, QKeySequence::SaveAs);
    c.add("file.export-step", tr("Export STEP..."), "export", [&w] { w.exportVisibleBodies(Format::Step); });
    c.add("file.export-stl", tr("Export STL..."), "export", [&w] { w.exportVisibleBodies(Format::Stl); });
    c.add("file.new-tab", tr("New Design Tab"), "plus", todo);
    c.add("sample.plate", tr("Mounting Plate"), "body", [&w] { w.openSample(&fuse::samples::mountingPlate); });
    c.add("sample.flange", tr("Flange"), "revolve", [&w] { w.openSample(&fuse::samples::flange); });
    c.add("edit.undo", tr("Undo"), "undo", [&doc] { doc.undo(); }, QKeySequence::Undo);
    c.add("edit.redo", tr("Redo"), "redo", [&doc] { doc.redo(); }, QKeySequence::Redo);
    c.add("edit.feature", tr("Edit Feature"), "sketch", todo);
    c.add("edit.sketch", tr("Edit Sketch"), "sketch", todo);

    c.add("view.fit", tr("Fit"), "fit", [&view] { view.fitAll(); }, QKeySequence(Qt::Key_F6));
    c.add("view.home", tr("Home"), "named-view", [&view] { view.homeView(); }, QKeySequence(Qt::Key_Home));
    auto standardView = [&view](ViewCube::Target t) {
        return [&view, t] {
            const auto o = ViewCube::orientation(t);
            view.setView(o.yawDeg, o.pitchDeg);
        };
    };
    c.add("view.top", tr("Top"), "named-view", standardView(ViewCube::Top), QKeySequence(Qt::Key_1));
    c.add("view.front", tr("Front"), "named-view", standardView(ViewCube::Front), QKeySequence(Qt::Key_2));
    c.add("view.right", tr("Right"), "named-view", standardView(ViewCube::Right), QKeySequence(Qt::Key_3));
    QAction* grid =
        c.add("view.grid", tr("Layout Grid"), "grid", [&w, &c] { w.setGridVisible(c.get("view.grid")->isChecked()); });
    grid->setCheckable(true);
    grid->setChecked(w.viewState().showGrid);

    auto* styles = new QActionGroup(&c);
    const std::tuple<const char*, QString, ViewState::Style> styleCommands[] = {
        {"view.style-shaded", tr("Shaded"), ViewState::Style::Shaded},
        {"view.style-edges", tr("Shaded with Visible Edges"), ViewState::Style::ShadedWithEdges},
        {"view.style-wireframe", tr("Wireframe"), ViewState::Style::Wireframe},
    };
    for (const auto& [id, text, style] : styleCommands) {
        QAction* a = c.add(id, text, {}, [&w, style] { w.setVisualStyle(style); });
        a->setCheckable(true);
        a->setChecked(style == w.viewState().style);
        styles->addAction(a);
    }
    c.add("view.environment", tr("Environment"), "display", todo);
    c.add("view.data-panel", tr("Data Panel"), "apps", todo);

    // Navigation happens with the mouse; the explicit modes are on the roadmap.
    const std::tuple<const char*, QString, const char*, QString> navModes[] = {
        {"nav.orbit", tr("Orbit"), "orbit", tr("drag with the left mouse button")},
        {"nav.pan", tr("Pan"), "pan", tr("drag with the middle or right mouse button")},
        {"nav.zoom", tr("Zoom"), "zoom", tr("scroll the mouse wheel")},
    };
    for (const auto& [id, text, icon, hint] : navModes)
        c.add(id, text, icon, todo)->setToolTip(tr("%1: %2 (mode coming soon)").arg(text, hint));
    c.add("nav.lookat", tr("Look At"), "lookat", todo);
    c.add("nav.viewports", tr("Multiple Views"), "viewports", todo);
    c.add("timeline.play", tr("Play History"), "tl-play", todo);

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
    c.add("create.box", tr("Box"), "box", [&w] { w.addBox(); }, QKeySequence(Qt::Key_B));
    c.add("create.cylinder", tr("Cylinder"), "cylinder", [&w] { w.addCylinder(); });
    c.add("create.sphere", tr("Sphere"), "sphere", todo);
    c.add("create.torus", tr("Torus"), "torus", todo);
    c.add("create.coil", tr("Coil"), "coil", todo);
    c.add("create.pipe", tr("Pipe"), "sweep", todo);
    c.add("create.pattern", tr("Rectangular Pattern"), "pattern", todo);
    c.add("create.pattern-circular", tr("Circular Pattern"), "pattern", todo);
    c.add("create.mirror", tr("Mirror"), "mirror", todo);

    // Solid / modify.
    c.add("modify.presspull", tr("Press Pull"), "presspull", todo, QKeySequence(Qt::Key_Q));
    c.add("modify.fillet", tr("Fillet"), "fillet", [&w] { w.addFillet(); }, QKeySequence(Qt::Key_F));
    c.add("modify.chamfer", tr("Chamfer"), "chamfer", [&w] { w.addChamfer(); });
    c.add("modify.shell", tr("Shell"), "shell", todo);
    c.add("modify.combine", tr("Combine"), "combine", todo);
    c.add("modify.move", tr("Move/Copy"), "move", todo, QKeySequence(Qt::Key_M));
    c.add("modify.align", tr("Align"), "align", todo);
    c.add("modify.scale", tr("Scale"), "scale", todo);
    c.add("modify.split", tr("Split Body"), "split", todo);
    c.add("modify.delete", tr("Delete"), "delete", [&w] { w.deleteSelectedFeature(); }, QKeySequence::Delete);
    c.add("modify.appearance", tr("Appearance"), "appearance", todo, QKeySequence(Qt::Key_A));
    c.add("modify.material", tr("Physical Material"), "material", todo);
    c.add("modify.parameters", tr("Change Parameters"), "parameters", [&w] { w.showParameters(); });
    c.add(
        "modify.compute", tr("Compute All"), "compute", [&doc] { doc.recompute(); },
        QKeySequence(Qt::CTRL | Qt::Key_B));

    // Assemble / construct / inspect / insert / select.
    c.add("assemble.component", tr("New Component"), "component", todo);
    c.add("assemble.joint", tr("Joint"), "joint", todo, QKeySequence(Qt::Key_J));
    c.add("assemble.motion", tr("Motion Link"), "motion", todo);
    c.add("construct.offset-plane", tr("Offset Plane"), "plane-offset", todo);
    c.add("construct.angle-plane", tr("Plane at Angle"), "plane-angle", todo);
    c.add("construct.midplane", tr("Midplane"), "midplane", todo);
    c.add("construct.axis", tr("Axis Through Cylinder"), "axis", todo);
    c.add("construct.point", tr("Point at Vertex"), "point", todo);
    c.add("inspect.measure", tr("Measure"), "measure", [&w] { w.showMeasure(); }, QKeySequence(Qt::Key_I));
    c.add("inspect.interference", tr("Interference"), "interference", todo);
    c.add("inspect.section", tr("Section Analysis"), "section", todo);
    c.add("inspect.zebra", tr("Zebra Analysis"), "zebra", todo);
    c.add("insert.step", tr("Insert STEP File..."), "import", [&w] { w.insertStep(); });
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

    // Other workspaces and the top bar's service buttons.
    c.add("workspace.generative", tr("Generative Design"), {}, todo);
    c.add("workspace.render", tr("Render"), {}, todo);
    c.add("workspace.animation", tr("Animation"), {}, todo);
    c.add("workspace.simulation", tr("Simulation"), {}, todo);
    c.add("workspace.manufacture", tr("Manufacture"), {}, todo);
    c.add("workspace.drawing", tr("Drawing"), {}, todo);
    c.add("app.jobs", tr("Job Status"), "jobs", todo);
    c.add("app.notifications", tr("Notifications"), "bell", todo);
    c.add("app.help", tr("Help"), "help", todo);
    c.add("app.account", tr("Account"), "user", todo);
    c.add("app.about", tr("About FuseCAD"), {}, [&w] { w.showAbout(); });
}

Ribbon* Workbench::buildRibbon(CommandRegistry& commands, QWidget* parent) {
    auto* ribbon = new Ribbon(commands, parent);

    // Design is the only workspace so far; the others are roadmap commands.
    auto* workspaces = new QMenu(ribbon);
    QAction* design = workspaces->addAction(tr("Design"));
    design->setCheckable(true);
    design->setChecked(true);
    (new QActionGroup(workspaces))->addAction(design);
    for (const char* id : {"workspace.generative", "workspace.render", "workspace.animation", "workspace.simulation",
                           "workspace.manufacture", "workspace.drawing"})
        workspaces->addAction(commands.get(id));
    ribbon->workspaceButton()->setMenu(workspaces);

    using G = Ribbon::Group;
    const G construct{tr("CONSTRUCT"),
                      {"construct.offset-plane"},
                      {"construct.angle-plane", "construct.midplane", "-", "construct.axis", "construct.point"}};
    const G inspect{tr("INSPECT"), {"inspect.measure"}, {"inspect.interference", "inspect.section", "inspect.zebra"}};
    const G insert{tr("INSERT"), {"insert.step"}, {"insert.mesh", "insert.decal", "insert.canvas"}};
    const G select{tr("SELECT"), {"select.select"}, {}};

    ribbon->addTab(
        tr("SOLID"),
        {
            G{tr("CREATE"),
              {"create.sketch", "create.extrude", "create.revolve", "create.hole", "create.pattern", "create.box"},
              {"create.sweep", "create.loft", "create.rib", "create.emboss", "create.thread", "-", "create.cylinder",
               "create.sphere", "create.torus", "create.coil", "create.pipe", "-", "create.pattern-circular",
               "create.mirror"}},
            G{tr("MODIFY"),
              {"modify.presspull", "modify.fillet", "modify.chamfer", "modify.shell", "modify.combine", "modify.move"},
              {"modify.align", "modify.scale", "modify.split", "modify.delete", "-", "modify.appearance",
               "modify.material", "-", "modify.parameters", "modify.compute"}},
            G{tr("ASSEMBLE"), {"assemble.component", "assemble.joint"}, {"assemble.motion"}},
            construct,
            inspect,
            insert,
            select,
        });
    ribbon->addTab(tr("SURFACE"),
                   {G{tr("CREATE"), {"surface.patch", "create.loft"}, {"create.sweep"}},
                    G{tr("MODIFY"), {"surface.trim", "surface.stitch"}, {}}, construct, inspect, insert, select});
    ribbon->addTab(tr("MESH"), {G{tr("PREPARE"), {"mesh.repair", "insert.mesh"}, {}}, inspect, select});
    ribbon->addTab(tr("SHEET METAL"),
                   {G{tr("CREATE"), {"create.sketch", "sheet.flange"}, {}}, construct, inspect, select});
    ribbon->addTab(tr("PLASTIC"),
                   {G{tr("CREATE"), {"create.sketch", "plastic.boss"}, {"create.rib"}}, construct, inspect, select});
    ribbon->addTab(tr("UTILITIES"), {G{tr("MAKE"), {"util.print3d"}, {}}, G{tr("ADD-INS"), {"util.scripts"}, {}},
                                     G{tr("PREFERENCES"), {"util.preferences"}, {}}});
    return ribbon;
}

void Workbench::addFileCommands(CommandRegistry& commands, QMenu& menu) {
    for (const char* id : {"file.new", "file.open"}) menu.addAction(commands.get(id));
    QMenu* samples = menu.addMenu(icons::get("doc"), tr("New from Sample"));
    samples->addAction(commands.get("sample.plate"));
    samples->addAction(commands.get("sample.flange"));
    menu.addSeparator();
    for (const char* id : {"file.save", "file.saveas"}) menu.addAction(commands.get(id));
    menu.addSeparator();
    for (const char* id : {"file.export-step", "file.export-stl"}) menu.addAction(commands.get(id));
}

QWidget* Workbench::buildTopBar(CommandRegistry& commands, QTabBar*& docTabs, QWidget* parent) {
    auto* bar = new QWidget(parent);
    bar->setObjectName("TopBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 4, 8, 0);
    layout->setSpacing(2);

    auto* file = new QToolButton(bar);
    file->setIcon(icons::get("file"));
    file->setIconSize(QSize(18, 18));
    file->setToolTip(tr("File"));
    file->setAutoRaise(true);
    auto* fileMenu = new QMenu(file);
    addFileCommands(commands, *fileMenu);
    file->setMenu(fileMenu);
    file->setPopupMode(QToolButton::InstantPopup);

    layout->addWidget(barButton(bar, commands.get("view.data-panel")));
    layout->addWidget(file);
    layout->addWidget(barButton(bar, commands.get("file.save")));
    layout->addSpacing(6);
    layout->addWidget(barButton(bar, commands.get("edit.undo")));
    layout->addWidget(barButton(bar, commands.get("edit.redo")));
    layout->addSpacing(16);

    docTabs = new QTabBar(bar);
    docTabs->setObjectName("DocTabs");
    docTabs->setDrawBase(false);
    docTabs->setExpanding(false);
    docTabs->addTab(icons::get("doc"), {});
    layout->addWidget(docTabs, 0, Qt::AlignBottom);
    layout->addWidget(barButton(bar, commands.get("file.new-tab"), 14));
    layout->addStretch();

    for (const char* id : {"app.jobs", "app.notifications", "app.help"})
        layout->addWidget(barButton(bar, commands.get(id)));
    layout->addWidget(barButton(bar, commands.get("app.account"), 22));
    return bar;
}

QMenu* Workbench::buildDisplayMenu(CommandRegistry& commands, QWidget* parent) {
    auto* menu = new QMenu(parent);
    QMenu* style = menu->addMenu(tr("Visual Style"));
    for (const char* id : {"view.style-shaded", "view.style-edges", "view.style-wireframe"})
        style->addAction(commands.get(id));
    menu->addAction(commands.get("view.environment"));
    menu->addAction(commands.get("view.grid"));
    return menu;
}

void Workbench::buildMenuBar(CommandRegistry& commands, QMenuBar& menuBar) {
    // The native menu bar mirrors the ribbon so every command has a keyboard path.
    addFileCommands(commands, *menuBar.addMenu(tr("&File")));

    QMenu* edit = menuBar.addMenu(tr("&Edit"));
    edit->addAction(commands.get("edit.undo"));
    edit->addAction(commands.get("edit.redo"));
    edit->addSeparator();
    edit->addAction(commands.get("modify.delete"));
    edit->addAction(commands.get("modify.parameters"));

    QMenu* create = menuBar.addMenu(tr("&Create"));
    for (const char* id : {"create.box", "create.cylinder", "modify.fillet", "modify.chamfer", "insert.step"})
        create->addAction(commands.get(id));

    QMenu* view = menuBar.addMenu(tr("&View"));
    for (const char* id : {"view.fit", "view.home", "view.top", "view.front", "view.right", "view.grid"})
        view->addAction(commands.get(id));

    QMenu* help = menuBar.addMenu(tr("&Help"));
    help->addAction(commands.get("app.about"));
}

QStringList Workbench::navigationBar() {
    return {"nav.orbit", "nav.lookat",   "|", "nav.pan", "nav.zoom", "view.fit", "|", NavigationBar::kDisplayMenu,
            "view.grid", "nav.viewports"};
}

QStringList Workbench::markingMenuRing(const QString& repeatCommand) {
    // Clockwise from north. North repeats the last modelling command.
    return {repeatCommand,   "modify.delete",   "modify.fillet", "edit.redo",
            "create.sketch", "create.cylinder", "edit.undo",     "modify.parameters"};
}

QStringList Workbench::markingMenuList() {
    return {"nav.pan",       "nav.zoom",       "nav.orbit",       "view.fit",          "-", "create.extrude",
            "create.box",    "modify.chamfer", "inspect.measure", "modify.appearance", "-", "insert.step",
            "modify.compute"};
}
