// SPDX-License-Identifier: GPL-3.0-or-later
#include "FakeKernel.h"
#include "fuse/command/FeatureCommands.h"
#include "fuse/command/Session.h"
#include "fuse/command/SketchCommands.h"
#include "fuse/core/Error.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>

using namespace fuse;
using namespace fuse::cmd;
using fuse::testing::FakeKernel;

namespace {

Actor agent() { return {ActorKind::Agent, "claude"}; }

// A 40 x 30 rectangle on XY, dimensioned by the parameters w and h. Returns the sketch id.
int plateSketch(Session& s) {
    s.execute("parameter_add", {{"name", "w"}, {"expression", "40 mm"}});
    s.execute("parameter_add", {{"name", "h"}, {"expression", "30 mm"}});
    const int sketch = s.execute("sketch_create").output["feature"];
    const json lines =
        s.execute("sketch_add_rectangle", {{"sketch", sketch}, {"corner1", {0, 0}}, {"corner2", {40, 30}}})
            .output["lines"];
    const json bottom = s.execute("sketch_describe", {{"sketch", sketch}}).output["entities"][lines[0].get<int>()];
    const json right = s.execute("sketch_describe", {{"sketch", sketch}}).output["entities"][lines[1].get<int>()];
    s.execute(
        "sketch_add_dimension",
        {{"sketch", sketch}, {"type", "horizontal_distance"}, {"entities", bottom["points"]}, {"expression", "w"}});
    s.execute("sketch_add_dimension",
              {{"sketch", sketch}, {"type", "vertical_distance"}, {"entities", right["points"]}, {"expression", "h"}});
    return sketch;
}

std::string tempPath(const char* name) {
    const char* dir = std::getenv("FUSE_TEST_TMPDIR"); // NOLINT(concurrency-mt-unsafe): single-threaded tests
    return (dir ? std::filesystem::path(dir) : std::filesystem::temp_directory_path()) / name;
}

} // namespace

TEST_CASE("every built-in command describes itself") {
    const Registry& r = Registry::builtin();
    REQUIRE(r.all().size() > 30);
    for (const auto& e : r.all()) {
        CAPTURE(e.info.id);
        CHECK(e.inputSchema["type"] == "object");
        CHECK(e.inputSchema["additionalProperties"] == false);
        CHECK(e.outputSchema["type"] == "object");
        CHECK(std::string(e.info.title).size() > 0);
        CHECK(std::string(e.info.description).size() > 10);
        CHECK(int(bool(e.edit)) + int(bool(e.query)) + int(bool(e.session)) == 1);
    }
    CHECK(r.describe().size() == r.all().size());
    CHECK_THROWS_AS(r.get("warp_drive"), Error);

    const json extrude = Registry::describe(r.get("extrude_create"));
    CHECK(extrude["kind"] == "edit");
    const json& in = extrude["input_schema"];
    CHECK(in["required"] == json::array({"sketch"}));
    CHECK(in["properties"]["distance"]["default"] == "10 mm");
    CHECK(in["properties"]["distance"]["format"] == "length-expression");
    CHECK(in["properties"]["operation"]["enum"] == json::array({"new_body", "join", "cut", "intersect"}));
    CHECK(in["properties"]["profiles"]["type"] == "array");
    CHECK(extrude["output_schema"]["properties"]["feature"]["format"] == "feature-id");

    CHECK(Registry::describe(r.get("document_describe"))["kind"] == "query");
    CHECK(Registry::describe(r.get("history_undo"))["kind"] == "session");
    CHECK(r.get("sketch_add_line").inputSchema["properties"]["start"].contains("oneOf"));
}

