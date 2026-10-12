// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/EnumTable.h"
#include "fuse/core/Math.h"
#include "fuse/sketch/Profile.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fuse {

class ParameterTable;

namespace sketch {

using EntityId = int;
using ConstraintId = int;

enum class EntityType { Point, Line, Circle, Arc };

inline constexpr auto kEntityTypes = makeEnumTable<EnumName<EntityType>>({
    {EntityType::Point, "point"},
    {EntityType::Line, "line"},
    {EntityType::Circle, "circle"},
    {EntityType::Arc, "arc"},
});
constexpr const auto& enumTable(EntityType) { return kEntityTypes; }

// Geometry is stored parametrically: every point owns two solver variables,
// every circle owns a radius variable. Lines and arcs reference point entities,
// so curves drawn end-to-end share a point and are coincident by construction.
struct Entity {
    EntityType type = EntityType::Point;
    int var = -1;     // Point: index of x (y = var + 1). Circle: index of radius.
    EntityId p1 = -1; // Line: start. Circle/Arc: center.
    EntityId p2 = -1; // Line: end.  Arc: start (arc runs CCW from start to end).
    EntityId p3 = -1; // Arc: end.
    bool construction = false;
};

enum class ConstraintType {
    Coincident,         // point, point
    Horizontal,         // line | point, point
    Vertical,           // line | point, point
    Parallel,           // line, line
    Perpendicular,      // line, line
    Tangent,            // line, circle|arc (either order)
    Equal,              // line, line (length) | circle|arc, circle|arc (radius)
    Fix,                // point (value, value2 = locked position)
    PointOnEntity,      // point, line|circle|arc
    Midpoint,           // point, line
    Symmetric,          // point, point, line (mirror axis)
    Distance,           // point, point | point, line   [dimension]
    HorizontalDistance, // point, point   [dimension, signed: e2.x - e1.x]
    VerticalDistance,   // point, point   [dimension, signed: e2.y - e1.y]
    Radius,             // circle|arc                   [dimension]
    Diameter,           // circle|arc                   [dimension]
    Angle,              // line, line                   [dimension, radians]
};

inline constexpr auto kConstraintTypes = makeEnumTable<EnumName<ConstraintType>>({
    {ConstraintType::Coincident, "coincident"},
    {ConstraintType::Horizontal, "horizontal"},
    {ConstraintType::Vertical, "vertical"},
    {ConstraintType::Parallel, "parallel"},
    {ConstraintType::Perpendicular, "perpendicular"},
    {ConstraintType::Tangent, "tangent"},
    {ConstraintType::Equal, "equal"},
    {ConstraintType::Fix, "fix"},
    {ConstraintType::PointOnEntity, "point_on"},
    {ConstraintType::Midpoint, "midpoint"},
    {ConstraintType::Symmetric, "symmetric"},
    {ConstraintType::Distance, "distance"},
    {ConstraintType::HorizontalDistance, "horizontal_distance"},
    {ConstraintType::VerticalDistance, "vertical_distance"},
    {ConstraintType::Radius, "radius"},
    {ConstraintType::Diameter, "diameter"},
    {ConstraintType::Angle, "angle"},
});
constexpr const auto& enumTable(ConstraintType) { return kConstraintTypes; }

const char* constraintTypeName(ConstraintType t);
std::optional<ConstraintType> parseConstraintType(std::string_view name);
// Dimensions carry a value (and optionally a driving expression); the rest are geometric.
bool isDimension(ConstraintType t);

struct Constraint {
    ConstraintType type = ConstraintType::Coincident;
    EntityId e1 = -1, e2 = -1, e3 = -1;
    double value = 0;  // dimension value in base units (mm / rad); Fix: x coordinate
    double value2 = 0; // Fix: y coordinate
    // Dimensions only: optional driving expression ("width / 2"). When non-empty
    // it is evaluated against the document's parameter table before every solve
    // and overrides value. Angle dimensions expect an angle, all others a length.
    std::string expression;
};

enum class SolveStatus {
    Solved,    // all constraints satisfied
    Redundant, // satisfied, but some constraints duplicate others
    Failed,    // could not satisfy constraints (over-constrained or conflicting)
};

inline constexpr auto kSolveStatuses = makeEnumTable<EnumName<SolveStatus>>({
    {SolveStatus::Solved, "solved"},
    {SolveStatus::Redundant, "redundant"},
    {SolveStatus::Failed, "failed"},
});
constexpr const auto& enumTable(SolveStatus) { return kSolveStatuses; }

struct SolveResult {
    SolveStatus status = SolveStatus::Solved;
    int degreesOfFreedom = 0;
    int redundantEquations = 0;
    // Per-entity: true when the entity's position/size is fully determined
    // (the UI draws these black instead of blue).
    std::vector<bool> fullyConstrained;
    std::string message;

