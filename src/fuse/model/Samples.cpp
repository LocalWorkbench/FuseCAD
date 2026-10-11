// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/model/Samples.h"

#include "fuse/model/Features.h"

namespace fuse::samples {

using sketch::ConstraintType;
using sketch::Sketch;

Document mountingPlate() {
    Document doc;
    auto& p = doc.parameters();
    p.add("width", "80 mm", UnitKind::Length, "Plate width (X)");
    p.add("height", "50 mm", UnitKind::Length, "Plate height (Y)");
    p.add("thickness", "10 mm", UnitKind::Length, "Plate thickness (Z)");
    p.add("corner_r", "5 mm", UnitKind::Length, "Corner fillet radius");
    p.add("hole_d", "20 mm", UnitKind::Length, "Centre hole diameter");

    // Initial geometry is placed at the parameter values; the dimensions keep it there.
    const double w = p.value("width"), h = p.value("height");

    // Sketch1: the outline, anchored at the origin.
    auto& outline = doc.add<SketchFeature>();
    {
        Sketch& s = outline.sketch;
        auto lines = s.addRectangle({0, 0}, {w, h});
        auto corner = s.entity(lines[0]).p1;
        auto right = s.entity(lines[1]).p1;
        auto top = s.entity(lines[1]).p2;
        s.addConstraint({ConstraintType::Coincident, corner, Sketch::kOrigin});
        s.addDimension(ConstraintType::HorizontalDistance, corner, right, "width");
        s.addDimension(ConstraintType::VerticalDistance, right, top, "height");
    }

    auto& body = doc.add<ExtrudeFeature>();
    body.profiles.sketchId = outline.id;
    body.distance = "thickness";

    auto& corners = doc.add<FilletFeature>();
    corners.body = body.id;
    corners.edges.mode = EdgeQuery::Mode::ParallelTo;
    corners.edges.direction = {0, 0, 1};
    corners.size = "corner_r";

    // Sketch2: the hole, on the top face plane.
    auto& holeSketch = doc.add<SketchFeature>();
    holeSketch.offset = "thickness";
    {
        Sketch& s = holeSketch.sketch;
        auto center = s.addPoint({w / 2, h / 2});
        auto circle = s.addCircle(center, p.value("hole_d") / 2);
        s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, center, "width / 2");
        s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, center, "height / 2");
        s.addDimension(ConstraintType::Diameter, circle, -1, "hole_d");
    }

    auto& hole = doc.add<ExtrudeFeature>();
    hole.profiles.sketchId = holeSketch.id;
    hole.distance = "-thickness";
    hole.op.operation = FeatureOperation::Cut;
    hole.op.targetBody = body.id;

    return doc;
}

Document flange() {
    Document doc;
    auto& p = doc.parameters();
    p.add("flange_d", "100 mm", UnitKind::Length, "Flange outer diameter");
    p.add("flange_t", "12 mm", UnitKind::Length, "Flange thickness");
    p.add("hub_d", "50 mm", UnitKind::Length, "Hub outer diameter");
    p.add("hub_h", "35 mm", UnitKind::Length, "Overall height");
    p.add("bore_d", "25 mm", UnitKind::Length, "Bore diameter");
    p.add("bolt_circle", "75 mm", UnitKind::Length, "Bolt circle diameter");
    p.add("bolt_d", "10 mm", UnitKind::Length, "Bolt hole diameter");

    // Initial geometry is placed where the dimension expressions put it.
    auto at = [&p](const char* x, const char* y) -> Vec2 {
        return {p.evaluate(x, UnitKind::Length), p.evaluate(y, UnitKind::Length)};
    };

    // Sketch1 (XZ plane: u = radius, v = height): the half cross-section.
    auto& section = doc.add<SketchFeature>();
    section.plane = SketchPlane::XZ;
    sketch::EntityId axis = -1;
    {
        Sketch& s = section.sketch;
        struct Corner {
            const char* r = nullptr;
            const char* z = nullptr;
        };
        const Corner corners[] = {
            {"bore_d / 2", "0"},       {"flange_d / 2", "0"},  {"flange_d / 2", "flange_t"},
            {"hub_d / 2", "flange_t"}, {"hub_d / 2", "hub_h"}, {"bore_d / 2", "hub_h"},
        };
        std::vector<sketch::EntityId> pts;
        for (const auto& c : corners) {
            auto id = s.addPoint(at(c.r, c.z));
            s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, id, c.r);
            s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, id, c.z);
            pts.push_back(id);
        }
        for (size_t i = 0; i < pts.size(); ++i) s.addLine(pts[i], pts[(i + 1) % pts.size()]);
        axis = s.addLine(Sketch::kOrigin, s.addPoint({0, p.value("hub_h")})); // length is free
        s.addConstraint({ConstraintType::Vertical, axis});
        s.setConstruction(axis, true);
    }

    auto& body = doc.add<RevolveFeature>();
    body.profiles.sketchId = section.id;
    body.axisLine = axis;

    // Sketch2 (XY plane): four bolt holes on the bolt circle.
    auto& holes = doc.add<SketchFeature>();
    {
        Sketch& s = holes.sketch;
        struct Hole {
            const char* x = nullptr;
            const char* y = nullptr;
        };
        const Hole positions[] = {
            {"bolt_circle / 2", "0"},
            {"0", "bolt_circle / 2"},
            {"-bolt_circle / 2", "0"},
            {"0", "-bolt_circle / 2"},
        };
        for (const auto& h : positions) {
            auto centre = s.addPoint(at(h.x, h.y));
            s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, centre, h.x);
            s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, centre, h.y);
            s.addDimension(ConstraintType::Diameter, s.addCircle(centre, p.value("bolt_d") / 2), -1, "bolt_d");
        }
    }

    auto& cut = doc.add<ExtrudeFeature>();
    cut.profiles.sketchId = holes.id;
    cut.distance = "flange_t";
    cut.op.operation = FeatureOperation::Cut;
    cut.op.targetBody = body.id;

    return doc;
}

} // namespace fuse::samples