TEST_CASE("bad arguments are refused with a message naming the argument") {
    FakeKernel k;
    Session s(k);
    auto refuses = [&](const char* command, const json& args, const std::string& message) {
        CAPTURE(command);
        try {
            s.execute(command, args);
            FAIL("expected an ArgumentError");
        } catch (const ArgumentError& e) {
            CHECK(std::string(e.what()) == message);
        }
    };
    refuses("sketch_create", {{"plane", "XW"}}, "'plane' must be one of: XY, XZ, YZ");
    refuses("sketch_create", {{"planes", "XY"}}, "unknown argument 'planes'");
    refuses("sketch_create", json::array(), "arguments must be a JSON object");
    refuses("parameter_set", {{"name", "w"}}, "missing argument 'expression'");
    refuses("parameter_set", {{"name", 3}, {"expression", "1"}}, "'name' must be a string");
    refuses("sketch_add_point", {{"sketch", 1}, {"at", {1, 2, 3}}}, "'at' must be an [x, y] array");
    refuses("fillet_create", {{"radius", "1 mm"}, {"edges", {{"mode", "some"}}}},
            "'edges.mode' must be one of: all, parallel_to, near_points");
    refuses("sketch_add_line", {{"sketch", 1}, {"start", "origin"}, {"end", {1, 1}}},
            "'start' must be a point id or an [x, y] array");

    CHECK(s.document().features().empty());
    CHECK_FALSE(s.canUndo());
    REQUIRE(s.journal().size() == 8);
    CHECK(s.journal().back().command == "sketch_add_line");
    CHECK(s.journal().back().error == "'start' must be a point id or an [x, y] array");
}

TEST_CASE("a part built from commands is fully parametric") {
    FakeKernel k;
    Session s(k);
    const int sketch = plateSketch(s);
    const Result r = s.execute("extrude_create", {{"sketch", sketch}, {"distance", "5 mm"}}, agent());
    const int extrude = r.output["feature"];
    CHECK(r.changes.created == std::vector<int>{extrude});
    CHECK(r.issues.empty());
    s.execute("fillet_create", {{"radius", "2 mm"}}, agent());

    REQUIRE(s.document().bodies().size() == 1);
    const auto volume = [&] { return k.volume(s.document().bodies().at(extrude).shape); };
    CHECK(volume() == doctest::Approx(40 * 30 * 5));

    const Result changed = s.execute("parameter_set", {{"name", "w"}, {"expression", "60 mm"}});
    CHECK(changed.changes.parameters == std::vector<std::string>{"w"});
    CHECK(changed.changes.modified.empty());
    CHECK(volume() == doctest::Approx(60 * 30 * 5));

    const json doc = s.execute("document_describe").output;
    REQUIRE(doc["features"].size() == 3);
    CHECK(doc["features"][1]["type"] == "extrude");
    CHECK(doc["features"][1]["definition"]["distance"] == "5 mm");
    CHECK(doc["features"][1]["created_by"] == json{{"kind", "agent"}, {"name", "claude"}});
    CHECK(doc["features"][0]["created_by"] == json{{"kind", "person"}, {"name", ""}});
    CHECK(doc["bodies"][0]["volume"].get<double>() == doctest::Approx(60 * 30 * 5));
    CHECK(doc["parameters"][0]["display"] == "60 mm");
}

TEST_CASE("sketch commands connect geometry and describe it") {
    FakeKernel k;
    Session s(k);
    const int sketch = s.execute("sketch_create", {{"plane", "XZ"}}).output["feature"];
    const int a =
        s.execute("sketch_add_line", {{"sketch", sketch}, {"start", {0, 0}}, {"end", {10, 0}}}).output["entity"];
    json d = s.execute("sketch_describe", {{"sketch", sketch}}).output;
    const int corner = d["entities"][a]["points"][1];
    const int b =
        s.execute("sketch_add_line", {{"sketch", sketch}, {"start", corner}, {"end", {10, 10}}}).output["entity"];
    const int top = s.execute("sketch_describe", {{"sketch", sketch}}).output["entities"][b]["points"][1];
    s.execute("sketch_add_line", {{"sketch", sketch}, {"start", top}, {"end", 0}}); // back to the origin

    d = s.execute("sketch_describe", {{"sketch", sketch}}).output;
    CHECK(d["status"] == "solved");
    REQUIRE(d["profiles"].size() == 1);
    CHECK(d["profiles"][0]["area"].get<double>() == doctest::Approx(50));
    CHECK(d["entities"][0]["fully_constrained"] == true); // the origin

    const int dim =
        s.execute("sketch_add_dimension",
                  {{"sketch", sketch}, {"type", "distance"}, {"entities", {0, corner}}, {"expression", "12 mm"}})
            .output["constraint"];
    s.execute("sketch_set_dimension", {{"sketch", sketch}, {"constraint", dim}, {"expression", "20 mm"}});
    d = s.execute("sketch_describe", {{"sketch", sketch}}).output;
    CHECK(d["constraints"][dim]["value"].get<double>() == doctest::Approx(20));
    CHECK(d["constraints"][dim]["expression"] == "20 mm");

    CHECK_THROWS_WITH_AS(
        s.execute("sketch_add_constraint", {{"sketch", sketch}, {"type", "radius"}, {"entities", {a}}}),
        "radius is a dimension; use sketch_add_dimension", Error);
    CHECK_THROWS_WITH_AS(s.execute("sketch_add_line", {{"sketch", sketch}, {"start", a}, {"end", {1, 1}}}),
                         ("start: sketch entity " + std::to_string(a) + " is not a point").c_str(), Error);
    CHECK_THROWS_WITH_AS(s.execute("sketch_add_point", {{"sketch", 99}, {"at", {0, 0}}}), "feature 99 does not exist",
                         Error);
}

