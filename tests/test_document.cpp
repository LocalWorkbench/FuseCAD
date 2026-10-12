// SPDX-License-Identifier: GPL-3.0-or-later
#include "FakeKernel.h"
#include "age/core/Error.h"
#include "age/model/Builders.h"
#include "age/model/Document.h"
#include "age/model/Features.h"
#include "age/model/Samples.h"

#include <doctest/doctest.h>

using namespace age;
using age::testing::FakeKernel;

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

TEST_CASE("malformed documents are rejected with age::Error") {
    CHECK_THROWS_AS(Document::fromJson("not json"), Error);
    CHECK_THROWS_AS(Document::fromJson(R"({"format":"other"})"), Error);
    CHECK_THROWS_AS(Document::fromJson(R"({"format":"agecad","version":999})"), Error);
    CHECK_THROWS_AS(Document::fromJson(R"({"version":1})"), Error);
    CHECK_THROWS_AS(
        Document::fromJson(R"({"format":"agecad","version":1,"parameters":[],"features":[{"id":1,"type":"warp"}]})"),
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

TEST_CASE("parameters referenced by features cannot be removed") {
    Document doc = samples::mountingPlate();
    doc.parameters().add("unused", "1 mm");
    CHECK_THROWS_WITH_AS(doc.removeParameter("corner_r"), doctest::Contains("Fillet1"), Error);
    CHECK_THROWS_WITH_AS(doc.removeParameter("hole_d"), doctest::Contains("Sketch2"), Error); // a dimension
    CHECK_THROWS_WITH_AS(doc.removeParameter("thickness"), doctest::Contains("Extrude1"), Error);
    CHECK(doc.parameters().contains("corner_r"));
    doc.parameters().add("half", "unused / 2");
    CHECK_THROWS_AS(doc.removeParameter("unused"), Error); // referenced by a parameter
    doc.removeParameter("half");
    doc.removeParameter("unused");
    CHECK_FALSE(doc.parameters().contains("unused"));
    CHECK_THROWS_AS(doc.removeParameter("nope"), Error);
}

TEST_CASE("renaming a parameter rewrites every reference") {
    Document doc = samples::mountingPlate();
    doc.parameters().add("half_w", "width / 2");
    doc.renameParameter("width", "plate_w");
    doc.renameParameter("thickness", "t");
    CHECK_FALSE(doc.parameters().contains("width"));
    CHECK(doc.parameters().get("half_w").expression.source() == "plate_w / 2");
    const auto* hole = featureCast<SketchFeature>(doc.features()[3].get());
    REQUIRE(hole);
    CHECK(hole->offset == "t");
    bool found = false;
    for (const auto& c : hole->sketch.constraints()) found = found || c.expression == "plate_w / 2";
    CHECK(found);
    const auto* cut = featureCast<ExtrudeFeature>(doc.features()[4].get());
    REQUIRE(cut);
    CHECK(cut->distance == "-t");

    CHECK_THROWS_AS(doc.renameParameter("t", "height"), Error); // taken
    CHECK_THROWS_AS(doc.renameParameter("t", "mm"), Error);     // reserved
    CHECK(cut->distance == "-t");

    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(k.volume(doc.bodies().begin()->second.shape) ==
          doctest::Approx(fakePlateVolume(80, 50, 10, 20)).epsilon(1e-3));
}

TEST_CASE("feature expressions are checked against their kinds") {
    Document doc = samples::flange();
    auto* revolve = featureCast<RevolveFeature>(doc.features()[1].get());
    REQUIRE(revolve);
    revolve->angle = "180"; // a bare number for an angle means degrees
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    revolve->angle = "10 mm";
    CHECK_FALSE(doc.recompute(k).ok());
    CHECK_FALSE(revolve->healthy());

    Document plate = samples::mountingPlate();
    plate.parameters().add("tilt", "5 deg", UnitKind::Angle);
    auto* extrude = featureCast<ExtrudeFeature>(plate.features()[1].get());
    REQUIRE(extrude);
    extrude->distance = "tilt";
    CHECK_FALSE(plate.recompute(k).ok());
    CHECK_FALSE(extrude->healthy());
}

TEST_CASE("suppressed features are skipped and their dependents fail") {
    Document doc = samples::mountingPlate();
    doc.features()[2]->suppressed = true; // the fillet
    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(k.log == std::vector<std::string>{"extrude", "extrude", "cut"});

    doc.features()[2]->suppressed = false;
    doc.features()[3]->suppressed = true; // the hole sketch
    FakeKernel k2;
    RecomputeReport r = doc.recompute(k2);
    CHECK(r.failed == 1);
    CHECK(doc.features()[3]->healthy()); // suppressed, not failed
    CHECK_FALSE(doc.features()[4]->healthy());
    CHECK(k2.volume(doc.bodies().begin()->second.shape) == doctest::Approx(80 * 50 * 10));
}

TEST_CASE("builders resolve the latest body when created") {
    Document doc;
    CHECK_THROWS_AS(build::addFillet(doc, {}, "1 mm"), Error); // nothing to fillet yet
    build::CylinderSpec cut;
    cut.op.operation = FeatureOperation::Cut;
    CHECK_THROWS_AS(build::addCylinder(doc, cut), Error);
    CHECK(doc.features().empty());

    auto& first = build::addBox(doc, {});
    auto& second = build::addBox(doc, {});
    CHECK(doc.latestBody() == second.id);
    auto& fillet = build::addFillet(doc, {}, "1 mm");
    CHECK(fillet.body == second.id);
    auto& chamfer = build::addChamfer(doc, {}, "1 mm", first.id);
    CHECK(chamfer.body == first.id);
    CHECK(doc.latestBody() == first.id);
    auto& hole = build::addCylinder(doc, cut);
    CHECK(hole.op.targetBody == first.id);
    CHECK_THROWS_AS(build::addFillet(doc, {}, "45 deg"), Error);

    // The body's creator is now a dependency and cannot be deleted.
    CHECK_THROWS_WITH_AS(doc.removeFeature(second.id), doctest::Contains("Fillet1"), Error);

    FakeKernel k;
    REQUIRE(doc.recompute(k).ok());
    CHECK(doc.bodies().size() == 2);
}

TEST_CASE("features without a body fail instead of guessing") {
    Document doc = samples::mountingPlate();
    auto& chamfer = doc.add<ChamferFeature>();
    auto& join = doc.add<ExtrudeFeature>();
    join.profiles.sketchId = doc.features()[0]->id;
    join.op.operation = FeatureOperation::Join;
    FakeKernel k;
    RecomputeReport r = doc.recompute(k);
    CHECK(r.failed == 2);
    CHECK_FALSE(chamfer.healthy());
    CHECK_FALSE(join.healthy());
    // Such documents are not loadable either.
    CHECK_THROWS_AS(Document::fromJson(doc.toJson()), Error);
}

TEST_CASE("the undo stack keeps at most its limit") {
    Document doc = samples::mountingPlate();
    UndoStack undo(doc, 3);
    for (int w = 81; w <= 85; ++w) {
        doc.parameters().setExpression("width", std::to_string(w) + " mm");
        undo.commit(doc, "width " + std::to_string(w));
    }
    doc = undo.undo();
    CHECK(doc.parameters().value("width") == doctest::Approx(84));
    doc = undo.undo();
    CHECK(doc.parameters().value("width") == doctest::Approx(83));
    CHECK_FALSE(undo.canUndo()); // older states were trimmed
    CHECK(undo.redoLabel() == "width 84");
}

TEST_CASE("the full flange round-trips through JSON") {
    Document doc = samples::flange();
    const int revolveId = doc.features()[1]->id;
    auto* cut = featureCast<ExtrudeFeature>(doc.features()[3].get());
    REQUIRE(cut);
    cut->profiles.profileIndices = {0, 2};

    auto& slab = doc.add<ExtrudeFeature>();
    slab.profiles.sketchId = doc.features()[2]->id;
    slab.distance = "4 mm";
    slab.symmetric = true;
    slab.suppressed = true;

    EdgeQuery near;
    near.mode = EdgeQuery::Mode::NearPoints;
    near.points = {{50, 0, 12}, {0, 25, 35}};
    build::addChamfer(doc, near, "1 mm", revolveId);
    EdgeQuery vertical;
    vertical.mode = EdgeQuery::Mode::ParallelTo;
    vertical.direction = {0, 0, 1};
    build::addFillet(doc, vertical, "bolt_d / 10", revolveId);
    doc.parameters().add("count", "4", UnitKind::Unitless, "Bolt count");
    doc.parameters().add("tilt", "15", UnitKind::Angle);

    const std::string json = doc.toJson();
    Document copy = Document::fromJson(json);
    CHECK(copy.toJson() == json);
    CHECK(copy.parameters().value("tilt") == doctest::Approx(kPi / 12));

    FakeKernel k1, k2;
    REQUIRE(doc.recompute(k1).ok());
    REQUIRE(copy.recompute(k2).ok());
    CHECK(k1.log == std::vector<std::string>{"revolve", "extrude", "cut", "chamfer", "fillet"});
    CHECK(k1.log == k2.log);
    CHECK(k2.volume(copy.bodies().at(revolveId).shape) == doctest::Approx(k1.volume(doc.bodies().at(revolveId).shape)));
}

TEST_CASE("documents are read strictly") {
    const std::string good = samples::mountingPlate().toJson();
    CHECK_NOTHROW(Document::fromJson(good));
    auto mutate = [&](const std::string& from, const std::string& to) {
        std::string s = good;
        auto pos = s.find(from);
        REQUIRE(pos != std::string::npos);
        return s.replace(pos, from.size(), to);
    };
    CHECK_THROWS_AS(Document::fromJson(mutate("\"version\": 2", "\"version\": 1")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"fillet\": 1", "\"warp\": 1")), Error); // name_counters key
    CHECK_THROWS_AS(Document::fromJson(mutate("\"symmetric\": false,", "")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"offset\": \"0\",", "")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"unit\": \"length\"", "\"unit\": \"furlong\"")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"marker\": 5", "\"marker\": 6")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"next_id\": 6", "\"next_id\": 5")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"type\": \"horizontal\"", "\"type\": \"radius\"")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"kind\": \"person\"", "\"kind\": \"robot\"")), Error);
    CHECK_THROWS_AS(Document::fromJson(mutate("\"created_by\"", "\"made_by\"")), Error);
}

