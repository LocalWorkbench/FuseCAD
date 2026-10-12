// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/command/Command.h"
#include "fuse/model/Features.h"

#include <optional>
#include <string>
#include <vector>

namespace fuse::cmd {

// A sketch point: the id of an existing point entity, or [x, y] coordinates
// (mm, in the sketch plane) at which to create one. Curves that share a point
// entity are connected.
struct PointRef {
    std::optional<sketch::EntityId> entity;
    Vec2 at;
};

template <>
struct Codec<PointRef> {
    static json schema();
    static PointRef read(const json& j, const std::string& path);
    static json write(const PointRef& p);
};

struct EntityRef {
    sketch::EntityId entity = -1;
    static constexpr auto fields = std::tuple{
        field("entity", &EntityRef::entity, "Id of the new sketch entity", Format::EntityId),
    };
};

struct ConstraintRef {
    sketch::ConstraintId constraint = -1;
    static constexpr auto fields = std::tuple{
        field("constraint", &ConstraintRef::constraint, "Id of the new constraint", Format::ConstraintId),
    };
};

struct CreateSketch {
    static constexpr CommandInfo info{
        "sketch_create", "Create Sketch",
        "Start a 2D sketch on an origin plane, optionally offset along its normal. Sketch coordinates are mm; on "
        "XY they are (x, y), on XZ (x, z), on YZ (y, z). Every sketch has a fixed origin point, entity 0."};
    SketchPlane plane = SketchPlane::XY;
    std::string offset = "0 mm";
    static constexpr auto fields = std::tuple{
        defaulted("plane", &CreateSketch::plane, "Origin plane to sketch on"),
        defaulted("offset", &CreateSketch::offset, "Distance along the plane's normal", Format::Length),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct EditSketch {
    static constexpr CommandInfo info{"sketch_edit", "Edit Sketch Plane", "Move a sketch to another plane or offset."};
    int sketchId = 0;
    std::optional<SketchPlane> plane;
    std::optional<std::string> offset;
    static constexpr auto fields = std::tuple{
        field("sketch", &EditSketch::sketchId, "The sketch feature", Format::FeatureId),
        field("plane", &EditSketch::plane, "New origin plane"),
        field("offset", &EditSketch::offset, "New offset along the plane's normal", Format::Length),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct AddSketchPoint {
    static constexpr CommandInfo info{"sketch_add_point", "Point", "Add a point to a sketch."};
    int sketchId = 0;
    Vec2 at;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchPoint::sketchId, "The sketch feature", Format::FeatureId),
        field("at", &AddSketchPoint::at, "Position [x, y] in mm"),
    };
    using Output = EntityRef;
    Output run(EditContext& ctx) const;
};

struct AddSketchLine {
    static constexpr CommandInfo info{
        "sketch_add_line", "Line",
        "Add a line segment. Pass an existing point's id as start or end to connect it to other curves."};
    int sketchId = 0;
    PointRef start, end;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchLine::sketchId, "The sketch feature", Format::FeatureId),
        field("start", &AddSketchLine::start, "Start point"),
        field("end", &AddSketchLine::end, "End point"),
    };
    using Output = EntityRef;
    Output run(EditContext& ctx) const;
};

struct AddSketchCircle {
    static constexpr CommandInfo info{"sketch_add_circle", "Circle", "Add a circle by centre and radius."};
    int sketchId = 0;
    PointRef center;
    double radius = 0;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchCircle::sketchId, "The sketch feature", Format::FeatureId),
        field("center", &AddSketchCircle::center, "Centre point"),
        field("radius", &AddSketchCircle::radius,
              "Initial radius in mm; add a radius or diameter dimension to drive it"),
    };
    using Output = EntityRef;
    Output run(EditContext& ctx) const;
};

struct AddSketchArc {
    static constexpr CommandInfo info{
        "sketch_add_arc", "Arc",
        "Add a circular arc around a centre, running counter-clockwise from start to end. The end is kept at the "
        "same distance from the centre as the start."};
    int sketchId = 0;
    PointRef center, start, end;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchArc::sketchId, "The sketch feature", Format::FeatureId),
        field("center", &AddSketchArc::center, "Centre point"),
        field("start", &AddSketchArc::start, "Start point"),
        field("end", &AddSketchArc::end, "End point"),
    };
    using Output = EntityRef;
    Output run(EditContext& ctx) const;
};