TEST_CASE("edits that would create or break a feature are refused") {
    FakeKernel k;
    Session s(k);
    s.execute("parameter_add", {{"name", "d"}, {"expression", "10 mm"}});
    const int sketch = s.execute("sketch_create").output["feature"];
    const int circle =
        s.execute("sketch_add_circle", {{"sketch", sketch}, {"center", {0, 0}}, {"radius", 5}}).output["entity"];
    const int dim = s.execute("sketch_add_dimension",
                              {{"sketch", sketch}, {"type", "diameter"}, {"entities", {circle}}, {"expression", "d"}})
                        .output["constraint"];
    const std::string before = s.document().toJson();
    const std::string undoLabel = s.undoLabel();

    // A feature that would fail is never created.
    const int empty = s.execute("sketch_create").output["feature"];
    CHECK_THROWS_WITH_AS(s.execute("extrude_create", {{"sketch", empty}}),
                         "Extrude1: sketch 'Sketch2' has no closed profiles", Error);
    s.execute("history_undo");
    CHECK(s.document().toJson() == before);

    // Nor is a working feature broken by editing it.
    CHECK_THROWS_AS(
        s.execute("sketch_set_dimension", {{"sketch", sketch}, {"constraint", dim}, {"expression", "-4 mm"}}), Error);
    CHECK(s.document().toJson() == before);
    CHECK(s.undoLabel() == undoLabel);

    // Typos are caught before anything runs.
    CHECK_THROWS_WITH_AS(s.execute("extrude_create", {{"sketch", sketch}, {"distance", "dd * 2"}}),
                         "distance: unknown parameter 'dd'", Error);

    // A parameter change may break dependents: that is allowed and reported.
    const Result r = s.execute("parameter_set", {{"name", "d"}, {"expression", "-1 mm"}});
    REQUIRE(r.issues.size() == 1);
    CHECK(r.issues[0].feature == sketch);
    CHECK(r.toJson()["issues"][0]["name"] == "Sketch1");

    // Changing nothing adds no undo step.
    s.execute("parameter_set", {{"name", "d"}, {"expression", "10 mm"}});
    const std::string label = s.undoLabel();
    const size_t journal = s.journal().size();
    s.execute("parameter_set", {{"name", "d"}, {"expression", "10 mm"}});
    CHECK(s.journal().size() == journal + 1);
    s.execute("history_undo");
    CHECK(s.document().parameters().get("d").expression.source() == "-1 mm");
    CHECK(label == "Change Parameter");
}

TEST_CASE("undo and redo restore the document and the geometry") {
    FakeKernel k;
    Session s(k);
    const int sketch = plateSketch(s);
    s.execute("extrude_create", {{"sketch", sketch}, {"distance", "5 mm"}});
    REQUIRE(s.document().bodies().size() == 1);

    CHECK(s.execute("history_undo").output["label"] == "Extrude");
    CHECK(s.document().bodies().empty());
    CHECK(s.canRedo());
    CHECK(s.redoLabel() == "Extrude");
    CHECK(s.execute("history_redo").output["label"] == "Extrude");
    CHECK(s.document().bodies().size() == 1);

    while (s.canUndo()) s.execute("history_undo");
    CHECK(s.document().features().empty());
    CHECK_THROWS_WITH_AS(s.execute("history_undo"), "nothing to undo", Error);
}