    bool ok() const { return status != SolveStatus::Failed; }
};

class Sketch {
public:
    Sketch();

    // The sketch origin is entity 0, a locked point at (0, 0).
    static constexpr EntityId kOrigin = 0;

    EntityId addPoint(Vec2 p);
    EntityId addLine(EntityId start, EntityId end);
    EntityId addLine(Vec2 start, Vec2 end);
    EntityId addCircle(EntityId center, double radius);
    EntityId addCircle(Vec2 center, double radius);
    // Arc centred on `center`, running counter-clockwise from `start` to `end`.
    // An implicit equal-radius condition keeps both endpoints on the circle.
    EntityId addArc(EntityId center, EntityId start, EntityId end);

    // Convenience: four lines sharing corner points plus horizontal/vertical
    // constraints. Returns the line ids in order bottom, right, top, left.
    std::vector<EntityId> addRectangle(Vec2 corner1, Vec2 corner2);

    // Throws fuse::Error unless the referenced entities have the kinds the
    // constraint type takes (see ConstraintType).
    ConstraintId addConstraint(Constraint c);
    ConstraintId addDimension(ConstraintType type, EntityId e1, EntityId e2, std::string expression);
    ConstraintId fixPoint(EntityId point); // lock at its current position
    // Replaces a dimension's driving expression. Throws, leaving the sketch
    // unchanged, unless `id` is a dimension and a constant expression is in range.
    void setDimension(ConstraintId id, std::string expression);
    void setConstruction(EntityId id, bool construction);

    const std::vector<Entity>& entities() const { return entities_; }
    const Entity& entity(EntityId id) const;
    const std::vector<Constraint>& constraints() const { return constraints_; }

    // Calls `fn` on every dimension's driving expression, which it may rewrite
    // (e.g. to rename a parameter). Takes effect at the next solve().
    void visitExpressions(const std::function<void(std::string&)>& fn);

    Vec2 point(EntityId pointId) const;
    // Move a point directly, e.g. to give the solver a new starting position
    // for interactive editing; constraints are re-applied by the next solve().
    void setPoint(EntityId pointId, Vec2 p);
    double radius(EntityId circleOrArc) const;
    Vec2 lineStart(EntityId line) const { return point(entity(line).p1); }
    Vec2 lineEnd(EntityId line) const { return point(entity(line).p2); }

    // Evaluate driving expressions (against params, if given) and solve all
    // constraints. On success geometry and dimension values are updated in
    // place, and unconstrained geometry moves as little as possible. If an
    // expression throws or the solve fails, the sketch is left unchanged.
    SolveResult solve(const ParameterTable* params = nullptr);

    // Closed regions formed by the non-construction curves, ready for extrusion.
    std::vector<Profile> profiles() const;

    // Raw solver state, exposed for serialisation.
    const std::vector<double>& variables() const { return vars_; }

    // Rebuild from serialised state. Throws fuse::Error if any index is out of
    // range or any entity or constraint references entities of the wrong kind.
    static Sketch fromRaw(std::vector<double> vars, std::vector<Entity> entities, std::vector<Constraint> constraints);

private:
    // Solver variables that never move: the origin's x and y.
    static constexpr int kLockedVars = 2;

    void checkEntity(EntityId id, EntityType type) const;
    void residuals(const std::vector<Constraint>& constraints, const std::vector<double>& x,
                   std::vector<double>& out) const;

    std::vector<double> vars_;
    std::vector<Entity> entities_;
    std::vector<Constraint> constraints_;
};

} // namespace sketch
} // namespace fuse
