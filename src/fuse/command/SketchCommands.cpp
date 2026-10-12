// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/command/SketchCommands.h"

#include "fuse/command/Lookup.h"

namespace fuse::cmd {

using sketch::ConstraintType;
using sketch::EntityId;
using sketch::EntityType;

json Codec<PointRef>::schema() {
    json existing = Codec<int>::schema();
    existing["description"] = "Id of an existing point entity";
    json fresh = Codec<Vec2>::schema();
    fresh["description"] = "[x, y] in mm, for a new point";
    return {{"oneOf", json::array({existing, fresh})}};
}

PointRef Codec<PointRef>::read(const json& j, const std::string& path) {
    if (j.is_number_integer()) return {Codec<int>::read(j, path), {}};
    if (j.is_array()) return {std::nullopt, Codec<Vec2>::read(j, path)};
    throw ArgumentError("'" + path + "' must be a point id or an [x, y] array");
}

json Codec<PointRef>::write(const PointRef& p) {
    return p.entity ? Codec<int>::write(*p.entity) : Codec<Vec2>::write(p.at);
}

namespace {

EntityId resolvePoint(sketch::Sketch& s, const PointRef& p, const char* argument) {
    if (!p.entity) return s.addPoint(p.at);
    try {
        s.point(*p.entity);
    } catch (const Error&) {
        throw Error(std::string(argument) + ": sketch entity " + std::to_string(*p.entity) + " is not a point");
    }
    return *p.entity;
}

UnitKind dimensionUnit(ConstraintType type) {
    return type == ConstraintType::Angle ? UnitKind::Angle : UnitKind::Length;
}

void checkEntityCount(const std::vector<EntityId>& entities, size_t max) {
    if (entities.empty() || entities.size() > max)
        throw Error("entities: expected 1 to " + std::to_string(max) + " entity ids, got " +
                    std::to_string(entities.size()));
}

EntityId entityAt(const std::vector<EntityId>& entities, size_t i) { return i < entities.size() ? entities[i] : -1; }

sketch::Sketch& sketchOf(EditContext& ctx, int id) { return requireFeature<SketchFeature>(ctx.doc, id).sketch; }

} // namespace

FeatureRef CreateSketch::run(EditContext& ctx) const {
    checkExpression(ctx.doc, "offset", offset, UnitKind::Length);
    auto& s = ctx.doc.add<SketchFeature>();
    s.plane = plane;
    s.offset = offset;
    return {s.id};
}

NoOutput EditSketch::run(EditContext& ctx) const {
    auto& s = requireFeature<SketchFeature>(ctx.doc, sketchId);
    if (offset) checkExpression(ctx.doc, "offset", *offset, UnitKind::Length);
    if (plane) s.plane = *plane;
    if (offset) s.offset = *offset;
    return {};
}

EntityRef AddSketchPoint::run(EditContext& ctx) const { return {sketchOf(ctx, sketchId).addPoint(at)}; }

EntityRef AddSketchLine::run(EditContext& ctx) const {
    sketch::Sketch& s = sketchOf(ctx, sketchId);
    const EntityId a = resolvePoint(s, start, "start");
    const EntityId b = resolvePoint(s, end, "end");
    if (a == b) throw Error("a line needs two different points");
    return {s.addLine(a, b)};
}

EntityRef AddSketchCircle::run(EditContext& ctx) const {
    if (radius <= 0) throw Error("radius must be positive");
    sketch::Sketch& s = sketchOf(ctx, sketchId);
    return {s.addCircle(resolvePoint(s, center, "center"), radius)};
}

EntityRef AddSketchArc::run(EditContext& ctx) const {
    sketch::Sketch& s = sketchOf(ctx, sketchId);
    const EntityId c = resolvePoint(s, center, "center");
    const EntityId a = resolvePoint(s, start, "start");
    const EntityId b = resolvePoint(s, end, "end");
    if (a == b || a == c || b == c) throw Error("an arc needs distinct centre, start and end points");
    return {s.addArc(c, a, b)};
}

AddSketchRectangle::Output AddSketchRectangle::run(EditContext& ctx) const {
    if (corner1.x == corner2.x || corner1.y == corner2.y) throw Error("the rectangle's corners must differ in x and y");
    return {sketchOf(ctx, sketchId).addRectangle(corner1, corner2)};
}

ConstraintRef AddSketchConstraint::run(EditContext& ctx) const {
    if (sketch::isDimension(type))
        throw Error(std::string(sketch::constraintTypeName(type)) + " is a dimension; use sketch_add_dimension");
    checkEntityCount(entities, 3);
    sketch::Sketch& s = sketchOf(ctx, sketchId);
    if (type == ConstraintType::Fix) {
        if (entities.size() != 1) throw Error("fix takes one point");
        return {s.fixPoint(entities[0])};
    }
    return {s.addConstraint({type, entityAt(entities, 0), entityAt(entities, 1), entityAt(entities, 2)})};
}

ConstraintRef AddSketchDimension::run(EditContext& ctx) const {
    if (!sketch::isDimension(type))
        throw Error(std::string(sketch::constraintTypeName(type)) + " is not a dimension; use sketch_add_constraint");
    checkEntityCount(entities, 2);
    checkExpression(ctx.doc, "expression", expression, dimensionUnit(type));
    return {sketchOf(ctx, sketchId).addDimension(type, entityAt(entities, 0), entityAt(entities, 1), expression)};
}

NoOutput SetSketchDimension::run(EditContext& ctx) const {
    sketch::Sketch& s = sketchOf(ctx, sketchId);
    if (constraint < 0 || static_cast<size_t>(constraint) >= s.constraints().size())
        throw Error("sketch constraint " + std::to_string(constraint) + " does not exist");
    checkExpression(ctx.doc, "expression", expression, dimensionUnit(s.constraints()[constraint].type));
    s.setDimension(constraint, expression);
    return {};
}

NoOutput SetSketchConstruction::run(EditContext& ctx) const {
    sketchOf(ctx, sketchId).setConstruction(entity, construction);
    return {};
}

NoOutput MoveSketchPoint::run(EditContext& ctx) const {
    sketchOf(ctx, sketchId).setPoint(point, to);
    return {};
}

DescribeSketch::Output DescribeSketch::run(QueryContext& ctx) const {
    const auto& f = requireFeature<SketchFeature>(ctx.doc, sketchId);
    const sketch::Sketch& s = f.sketch;
    Output out;
    out.status = f.solveResult.status;
    out.message = f.solveResult.message;
    out.degreesOfFreedom = f.solveResult.degreesOfFreedom;

    const auto& solved = f.solveResult.fullyConstrained;
    for (EntityId id = 0; static_cast<size_t>(id) < s.entities().size(); ++id) {
        const sketch::Entity& e = s.entity(id);
        Entity item{id, e.type, e.construction, static_cast<size_t>(id) < solved.size() && solved[id], {}, {}, {}};
        switch (e.type) {
        case EntityType::Point:
            item.coordinates = {s.point(id)};
            break;
        case EntityType::Line:
            item.points = {e.p1, e.p2};
            break;
        case EntityType::Circle:
            item.points = {e.p1};
            item.radius = s.radius(id);
            break;
        case EntityType::Arc:
            item.points = {e.p1, e.p2, e.p3};
            item.radius = s.radius(id);
            break;
        }
        for (EntityId p : item.points) item.coordinates.push_back(s.point(p));
        out.entities.push_back(std::move(item));
    }

    for (sketch::ConstraintId id = 0; static_cast<size_t>(id) < s.constraints().size(); ++id) {
        const sketch::Constraint& c = s.constraints()[id];
        Constraint item{id, c.type, {}, {}, {}};
        for (EntityId e : {c.e1, c.e2, c.e3})
            if (e >= 0) item.entities.push_back(e);
        if (sketch::isDimension(c.type)) item.value = c.value;
        if (!c.expression.empty()) item.expression = c.expression;
        out.constraints.push_back(std::move(item));
    }

    for (size_t i = 0; i < f.profiles.size(); ++i) out.profiles.push_back({static_cast<int>(i), f.profiles[i].area});
    return out;
}

} // namespace fuse::cmd
