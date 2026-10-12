// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/model/Samples.h"

#include "age/model/Features.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace age::samples {

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

Document pillowBlock() {
    Document doc;
    auto& p = doc.parameters();
    p.add("base_l", "150 mm", UnitKind::Length, "Base length (X)");
    p.add("base_w", "56 mm", UnitKind::Length, "Base and boss width (Y)");
    p.add("base_t", "16 mm", UnitKind::Length, "Base thickness");
    p.add("base_r", "8 mm", UnitKind::Length, "Base corner radius");
    p.add("centre_h", "62 mm", UnitKind::Length, "Bore centre height");
    p.add("boss_d", "72 mm", UnitKind::Length, "Boss diameter");
    p.add("bore_d", "40 mm", UnitKind::Length, "Bore diameter");
    p.add("web_w", "88 mm", UnitKind::Length, "Web width at the base");
    p.add("web_t", "30 mm", UnitKind::Length, "Web thickness (Y)");
    p.add("bolt_spacing", "120 mm", UnitKind::Length, "Distance between bolt holes");
    p.add("bolt_d", "13 mm", UnitKind::Length, "Bolt hole diameter");
    p.add("cbore_d", "22 mm", UnitKind::Length, "Counterbore diameter");
    p.add("cbore_depth", "4 mm", UnitKind::Length, "Counterbore depth");
    p.add("nipple_d", "14 mm", UnitKind::Length, "Lubrication boss diameter");
    p.add("nipple_h", "8 mm", UnitKind::Length, "Lubrication boss height above the boss");
    p.add("oil_d", "4 mm", UnitKind::Length, "Oil hole diameter");

    auto v = [&p](const char* expr) { return p.evaluate(expr, UnitKind::Length); };
    // A point dimensioned from the sketch origin, placed where its dimensions put it.
    auto point = [&v](Sketch& s, const char* x, const char* y) {
        auto id = s.addPoint({v(x), v(y)});
        s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, id, x);
        s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, id, y);
        return id;
    };
    auto circle = [&point, &v](Sketch& s, const char* x, const char* y, const char* diameter) {
        auto c = s.addCircle(point(s, x, y), v(diameter) / 2);
        s.addDimension(ConstraintType::Diameter, c, -1, diameter);
    };
    auto sketchOn = [&doc](const char* name, SketchPlane plane, std::string offset = "0") -> SketchFeature& {
        auto& f = doc.add<SketchFeature>();
        f.name = name;
        f.plane = plane;
        f.offset = std::move(offset);
        return f;
    };
    auto extrude = [&doc](const SketchFeature& from, std::string distance, FeatureOperation op, int target,
                          bool symmetric = false) -> ExtrudeFeature& {
        auto& e = doc.add<ExtrudeFeature>();
        e.profiles.sketchId = from.id;
        e.distance = std::move(distance);
        e.symmetric = symmetric;
        e.op = {op, target};
        return e;
    };
    auto dressUp = [](auto& feature, int body, std::vector<Vec3> near, std::string size) {
        feature.body = body;
        feature.edges.mode = EdgeQuery::Mode::NearPoints;
        feature.edges.points = std::move(near);
        feature.size = std::move(size);
    };

    // Base plate with rounded corners.
    auto& baseSketch = sketchOn("Base", SketchPlane::XY);
    {
        Sketch& s = baseSketch.sketch;
        auto lines = s.addRectangle({v("-base_l / 2"), v("-base_w / 2")}, {v("base_l / 2"), v("base_w / 2")});
        auto lowerLeft = s.entity(lines[0]).p1;
        auto upperRight = s.entity(lines[1]).p2;
        s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, lowerLeft, "-base_l / 2");
        s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, lowerLeft, "-base_w / 2");
        s.addDimension(ConstraintType::HorizontalDistance, Sketch::kOrigin, upperRight, "base_l / 2");
        s.addDimension(ConstraintType::VerticalDistance, Sketch::kOrigin, upperRight, "base_w / 2");
    }
    const int body = extrude(baseSketch, "base_t", FeatureOperation::NewBody, -1).id;
    auto& corners = doc.add<FilletFeature>();
    corners.body = body;
    corners.edges.mode = EdgeQuery::Mode::ParallelTo;
    corners.edges.direction = {0, 0, 1};
    corners.size = "base_r";

    // Tapered web from the base up to the boss (XZ plane: u = x, v = z).
    auto& webSketch = sketchOn("Web", SketchPlane::XZ);
    {
        Sketch& s = webSketch.sketch;
        const sketch::EntityId pts[] = {
            point(s, "-web_w / 2", "base_t"),
            point(s, "web_w / 2", "base_t"),
            point(s, "boss_d / 2", "centre_h"),
            point(s, "-boss_d / 2", "centre_h"),
        };
        for (size_t i = 0; i < std::size(pts); ++i) s.addLine(pts[i], pts[(i + 1) % std::size(pts)]);
    }
    extrude(webSketch, "web_t", FeatureOperation::Join, body, true);
    const double webFace = v("web_t / 2"), baseTop = v("base_t");
    dressUp(doc.add<FilletFeature>(), body, {{0, webFace, baseTop}, {0, -webFace, baseTop}}, "6 mm");

    // Bearing boss and bore, both through the full base width.
    auto& bossSketch = sketchOn("Boss", SketchPlane::XZ);
    circle(bossSketch.sketch, "0", "centre_h", "boss_d");
    extrude(bossSketch, "base_w", FeatureOperation::Join, body, true);

    auto& boreSketch = sketchOn("Bore", SketchPlane::XZ);
    circle(boreSketch.sketch, "0", "centre_h", "bore_d");
    extrude(boreSketch, "base_w + 2 mm", FeatureOperation::Cut, body, true);

    const double face = v("base_w / 2"), centre = v("centre_h");
    dressUp(doc.add<ChamferFeature>(), body,
            {{0, face, centre + v("bore_d / 2")}, {0, -face, centre + v("bore_d / 2")}}, "2 mm");
    dressUp(doc.add<FilletFeature>(), body, {{0, face, centre + v("boss_d / 2")}, {0, -face, centre + v("boss_d / 2")}},
            "3 mm");

    // Counterbored bolt holes through the base.
    auto& boltSketch = sketchOn("Bolt Holes", SketchPlane::XY, "base_t");
    circle(boltSketch.sketch, "-bolt_spacing / 2", "0", "bolt_d");
    circle(boltSketch.sketch, "bolt_spacing / 2", "0", "bolt_d");
    extrude(boltSketch, "-base_t", FeatureOperation::Cut, body);

    auto& cboreSketch = sketchOn("Counterbores", SketchPlane::XY, "base_t");
    circle(cboreSketch.sketch, "-bolt_spacing / 2", "0", "cbore_d");
    circle(cboreSketch.sketch, "bolt_spacing / 2", "0", "cbore_d");
    extrude(cboreSketch, "-cbore_depth", FeatureOperation::Cut, body);

    // Lubrication boss on top of the housing, drilled through to the bore.
    auto& nippleSketch = sketchOn("Lubrication Boss", SketchPlane::XY, "centre_h + bore_d / 2 + 2 mm");
    circle(nippleSketch.sketch, "0", "0", "nipple_d");
    extrude(nippleSketch, "boss_d / 2 - bore_d / 2 - 2 mm + nipple_h", FeatureOperation::Join, body);

    auto& oilSketch = sketchOn("Oil Hole", SketchPlane::XY, "centre_h + boss_d / 2 + nipple_h");
    circle(oilSketch.sketch, "0", "0", "oil_d");
    extrude(oilSketch, "-(boss_d / 2 + nipple_h)", FeatureOperation::Cut, body);

    const double nippleTop = centre + v("boss_d / 2 + nipple_h");
    dressUp(doc.add<ChamferFeature>(), body, {{v("nipple_d / 2"), 0, nippleTop}}, "1 mm");

    return doc;
}

} // namespace age::samples
