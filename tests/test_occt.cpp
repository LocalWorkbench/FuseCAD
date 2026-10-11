// SPDX-License-Identifier: GPL-3.0-or-later
//
// Integration tests against the real OpenCASCADE kernel: volumes are checked
// against closed-form values, so these catch genuine geometry regressions.

#include "fuse/core/Error.h"
#include "fuse/model/Builders.h"
#include "fuse/model/Document.h"
#include "fuse/model/Features.h"
#include "fuse/model/Samples.h"
#include "fuse/occt/OcctKernel.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

using namespace fuse;

namespace {

std::filesystem::path tmpPath(const char* name) {
    const char* dir = std::getenv("FUSE_TEST_TMPDIR"); // NOLINT(concurrency-mt-unsafe): single-threaded tests
    return std::filesystem::path(dir ? dir : std::filesystem::temp_directory_path().string()) / name;
}

// Plate with filleted vertical corners and a through-hole.
double plateVolume(double w, double h, double t, double r, double d) {
    return (w * h - (4 - kPi) * r * r) * t - kPi * d * d / 4 * t;
}

const Body& onlyBody(const Document& doc) {
    REQUIRE(doc.bodies().size() == 1);
    return doc.bodies().begin()->second;
}

} // namespace

TEST_CASE("mounting plate matches its analytic volume") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    RecomputeReport r = doc.recompute(*kernel);
    REQUIRE_MESSAGE(r.ok(), (r.messages.empty() ? "" : r.messages.front()));

    const Body& b = onlyBody(doc);
    CHECK(kernel->isValid(b.shape));
    CHECK(kernel->volume(b.shape) == doctest::Approx(plateVolume(80, 50, 10, 5, 20)).epsilon(1e-6));

    BoundingBox box = kernel->boundingBox(b.shape);
    CHECK(box.max.x - box.min.x == doctest::Approx(80).epsilon(1e-4));
    CHECK(box.max.z - box.min.z == doctest::Approx(10).epsilon(1e-4));
}

TEST_CASE("parameter edits regenerate the solid") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    doc.parameters().setExpression("width", "120 mm");
    doc.parameters().setExpression("thickness", "6 mm");
    doc.parameters().setExpression("corner_r", "8 mm");
    REQUIRE(doc.recompute(*kernel).ok());
    CHECK(kernel->volume(onlyBody(doc).shape) == doctest::Approx(plateVolume(120, 50, 6, 8, 20)).epsilon(1e-6));
}

TEST_CASE("oversized fillets fail cleanly") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    doc.parameters().setExpression("corner_r", "40 mm");
    RecomputeReport r = doc.recompute(*kernel);
    CHECK_FALSE(r.ok());
    CHECK_FALSE(doc.features()[2]->healthy());
    CHECK(doc.features()[4]->healthy()); // the hole still cuts the unfilleted body
}

TEST_CASE("symmetric extrude and revolve") {
    auto kernel = occt::createKernel();
    Document doc;
    auto& sk = doc.add<SketchFeature>();
    sk.plane = SketchPlane::XZ;
    // A 10 x 20 rectangle starting 5 mm off the Z axis; revolving it about Z
    // yields a tube with inner radius 5 and outer radius 15.
    sk.sketch.addRectangle({5, 0}, {15, 20});
    sketch::EntityId axis = sk.sketch.addLine({0, 0}, {0, 1});
    sk.sketch.setConstruction(axis, true);

    auto& rev = doc.add<RevolveFeature>();
    rev.profiles.sketchId = sk.id;
    rev.axisLine = axis;

    auto& block = doc.add<ExtrudeFeature>();
    block.profiles.sketchId = sk.id;
    block.distance = "4 mm";
    block.symmetric = true;

    REQUIRE(doc.recompute(*kernel).ok());
    REQUIRE(doc.bodies().size() == 2);
    const ShapePtr& tube = doc.bodies().at(rev.id).shape;
    const ShapePtr& slab = doc.bodies().at(block.id).shape;
    CHECK(kernel->volume(tube) == doctest::Approx(kPi * (15 * 15 - 5 * 5) * 20).epsilon(1e-6));
    CHECK(kernel->volume(slab) == doctest::Approx(10 * 20 * 4).epsilon(1e-6));
    BoundingBox bb = kernel->boundingBox(slab);
    CHECK(bb.min.y == doctest::Approx(-2).epsilon(1e-4));
    CHECK(bb.max.y == doctest::Approx(2).epsilon(1e-4));
}

