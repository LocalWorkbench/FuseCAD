// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/sketch/Sketch.h"

#include "age/core/EnumTable.h"
#include "age/core/Error.h"
#include "age/core/Parameters.h"
#include "age/sketch/Solver.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace age::sketch {

namespace {

// Entity kinds a constraint slot accepts, as a bit mask over EntityType.
// kEmpty: the slot must be unused (-1).
constexpr unsigned kind(EntityType t) { return 1U << static_cast<unsigned>(t); }
constexpr unsigned kEmpty = 0;
constexpr unsigned kPoint = kind(EntityType::Point);
constexpr unsigned kLine = kind(EntityType::Line);
constexpr unsigned kRound = kind(EntityType::Circle) | kind(EntityType::Arc);

using Form = std::array<unsigned, 3>; // accepted kinds for e1, e2, e3

enum class Dimension {
    None,
    Length,         // signed length
    UnsignedLength, // length, must evaluate to >= 0
    Angle,
};

// Solver-side facts per constraint type; names live in kConstraintTypes.
struct ConstraintInfo {
    ConstraintType value;
    Dimension dimension;
    // Accepted entity combinations. The second form is an alternative, unused
    // when its first slot is kEmpty (every constraint takes at least e1).
    std::array<Form, 2> forms;
};

constexpr auto kConstraints = makeEnumTable<ConstraintInfo>({
    {ConstraintType::Coincident, Dimension::None, {{{kPoint, kPoint}}}},
    {ConstraintType::Horizontal, Dimension::None, {{{kLine}, {kPoint, kPoint}}}},
    {ConstraintType::Vertical, Dimension::None, {{{kLine}, {kPoint, kPoint}}}},
    {ConstraintType::Parallel, Dimension::None, {{{kLine, kLine}}}},
    {ConstraintType::Perpendicular, Dimension::None, {{{kLine, kLine}}}},
    {ConstraintType::Tangent, Dimension::None, {{{kLine, kRound}, {kRound, kLine}}}},
    {ConstraintType::Equal, Dimension::None, {{{kLine, kLine}, {kRound, kRound}}}},
    {ConstraintType::Fix, Dimension::None, {{{kPoint}}}},
    {ConstraintType::PointOnEntity, Dimension::None, {{{kPoint, kLine | kRound}}}},
    {ConstraintType::Midpoint, Dimension::None, {{{kPoint, kLine}}}},
    {ConstraintType::Symmetric, Dimension::None, {{{kPoint, kPoint, kLine}}}},
    {ConstraintType::Distance, Dimension::UnsignedLength, {{{kPoint, kPoint | kLine}}}},
    {ConstraintType::HorizontalDistance, Dimension::Length, {{{kPoint, kPoint}}}},
    {ConstraintType::VerticalDistance, Dimension::Length, {{{kPoint, kPoint}}}},
    {ConstraintType::Radius, Dimension::UnsignedLength, {{{kRound}}}},
    {ConstraintType::Diameter, Dimension::UnsignedLength, {{{kRound}}}},
    {ConstraintType::Angle, Dimension::Angle, {{{kLine, kLine}}}},
});

std::string describeKinds(unsigned mask) {
    std::string out;
    for (auto [bit, name] : {std::pair{kPoint, "point"}, std::pair{kLine, "line"},
                             std::pair{kind(EntityType::Circle), "circle"}, std::pair{kind(EntityType::Arc), "arc"}})
        if (mask & bit) out += (out.empty() ? "" : "|") + std::string(name);
    return out;
}

std::string describeForm(const Form& form) {
    std::string out;
    for (unsigned mask : form)
        if (mask != kEmpty) out += (out.empty() ? "" : ", ") + describeKinds(mask);
    return "(" + out + ")";
}

// Throws unless c's entity references match one of its type's forms.
void checkConstraint(const std::vector<Entity>& entities, const Constraint& c) {
    const ConstraintInfo& info = kConstraints[c.type];
    const char* name = kConstraintTypes.name(c.type);
    const std::array<EntityId, 3> ids{c.e1, c.e2, c.e3};
    auto matches = [&](const Form& form) {
        for (size_t i = 0; i < ids.size(); ++i) {
            if (form[i] == kEmpty) {
                if (ids[i] != -1) return false;
            } else if (ids[i] < 0 || static_cast<size_t>(ids[i]) >= entities.size() ||
                       !(form[i] & kind(entities[ids[i]].type))) {
                return false;
            }
        }
        return true;
    };
    const bool hasAlternative = info.forms[1][0] != kEmpty;
    if (!matches(info.forms[0]) && !(hasAlternative && matches(info.forms[1])))
        throw Error(std::string("constraint '") + name + "' takes " + describeForm(info.forms[0]) +
                    (hasAlternative ? " or " + describeForm(info.forms[1]) : std::string()));
    if (!c.expression.empty() && !isDimension(c.type))
        throw Error(std::string("constraint '") + name + "' is not a dimension and cannot have an expression");
}

// Value of a dimension's driving expression, in base units.
double evaluateDimension(ConstraintType type, const std::string& expression, const ParameterTable* params) {
    const Dimension dim = kConstraints[type].dimension;
    const UnitKind kind = dim == Dimension::Angle ? UnitKind::Angle : UnitKind::Length;
    const Expression e = Expression::parse(expression);
    const double v = params ? params->evaluate(e, kind) : e.evaluate(kind);
    if (dim == Dimension::UnsignedLength && v < 0)
        throw Error("dimension \"" + expression + "\" evaluates to a negative length");
    return v;
}

// Guards normalisations against degenerate (zero-length) geometry.
constexpr double kMinLength = 1e-12;

} // namespace

