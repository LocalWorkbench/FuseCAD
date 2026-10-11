// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/sketch/Sketch.h"

#include "oc/core/Error.h"
#include "oc/core/Parameters.h"
#include "oc/sketch/Solver.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oc::sketch {

namespace {

struct TypeName {
    ConstraintType type;
    const char* name;
};

constexpr TypeName kTypeNames[] = {
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
};

} // namespace

const char* constraintTypeName(ConstraintType t) {
    for (const auto& tn : kTypeNames)
        if (tn.type == t) return tn.name;
    return "unknown";
}

bool parseConstraintType(const std::string& name, ConstraintType& out) {
    for (const auto& tn : kTypeNames)
        if (name == tn.name) {
            out = tn.type;
            return true;
        }
    return false;
}

bool isDimension(ConstraintType t) {
    switch (t) {
    case ConstraintType::Distance:
    case ConstraintType::HorizontalDistance:
    case ConstraintType::VerticalDistance:
    case ConstraintType::Radius:
    case ConstraintType::Diameter:
    case ConstraintType::Angle:
        return true;
    default:
        return false;
    }
}

bool isUnsignedDimension(ConstraintType t) {
    return t == ConstraintType::Distance || t == ConstraintType::Radius || t == ConstraintType::Diameter;
}

Sketch::Sketch() {
    // Entity 0: the locked sketch origin.
    vars_ = {0.0, 0.0};
    entities_.push_back({EntityType::Point, 0});
}

Sketch Sketch::fromRaw(std::vector<double> vars, std::vector<Entity> entities, std::vector<Constraint> constraints) {
    Sketch s;
    if (vars.size() < 2 || entities.empty() || entities[0].type != EntityType::Point || entities[0].var != 0)
        throw Error("sketch data is missing the origin point");
    auto checkVar = [&](int v, int width) {
        if (v < 0 || static_cast<size_t>(v) + static_cast<size_t>(width) > vars.size())
            throw Error("sketch entity variable out of range");
    };
    auto checkRef = [&](EntityId id) {
        if (id < 0 || static_cast<size_t>(id) >= entities.size()) throw Error("sketch entity reference out of range");
    };
    for (const auto& e : entities) {
        switch (e.type) {
        case EntityType::Point:
            checkVar(e.var, 2);
            break;
        case EntityType::Line:
            checkRef(e.p1);
            checkRef(e.p2);
            break;
        case EntityType::Circle:
            checkVar(e.var, 1);
            checkRef(e.p1);
            break;
        case EntityType::Arc:
            checkRef(e.p1);
            checkRef(e.p2);
            checkRef(e.p3);
            break;
        }
    }
    for (const auto& c : constraints) {
        for (EntityId id : {c.e1, c.e2, c.e3})
            if (id != -1) checkRef(id);
    }
    s.vars_ = std::move(vars);
    s.entities_ = std::move(entities);
    s.constraints_ = std::move(constraints);
    return s;
}

void Sketch::checkEntity(EntityId id, EntityType type) const {
    if (id < 0 || static_cast<size_t>(id) >= entities_.size())
        throw Error("sketch entity " + std::to_string(id) + " does not exist");
    if (entities_[id].type != type) throw Error("sketch entity " + std::to_string(id) + " has the wrong type");
}

const Entity& Sketch::entity(EntityId id) const {
    if (id < 0 || static_cast<size_t>(id) >= entities_.size())
        throw Error("sketch entity " + std::to_string(id) + " does not exist");
    return entities_[id];
}

Constraint& Sketch::constraint(ConstraintId id) {
    if (id < 0 || static_cast<size_t>(id) >= constraints_.size())
        throw Error("sketch constraint " + std::to_string(id) + " does not exist");
    return constraints_[id];
}

EntityId Sketch::addPoint(Vec2 p) {
    int var = static_cast<int>(vars_.size());
    vars_.push_back(p.x);
    vars_.push_back(p.y);
    entities_.push_back({EntityType::Point, var});
    return static_cast<EntityId>(entities_.size() - 1);
}

EntityId Sketch::addLine(EntityId start, EntityId end) {
    checkEntity(start, EntityType::Point);
    checkEntity(end, EntityType::Point);
    if (start == end) throw Error("a line needs two distinct points");
    Entity e{EntityType::Line};
    e.p1 = start;
    e.p2 = end;
    entities_.push_back(e);
    return static_cast<EntityId>(entities_.size() - 1);
}