TEST_CASE("join and intersect") {
    auto kernel = occt::createKernel();
    Document doc;
    auto& sk = doc.add<SketchFeature>();
    sk.sketch.addRectangle({0, 0}, {10, 10});
    auto& sk2 = doc.add<SketchFeature>();
    sk2.sketch.addRectangle({5, 0}, {15, 10});

    auto& a = doc.add<ExtrudeFeature>();
    a.profiles.sketchId = sk.id;
    a.distance = "10";
    auto& b = doc.add<ExtrudeFeature>();
    b.profiles.sketchId = sk2.id;
    b.distance = "10";
    b.op.operation = FeatureOperation::Join;
    b.op.targetBody = a.id;

    REQUIRE(doc.recompute(*kernel).ok());
    const ShapePtr& joined = onlyBody(doc).shape;
    CHECK(kernel->volume(joined) == doctest::Approx(1500).epsilon(1e-6));
    CHECK(kernel->countFaces(joined) == 6); // coplanar faces merged

    b.op.operation = FeatureOperation::Intersect;
    REQUIRE(doc.recompute(*kernel).ok());
    CHECK(kernel->volume(onlyBody(doc).shape) == doctest::Approx(500).epsilon(1e-6));
}

TEST_CASE("chamfer edges near points") {
    auto kernel = occt::createKernel();
    Document doc;
    auto& sk = doc.add<SketchFeature>();
    sk.sketch.addRectangle({0, 0}, {10, 10});
    auto& e = doc.add<ExtrudeFeature>();
    e.profiles.sketchId = sk.id;
    e.distance = "10";
    auto& c = doc.add<ChamferFeature>();
    c.edges.mode = EdgeQuery::Mode::NearPoints;
    c.edges.points = {{5, 0, 10}}; // top front edge
    c.body = e.id;
    c.size = "2 mm";
    REQUIRE(doc.recompute(*kernel).ok());
    CHECK(kernel->volume(onlyBody(doc).shape) == doctest::Approx(1000 - 2.0 * 2.0 / 2 * 10).epsilon(1e-6));
}

TEST_CASE("STEP and STL export, STEP import round trip") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    REQUIRE(doc.recompute(*kernel).ok());
    const ShapePtr& s = onlyBody(doc).shape;
    double v = kernel->volume(s);

    auto step = tmpPath("plate_roundtrip.step");
    auto stl = tmpPath("plate_roundtrip.stl");
    kernel->exportStep({s}, step.string());
    kernel->exportStl({s}, stl.string(), kDefaultStlDeflection);
    CHECK(std::filesystem::file_size(step) > 0);
    CHECK(std::filesystem::file_size(stl) > 0);

    Document imported;
    auto& imp = imported.add<ImportStepFeature>();
    imp.path = step.string();
    REQUIRE(imported.recompute(*kernel).ok());
    CHECK(kernel->volume(onlyBody(imported).shape) == doctest::Approx(v).epsilon(1e-6));

    std::filesystem::remove(step);
    std::filesystem::remove(stl);
}

TEST_CASE("tessellation produces a consistent mesh") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    REQUIRE(doc.recompute(*kernel).ok());
    Mesh m = kernel->tessellate(onlyBody(doc).shape, 0.1);
    REQUIRE_FALSE(m.indices.empty());
    CHECK(m.positions.size() == m.normals.size());
    CHECK(m.indices.size() % 3 == 0);
    for (auto i : m.indices) CHECK(i < m.positions.size() / 3);
    CHECK_FALSE(m.edges.empty());
}

