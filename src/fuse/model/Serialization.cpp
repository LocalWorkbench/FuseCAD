// SPDX-License-Identifier: GPL-3.0-or-later
//
// Native .fcad format: a JSON document holding the parametric definition only
// (parameters, sketches, feature timeline). Geometry is regenerated on load,
// so files are small, diffable and kernel-independent.

#include "fuse/core/EnumTable.h"
#include "fuse/core/Error.h"
#include "fuse/model/Document.h"
#include "fuse/model/Features.h"

#include <nlohmann/json.hpp>

#include <concepts>

namespace fuse {

using nlohmann::json;

namespace {

// --- enums -----------------------------------------------------------------

constexpr auto kPlanes = makeEnumTable<EnumName<SketchPlane>>({
    {SketchPlane::XY, "XY"},
    {SketchPlane::XZ, "XZ"},
    {SketchPlane::YZ, "YZ"},
});

constexpr auto kEntityTypes = makeEnumTable<EnumName<sketch::EntityType>>({
    {sketch::EntityType::Point, "point"},
    {sketch::EntityType::Line, "line"},
    {sketch::EntityType::Circle, "circle"},
    {sketch::EntityType::Arc, "arc"},
});

constexpr auto kEdgeModes = makeEnumTable<EnumName<EdgeQuery::Mode>>({
    {EdgeQuery::Mode::All, "all"},
    {EdgeQuery::Mode::ParallelTo, "parallel_to"},
    {EdgeQuery::Mode::NearPoints, "near_points"},
});

// Reads a string key and maps it through `parse` (a parseX function or an EnumTable).
template <class Parse>
    requires std::invocable<const Parse&, const std::string&>
auto parseKey(const json& j, const char* key, const char* what, const Parse& parse) {
    const auto name = j.at(key).get<std::string>();
    auto value = parse(name);
    if (!value) throw Error(std::string("unknown ") + what + " '" + name + "'");
    return *value;
}

template <class Entry, size_t N>
auto parseKey(const json& j, const char* key, const char* what, const EnumTable<Entry, N>& table) {
    return parseKey(j, key, what, [&](const std::string& s) { return table.parse(s); });
}

// --- small values ----------------------------------------------------------

json vec3(Vec3 v) { return json::array({v.x, v.y, v.z}); }
Vec3 vec3(const json& j) { return {j.at(0).get<double>(), j.at(1).get<double>(), j.at(2).get<double>()}; }

json edgeQueryToJson(const EdgeQuery& q) {
    json j{{"mode", kEdgeModes.name(q.mode)}};
    if (q.mode == EdgeQuery::Mode::ParallelTo) j["direction"] = vec3(q.direction);
    if (q.mode == EdgeQuery::Mode::NearPoints) {
        j["points"] = json::array();
        for (const auto& p : q.points) j["points"].push_back(vec3(p));
    }
    return j;
}

EdgeQuery edgeQueryFromJson(const json& j) {
    EdgeQuery q;
    q.mode = parseKey(j, "mode", "edge selection mode", kEdgeModes);
    if (q.mode == EdgeQuery::Mode::ParallelTo) q.direction = vec3(j.at("direction"));
    if (q.mode == EdgeQuery::Mode::NearPoints)
        for (const auto& p : j.at("points")) q.points.push_back(vec3(p));
    return q;
}

json selectionToJson(const ProfileSelection& s) { return {{"sketch", s.sketchId}, {"profiles", s.profileIndices}}; }
ProfileSelection selectionFromJson(const json& j) {
    return {j.at("sketch").get<int>(), j.at("profiles").get<std::vector<int>>()};
}

json operationToJson(const BodyOperation& op) {
    return {{"operation", operationName(op.operation)}, {"target", op.targetBody}};
}
BodyOperation operationFromJson(const json& j) {
    BodyOperation op;
    op.operation = parseKey(j, "operation", "body operation", parseOperation);
    op.targetBody = j.at("target").get<int>();
    if (op.operation != FeatureOperation::NewBody && op.targetBody < 0)
        throw Error(std::string(operationName(op.operation)) + " operation has no target body");
    return op;
}

// --- sketch ----------------------------------------------------------------

json sketchToJson(const sketch::Sketch& s) {
    json entities = json::array();
    for (const auto& e : s.entities()) {
        json je{{"type", kEntityTypes.name(e.type)}};
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
        e.type = parseKey(je, "type", "sketch entity type", kEntityTypes);
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
        c.type = parseKey(jc, "type", "constraint type", sketch::parseConstraintType);
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
        j["plane"] = kPlanes.name(s->plane);
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
    } else if (const auto* d = dynamic_cast<const DressUpFeature*>(&f)) {
        j["body"] = d->body;
        j["edges"] = edgeQueryToJson(d->edges);
        j[d->sizeName()] = d->size;
    } else if (const auto* i = featureCast<ImportStepFeature>(&f)) {
        j["path"] = i->path;
    }
    return j;
}

template <class T>
std::unique_ptr<Feature> dressUpFromJson(const json& j) {
    auto f = std::make_unique<T>();
    f->body = j.at("body").get<int>();
    if (f->body < 0) throw Error(std::string(featureTypeName(T::kStaticType)) + " has no body");
    f->edges = edgeQueryFromJson(j.at("edges"));
    f->size = j.at(f->sizeName()).template get<std::string>();
    return f;
}

std::unique_ptr<Feature> featureFromJson(const json& j) {
    const FeatureType type = parseKey(j, "type", "feature type", parseFeatureType);

    std::unique_ptr<Feature> f;
    switch (type) {
    case FeatureType::Sketch: {
        auto s = std::make_unique<SketchFeature>();
        s->plane = parseKey(j, "plane", "sketch plane", kPlanes);
        s->offset = j.at("offset").get<std::string>();
        s->sketch = sketchFromJson(j.at("sketch"));
        f = std::move(s);
        break;
    }
    case FeatureType::Extrude: {
        auto e = std::make_unique<ExtrudeFeature>();
        e->profiles = selectionFromJson(j.at("profiles"));
        e->distance = j.at("distance").get<std::string>();
        e->symmetric = j.at("symmetric").get<bool>();
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
    case FeatureType::Fillet:
        f = dressUpFromJson<FilletFeature>(j);
        break;
    case FeatureType::Chamfer:
        f = dressUpFromJson<ChamferFeature>(j);
        break;
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

    json doc{{"format", "fusecad"}, {"version", kFormatVersion}, {"parameters", params},     {"features", features},
             {"marker", marker_},   {"next_id", nextId_},        {"name_counters", counters}};
    return doc.dump(2) + "\n";
}

Document Document::fromJson(const std::string& text) {
    try {
        json j = json::parse(text);
        if (j.at("format").get<std::string>() != "fusecad") throw Error("not a FuseCAD document");
        int version = j.at("version").get<int>();
        if (version != kFormatVersion)
            throw Error("unsupported document format version " + std::to_string(version) + " (this build reads " +
                        std::to_string(kFormatVersion) + ")");

        Document d;
        // Parameters may reference each other in any order: add all with a
        // placeholder, then assign the real expressions.
        for (const auto& jp : j.at("parameters")) {
            const UnitKind unit = parseKey(jp, "unit", "parameter unit", parseUnitKind);
            d.params_.add(jp.at("name").get<std::string>(), "0", unit, jp.value("comment", std::string{}));
        }
        for (const auto& jp : j.at("parameters"))
            d.params_.setExpression(jp.at("name").get<std::string>(), jp.at("expression").get<std::string>());

        int maxId = 0;
        for (const auto& jf : j.at("features")) {
            auto f = featureFromJson(jf);
            if (f->id <= 0) throw Error("invalid feature id " + std::to_string(f->id));
            if (d.findFeature(f->id)) throw Error("duplicate feature id " + std::to_string(f->id));
            maxId = std::max(maxId, f->id);
            d.features_.push_back(std::move(f));
        }
        d.marker_ = j.at("marker").get<size_t>();
        if (d.marker_ > d.features_.size()) throw Error("timeline marker is past the last feature");
        d.nextId_ = j.at("next_id").get<int>();
        if (d.nextId_ <= maxId) throw Error("next_id must exceed every feature id");
        for (const auto& [name, n] : j.at("name_counters").items()) {
            auto type = parseFeatureType(name);
            if (!type) throw Error("unknown feature type '" + name + "' in name_counters");
            d.nameCounters_[*type] = n.get<int>();
        }
        return d;
    } catch (const json::exception& e) {
        throw Error(std::string("malformed document: ") + e.what());
    }
}

} // namespace fuse
