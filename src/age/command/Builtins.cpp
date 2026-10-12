// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/command/DocumentCommands.h"
#include "age/command/FeatureCommands.h"
#include "age/command/ParameterCommands.h"
#include "age/command/SketchCommands.h"

namespace age::cmd {

namespace {

template <class... Cs>
void addAll(Registry& r) {
    (r.add<Cs>(), ...);
}

Registry makeBuiltin() {
    Registry r;
    addAll<AddParameter, SetParameter, RenameParameter, RemoveParameter>(r);
    addAll<CreateSketch, EditSketch, AddSketchPoint, AddSketchLine, AddSketchCircle, AddSketchArc, AddSketchRectangle,
           AddSketchConstraint, AddSketchDimension, SetSketchDimension, SetSketchConstruction, MoveSketchPoint,
           DescribeSketch>(r);
    addAll<CreateExtrude, EditExtrude, CreateRevolve, EditRevolve, CreateFillet, EditFillet, CreateChamfer, EditChamfer,
           ImportStep, CreateBox, CreateCylinder, RenameFeature, SuppressFeature, DeleteFeature, SetTimelineMarker>(r);
    addAll<DescribeDocument, ExportDocument, NewDocument, OpenDocument, SaveDocument, Undo, Redo>(r);
    return r;
}

} // namespace

const Registry& Registry::builtin() {
    static const Registry registry = makeBuiltin();
    return registry;
}

} // namespace age::cmd