struct AddSketchRectangle {
    static constexpr CommandInfo info{
        "sketch_add_rectangle", "Rectangle",
        "Add an axis-aligned rectangle from two opposite corners: four connected lines with horizontal and "
        "vertical constraints."};
    int sketchId = 0;
    Vec2 corner1, corner2;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchRectangle::sketchId, "The sketch feature", Format::FeatureId),
        field("corner1", &AddSketchRectangle::corner1, "First corner [x, y] in mm"),
        field("corner2", &AddSketchRectangle::corner2, "Opposite corner [x, y] in mm"),
    };
    struct Output {
        std::vector<sketch::EntityId> lines;
        static constexpr auto fields = std::tuple{
            field("lines", &Output::lines, "Ids of the bottom, right, top and left lines"),
        };
    };
    Output run(EditContext& ctx) const;
};

struct AddSketchConstraint {
    static constexpr CommandInfo info{
        "sketch_add_constraint", "Constrain",
        "Add a geometric constraint. Entities by type: coincident (point, point); horizontal, vertical (line, or "
        "point, point); parallel, perpendicular (line, line); tangent (line, circle|arc); equal (line, line or "
        "circle|arc, circle|arc); fix (point, locked where it is); point_on (point, line|circle|arc); midpoint "
        "(point, line); symmetric (point, point, mirror line). Dimensions use sketch_add_dimension."};
    int sketchId = 0;
    sketch::ConstraintType type = sketch::ConstraintType::Coincident;
    std::vector<sketch::EntityId> entities;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchConstraint::sketchId, "The sketch feature", Format::FeatureId),
        field("type", &AddSketchConstraint::type, "The constraint"),
        field("entities", &AddSketchConstraint::entities, "One to three entity ids, as the type requires"),
    };
    using Output = ConstraintRef;
    Output run(EditContext& ctx) const;
};

struct AddSketchDimension {
    static constexpr CommandInfo info{
        "sketch_add_dimension", "Dimension",
        "Add a driving dimension. Entities by type: distance (point, point or point, line); horizontal_distance, "
        "vertical_distance (point, point; signed, second minus first); radius, diameter (circle|arc); angle (line, "
        "line). The expression may reference parameters, so the sketch follows them."};
    int sketchId = 0;
    sketch::ConstraintType type = sketch::ConstraintType::Distance;
    std::vector<sketch::EntityId> entities;
    std::string expression;
    static constexpr auto fields = std::tuple{
        field("sketch", &AddSketchDimension::sketchId, "The sketch feature", Format::FeatureId),
        field("type", &AddSketchDimension::type, "The dimension"),
        field("entities", &AddSketchDimension::entities, "One or two entity ids, as the type requires"),
        field("expression", &AddSketchDimension::expression,
              R"(Length, or angle for angle dimensions: "25 mm", "width / 2", "30 deg")", Format::Expression),
    };
    using Output = ConstraintRef;
    Output run(EditContext& ctx) const;
};