TEST_CASE("transactions preview an edit and commit it as one undo step") {
    FakeKernel k;
    Session s(k);
    const int sketch = plateSketch(s);
    const std::string committed = s.document().toJson();
    const std::string label = s.undoLabel();

    Result r = s.begin("extrude_create", {{"sketch", sketch}, {"distance", "5 mm"}}, agent());
    const int extrude = r.output["feature"];
    CHECK(s.inTransaction());
    CHECK(s.document().bodies().size() == 1);
    CHECK(s.execute("document_describe").output["features"].size() == 2); // queries see the preview
    CHECK_THROWS_AS(s.execute("parameter_set", {{"name", "w"}, {"expression", "1 mm"}}), Error);
    CHECK_THROWS_AS(s.execute("history_undo"), Error);
    CHECK_THROWS_AS(s.begin("sketch_create", json::object()), Error);

    for (int d : {6, 7, 8}) s.update({{"sketch", sketch}, {"distance", std::to_string(d) + " mm"}});
    CHECK(k.volume(s.document().bodies().at(extrude).shape) == doctest::Approx(40 * 30 * 8));
    CHECK_THROWS_AS(s.update({{"sketch", sketch}, {"distance", 8}}), ArgumentError);
    CHECK(k.volume(s.document().bodies().at(extrude).shape) == doctest::Approx(40 * 30 * 8)); // previous preview kept

    s.commit();
    CHECK_FALSE(s.inTransaction());
    CHECK(s.undoLabel() == "Extrude");
    CHECK(s.document().findFeature(extrude)->createdBy == agent());
    CHECK(s.journal().back().command == "extrude_create");
    CHECK(s.journal().back().args["distance"] == "8 mm");
    s.execute("history_undo");
    CHECK(s.document().toJson() == committed);
    CHECK(s.undoLabel() == label);

    s.begin("extrude_create", {{"sketch", sketch}});
    s.cancel();
    CHECK(s.document().toJson() == committed);
    CHECK_THROWS_AS(s.commit(), Error);
    CHECK_THROWS_AS(s.begin("document_describe", json::object()), Error);
}

TEST_CASE("a broken preview cannot be committed") {
    FakeKernel k;
    Session s(k);
    const int sketch = s.execute("sketch_create").output["feature"];
    const int circle =
        s.execute("sketch_add_circle", {{"sketch", sketch}, {"center", {0, 0}}, {"radius", 5}}).output["entity"];
    const int dim = s.execute("sketch_add_dimension",
                              {{"sketch", sketch}, {"type", "radius"}, {"entities", {circle}}, {"expression", "5 mm"}})
                        .output["constraint"];

    // A constant negative radius is refused outright; one from a parameter only fails on solve.
    CHECK_THROWS_AS(s.begin("sketch_set_dimension", {{"sketch", sketch}, {"constraint", dim}, {"expression", "-2 mm"}}),
                    Error);
    s.execute("parameter_add", {{"name", "r"}, {"expression", "-2 mm"}});
    const Result r = s.begin("sketch_set_dimension", {{"sketch", sketch}, {"constraint", dim}, {"expression", "r"}});
    REQUIRE(r.issues.size() == 1); // shown in the preview
    CHECK_THROWS_AS(s.commit(), Error);
    CHECK(s.inTransaction()); // still open, so the value can be corrected
    s.update({{"sketch", sketch}, {"constraint", dim}, {"expression", "3 mm"}});
    CHECK(s.commit().issues.empty());
}

TEST_CASE("provenance follows who created and who last changed a feature") {
    FakeKernel k;
    Session s(k);
    const int sketch = plateSketch(s);
    const int extrude = s.execute("extrude_create", {{"sketch", sketch}}, agent()).output["feature"];
    s.execute("extrude_edit", {{"feature", extrude}, {"distance", "12 mm"}, {"symmetric", true}});
    const Feature* f = s.document().findFeature(extrude);
    CHECK(f->createdBy == agent());
    CHECK(f->modifiedBy == Actor{});
    CHECK(s.document().findFeature(sketch)->createdBy == Actor{});

    const Actor script{ActorKind::Script, "batch.py"};
    s.execute("feature_rename", {{"feature", sketch}, {"name", "Outline"}}, script);
    CHECK(s.document().findFeature(sketch)->modifiedBy == script);
    CHECK(s.document().findFeature(extrude)->modifiedBy == Actor{});
}