TEST_CASE("booleans that leave no solid are errors") {
    auto kernel = occt::createKernel();
    auto block = [&](Vec2 lo, Vec2 hi) {
        sketch::Profile p;
        const Vec2 corners[] = {lo, {hi.x, lo.y}, hi, {lo.x, hi.y}};
        for (int i = 0; i < 4; ++i) p.outer.edges.push_back({.start = corners[i], .end = corners[(i + 1) % 4]});
        return kernel->extrude(Plane::XY(), {p}, 0, 10);
    };
    const ShapePtr inner = block({0, 0}, {10, 10});
    const ShapePtr outer = block({-5, -5}, {15, 15});
    const ShapePtr apart = block({50, 50}, {60, 60});

    CHECK_THROWS_WITH_AS(kernel->boolean(BooleanOp::Cut, inner, outer), "the cut removes the whole body", Error);
    CHECK_THROWS_WITH_AS(kernel->boolean(BooleanOp::Intersect, inner, apart),
                         "the bodies do not overlap, so their intersection is empty", Error);
    CHECK(kernel->volume(kernel->boolean(BooleanOp::Cut, outer, inner)) == doctest::Approx(1000.0 * 3).epsilon(1e-6));
}

TEST_CASE("meshing does not modify the shape") {
    auto kernel = occt::createKernel();
    Document doc = samples::mountingPlate();
    REQUIRE(doc.recompute(*kernel).ok());
    const ShapePtr& s = onlyBody(doc).shape;
    Mesh coarse = kernel->tessellate(s, 1.0);
    Mesh fine = kernel->tessellate(s, 0.01);
    CHECK(fine.indices.size() > coarse.indices.size());
    // A coarser request after a finer one must not reuse the finer mesh.
    CHECK(kernel->tessellate(s, 1.0).indices.size() == coarse.indices.size());
}

TEST_CASE("kernel errors surface as fuse::Error") {
    auto kernel = occt::createKernel();
    CHECK_THROWS_AS(kernel->importStep("/nonexistent/file.step"), Error);
    CHECK_THROWS_AS(kernel->extrude(Plane::XY(), {}, 0, 10), Error);
}

TEST_CASE("flange sample matches its analytic volume") {
    auto kernel = occt::createKernel();
    Document doc = samples::flange();
    RecomputeReport r = doc.recompute(*kernel);
    REQUIRE_MESSAGE(r.ok(), (r.messages.empty() ? "" : r.messages.front()));
    const double F = 50, B = 12.5, H = 25, ft = 12, hh = 35, bolt = 5;
    const double expected = kPi * (F * F - B * B) * ft + kPi * (H * H - B * B) * (hh - ft) - 4 * kPi * bolt * bolt * ft;
    const Body& b = onlyBody(doc);
    CHECK(kernel->isValid(b.shape));
    CHECK(kernel->volume(b.shape) == doctest::Approx(expected).epsilon(1e-6));
}

TEST_CASE("builders: box with a cylindrical hole, filleted") {
    auto kernel = occt::createKernel();
    Document doc;
    doc.parameters().add("size", "40 mm");

    build::BoxSpec box;
    box.width = "size";
    box.depth = "size";
    box.height = "10 mm";
    box.at.x = "-5 mm"; // off-centre, negative coordinates are fine
    auto& boxFeature = build::addBox(doc, box);

    build::CylinderSpec hole;
    hole.diameter = "size / 4";
    hole.height = "10 mm";
    hole.at.x = "-5 mm";
    hole.op.operation = FeatureOperation::Cut;
    build::addCylinder(doc, hole);

    EdgeQuery vertical;
    vertical.mode = EdgeQuery::Mode::ParallelTo;
    build::addFillet(doc, vertical, "2 mm");

    REQUIRE(doc.recompute(*kernel).ok());
    const Body& b = onlyBody(doc);
    CHECK(b.id == boxFeature.id);
    const double expected = (40.0 * 40 - (4 - kPi) * 4) * 10 - kPi * 25 * 10;
    CHECK(kernel->volume(b.shape) == doctest::Approx(expected).epsilon(1e-6));
    BoundingBox bb = kernel->boundingBox(b.shape);
    CHECK(bb.min.x == doctest::Approx(-25).epsilon(1e-4));
    CHECK(bb.max.x == doctest::Approx(15).epsilon(1e-4));

    doc.parameters().setExpression("size", "60 mm");
    REQUIRE(doc.recompute(*kernel).ok());
    CHECK(kernel->volume(onlyBody(doc).shape) ==
          doctest::Approx((60.0 * 60 - (4 - kPi) * 4) * 10 - kPi * 7.5 * 7.5 * 10).epsilon(1e-6));
}
