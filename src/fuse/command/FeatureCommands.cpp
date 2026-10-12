// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/command/FeatureCommands.h"

#include "fuse/command/Lookup.h"
#include "fuse/model/Builders.h"

namespace fuse::cmd {

namespace {

BodyOperation bodyOperation(const Document& doc, FeatureOperation operation, int targetBody) {
    if (operation == FeatureOperation::NewBody) return {operation, -1};
    return {operation, resolveBody(doc, targetBody)};
}

// Shared by extrude and revolve edits: apply the optional changes, then check
// that a boolean operation still names its target.
void editSweep(const Document& doc, ProfileSelection& sel, BodyOperation& op, const std::optional<int>& sketchId,
               const std::optional<std::vector<int>>& profiles, const std::optional<FeatureOperation>& operation,
               const std::optional<int>& targetBody) {
    if (sketchId) {
        requireFeature<SketchFeature>(doc, *sketchId);
        sel.sketchId = *sketchId;
    }
    if (profiles) sel.profileIndices = *profiles;
    if (operation) op.operation = *operation;
    if (targetBody) op.targetBody = *targetBody;
    if (op.operation == FeatureOperation::NewBody)
        op.targetBody = -1;
    else if (op.targetBody < 0)
        throw Error("target_body is required to join, cut or intersect");
}

template <class T>
void editDressUp(Document& doc, int feature, const std::optional<int>& body, const std::optional<EdgeQuery>& edges,
                 const std::optional<std::string>& size, const char* sizeArgument) {
    T& f = requireFeature<T>(doc, feature);
    if (size) checkExpression(doc, sizeArgument, *size, UnitKind::Length);
    if (body) f.body = *body;
    if (edges) f.edges = *edges;
    if (size) f.size = *size;
}

} // namespace

FeatureRef CreateExtrude::run(EditContext& ctx) const {
    requireFeature<SketchFeature>(ctx.doc, sketchId);
    checkExpression(ctx.doc, "distance", distance, UnitKind::Length);
    const BodyOperation op = bodyOperation(ctx.doc, operation, targetBody);
    auto& f = ctx.doc.add<ExtrudeFeature>();
    f.profiles = {sketchId, profiles};
    f.distance = distance;
    f.symmetric = symmetric;
    f.op = op;
    return {f.id};
}

NoOutput EditExtrude::run(EditContext& ctx) const {
    auto& f = requireFeature<ExtrudeFeature>(ctx.doc, feature);
    if (distance) checkExpression(ctx.doc, "distance", *distance, UnitKind::Length);
    editSweep(ctx.doc, f.profiles, f.op, sketchId, profiles, operation, targetBody);
    if (distance) f.distance = *distance;
    if (symmetric) f.symmetric = *symmetric;
    return {};
}

FeatureRef CreateRevolve::run(EditContext& ctx) const {
    requireFeature<SketchFeature>(ctx.doc, sketchId);
    checkExpression(ctx.doc, "angle", angle, UnitKind::Angle);
    const BodyOperation op = bodyOperation(ctx.doc, operation, targetBody);
    auto& f = ctx.doc.add<RevolveFeature>();
    f.profiles = {sketchId, profiles};
    f.axisLine = axis;
    f.angle = angle;
    f.op = op;
    return {f.id};
}

NoOutput EditRevolve::run(EditContext& ctx) const {
    auto& f = requireFeature<RevolveFeature>(ctx.doc, feature);
    if (angle) checkExpression(ctx.doc, "angle", *angle, UnitKind::Angle);
    editSweep(ctx.doc, f.profiles, f.op, sketchId, profiles, operation, targetBody);
    if (axis) f.axisLine = *axis;
    if (angle) f.angle = *angle;
    return {};
}

FeatureRef CreateFillet::run(EditContext& ctx) const {
    checkExpression(ctx.doc, "radius", radius, UnitKind::Length);
    return {build::addFillet(ctx.doc, edges, radius, body).id};
}

NoOutput EditFillet::run(EditContext& ctx) const {
    editDressUp<FilletFeature>(ctx.doc, feature, body, edges, radius, "radius");
    return {};
}

FeatureRef CreateChamfer::run(EditContext& ctx) const {
    checkExpression(ctx.doc, "distance", distance, UnitKind::Length);
    return {build::addChamfer(ctx.doc, edges, distance, body).id};
}

NoOutput EditChamfer::run(EditContext& ctx) const {
    editDressUp<ChamferFeature>(ctx.doc, feature, body, edges, distance, "distance");
    return {};
}

FeatureRef ImportStep::run(EditContext& ctx) const {
    auto& f = ctx.doc.add<ImportStepFeature>();
    f.path = path;
    return {f.id};
}

FeatureRef CreateBox::run(EditContext& ctx) const {
    build::BoxSpec spec{width, depth, height, {x, y, z}, {operation, targetBody}};
    return {build::addBox(ctx.doc, spec).id};
}

FeatureRef CreateCylinder::run(EditContext& ctx) const {
    build::CylinderSpec spec{diameter, height, {x, y, z}, {operation, targetBody}};
    return {build::addCylinder(ctx.doc, spec).id};
}

NoOutput RenameFeature::run(EditContext& ctx) const {
    if (name.find_first_not_of(" \t") == std::string::npos) throw Error("a feature name cannot be empty");
    requireFeature(ctx.doc, feature).name = name;
    return {};
}

NoOutput SuppressFeature::run(EditContext& ctx) const {
    requireFeature(ctx.doc, feature).suppressed = suppressed;
    return {};
}

NoOutput DeleteFeature::run(EditContext& ctx) const {
    ctx.doc.removeFeature(feature);
    return {};
}

NoOutput SetTimelineMarker::run(EditContext& ctx) const {
    const auto count = static_cast<int>(ctx.doc.features().size());
    if (position < 0 || position > count)
        throw Error("position must be between 0 and " + std::to_string(count) + " (the number of features)");
    ctx.doc.setMarker(static_cast<size_t>(position));
    return {};
}

} // namespace fuse::cmd
