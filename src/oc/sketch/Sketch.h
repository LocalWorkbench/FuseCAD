// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/core/Math.h"
#include "oc/sketch/Profile.h"

#include <string>
#include <vector>

namespace oc {

class ParameterTable;

namespace sketch {

using EntityId = int;
using ConstraintId = int;

enum class EntityType { Point, Line, Circle, Arc };

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
    Tangent,            // line, circle|arc
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

const char* constraintTypeName(ConstraintType t);
bool parseConstraintType(const std::string& name, ConstraintType& out);
bool isDimension(ConstraintType t);
bool isUnsignedDimension(ConstraintType t); // must evaluate to >= 0

struct Constraint {
    ConstraintType type = ConstraintType::Coincident;
    EntityId e1 = -1, e2 = -1, e3 = -1;
    double value = 0;  // dimension value in base units (mm / rad)
    double value2 = 0; // Fix: y coordinate
    // Optional driving expression ("width / 2"). When non-empty it is evaluated
    // against the document's parameter table before every solve and overrides value.
    std::string expression;
};

enum class SolveStatus {
    Solved,    // all constraints satisfied
    Redundant, // satisfied, but some constraints duplicate others
    Failed,    // could not satisfy constraints (over-constrained or conflicting)
};

struct SolveResult {
    SolveStatus status = SolveStatus::Solved;
    int iterations = 0;
    double residual = 0; // max |f_i| at the end
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

    ConstraintId addConstraint(Constraint c);
    ConstraintId addDimension(ConstraintType type, EntityId e1, EntityId e2, std::string expression);
    ConstraintId fixPoint(EntityId point); // lock at its current position
    void removeConstraint(ConstraintId id);
    void setConstruction(EntityId id, bool construction);

    const std::vector<Entity>& entities() const { return entities_; }
    const Entity& entity(EntityId id) const;
    const std::vector<Constraint>& constraints() const { return constraints_; }
    Constraint& constraint(ConstraintId id);

    Vec2 point(EntityId pointId) const;
    void setPoint(EntityId pointId, Vec2 p); // e.g. while dragging, before re-solving
    double radius(EntityId circleOrArc) const;
    Vec2 lineStart(EntityId line) const { return point(entity(line).p1); }
    Vec2 lineEnd(EntityId line) const { return point(entity(line).p2); }

    // Evaluate driving expressions (if params given) and solve all constraints.
    // Geometry is updated in place; unconstrained geometry moves as little as possible.
    SolveResult solve(const ParameterTable* params = nullptr);

    // Closed regions formed by the non-construction curves, ready for extrusion.
    std::vector<Profile> profiles() const;

    // Raw solver state, exposed for serialisation.
    const std::vector<double>& variables() const { return vars_; }

    // Rebuild from serialised state (no validation beyond index bounds).
    static Sketch fromRaw(std::vector<double> vars, std::vector<Entity> entities, std::vector<Constraint> constraints);

private:
    void checkEntity(EntityId id, EntityType type) const;
    void residuals(const std::vector<double>& x, std::vector<double>& out) const;
    int lockedVarCount() const { return 2; } // origin x, y

    std::vector<double> vars_;
    std::vector<Entity> entities_;
    std::vector<Constraint> constraints_;
};

} // namespace sketch
} // namespace oc
