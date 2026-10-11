// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/model/Features.h"

#include "oc/core/Error.h"
#include "oc/core/Parameters.h"

#include <algorithm>

namespace oc {

namespace {

struct Named {
    FeatureType type;
    const char* name;
};
constexpr Named kFeatureNames[] = {
    {FeatureType::Sketch, "sketch"}, {FeatureType::Extrude, "extrude"}, {FeatureType::Revolve, "revolve"},
    {FeatureType::Fillet, "fillet"}, {FeatureType::Chamfer, "chamfer"}, {FeatureType::ImportStep, "import_step"},
};

struct NamedOp {
    FeatureOperation op;
    const char* name;
};
constexpr NamedOp kOpNames[] = {
    {FeatureOperation::NewBody, "new_body"},
    {FeatureOperation::Join, "join"},
    {FeatureOperation::Cut, "cut"},
    {FeatureOperation::Intersect, "intersect"},
};

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

Body& requireBody(RecomputeContext& ctx, int requested) {
    int id = requested >= 0 ? requested : ctx.lastBody;
    auto it = ctx.bodies.find(id);
    if (it == ctx.bodies.end())
        throw Error(requested >= 0 ? "target body " + std::to_string(requested) + " does not exist"
                                   : std::string("there is no body to operate on"));
    return it->second;
}

void applyOperation(RecomputeContext& ctx, const Feature& f, const BodyOperation& op, ShapePtr tool) {
    if (op.operation == FeatureOperation::NewBody) {
        ctx.bodies[f.id] = Body{f.id, "Body" + std::to_string(++ctx.bodyCounter), std::move(tool)};
        ctx.lastBody = f.id;
        return;
    }
    Body& target = requireBody(ctx, op.targetBody);
    BooleanOp b = op.operation == FeatureOperation::Join  ? BooleanOp::Join
                  : op.operation == FeatureOperation::Cut ? BooleanOp::Cut
                                                          : BooleanOp::Intersect;
    target.shape = ctx.kernel.boolean(b, target.shape, tool);
    ctx.lastBody = target.id;
}

void appendBodyDependency(std::vector<int>& deps, int body) {
    // Body ids are the ids of the features that created them.
    if (body >= 0) deps.push_back(body);
}

} // namespace

const char* featureTypeName(FeatureType t) {
    for (const auto& n : kFeatureNames)
        if (n.type == t) return n.name;
    return "unknown";
}

bool parseFeatureType(const std::string& name, FeatureType& out) {
    for (const auto& n : kFeatureNames)
        if (name == n.name) {
            out = n.type;
            return true;
        }
    return false;
}

const char* operationName(FeatureOperation op) {
    for (const auto& n : kOpNames)
        if (n.op == op) return n.name;
    return "unknown";
}

bool parseOperation(const std::string& name, FeatureOperation& out) {
    for (const auto& n : kOpNames)
        if (name == n.name) {
            out = n.op;
            return true;
        }
    return false;
}

const Feature* RecomputeContext::findFeature(int id) const {
    auto it = std::find_if(executed.begin(), executed.end(), [id](const Feature* f) { return f->id == id; });
    return it == executed.end() ? nullptr : *it;
}

void SketchFeature::execute(RecomputeContext& ctx) {
    profiles.clear();
    double off = ctx.params.evaluate(offset);
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

void ExtrudeFeature::execute(RecomputeContext& ctx) {
    const SketchFeature& sk = requireSketch(ctx, profiles.sketchId);
    double d = ctx.params.evaluate(distance);
    double start = symmetric ? -d / 2 : 0;
    ShapePtr tool = ctx.kernel.extrude(sk.resolvedPlane, selectProfiles(sk, profiles), start, d);
    applyOperation(ctx, *this, op, std::move(tool));
}

std::vector<int> ExtrudeFeature::dependencies() const {
    std::vector<int> deps{profiles.sketchId};
    if (op.operation != FeatureOperation::NewBody) appendBodyDependency(deps, op.targetBody);
    return deps;
}

void RevolveFeature::execute(RecomputeContext& ctx) {
    const SketchFeature& sk = requireSketch(ctx, profiles.sketchId);
    const auto& axis = sk.sketch.entity(axisLine);
    if (axis.type != sketch::EntityType::Line) throw Error("revolve axis must be a sketch line");
    Vec2 a = sk.sketch.point(axis.p1), b = sk.sketch.point(axis.p2);
    double ang = ctx.params.evaluate(angle);
    ShapePtr tool = ctx.kernel.revolve(sk.resolvedPlane, selectProfiles(sk, profiles), a, b - a, ang);
    applyOperation(ctx, *this, op, std::move(tool));
}

std::vector<int> RevolveFeature::dependencies() const {
    std::vector<int> deps{profiles.sketchId};
    if (op.operation != FeatureOperation::NewBody) appendBodyDependency(deps, op.targetBody);
    return deps;
}

void FilletFeature::execute(RecomputeContext& ctx) {
    Body& b = requireBody(ctx, body);
    b.shape = ctx.kernel.fillet(b.shape, edges, ctx.params.evaluate(radius));
    ctx.lastBody = b.id;
}

std::vector<int> FilletFeature::dependencies() const {
    std::vector<int> deps;
    appendBodyDependency(deps, body);
    return deps;
}

void ChamferFeature::execute(RecomputeContext& ctx) {
    Body& b = requireBody(ctx, body);
    b.shape = ctx.kernel.chamfer(b.shape, edges, ctx.params.evaluate(distance));
    ctx.lastBody = b.id;
}

std::vector<int> ChamferFeature::dependencies() const {
    std::vector<int> deps;
    appendBodyDependency(deps, body);
    return deps;
}

void ImportStepFeature::execute(RecomputeContext& ctx) {
    if (path.empty()) throw Error("no STEP file selected");
    applyOperation(ctx, *this, {}, ctx.kernel.importStep(path));
}

} // namespace oc