const char* constraintTypeName(ConstraintType t) { return kConstraintTypes.name(t); }

std::optional<ConstraintType> parseConstraintType(std::string_view name) { return kConstraintTypes.parse(name); }

bool isDimension(ConstraintType t) { return kConstraints[t].dimension != Dimension::None; }

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
    auto checkPoint = [&](EntityId id) {
        if (id < 0 || static_cast<size_t>(id) >= entities.size()) throw Error("sketch entity reference out of range");
        if (entities[id].type != EntityType::Point) throw Error("sketch curve endpoint is not a point");
    };
    for (const auto& e : entities) {
        switch (e.type) {
        case EntityType::Point:
            checkVar(e.var, 2);
            break;
        case EntityType::Line:
            checkPoint(e.p1);
            checkPoint(e.p2);
            break;
        case EntityType::Circle:
            checkVar(e.var, 1);
            checkPoint(e.p1);
            break;
        case EntityType::Arc:
            checkPoint(e.p1);
            checkPoint(e.p2);
            checkPoint(e.p3);
            break;
        }
    }
    for (const auto& c : constraints) checkConstraint(entities, c);
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
    checkConstraint(entities_, c);
    constraints_.push_back(std::move(c));
    return static_cast<ConstraintId>(constraints_.size() - 1);
}

ConstraintId Sketch::addDimension(ConstraintType type, EntityId e1, EntityId e2, std::string expression) {
    if (!isDimension(type)) throw Error(std::string(constraintTypeName(type)) + " is not a dimension");
    Constraint c{type, e1, e2};
    c.expression = std::move(expression);
    // Constant expressions can be resolved now; parameter references wait for solve().
    if (Expression::parse(c.expression).references().empty()) c.value = evaluateDimension(type, c.expression, nullptr);
    return addConstraint(std::move(c));
}

void Sketch::setDimension(ConstraintId id, std::string expression) {
    if (id < 0 || static_cast<size_t>(id) >= constraints_.size())
        throw Error("sketch constraint " + std::to_string(id) + " does not exist");
    Constraint& c = constraints_[id];
    if (!isDimension(c.type)) throw Error(std::string(constraintTypeName(c.type)) + " is not a dimension");
    // Same rule as addDimension: resolve constants now, parameter references at solve().
    const double value =
        Expression::parse(expression).references().empty() ? evaluateDimension(c.type, expression, nullptr) : c.value;
    c.expression = std::move(expression);
    c.value = value;
}

ConstraintId Sketch::fixPoint(EntityId id) {
    Vec2 p = point(id);
    Constraint c{ConstraintType::Fix, id};
    c.value = p.x;
    c.value2 = p.y;
    return addConstraint(std::move(c));
}

