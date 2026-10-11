// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/model/Builders.h"

#include "fuse/core/Error.h"

#include <utility>

namespace fuse::build {

namespace {

using sketch::ConstraintType;
using sketch::EntityId;
using sketch::Sketch;

// Evaluates (and so validates) a length expression. The value only places the
// initial geometry; the solver enforces the expression itself (and keeps
// tracking it as parameters change).
double length(const Document& doc, const std::string& expr) {
    return doc.parameters().evaluate(expr, UnitKind::Length);
}

int resolveBody(const Document& doc, int body) {
    if (body != kLatestBody) return body;
    const int latest = doc.latestBody();
    if (latest < 0) throw Error("there is no body to operate on");
    return latest;
}

BodyOperation resolve(const Document& doc, BodyOperation op) {
    if (op.operation != FeatureOperation::NewBody) op.targetBody = resolveBody(doc, op.targetBody);
    return op;
}

// The placement's centre (x, y), with its base height validated too.
Vec2 placementCentre(const Document& doc, const Placement& at) {
    length(doc, at.z);
    return {length(doc, at.x), length(doc, at.y)};
}

SketchFeature& addPlacedSketch(Document& doc, const Placement& at) {
    auto& sk = doc.add<SketchFeature>();
    sk.offset = at.z;
    return sk;
}

// A construction point at `position`, locked to (at.x, at.y) by signed dimensions from the origin.
EntityId addCentre(Sketch& s, const Placement& at, Vec2 position) {
    EntityId c = s.addPoint(position);
    s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, c, at.x);
    s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, c, at.y);
    return c;
}

ExtrudeFeature& addExtrude(Document& doc, const SketchFeature& sk, const std::string& height, const BodyOperation& op) {
    auto& ex = doc.add<ExtrudeFeature>();
    ex.profiles.sketchId = sk.id;
    ex.distance = height;
    ex.op = op;
    return ex;
}

template <class T>
T& addDressUp(Document& doc, EdgeQuery edges, std::string size, int body) {
    length(doc, size);
    const int target = resolveBody(doc, body);
    auto& f = doc.add<T>();
    f.edges = std::move(edges);
    f.size = std::move(size);
    f.body = target;
    return f;
}

} // namespace

ExtrudeFeature& addBox(Document& doc, const BoxSpec& spec) {
    const double w = length(doc, spec.width), d = length(doc, spec.depth);
    if (w <= 0 || d <= 0) throw Error("box width and depth must be positive");
    length(doc, spec.height);
    const Vec2 c = placementCentre(doc, spec.at);
    const BodyOperation op = resolve(doc, spec.op);

    auto& sk = addPlacedSketch(doc, spec.at);
    Sketch& s = sk.sketch;
    const EntityId centre = addCentre(s, spec.at, c);
    auto lines = s.addRectangle({c.x - w / 2, c.y - d / 2}, {c.x + w / 2, c.y + d / 2});
    const EntityId a = s.entity(lines[0]).p1, b = s.entity(lines[0]).p2, top = s.entity(lines[1]).p2;
    s.addDimension(ConstraintType::HorizontalDistance, a, b, spec.width);
    s.addDimension(ConstraintType::VerticalDistance, b, top, spec.depth);
    // Centre the rectangle: the centre point is the midpoint of a construction diagonal.
    const EntityId diagonal = s.addLine(a, top);
    s.setConstruction(diagonal, true);
    s.addConstraint({ConstraintType::Midpoint, centre, diagonal});

    return addExtrude(doc, sk, spec.height, op);
}

ExtrudeFeature& addCylinder(Document& doc, const CylinderSpec& spec) {
    const double dia = length(doc, spec.diameter);
    if (dia <= 0) throw Error("cylinder diameter must be positive");
    length(doc, spec.height);
    const Vec2 c = placementCentre(doc, spec.at);
    const BodyOperation op = resolve(doc, spec.op);

    auto& sk = addPlacedSketch(doc, spec.at);
    Sketch& s = sk.sketch;
    const EntityId centre = addCentre(s, spec.at, c);
    const EntityId circle = s.addCircle(centre, dia / 2);
    s.addDimension(ConstraintType::Diameter, circle, -1, spec.diameter);

    return addExtrude(doc, sk, spec.height, op);
}

FilletFeature& addFillet(Document& doc, EdgeQuery edges, std::string radius, int body) {
    return addDressUp<FilletFeature>(doc, std::move(edges), std::move(radius), body);
}

ChamferFeature& addChamfer(Document& doc, EdgeQuery edges, std::string distance, int body) {
    return addDressUp<ChamferFeature>(doc, std::move(edges), std::move(distance), body);
}

} // namespace fuse::build
