// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/model/Builders.h"

#include "oc/core/Error.h"

#include <initializer_list>

namespace oc::build {

namespace {

using sketch::ConstraintType;
using sketch::EntityId;
using sketch::Sketch;

// Evaluate for initial geometry placement only; the solver enforces the real
// values (and keeps tracking them as parameters change).
double initial(const Document& doc, const std::string& expr) { return doc.parameters().evaluate(expr); }

void validate(const Document& doc, std::initializer_list<const std::string*> exprs) {
    for (const std::string* e : exprs) initial(doc, *e); // throws oc::Error with a clear message
}

SketchFeature& addPlacedSketch(Document& doc, const Placement& at) {
    auto& sk = doc.add<SketchFeature>();
    sk.offset = at.z;
    return sk;
}

// A construction point locked to (at.x, at.y) by signed dimensions from the origin.
EntityId addCentre(Sketch& s, const Document& doc, const Placement& at) {
    EntityId c = s.addPoint({initial(doc, at.x), initial(doc, at.y)});
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

} // namespace

ExtrudeFeature& addBox(Document& doc, const BoxSpec& spec) {
    validate(doc, {&spec.width, &spec.depth, &spec.height, &spec.at.x, &spec.at.y, &spec.at.z});
    const double w = initial(doc, spec.width), d = initial(doc, spec.depth);
    if (w <= 0 || d <= 0) throw Error("box width and depth must be positive");

    auto& sk = addPlacedSketch(doc, spec.at);
    Sketch& s = sk.sketch;
    const EntityId centre = addCentre(s, doc, spec.at);
    const Vec2 c = s.point(centre);
    auto lines = s.addRectangle({c.x - w / 2, c.y - d / 2}, {c.x + w / 2, c.y + d / 2});
    const EntityId a = s.entity(lines[0]).p1, b = s.entity(lines[0]).p2, top = s.entity(lines[1]).p2;
    s.addDimension(ConstraintType::HorizontalDistance, a, b, spec.width);
    s.addDimension(ConstraintType::VerticalDistance, b, top, spec.depth);
    // Centre the rectangle: the centre point is the midpoint of a construction diagonal.
    const EntityId diagonal = s.addLine(a, top);
    s.setConstruction(diagonal, true);
    s.addConstraint({ConstraintType::Midpoint, centre, diagonal});

    return addExtrude(doc, sk, spec.height, spec.op);
}

ExtrudeFeature& addCylinder(Document& doc, const CylinderSpec& spec) {
    validate(doc, {&spec.diameter, &spec.height, &spec.at.x, &spec.at.y, &spec.at.z});
    const double dia = initial(doc, spec.diameter);
    if (dia <= 0) throw Error("cylinder diameter must be positive");

    auto& sk = addPlacedSketch(doc, spec.at);
    Sketch& s = sk.sketch;
    const EntityId centre = addCentre(s, doc, spec.at);
    const EntityId circle = s.addCircle(centre, dia / 2);
    s.addDimension(ConstraintType::Diameter, circle, -1, spec.diameter);

    return addExtrude(doc, sk, spec.height, spec.op);
}

FilletFeature& addFillet(Document& doc, EdgeQuery edges, std::string radius, int body) {
    initial(doc, radius);
    auto& f = doc.add<FilletFeature>();
    f.edges = std::move(edges);
    f.radius = std::move(radius);
    f.body = body;
    return f;
}

ChamferFeature& addChamfer(Document& doc, EdgeQuery edges, std::string distance, int body) {
    initial(doc, distance);
    auto& c = doc.add<ChamferFeature>();
    c.edges = std::move(edges);
    c.distance = std::move(distance);
    c.body = body;
    return c;
}

} // namespace oc::build
