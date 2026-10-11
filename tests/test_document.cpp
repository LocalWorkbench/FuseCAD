// SPDX-License-Identifier: GPL-3.0-or-later
#include "FakeKernel.h"
#include "oc/core/Error.h"
#include "oc/model/Builders.h"
#include "oc/model/Document.h"
#include "oc/model/Features.h"
#include "oc/model/Samples.h"

#include <doctest/doctest.h>

using namespace oc;
using oc::testing::FakeKernel;

namespace {

// Expected nominal volume from the fake kernel: plate extrude minus hole extrude.
double fakePlateVolume(double w, double h, double t, double d) { return w * h * t - kPi * d * d / 4 * t; }

} // namespace

TEST_CASE("the sample document recomputes through the whole timeline") {
    Document doc = samples::mountingPlate();
    FakeKernel k;
    RecomputeReport r = doc.recompute(k);
    REQUIRE_MESSAGE(r.ok(), (r.messages.empty() ? "" : r.messages.front()));
    CHECK(r.executed == 5);
    CHECK(k.log == std::vector<std::string>{"extrude", "fillet", "extrude", "cut"});
    REQUIRE(doc.bodies().size() == 1);
    const Body& body = doc.bodies().begin()->second;
    CHECK(body.name == "Body1");
    CHECK(k.volume(body.shape) == doctest::Approx(fakePlateVolume(80, 50, 10, 20)).epsilon(1e-3));
}

TEST_CASE("changing a parameter changes the geometry") {
    Document doc = samples::mountingPlate();
    doc.parameters().setExpression("width", "120 mm");
    doc.parameters().setExpression("hole_d", "1 in");
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(k.volume(doc.bodies().begin()->second.shape) ==
          doctest::Approx(fakePlateVolume(120, 50, 10, 25.4)).epsilon(1e-3));
}

TEST_CASE("the rollback marker limits recompute") {
    Document doc = samples::mountingPlate();
    doc.setMarker(2); // Sketch1, Extrude1
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(k.log == std::vector<std::string>{"extrude"});
    CHECK(k.volume(doc.bodies().begin()->second.shape) == doctest::Approx(80 * 50 * 10));

    // New features insert at the marker, before the rolled-back ones.
    auto& chamfer = doc.add<ChamferFeature>();
    CHECK(doc.features()[2]->id == chamfer.id);
    CHECK(chamfer.name == "Chamfer1");
    CHECK(doc.marker() == 3);
}

TEST_CASE("failures are recorded per feature and propagate to dependents") {
    Document doc = samples::mountingPlate();
    // Break the outline sketch: a conflicting fixed dimension.
    auto* sketch = featureCast<SketchFeature>(doc.features()[0].get());
    REQUIRE(sketch);
    auto& s = sketch->sketch;
    auto line = s.entities().size() - 4; // first rectangle line
    s.addDimension(sketch::ConstraintType::Distance, s.entity(static_cast<int>(line)).p1,
                   s.entity(static_cast<int>(line)).p2, "1 mm");

    FakeKernel k;
    RecomputeReport r = doc.recompute(k);
    CHECK_FALSE(r.ok());
    CHECK_FALSE(doc.features()[0]->healthy());
    CHECK_FALSE(doc.features()[1]->healthy()); // extrude references the broken sketch
    CHECK(doc.bodies().empty());
}

TEST_CASE("features with dependents cannot be deleted") {
    Document doc = samples::mountingPlate();
    int sketchId = doc.features()[0]->id;
    CHECK_THROWS_AS(doc.removeFeature(sketchId), Error);
    int lastId = doc.features().back()->id;
    doc.removeFeature(lastId);
    CHECK(doc.features().size() == 4);
    CHECK(doc.marker() == 4);
}

