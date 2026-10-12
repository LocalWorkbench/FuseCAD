// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/command/DocumentCommands.h"
#include "fuse/command/FeatureCommands.h"
#include "fuse/command/ParameterCommands.h"
#include "fuse/command/SketchCommands.h"

namespace fuse::cmd {

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

} // namespace fuse::cmd