TEST_CASE("feature provenance round-trips") {
    Document doc = samples::mountingPlate();
    doc.features()[0]->createdBy = {ActorKind::Agent, "claude"};
    doc.features()[0]->modifiedBy = {ActorKind::Person, ""};
    doc.features()[1]->modifiedBy = {ActorKind::Script, "agecad-cli"};

    Document copy = Document::fromJson(doc.toJson());
    CHECK(copy.features()[0]->createdBy == Actor{ActorKind::Agent, "claude"});
    CHECK(copy.features()[0]->modifiedBy == Actor{});
    CHECK(copy.features()[1]->createdBy == Actor{});
    CHECK(copy.features()[1]->modifiedBy == Actor{ActorKind::Script, "agecad-cli"});
}

TEST_CASE("diff reports what an edit changed") {
    const Document before = samples::mountingPlate();
    CHECK(diff(before, before).empty());

    Document after = before;
    after.parameters().setExpression("width", "100 mm");
    after.parameters().add("depth", "3 mm");
    auto* extrude = featureCast<ExtrudeFeature>(after.features()[1].get());
    REQUIRE(extrude);
    extrude->distance = "thickness * 2";
    const int removed = after.features().back()->id;
    after.removeFeature(removed);
    auto& chamfer = after.add<ChamferFeature>();
    chamfer.body = extrude->id;

    // Provenance alone is not a change to the definition.
    after.features()[0]->modifiedBy = {ActorKind::Agent, "claude"};

    const Changes c = diff(before, after);
    CHECK(c.created == std::vector<int>{chamfer.id});
    CHECK(c.modified == std::vector<int>{extrude->id});
    CHECK(c.removed == std::vector<int>{removed});
    CHECK(c.parameters == std::vector<std::string>{"width", "depth"});
    CHECK_FALSE(c.marker);

    Document rolledBack = before;
    rolledBack.setMarker(1);
    CHECK(diff(before, rolledBack).marker);
}