EntityId Sketch::addLine(Vec2 start, Vec2 end) { return addLine(addPoint(start), addPoint(end)); }

EntityId Sketch::addCircle(EntityId center, double radius) {
    checkEntity(center, EntityType::Point);
    Entity e{EntityType::Circle, static_cast<int>(vars_.size())};
    e.p1 = center;
    vars_.push_back(radius);
    entities_.push_back(e);
    return static_cast<EntityId>(entities_.size() - 1);
}

EntityId Sketch::addCircle(Vec2 center, double radius) { return addCircle(addPoint(center), radius); }

EntityId Sketch::addArc(EntityId center, EntityId start, EntityId end) {
    checkEntity(center, EntityType::Point);
    checkEntity(start, EntityType::Point);
    checkEntity(end, EntityType::Point);
    if (start == end || start == center || end == center) throw Error("an arc needs three distinct points");
    Entity e{EntityType::Arc};
    e.p1 = center;
    e.p2 = start;
    e.p3 = end;
    entities_.push_back(e);
    return static_cast<EntityId>(entities_.size() - 1);
}

std::vector<EntityId> Sketch::addRectangle(Vec2 c1, Vec2 c2) {
    EntityId a = addPoint({std::min(c1.x, c2.x), std::min(c1.y, c2.y)});
    EntityId b = addPoint({std::max(c1.x, c2.x), std::min(c1.y, c2.y)});
    EntityId c = addPoint({std::max(c1.x, c2.x), std::max(c1.y, c2.y)});
    EntityId d = addPoint({std::min(c1.x, c2.x), std::max(c1.y, c2.y)});
    std::vector<EntityId> lines{addLine(a, b), addLine(b, c), addLine(c, d), addLine(d, a)};
    addConstraint({ConstraintType::Horizontal, lines[0]});
    addConstraint({ConstraintType::Vertical, lines[1]});
    addConstraint({ConstraintType::Horizontal, lines[2]});
    addConstraint({ConstraintType::Vertical, lines[3]});
    return lines;
}

ConstraintId Sketch::addConstraint(Constraint c) {
    for (EntityId id : {c.e1, c.e2, c.e3})
        if (id != -1) entity(id); // bounds check
    constraints_.push_back(std::move(c));
    return static_cast<ConstraintId>(constraints_.size() - 1);
}

ConstraintId Sketch::addDimension(ConstraintType type, EntityId e1, EntityId e2, std::string expression) {
    if (!isDimension(type)) throw Error(std::string(constraintTypeName(type)) + " is not a dimension");
    Constraint c{type, e1, e2};
    c.expression = std::move(expression);
    // Constant expressions can be resolved now; parameter references wait for solve().
    Expression e = Expression::parse(c.expression);
    if (e.references().empty()) c.value = e.evaluate();
    return addConstraint(std::move(c));
}

ConstraintId Sketch::fixPoint(EntityId id) {
    Vec2 p = point(id);
    Constraint c{ConstraintType::Fix, id};
    c.value = p.x;
    c.value2 = p.y;
    return addConstraint(std::move(c));
}

void Sketch::removeConstraint(ConstraintId id) {
    constraint(id);
    constraints_.erase(constraints_.begin() + id);
}

void Sketch::setConstruction(EntityId id, bool construction) {
    entity(id);
    entities_[id].construction = construction;
}

Vec2 Sketch::point(EntityId id) const {
    checkEntity(id, EntityType::Point);
    int v = entities_[id].var;
    return {vars_[v], vars_[v + 1]};
}

void Sketch::setPoint(EntityId id, Vec2 p) {
    checkEntity(id, EntityType::Point);
    if (id == kOrigin) throw Error("the sketch origin cannot be moved");
    int v = entities_[id].var;
    vars_[v] = p.x;
    vars_[v + 1] = p.y;
}

double Sketch::radius(EntityId id) const {
    const Entity& e = entity(id);
    if (e.type == EntityType::Circle) return vars_[e.var];
    if (e.type == EntityType::Arc) return (point(e.p2) - point(e.p1)).length();
    throw Error("sketch entity " + std::to_string(id) + " has no radius");
}

// ---------------------------------------------------------------------------
// Residual equations. Each constraint contributes one or more f_i(x) that are
// zero when the constraint is satisfied.