TEST_CASE("documents round-trip through JSON") {
    Document doc = samples::mountingPlate();
    doc.setMarker(4);
    std::string json = doc.toJson();
    Document copy = Document::fromJson(json);
    CHECK(copy.toJson() == json);
    CHECK(copy.features().size() == 5);
    CHECK(copy.marker() == 4);
    CHECK(copy.parameters().value("hole_d") == doctest::Approx(20));

    // Ids keep increasing after a reload.
    auto& next = copy.add<FilletFeature>();
    CHECK(next.id > doc.features().back()->id);
    CHECK(next.name == "Fillet2");

    FakeKernel k1, k2;
    doc.setMarker(5);
    copy.removeFeature(next.id);
    copy.setMarker(5);
    REQUIRE(doc.recompute(k1).ok());
    REQUIRE(copy.recompute(k2).ok());
    CHECK(k1.log == k2.log);
}

TEST_CASE("malformed documents are rejected with oc::Error") {
    CHECK_THROWS_AS(Document::fromJson("not json"), Error);
    CHECK_THROWS_AS(Document::fromJson(R"({"format":"other"})"), Error);
    CHECK_THROWS_AS(Document::fromJson(R"({"format":"opencircuit","version":999})"), Error);
    CHECK_THROWS_AS(Document::fromJson(
                        R"({"format":"opencircuit","version":1,"parameters":[],"features":[{"id":1,"type":"warp"}]})"),
                    Error);
}

TEST_CASE("copies are deep") {
    Document a = samples::mountingPlate();
    Document b = a;
    b.parameters().setExpression("width", "10 mm");
    b.add<FilletFeature>();
    CHECK(a.parameters().value("width") == doctest::Approx(80));
    CHECK(a.features().size() == 5);
    CHECK(b.features().size() == 6);
}

TEST_CASE("undo and redo restore snapshots") {
    Document doc = samples::mountingPlate();
    UndoStack undo(doc);
    CHECK_FALSE(undo.canUndo());

    doc.parameters().setExpression("width", "100 mm");
    undo.commit(doc, "Change width");
    doc.parameters().setExpression("height", "70 mm");
    undo.commit(doc, "Change height");

    REQUIRE(undo.canUndo());
    CHECK(undo.undoLabel() == "Change height");
    doc = undo.undo();
    CHECK(doc.parameters().value("height") == doctest::Approx(50));
    CHECK(doc.parameters().value("width") == doctest::Approx(100));
    doc = undo.undo();
    CHECK(doc.parameters().value("width") == doctest::Approx(80));
    CHECK_FALSE(undo.canUndo());

    doc = undo.redo();
    CHECK(doc.parameters().value("width") == doctest::Approx(100));

    // A new edit discards the redo branch.
    doc.parameters().setExpression("thickness", "3 mm");
    undo.commit(doc, "Change thickness");
    CHECK_FALSE(undo.canRedo());
}

TEST_CASE("builders validate expressions before touching the document") {
    Document doc;
    build::BoxSpec bad;
    bad.width = "nope * 2";
    CHECK_THROWS_AS(build::addBox(doc, bad), Error);
    CHECK(doc.features().empty());

    build::CylinderSpec zero;
    zero.diameter = "0 mm";
    CHECK_THROWS_AS(build::addCylinder(doc, zero), Error);
    CHECK(doc.features().empty());

    build::BoxSpec ok;
    auto& ex = build::addBox(doc, ok);
    CHECK(doc.features().size() == 2);
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(k.volume(doc.bodies().at(ex.id).shape) == doctest::Approx(40.0 * 30 * 20));
    auto* sk = featureCast<SketchFeature>(doc.features()[0].get());
    REQUIRE(sk);
    CHECK(sk->solveResult.degreesOfFreedom == 0);
}

TEST_CASE("the flange sample is fully constrained except its axis length") {
    Document doc = samples::flange();
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    auto* section = featureCast<SketchFeature>(doc.features()[0].get());
    REQUIRE(section);
    CHECK(section->solveResult.degreesOfFreedom == 1);
    auto* holes = featureCast<SketchFeature>(doc.features()[2].get());
    REQUIRE(holes);
    CHECK(holes->solveResult.degreesOfFreedom == 0);
    CHECK(holes->profiles.size() == 4);
}
