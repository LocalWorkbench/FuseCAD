// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/model/Features.h"

#include "fuse/core/EnumTable.h"
#include "fuse/core/Error.h"
#include "fuse/core/Parameters.h"

#include <algorithm>

namespace fuse {

namespace {

struct FeatureTypeInfo {
    FeatureType value;
    const char* name;        // file format key
    const char* displayName; // base of default feature names
};
constexpr auto kFeatureTypes = makeEnumTable<FeatureTypeInfo>({
    {FeatureType::Sketch, "sketch", "Sketch"},
    {FeatureType::Extrude, "extrude", "Extrude"},
    {FeatureType::Revolve, "revolve", "Revolve"},
    {FeatureType::Fillet, "fillet", "Fillet"},
    {FeatureType::Chamfer, "chamfer", "Chamfer"},
    {FeatureType::ImportStep, "import_step", "Import"},
});

constexpr auto kOperations = makeEnumTable<EnumName<FeatureOperation>>({
    {FeatureOperation::NewBody, "new_body"},
    {FeatureOperation::Join, "join"},
    {FeatureOperation::Cut, "cut"},
    {FeatureOperation::Intersect, "intersect"},
});

const SketchFeature& requireSketch(const RecomputeContext& ctx, int id) {
    const auto* sk = featureCast<SketchFeature>(ctx.findFeature(id));
    if (!sk) throw Error("referenced sketch (feature " + std::to_string(id) + ") is missing or after this feature");
    if (!sk->healthy()) throw Error("referenced sketch '" + sk->name + "' has errors");
    return *sk;
}

std::vector<sketch::Profile> selectProfiles(const SketchFeature& sk, const ProfileSelection& sel) {
    if (sk.profiles.empty()) throw Error("sketch '" + sk.name + "' has no closed profiles");
    if (sel.profileIndices.empty()) return sk.profiles;
    std::vector<sketch::Profile> out;
    for (int i : sel.profileIndices) {
        if (i < 0 || static_cast<size_t>(i) >= sk.profiles.size())
            throw Error("profile " + std::to_string(i) + " no longer exists in sketch '" + sk.name + "'");
        out.push_back(sk.profiles[i]);
    }
    return out;
}

Body& requireBody(RecomputeContext& ctx, int id) {
    if (id < 0) throw Error("no body selected");
    auto it = ctx.bodies.find(id);
    if (it == ctx.bodies.end()) throw Error("body " + std::to_string(id) + " does not exist at this point");
    return it->second;
}

BooleanOp booleanOp(FeatureOperation op) {
    switch (op) {
    case FeatureOperation::Join:
        return BooleanOp::Join;
    case FeatureOperation::Cut:
        return BooleanOp::Cut;
    case FeatureOperation::Intersect:
        return BooleanOp::Intersect;
    case FeatureOperation::NewBody:
        break;
    }
    throw Error("a new body is not a boolean operation");
}

void applyOperation(RecomputeContext& ctx, const Feature& f, const BodyOperation& op, ShapePtr tool) {
    if (op.operation == FeatureOperation::NewBody) {
        ctx.bodies[f.id] = Body{f.id, "Body" + std::to_string(++ctx.bodyCounter), std::move(tool)};
        return;
    }
    Body& target = requireBody(ctx, op.targetBody);
    target.shape = ctx.kernel.boolean(booleanOp(op.operation), target.shape, tool);
}

// Extrude and revolve: the sketch, plus the target body when modifying one
// (body ids are the ids of the features that created them).
std::vector<int> sweepDependencies(const ProfileSelection& profiles, const BodyOperation& op) {
    std::vector<int> deps{profiles.sketchId};
    if (op.operation != FeatureOperation::NewBody) deps.push_back(op.targetBody);
    return deps;
}

int sweepOutputBody(const Feature& f, const BodyOperation& op) {
    return op.operation == FeatureOperation::NewBody ? f.id : op.targetBody;
}

} // namespace

const char* featureTypeName(FeatureType t) { return kFeatureTypes.name(t); }
const char* featureDisplayName(FeatureType t) { return kFeatureTypes[t].displayName; }
std::optional<FeatureType> parseFeatureType(std::string_view name) { return kFeatureTypes.parse(name); }

const char* operationName(FeatureOperation op) { return kOperations.name(op); }
std::optional<FeatureOperation> parseOperation(std::string_view name) { return kOperations.parse(name); }

const Feature* RecomputeContext::findFeature(int id) const {
    auto it = std::find_if(executed.begin(), executed.end(), [id](const Feature* f) { return f->id == id; });
    return it == executed.end() ? nullptr : *it;
}

void SketchFeature::execute(RecomputeContext& ctx) {
    profiles.clear();
    double off = ctx.params.evaluate(offset, UnitKind::Length);
    switch (plane) {
    case SketchPlane::XY:
        resolvedPlane = Plane::XY(off);
        break;
    case SketchPlane::XZ:
        resolvedPlane = Plane::XZ(off);
        break;
    case SketchPlane::YZ:
        resolvedPlane = Plane::YZ(off);
        break;
    }
    solveResult = sketch.solve(&ctx.params);
    if (!solveResult.ok()) throw Error(solveResult.message);
    profiles = sketch.profiles();
}

void SketchFeature::visitExpressions(const ExpressionVisitor& fn) {
    fn(offset);
    sketch.visitExpressions(fn);
}

void ExtrudeFeature::execute(RecomputeContext& ctx) {
    const SketchFeature& sk = requireSketch(ctx, profiles.sketchId);
    double d = ctx.params.evaluate(distance, UnitKind::Length);
    double start = symmetric ? -d / 2 : 0;
    ShapePtr tool = ctx.kernel.extrude(sk.resolvedPlane, selectProfiles(sk, profiles), start, d);
    applyOperation(ctx, *this, op, std::move(tool));
}

std::vector<int> ExtrudeFeature::dependencies() const { return sweepDependencies(profiles, op); }
int ExtrudeFeature::outputBody() const { return sweepOutputBody(*this, op); }

void RevolveFeature::execute(RecomputeContext& ctx) {
    const SketchFeature& sk = requireSketch(ctx, profiles.sketchId);
    const auto& axis = sk.sketch.entity(axisLine);
    if (axis.type != sketch::EntityType::Line) throw Error("revolve axis must be a sketch line");
    Vec2 a = sk.sketch.point(axis.p1), b = sk.sketch.point(axis.p2);
    double ang = ctx.params.evaluate(angle, UnitKind::Angle);
    ShapePtr tool = ctx.kernel.revolve(sk.resolvedPlane, selectProfiles(sk, profiles), a, b - a, ang);
    applyOperation(ctx, *this, op, std::move(tool));
}

std::vector<int> RevolveFeature::dependencies() const { return sweepDependencies(profiles, op); }
int RevolveFeature::outputBody() const { return sweepOutputBody(*this, op); }

void DressUpFeature::execute(RecomputeContext& ctx) {
    Body& b = requireBody(ctx, body);
    b.shape = apply(ctx.kernel, b.shape, ctx.params.evaluate(size, UnitKind::Length));
}

void ImportStepFeature::execute(RecomputeContext& ctx) {
    if (path.empty()) throw Error("no STEP file selected");
    applyOperation(ctx, *this, {}, ctx.kernel.importStep(path));
}

} // namespace fuse