void Sketch::residuals(const std::vector<double>& x, std::vector<double>& out) const {
    out.clear();
    auto P = [&](EntityId id) {
        int v = entities_[id].var;
        return Vec2{x[v], x[v + 1]};
    };
    auto isLine = [&](EntityId id) { return entities_[id].type == EntityType::Line; };
    auto isRound = [&](EntityId id) {
        return entities_[id].type == EntityType::Circle || entities_[id].type == EntityType::Arc;
    };
    auto lineDir = [&](EntityId id) { return P(entities_[id].p2) - P(entities_[id].p1); };
    auto center = [&](EntityId id) { return P(entities_[id].p1); };
    auto radiusOf = [&](EntityId id) {
        const Entity& e = entities_[id];
        return e.type == EntityType::Circle ? x[e.var] : (P(e.p2) - P(e.p1)).length();
    };
    auto safeLen = [](Vec2 v) { return std::max(v.length(), 1e-12); };
    // Signed distance of p from the infinite line through `line`.
    auto lineDist = [&](EntityId line, Vec2 p) {
        Vec2 a = P(entities_[line].p1);
        Vec2 d = lineDir(line);
        return cross(d, p - a) / safeLen(d);
    };
    auto bad = [&](const Constraint& c) {
        return Error(std::string("constraint '") + constraintTypeName(c.type) + "' has invalid references");
    };

    // Implicit: arc endpoints are equidistant from the centre.
    for (const auto& e : entities_) {
        if (e.type != EntityType::Arc) continue;
        out.push_back((P(e.p2) - P(e.p1)).length() - (P(e.p3) - P(e.p1)).length());
    }

    for (const auto& c : constraints_) {
        switch (c.type) {
        case ConstraintType::Coincident: {
            Vec2 d = P(c.e1) - P(c.e2);
            out.push_back(d.x);
            out.push_back(d.y);
            break;
        }
        case ConstraintType::Horizontal:
        case ConstraintType::Vertical: {
            Vec2 a, b;
            if (isLine(c.e1)) {
                a = P(entities_[c.e1].p1);
                b = P(entities_[c.e1].p2);
            } else {
                a = P(c.e1);
                b = P(c.e2);
            }
            out.push_back(c.type == ConstraintType::Horizontal ? b.y - a.y : b.x - a.x);
            break;
        }
        case ConstraintType::Parallel:
        case ConstraintType::Perpendicular: {
            if (!isLine(c.e1) || !isLine(c.e2)) throw bad(c);
            Vec2 d1 = lineDir(c.e1), d2 = lineDir(c.e2);
            double n = safeLen(d1) * safeLen(d2);
            out.push_back((c.type == ConstraintType::Parallel ? cross(d1, d2) : dot(d1, d2)) / n);
            break;
        }
        case ConstraintType::Tangent: {
            EntityId line = isLine(c.e1) ? c.e1 : c.e2;
            EntityId round = isLine(c.e1) ? c.e2 : c.e1;
            if (!isLine(line) || !isRound(round)) throw bad(c);
            out.push_back(std::fabs(lineDist(line, center(round))) - radiusOf(round));
            break;
        }
        case ConstraintType::Equal: {
            if (isLine(c.e1) && isLine(c.e2))
                out.push_back(lineDir(c.e1).length() - lineDir(c.e2).length());
            else if (isRound(c.e1) && isRound(c.e2))
                out.push_back(radiusOf(c.e1) - radiusOf(c.e2));
            else
                throw bad(c);
            break;
        }
        case ConstraintType::Fix: {
            Vec2 p = P(c.e1);
            out.push_back(p.x - c.value);
            out.push_back(p.y - c.value2);
            break;
        }
        case ConstraintType::PointOnEntity: {
            Vec2 p = P(c.e1);
            if (isLine(c.e2))
                out.push_back(lineDist(c.e2, p));
            else if (isRound(c.e2))
                out.push_back((p - center(c.e2)).length() - radiusOf(c.e2));
            else
                throw bad(c);
            break;
        }
        case ConstraintType::Midpoint: {
            if (!isLine(c.e2)) throw bad(c);
            Vec2 m = (P(entities_[c.e2].p1) + P(entities_[c.e2].p2)) * 0.5;
            Vec2 d = P(c.e1) - m;
            out.push_back(d.x);
            out.push_back(d.y);
            break;
        }
        case ConstraintType::Symmetric: {
            if (!isLine(c.e3)) throw bad(c);
            Vec2 p = P(c.e1), q = P(c.e2);
            Vec2 axis = lineDir(c.e3);
            out.push_back(lineDist(c.e3, (p + q) * 0.5));
            out.push_back(dot(q - p, axis) / safeLen(axis));
            break;
        }
        case ConstraintType::Distance: {
            if (isLine(c.e2))
                out.push_back(std::fabs(lineDist(c.e2, P(c.e1))) - c.value);
            else
                out.push_back((P(c.e1) - P(c.e2)).length() - c.value);
            break;
        }
        case ConstraintType::HorizontalDistance: // signed, from e1 to e2
            out.push_back(P(c.e2).x - P(c.e1).x - c.value);
            break;
        case ConstraintType::VerticalDistance: // signed, from e1 to e2
            out.push_back(P(c.e2).y - P(c.e1).y - c.value);
            break;
        case ConstraintType::Radius:
            if (!isRound(c.e1)) throw bad(c);
            out.push_back(radiusOf(c.e1) - c.value);
            break;
        case ConstraintType::Diameter:
            if (!isRound(c.e1)) throw bad(c);
            out.push_back(2 * radiusOf(c.e1) - c.value);
            break;
        case ConstraintType::Angle: {
            if (!isLine(c.e1) || !isLine(c.e2)) throw bad(c);
            Vec2 d1 = lineDir(c.e1), d2 = lineDir(c.e2);
            out.push_back(wrapAngle(std::atan2(cross(d1, d2), dot(d1, d2)) - c.value));
            break;
        }
        }
    }
}

