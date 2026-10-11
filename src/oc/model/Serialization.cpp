// SPDX-License-Identifier: GPL-3.0-or-later
//
// Native .ocd format: a JSON document holding the parametric definition only
// (parameters, sketches, feature timeline). Geometry is regenerated on load,
// so files are small, diffable and kernel-independent.

#include "oc/core/Error.h"
#include "oc/model/Document.h"
#include "oc/model/Features.h"

#include <nlohmann/json.hpp>

namespace oc {

using nlohmann::json;

namespace {

// --- enums -----------------------------------------------------------------

const char* planeName(SketchPlane p) {
    switch (p) {
    case SketchPlane::XY:
        return "XY";
    case SketchPlane::XZ:
        return "XZ";
    case SketchPlane::YZ:
        return "YZ";
    }
    return "XY";
}

SketchPlane parsePlane(const std::string& s) {
    if (s == "XY") return SketchPlane::XY;
    if (s == "XZ") return SketchPlane::XZ;
    if (s == "YZ") return SketchPlane::YZ;
    throw Error("unknown sketch plane '" + s + "'");
}

const char* entityTypeName(sketch::EntityType t) {
    switch (t) {
    case sketch::EntityType::Point:
        return "point";
    case sketch::EntityType::Line:
        return "line";
    case sketch::EntityType::Circle:
        return "circle";
    case sketch::EntityType::Arc:
        return "arc";
    }
    return "point";
}

sketch::EntityType parseEntityType(const std::string& s) {
    if (s == "point") return sketch::EntityType::Point;
    if (s == "line") return sketch::EntityType::Line;
    if (s == "circle") return sketch::EntityType::Circle;
    if (s == "arc") return sketch::EntityType::Arc;
    throw Error("unknown sketch entity type '" + s + "'");
}

const char* edgeModeName(EdgeQuery::Mode m) {
    switch (m) {
    case EdgeQuery::Mode::All:
        return "all";
    case EdgeQuery::Mode::ParallelTo:
        return "parallel_to";
    case EdgeQuery::Mode::NearPoints:
        return "near_points";
    }
    return "all";
}

EdgeQuery::Mode parseEdgeMode(const std::string& s) {
    if (s == "all") return EdgeQuery::Mode::All;
    if (s == "parallel_to") return EdgeQuery::Mode::ParallelTo;
    if (s == "near_points") return EdgeQuery::Mode::NearPoints;
    throw Error("unknown edge selection mode '" + s + "'");
}

// --- small values ----------------------------------------------------------

json vec3(Vec3 v) { return json::array({v.x, v.y, v.z}); }
Vec3 vec3(const json& j) { return {j.at(0).get<double>(), j.at(1).get<double>(), j.at(2).get<double>()}; }

json edgeQueryToJson(const EdgeQuery& q) {
    json j{{"mode", edgeModeName(q.mode)}};
    if (q.mode == EdgeQuery::Mode::ParallelTo) j["direction"] = vec3(q.direction);
    if (q.mode == EdgeQuery::Mode::NearPoints) {
        j["points"] = json::array();
        for (const auto& p : q.points) j["points"].push_back(vec3(p));
    }
    return j;
}

EdgeQuery edgeQueryFromJson(const json& j) {
    EdgeQuery q;
    q.mode = parseEdgeMode(j.at("mode").get<std::string>());
    if (j.contains("direction")) q.direction = vec3(j["direction"]);
    if (j.contains("points"))
        for (const auto& p : j["points"]) q.points.push_back(vec3(p));
    return q;
}

json selectionToJson(const ProfileSelection& s) { return {{"sketch", s.sketchId}, {"profiles", s.profileIndices}}; }
ProfileSelection selectionFromJson(const json& j) {
    return {j.at("sketch").get<int>(), j.value("profiles", std::vector<int>{})};
}

json operationToJson(const BodyOperation& op) {
    return {{"operation", operationName(op.operation)}, {"target", op.targetBody}};
}
BodyOperation operationFromJson(const json& j) {
    BodyOperation op;
    if (!parseOperation(j.at("operation").get<std::string>(), op.operation))
        throw Error("unknown body operation '" + j["operation"].get<std::string>() + "'");
    op.targetBody = j.value("target", -1);
    return op;
}

// --- sketch ----------------------------------------------------------------

json sketchToJson(const sketch::Sketch& s) {
    json entities = json::array();
    for (const auto& e : s.entities()) {
        json je{{"type", entityTypeName(e.type)}};
        if (e.var >= 0) je["var"] = e.var;
        if (e.p1 >= 0) je["p1"] = e.p1;
        if (e.p2 >= 0) je["p2"] = e.p2;
        if (e.p3 >= 0) je["p3"] = e.p3;
        if (e.construction) je["construction"] = true;
        entities.push_back(std::move(je));
    }
    json constraints = json::array();
    for (const auto& c : s.constraints()) {
        json jc{{"type", sketch::constraintTypeName(c.type)}, {"entities", json::array()}};
        for (int id : {c.e1, c.e2, c.e3})
            if (id >= 0) jc["entities"].push_back(id);
        if (c.value != 0) jc["value"] = c.value;
        if (c.value2 != 0) jc["value2"] = c.value2;
        if (!c.expression.empty()) jc["expression"] = c.expression;
        constraints.push_back(std::move(jc));
    }
    return {{"variables", s.variables()}, {"entities", entities}, {"constraints", constraints}};
}

sketch::Sketch sketchFromJson(const json& j) {
    std::vector<sketch::Entity> entities;
    for (const auto& je : j.at("entities")) {
        sketch::Entity e;
        e.type = parseEntityType(je.at("type").get<std::string>());
        e.var = je.value("var", -1);
        e.p1 = je.value("p1", -1);
        e.p2 = je.value("p2", -1);
        e.p3 = je.value("p3", -1);
        e.construction = je.value("construction", false);
        entities.push_back(e);
    }
    std::vector<sketch::Constraint> constraints;
    for (const auto& jc : j.at("constraints")) {
        sketch::Constraint c;
        auto typeName = jc.at("type").get<std::string>();
        if (!sketch::parseConstraintType(typeName, c.type)) throw Error("unknown constraint type '" + typeName + "'");
        const auto ids = jc.at("entities").get<std::vector<int>>();
        if (ids.size() > 3) throw Error("constraint references too many entities");
        if (!ids.empty()) c.e1 = ids[0];
        if (ids.size() > 1) c.e2 = ids[1];
        if (ids.size() > 2) c.e3 = ids[2];
        c.value = jc.value("value", 0.0);
        c.value2 = jc.value("value2", 0.0);
        c.expression = jc.value("expression", std::string{});
        constraints.push_back(std::move(c));
    }
    return sketch::Sketch::fromRaw(j.at("variables").get<std::vector<double>>(), std::move(entities),
                                   std::move(constraints));
}

// --- features --------------------------------------------------------------

json featureToJson(const Feature& f) {
    json j{{"id", f.id}, {"type", featureTypeName(f.type())}, {"name", f.name}};
    if (f.suppressed) j["suppressed"] = true;

    if (const auto* s = featureCast<SketchFeature>(&f)) {
        j["plane"] = planeName(s->plane);
        j["offset"] = s->offset;
        j["sketch"] = sketchToJson(s->sketch);
    } else if (const auto* e = featureCast<ExtrudeFeature>(&f)) {
        j["profiles"] = selectionToJson(e->profiles);
        j["distance"] = e->distance;
        j["symmetric"] = e->symmetric;
        j["operation"] = operationToJson(e->op);
    } else if (const auto* r = featureCast<RevolveFeature>(&f)) {
        j["profiles"] = selectionToJson(r->profiles);
        j["axis"] = r->axisLine;
        j["angle"] = r->angle;
        j["operation"] = operationToJson(r->op);
    } else if (const auto* fi = featureCast<FilletFeature>(&f)) {
        j["body"] = fi->body;
        j["edges"] = edgeQueryToJson(fi->edges);
        j["radius"] = fi->radius;
    } else if (const auto* c = featureCast<ChamferFeature>(&f)) {
        j["body"] = c->body;
        j["edges"] = edgeQueryToJson(c->edges);
        j["distance"] = c->distance;
    } else if (const auto* i = featureCast<ImportStepFeature>(&f)) {
        j["path"] = i->path;
    }
    return j;
}

std::unique_ptr<Feature> featureFromJson(const json& j) {
    FeatureType type;
    auto typeName = j.at("type").get<std::string>();
    if (!parseFeatureType(typeName, type)) throw Error("unknown feature type '" + typeName + "'");

    std::unique_ptr<Feature> f;
    switch (type) {
    case FeatureType::Sketch: {
        auto s = std::make_unique<SketchFeature>();
        s->plane = parsePlane(j.at("plane").get<std::string>());
        s->offset = j.value("offset", std::string("0"));
        s->sketch = sketchFromJson(j.at("sketch"));
        f = std::move(s);
        break;
    }
    case FeatureType::Extrude: {
        auto e = std::make_unique<ExtrudeFeature>();
        e->profiles = selectionFromJson(j.at("profiles"));
        e->distance = j.at("distance").get<std::string>();
        e->symmetric = j.value("symmetric", false);
        e->op = operationFromJson(j.at("operation"));
        f = std::move(e);
        break;
    }
    case FeatureType::Revolve: {
        auto r = std::make_unique<RevolveFeature>();
        r->profiles = selectionFromJson(j.at("profiles"));
        r->axisLine = j.at("axis").get<int>();
        r->angle = j.at("angle").get<std::string>();
        r->op = operationFromJson(j.at("operation"));
        f = std::move(r);
        break;
    }
    case FeatureType::Fillet: {
        auto fi = std::make_unique<FilletFeature>();
        fi->body = j.value("body", -1);
        fi->edges = edgeQueryFromJson(j.at("edges"));
        fi->radius = j.at("radius").get<std::string>();
        f = std::move(fi);
        break;
    }
    case FeatureType::Chamfer: {
        auto c = std::make_unique<ChamferFeature>();
        c->body = j.value("body", -1);
        c->edges = edgeQueryFromJson(j.at("edges"));
        c->distance = j.at("distance").get<std::string>();
        f = std::move(c);
        break;
    }
    case FeatureType::ImportStep: {
        auto i = std::make_unique<ImportStepFeature>();
        i->path = j.at("path").get<std::string>();
        f = std::move(i);
        break;
    }
    }
    f->id = j.at("id").get<int>();
    f->name = j.at("name").get<std::string>();
    f->suppressed = j.value("suppressed", false);
    return f;
}

} // namespace

std::string Document::toJson() const {
    json params = json::array();
    for (const auto& p : params_.all()) {
        json jp{{"name", p.name}, {"expression", p.expression.source()}, {"unit", unitKindName(p.unit)}};
        if (!p.comment.empty()) jp["comment"] = p.comment;
        params.push_back(std::move(jp));
    }
    json features = json::array();
    for (const auto& f : features_) features.push_back(featureToJson(*f));
    json counters = json::object();
    for (const auto& [type, n] : nameCounters_) counters[featureTypeName(type)] = n;

    json doc{{"format", "opencircuit"}, {"version", kFormatVersion}, {"parameters", params},     {"features", features},
             {"marker", marker_},       {"next_id", nextId_},        {"name_counters", counters}};
    return doc.dump(2) + "\n";
}

Document Document::fromJson(const std::string& text) {
    try {
        json j = json::parse(text);
        if (j.value("format", std::string{}) != "opencircuit") throw Error("not an OpenCircuit document");
        int version = j.at("version").get<int>();
        if (version > kFormatVersion)
            throw Error("document format version " + std::to_string(version) + " is newer than this build supports");

        Document d;
        // Parameters may reference each other in any order: add all with a
        // placeholder, then assign the real expressions.
        for (const auto& jp : j.at("parameters")) {
            auto unitName = jp.value("unit", std::string("length"));
            auto unit = parseUnitKind(unitName);
            if (!unit) throw Error("unknown parameter unit '" + unitName + "'");
            d.params_.add(jp.at("name").get<std::string>(), "0", *unit, jp.value("comment", std::string{}));
        }
        for (const auto& jp : j.at("parameters"))
            d.params_.setExpression(jp.at("name").get<std::string>(), jp.at("expression").get<std::string>());

        for (const auto& jf : j.at("features")) {
            auto f = featureFromJson(jf);
            if (d.findFeature(f->id)) throw Error("duplicate feature id " + std::to_string(f->id));
            d.nextId_ = std::max(d.nextId_, f->id + 1);
            d.features_.push_back(std::move(f));
        }
        d.marker_ = std::min(j.value("marker", d.features_.size()), d.features_.size());
        d.nextId_ = std::max(d.nextId_, j.value("next_id", 1));
        const json counters = j.value("name_counters", json::object()); // keep alive for items()
        for (const auto& [name, n] : counters.items()) {
            FeatureType t;
            if (parseFeatureType(name, t)) d.nameCounters_[t] = n.get<int>();
        }
        return d;
    } catch (const json::exception& e) {
        throw Error(std::string("malformed document: ") + e.what());
    }
}

} // namespace oc