TEST_CASE("feature and timeline commands") {
    FakeKernel k;
    Session s(k);
    const int box =
        s.execute("box_create", {{"width", "10 mm"}, {"depth", "10 mm"}, {"height", "10 mm"}}).output["feature"];
    const int hole = s.execute("cylinder_create", {{"diameter", "4 mm"}, {"operation", "cut"}}).output["feature"];
    const auto volume = [&] { return k.volume(s.document().bodies().at(box).shape); };
    const double cut = 1000 - kPi * 4 * 20;
    CHECK(volume() == doctest::Approx(cut).epsilon(1e-3));

    s.execute("feature_suppress", {{"feature", hole}});
    CHECK(volume() == doctest::Approx(1000));
    s.execute("feature_suppress", {{"feature", hole}, {"suppressed", false}});

    s.execute("timeline_set_marker", {{"position", 2}});
    CHECK(volume() == doctest::Approx(1000));
    CHECK_THROWS_AS(s.execute("timeline_set_marker", {{"position", 9}}), Error);
    s.execute("timeline_set_marker", {{"position", 4}});

    CHECK_THROWS_WITH_AS(s.execute("feature_delete", {{"feature", box}}),
                         "cannot delete 'Extrude1': 'Extrude2' depends on it", Error);
    s.execute("feature_delete", {{"feature", hole}});
    CHECK(volume() == doctest::Approx(1000));

    const int fillet = s.execute("fillet_create", {{"radius", "1 mm"}}).output["feature"];
    CHECK(s.document().findFeature(fillet)->name == "Fillet1");
    s.execute("fillet_edit", {{"feature", fillet}, {"radius", "2 mm"}, {"edges", {{"mode", "parallel_to"}}}});
    const auto* f = featureCast<FilletFeature>(s.document().findFeature(fillet));
    REQUIRE(f);
    CHECK(f->size == "2 mm");
    CHECK(f->edges.mode == EdgeQuery::Mode::ParallelTo);
    CHECK_THROWS_WITH_AS(s.execute("extrude_edit", {{"feature", fillet}, {"distance", "1 mm"}}),
                         "'Fillet1' is not a extrude feature", Error);
}

TEST_CASE("call() reports results and errors as JSON for transports") {
    FakeKernel k;
    Session s(k);
    const json ok = s.call("sketch_create", {{"plane", "YZ"}}, agent());
    CHECK(ok["ok"] == true);
    CHECK(ok["output"]["feature"] == 1);
    CHECK(ok["changes"]["created"] == json::array({1}));
    const json bad = s.call("sketch_create", {{"plane", "Q"}}, agent());
    CHECK(bad["ok"] == false);
    CHECK(bad["error"] == "'plane' must be one of: XY, XZ, YZ");
    CHECK(s.call("warp", json::object(), agent())["error"] == "unknown command 'warp'");
}

TEST_CASE("the typed interface goes through the same path") {
    FakeKernel k;
    Session s(k);
    const FeatureRef sketch = s.run(CreateSketch{SketchPlane::XZ, "5 mm"});
    AddSketchCircle circle;
    circle.sketchId = sketch.feature;
    circle.center = {std::nullopt, {1, 2}};
    circle.radius = 3;
    const EntityRef e = s.run(circle, agent());
    CHECK(e.entity > 0);
    CHECK(s.journal().back().command == "sketch_add_circle");
    CHECK(s.journal().back().args["center"] == json::array({1.0, 2.0}));
    CHECK(s.journal().back().actor == agent());
}

TEST_CASE("documents are saved and opened through commands") {
    FakeKernel k;
    Session s(k);
    CHECK_THROWS_AS(s.execute("document_save"), Error); // no file yet
    plateSketch(s);
    CHECK(s.modified());
    const std::string path = tempPath("commands-roundtrip.fcad");
    CHECK(s.execute("document_save", {{"path", path}}).output["path"] == path);
    CHECK_FALSE(s.modified());
    CHECK(s.path() == path);

    s.execute("document_new");
    CHECK(s.document().features().empty());
    CHECK_FALSE(s.canUndo());
    s.execute("document_open", {{"path", path}});
    CHECK(s.document().features().size() == 1);
    CHECK(s.document().parameters().contains("w"));
    CHECK_FALSE(s.modified());
    CHECK_THROWS_AS(s.execute("document_open", {{"path", tempPath("missing.fcad")}}), Error);
}