struct SetSketchDimension {
    static constexpr CommandInfo info{"sketch_set_dimension", "Edit Dimension", "Change a dimension's value."};
    int sketchId = 0;
    sketch::ConstraintId constraint = -1;
    std::string expression;
    static constexpr auto fields = std::tuple{
        field("sketch", &SetSketchDimension::sketchId, "The sketch feature", Format::FeatureId),
        field("constraint", &SetSketchDimension::constraint, "The dimension", Format::ConstraintId),
        field("expression", &SetSketchDimension::expression, "Its new value", Format::Expression),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct SetSketchConstruction {
    static constexpr CommandInfo info{
        "sketch_set_construction", "Construction",
        "Mark a curve as construction geometry (a helper that constraints can use but that never forms profiles), "
        "or back."};
    int sketchId = 0;
    sketch::EntityId entity = -1;
    bool construction = true;
    static constexpr auto fields = std::tuple{
        field("sketch", &SetSketchConstruction::sketchId, "The sketch feature", Format::FeatureId),
        field("entity", &SetSketchConstruction::entity, "The curve", Format::EntityId),
        defaulted("construction", &SetSketchConstruction::construction, "true for construction geometry"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct MoveSketchPoint {
    static constexpr CommandInfo info{
        "sketch_move_point", "Drag",
        "Drag a point towards a position. The sketch is re-solved: constraints still hold and geometry they "
        "do not fix moves as little as possible. A fully constrained point stays where it is."};
    int sketchId = 0;
    sketch::EntityId point = -1;
    Vec2 to;
    static constexpr auto fields = std::tuple{
        field("sketch", &MoveSketchPoint::sketchId, "The sketch feature", Format::FeatureId),
        field("point", &MoveSketchPoint::point, "The point to drag", Format::EntityId),
        field("to", &MoveSketchPoint::to, "Target position [x, y] in mm"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct DescribeSketch {
    static constexpr CommandInfo info{
        "sketch_describe", "Inspect Sketch",
        "Report a sketch's entities, constraints, solver state and closed profiles as of the last recompute."};
    int sketchId = 0;
    static constexpr auto fields = std::tuple{
        field("sketch", &DescribeSketch::sketchId, "The sketch feature", Format::FeatureId),
    };

    struct Entity {
        sketch::EntityId id = -1;
        sketch::EntityType type = sketch::EntityType::Point;
        bool construction = false;
        bool fullyConstrained = false;
        std::vector<sketch::EntityId> points;
        std::vector<Vec2> coordinates;
        std::optional<double> radius;
        static constexpr auto fields = std::tuple{
            field("id", &Entity::id, "Entity id", Format::EntityId),
            field("type", &Entity::type, "Kind of entity"),
            field("construction", &Entity::construction, "Construction geometry"),
            field("fully_constrained", &Entity::fullyConstrained, "Position and size are fully determined"),
            field("points", &Entity::points,
                  "Point entities it uses: line (start, end), circle (centre), arc (centre, start, end)"),
            field("coordinates", &Entity::coordinates, "Positions of a point, or of `points`, in mm"),
            field("radius", &Entity::radius, "Circles and arcs, in mm"),
        };
    };
    struct Constraint {
        sketch::ConstraintId id = -1;
        sketch::ConstraintType type = sketch::ConstraintType::Coincident;
        std::vector<sketch::EntityId> entities;
        std::optional<double> value;
        std::optional<std::string> expression;
        static constexpr auto fields = std::tuple{
            field("id", &Constraint::id, "Constraint id", Format::ConstraintId),
            field("type", &Constraint::type, "Kind of constraint"),
            field("entities", &Constraint::entities, "Constrained entities"),
            field("value", &Constraint::value, "Dimensions: current value in mm or radians"),
            field("expression", &Constraint::expression, "Dimensions: driving expression", Format::Expression),
        };
    };
    struct Profile {
        int index = 0;
        double area = 0;
        static constexpr auto fields = std::tuple{
            field("index", &Profile::index, "Index to pass in an extrude or revolve's profiles"),
            field("area", &Profile::area, "Net area in mm^2"),
        };
    };
    struct Output {
        sketch::SolveStatus status = sketch::SolveStatus::Solved;
        std::string message;
        int degreesOfFreedom = 0;
        std::vector<Entity> entities;
        std::vector<Constraint> constraints;
        std::vector<Profile> profiles;
        static constexpr auto fields = std::tuple{
            field("status", &Output::status, "Solver result"),
            field("message", &Output::message, "Solver explanation when not solved"),
            field("degrees_of_freedom", &Output::degreesOfFreedom, "Remaining freedom; 0 = fully constrained"),
            field("entities", &Output::entities, "Points and curves"),
            field("constraints", &Output::constraints, "Constraints and dimensions"),
            field("profiles", &Output::profiles, "Closed regions that can be extruded or revolved"),
        };
    };
    Output run(QueryContext& ctx) const;
};

} // namespace fuse::cmd