void Sketch::visitExpressions(const std::function<void(std::string&)>& fn) {
    for (auto& c : constraints_)
        if (!c.expression.empty()) fn(c.expression);
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

void Sketch::residuals(const std::vector<Constraint>& constraints, const std::vector<double>& x,
                       std::vector<double>& out) const {
    out.clear();
    auto P = [&](EntityId id) {
        int v = entities_[id].var;
        return Vec2{x[v], x[v + 1]};
    };
    auto isLine = [&](EntityId id) { return entities_[id].type == EntityType::Line; };
    auto lineDir = [&](EntityId id) { return P(entities_[id].p2) - P(entities_[id].p1); };
    auto center = [&](EntityId id) { return P(entities_[id].p1); };
    auto radiusOf = [&](EntityId id) {
        const Entity& e = entities_[id];
        return e.type == EntityType::Circle ? x[e.var] : (P(e.p2) - P(e.p1)).length();
    };
    auto safeLen = [](Vec2 v) { return std::max(v.length(), kMinLength); };
    // Signed distance of p from the infinite line through `line`.
    auto lineDist = [&](EntityId line, Vec2 p) {
        Vec2 a = P(entities_[line].p1);
        Vec2 d = lineDir(line);
        return cross(d, p - a) / safeLen(d);
    };

    // Implicit: arc endpoints are equidistant from the centre.
    for (const auto& e : entities_) {
        if (e.type != EntityType::Arc) continue;
        out.push_back((P(e.p2) - P(e.p1)).length() - (P(e.p3) - P(e.p1)).length());
    }

    // Entity kinds were validated when each constraint was added (checkConstraint).
    for (const auto& c : constraints) {
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
            Vec2 d1 = lineDir(c.e1), d2 = lineDir(c.e2);
            double n = safeLen(d1) * safeLen(d2);
            out.push_back((c.type == ConstraintType::Parallel ? cross(d1, d2) : dot(d1, d2)) / n);
            break;
        }
        case ConstraintType::Tangent: {
            EntityId line = isLine(c.e1) ? c.e1 : c.e2;
            EntityId round = isLine(c.e1) ? c.e2 : c.e1;
            out.push_back(std::fabs(lineDist(line, center(round))) - radiusOf(round));
            break;
        }
        case ConstraintType::Equal: {
            if (isLine(c.e1))
                out.push_back(lineDir(c.e1).length() - lineDir(c.e2).length());
            else
                out.push_back(radiusOf(c.e1) - radiusOf(c.e2));
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
            else
                out.push_back((p - center(c.e2)).length() - radiusOf(c.e2));
            break;
        }
        case ConstraintType::Midpoint: {
            Vec2 m = (P(entities_[c.e2].p1) + P(entities_[c.e2].p2)) * 0.5;
            Vec2 d = P(c.e1) - m;
            out.push_back(d.x);
            out.push_back(d.y);
            break;
        }
        case ConstraintType::Symmetric: {
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
            out.push_back(radiusOf(c.e1) - c.value);
            break;
        case ConstraintType::Diameter:
            out.push_back(2 * radiusOf(c.e1) - c.value);
            break;
        case ConstraintType::Angle: {
            Vec2 d1 = lineDir(c.e1), d2 = lineDir(c.e2);
            out.push_back(wrapAngle(std::atan2(cross(d1, d2), dot(d1, d2)) - c.value));
            break;
        }
        }
    }
}

SolveResult Sketch::solve(const ParameterTable* params) {
    // Resolve driving dimensions into a copy: nothing is stored unless the solve succeeds.
    std::vector<Constraint> resolved = constraints_;
    for (auto& c : resolved)
        if (!c.expression.empty()) c.value = evaluateDimension(c.type, c.expression, params);

    const int locked = kLockedVars;
    const int n = static_cast<int>(vars_.size()) - locked;

    auto F = [&](const std::vector<double>& free, std::vector<double>& out) {
        std::vector<double> full(vars_.begin(), vars_.begin() + locked);
        full.insert(full.end(), free.begin(), free.end());
        residuals(resolved, full, out);
    };

    std::vector<double> x(vars_.begin() + locked, vars_.end());
    SolverOutput so = solveLeastSquares(F, x);

    SolveResult r;
    r.degreesOfFreedom = n - so.rank;
    r.redundantEquations = so.equations - so.rank;
    if (!so.converged) {
        r.status = SolveStatus::Failed;
        r.message = "sketch is over-constrained or has conflicting constraints";
    } else {
        std::copy(x.begin(), x.end(), vars_.begin() + locked);
        constraints_ = std::move(resolved);
        if (r.redundantEquations > 0) {
            r.status = SolveStatus::Redundant;
            r.message = std::to_string(r.redundantEquations) + " redundant constraint equation(s)";
        }
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

} // namespace age::sketch