SolveResult Sketch::solve(const ParameterTable* params) {
    // Resolve driving dimensions.
    for (auto& c : constraints_) {
        if (c.expression.empty()) continue;
        Expression e = Expression::parse(c.expression);
        if (params)
            c.value = params->evaluate(e);
        else
            c.value = e.evaluate();
        if (isUnsignedDimension(c.type) && c.value < 0)
            throw Error("dimension \"" + c.expression + "\" evaluates to a negative length");
    }

    const int locked = lockedVarCount();
    const int n = static_cast<int>(vars_.size()) - locked;

    auto F = [&](const std::vector<double>& free, std::vector<double>& out) {
        std::vector<double> full(vars_.begin(), vars_.begin() + locked);
        full.insert(full.end(), free.begin(), free.end());
        residuals(full, out);
    };

    std::vector<double> x(vars_.begin() + locked, vars_.end());
    SolverOutput so = solveLeastSquares(F, x);

    std::copy(x.begin(), x.end(), vars_.begin() + locked);

    SolveResult r;
    r.iterations = so.iterations;
    r.residual = so.residual;
    r.degreesOfFreedom = n - so.rank;
    r.redundantEquations = so.equations - so.rank;
    if (!so.converged) {
        r.status = SolveStatus::Failed;
        r.message = "sketch is over-constrained or has conflicting constraints";
    } else if (r.redundantEquations > 0) {
        r.status = SolveStatus::Redundant;
        r.message = std::to_string(r.redundantEquations) + " redundant constraint equation(s)";
    }

    // Map determined variables back onto entities.
    auto varDetermined = [&](int var) { return var < locked || so.determined[var - locked]; };
    std::vector<bool> pointDone(entities_.size(), false);
    for (size_t i = 0; i < entities_.size(); ++i)
        if (entities_[i].type == EntityType::Point)
            pointDone[i] = varDetermined(entities_[i].var) && varDetermined(entities_[i].var + 1);
    r.fullyConstrained.resize(entities_.size());
    for (size_t i = 0; i < entities_.size(); ++i) {
        const Entity& e = entities_[i];
        switch (e.type) {
        case EntityType::Point:
            r.fullyConstrained[i] = pointDone[i];
            break;
        case EntityType::Line:
            r.fullyConstrained[i] = pointDone[e.p1] && pointDone[e.p2];
            break;
        case EntityType::Circle:
            r.fullyConstrained[i] = pointDone[e.p1] && varDetermined(e.var);
            break;
        case EntityType::Arc:
            r.fullyConstrained[i] = pointDone[e.p1] && pointDone[e.p2] && pointDone[e.p3];
            break;
        }
    }
    return r;
}

} // namespace oc::sketch
