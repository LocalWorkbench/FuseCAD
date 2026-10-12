// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/core/Error.h"
#include "age/core/Parameters.h"
#include "age/sketch/Sketch.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace age;
using namespace age::sketch;

namespace {

constexpr double kTol = 1e-6;

void checkPoint(const Sketch& s, EntityId p, double x, double y) {
    CHECK(s.point(p).x == doctest::Approx(x).epsilon(kTol));
    CHECK(s.point(p).y == doctest::Approx(y).epsilon(kTol));
}

} // namespace

TEST_CASE("an empty sketch solves trivially") {
    Sketch s;
    SolveResult r = s.solve();
    CHECK(r.status == SolveStatus::Solved);
    CHECK(r.degreesOfFreedom == 0);
    CHECK(r.fullyConstrained.at(Sketch::kOrigin));
}

TEST_CASE("a dimensioned rectangle is fully constrained") {
    Sketch s;
    auto lines = s.addRectangle({1, 2}, {30, 17});
    EntityId a = s.entity(lines[0]).p1, b = s.entity(lines[0]).p2, c = s.entity(lines[1]).p2;
    s.addConstraint({ConstraintType::Coincident, a, Sketch::kOrigin});
    s.addDimension(ConstraintType::HorizontalDistance, a, b, "40 mm");
    s.addDimension(ConstraintType::VerticalDistance, b, c, "25 mm");

    SolveResult r = s.solve();
    REQUIRE(r.status == SolveStatus::Solved);
    CHECK(r.degreesOfFreedom == 0);
    checkPoint(s, a, 0, 0);
    checkPoint(s, b, 40, 0);
    checkPoint(s, c, 40, 25);
    for (EntityId l : lines) CHECK(r.fullyConstrained[l]);
}

TEST_CASE("degrees of freedom are reported for partial sketches") {
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 1});
    CHECK(s.solve().degreesOfFreedom == 4);

    s.addConstraint({ConstraintType::Horizontal, l});
    CHECK(s.solve().degreesOfFreedom == 3);

    s.addConstraint({ConstraintType::Coincident, s.entity(l).p1, Sketch::kOrigin});
    SolveResult r = s.solve();
    CHECK(r.degreesOfFreedom == 1);
    CHECK(r.fullyConstrained[s.entity(l).p1]);
    CHECK_FALSE(r.fullyConstrained[s.entity(l).p2]);
    CHECK_FALSE(r.fullyConstrained[l]);
}

TEST_CASE("unconstrained geometry stays put") {
    Sketch s;
    EntityId free = s.addPoint({7, 8});
    EntityId l = s.addLine({0, 0}, {10, 3});
    s.addConstraint({ConstraintType::Horizontal, l});
    REQUIRE(s.solve().ok());
    checkPoint(s, free, 7, 8);
    CHECK(s.lineStart(l).y == doctest::Approx(s.lineEnd(l).y));
}

TEST_CASE("dimensions follow parameters") {
    ParameterTable params;
    params.add("len", "50 mm");
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 0});
    s.addConstraint({ConstraintType::Coincident, s.entity(l).p1, Sketch::kOrigin});
    s.addConstraint({ConstraintType::Horizontal, l});
    s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "len * 2");
    REQUIRE(s.solve(&params).ok());
    checkPoint(s, s.entity(l).p2, 100, 0);

    params.setExpression("len", "1 in");
    REQUIRE(s.solve(&params).ok());
    checkPoint(s, s.entity(l).p2, 50.8, 0);
}

TEST_CASE("parallel, perpendicular, equal and angle") {
    Sketch s;
    EntityId l1 = s.addLine({0, 0}, {10, 1});
    EntityId l2 = s.addLine({0, 5}, {9, 7});
    EntityId l3 = s.addLine({20, 0}, {21, 9});
    s.addConstraint({ConstraintType::Parallel, l1, l2});
    s.addConstraint({ConstraintType::Perpendicular, l1, l3});
    s.addConstraint({ConstraintType::Equal, l1, l3});
    REQUIRE(s.solve().ok());
    auto dir = [&](EntityId l) { return s.lineEnd(l) - s.lineStart(l); };
    CHECK(cross(dir(l1), dir(l2)) == doctest::Approx(0).epsilon(1e-6));
    CHECK(dot(dir(l1), dir(l3)) == doctest::Approx(0).epsilon(1e-6));
    CHECK(dir(l1).length() == doctest::Approx(dir(l3).length()));

    Sketch a;
    EntityId h = a.addLine({0, 0}, {10, 0});
    EntityId d = a.addLine({0, 0}, {5, 5});
    a.addConstraint({ConstraintType::Horizontal, h});
    a.addDimension(ConstraintType::Angle, h, d, "30 deg");
    REQUIRE(a.solve().ok());
    Vec2 v = a.lineEnd(d) - a.lineStart(d);
    CHECK(std::atan2(v.y, v.x) == doctest::Approx(kPi / 6).epsilon(1e-6));
}

TEST_CASE("circles, arcs and tangency") {
    Sketch s;
    EntityId c = s.addCircle({5, 5}, 3);
    s.addConstraint({ConstraintType::Coincident, s.entity(c).p1, Sketch::kOrigin});
    s.addDimension(ConstraintType::Diameter, c, -1, "20 mm");
    EntityId l = s.addLine({-20, 12}, {20, 11});
    s.addConstraint({ConstraintType::Horizontal, l});
    s.addConstraint({ConstraintType::Tangent, l, c});
    REQUIRE(s.solve().ok());
    CHECK(s.radius(c) == doctest::Approx(10));
    CHECK(s.lineStart(l).y == doctest::Approx(10).epsilon(1e-6));

    Sketch arcSketch;
    EntityId ctr = arcSketch.addPoint({0, 0});
    EntityId p1 = arcSketch.addPoint({10, 0});
    EntityId p2 = arcSketch.addPoint({0, 7});
    EntityId arc = arcSketch.addArc(ctr, p1, p2);
    arcSketch.addDimension(ConstraintType::Radius, arc, -1, "8 mm");
    REQUIRE(arcSketch.solve().ok());
    CHECK((arcSketch.point(p1) - arcSketch.point(ctr)).length() == doctest::Approx(8));
    CHECK((arcSketch.point(p2) - arcSketch.point(ctr)).length() == doctest::Approx(8));
}

TEST_CASE("midpoint, symmetric and point-on") {
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 4});
    EntityId m = s.addPoint({3, 3});
    s.addConstraint({ConstraintType::Midpoint, m, l});
    REQUIRE(s.solve().ok());
    Vec2 mid = (s.lineStart(l) + s.lineEnd(l)) * 0.5;
    CHECK((s.point(m) - mid).length() == doctest::Approx(0).epsilon(1e-6));

    Sketch y;
    EntityId axis = y.addLine({0, -10}, {0, 10});
    y.addConstraint({ConstraintType::Vertical, axis});
    EntityId p = y.addPoint({3, 1});
    EntityId q = y.addPoint({-5, 2});
    y.addConstraint({ConstraintType::Symmetric, p, q, axis});
    EntityId c = y.addCircle({0, 0}, 4);
    EntityId onC = y.addPoint({5, 5});
    y.addConstraint({ConstraintType::PointOnEntity, onC, c});
    REQUIRE(y.solve().ok());
    Vec2 a = y.lineStart(axis);
    CHECK((y.point(p).x - a.x) == doctest::Approx(-(y.point(q).x - a.x)).epsilon(1e-6));
    CHECK(y.point(p).y == doctest::Approx(y.point(q).y).epsilon(1e-6));
    CHECK((y.point(onC) - y.point(y.entity(c).p1)).length() == doctest::Approx(y.radius(c)));
}

TEST_CASE("conflicting constraints fail; duplicates are redundant") {
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 0});
    s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "10 mm");
    s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "20 mm");
    CHECK(s.solve().status == SolveStatus::Failed);

    Sketch r;
    EntityId h = r.addLine({0, 0}, {10, 0});
    r.addConstraint({ConstraintType::Horizontal, h});
    r.addConstraint({ConstraintType::Horizontal, h});
    SolveResult res = r.solve();
    CHECK(res.status == SolveStatus::Redundant);
    CHECK(res.redundantEquations == 1);
}

TEST_CASE("fixed points hold their position") {
    Sketch s;
    EntityId l = s.addLine({2, 3}, {10, 5});
    s.fixPoint(s.entity(l).p1);
    s.addConstraint({ConstraintType::Horizontal, l});
    s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "5 mm");
    REQUIRE(s.solve().ok());
    checkPoint(s, s.entity(l).p1, 2, 3);
    checkPoint(s, s.entity(l).p2, 7, 3);
}

TEST_CASE("invalid references are reported") {
    Sketch s;
    CHECK_THROWS_AS(s.addLine(5, 6), age::Error);
    EntityId p = s.addPoint({1, 1});
    CHECK_THROWS_AS(s.addLine(p, p), age::Error);
    CHECK_THROWS_AS(s.radius(p), age::Error);
    CHECK_THROWS_AS(s.setPoint(Sketch::kOrigin, {1, 1}), age::Error);
}

TEST_CASE("constraints reject entities of the wrong kind") {
    Sketch s;
    EntityId p = s.addPoint({1, 1});
    EntityId l = s.addLine({0, 0}, {10, 0});
    EntityId c = s.addCircle({5, 5}, 2);
    using C = Constraint;
    auto rejects = [&](C con) { CHECK_THROWS_AS(s.addConstraint(std::move(con)), age::Error); };
    rejects(C{ConstraintType::Parallel, p, Sketch::kOrigin});
    rejects(C{ConstraintType::Parallel, l, -1});                   // missing entity
    rejects(C{ConstraintType::Horizontal, -1});                    // missing entity
    rejects(C{ConstraintType::Horizontal, p});                     // a point needs a second point
    rejects(C{ConstraintType::Horizontal, l, p});                  // a line takes no second entity
    rejects(C{ConstraintType::Coincident, p, l});                  // point and line
    rejects(C{ConstraintType::Coincident, p, Sketch::kOrigin, l}); // unexpected third entity
    rejects(C{ConstraintType::Tangent, l, l});
    rejects(C{ConstraintType::Equal, l, c});
    rejects(C{ConstraintType::Radius, l});
    rejects(C{ConstraintType::Distance, p, c});
    rejects(C{ConstraintType::Symmetric, p, Sketch::kOrigin, c});
    rejects(C{ConstraintType::Fix, 99}); // out of range
    C withExpression{ConstraintType::Parallel, l, s.addLine({0, 1}, {9, 2})};
    withExpression.expression = "1 mm";
    rejects(withExpression); // only dimensions are driven by expressions
    CHECK_THROWS_AS(s.addDimension(ConstraintType::Coincident, p, Sketch::kOrigin, "1 mm"), age::Error);
    CHECK_THROWS_AS(s.addDimension(ConstraintType::Angle, l, l, "10 mm"), age::Error); // wrong kind
    CHECK_THROWS_AS(s.addDimension(ConstraintType::Diameter, c, -1, "-4 mm"), age::Error);
    CHECK(s.constraints().empty());

    // Both entity orders are accepted where the constraint is symmetric in kind.
    s.addConstraint({ConstraintType::Tangent, l, c});
    s.addConstraint({ConstraintType::Tangent, c, l});
    s.addConstraint({ConstraintType::Distance, p, l});
    CHECK(s.constraints().size() == 3);
}

TEST_CASE("fromRaw validates entity and constraint references") {
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 0});
    s.addConstraint({ConstraintType::Horizontal, l});
    auto vars = s.variables();
    auto entities = s.entities();
    auto constraints = s.constraints();
    CHECK_NOTHROW(Sketch::fromRaw(vars, entities, constraints));

    auto badEndpoint = entities;
    badEndpoint.push_back({EntityType::Line, -1, l, Sketch::kOrigin}); // endpoint is a line
    CHECK_THROWS_AS(Sketch::fromRaw(vars, badEndpoint, constraints), age::Error);

    auto badArc = entities;
    badArc.push_back({EntityType::Arc, -1, Sketch::kOrigin, 1, -1}); // missing end point
    CHECK_THROWS_AS(Sketch::fromRaw(vars, badArc, constraints), age::Error);

    auto badVar = entities;
    badVar.push_back({EntityType::Point, static_cast<int>(vars.size()) - 1}); // y past the end
    CHECK_THROWS_AS(Sketch::fromRaw(vars, badVar, constraints), age::Error);

    auto badConstraint = constraints;
    badConstraint.push_back({ConstraintType::Radius, l});
    CHECK_THROWS_AS(Sketch::fromRaw(vars, entities, badConstraint), age::Error);
    badConstraint.back() = {ConstraintType::Coincident, 1, -1};
    CHECK_THROWS_AS(Sketch::fromRaw(vars, entities, badConstraint), age::Error);
    badConstraint.back() = {ConstraintType::Coincident, 1, 42};
    CHECK_THROWS_AS(Sketch::fromRaw(vars, entities, badConstraint), age::Error);
}

TEST_CASE("a failed solve leaves the sketch unchanged") {
    ParameterTable params;
    params.add("len", "10 mm");
    Sketch s;
    EntityId l = s.addLine({0, 0}, {10, 0});
    s.fixPoint(s.entity(l).p1);
    s.fixPoint(s.entity(l).p2);
    ConstraintId dim = s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "len");
    EntityId free = s.addLine({3, 4}, {8, 9});
    s.addConstraint({ConstraintType::Horizontal, free});
    REQUIRE(s.solve(&params).ok());
    const auto before = s.variables();
    CHECK(s.constraints()[dim].value == doctest::Approx(10));

    params.setExpression("len", "20 mm"); // conflicts with the fixed endpoints
    CHECK(s.solve(&params).status == SolveStatus::Failed);
    CHECK(s.variables() == before);
    CHECK(s.constraints()[dim].value == doctest::Approx(10));

    params.setExpression("len", "-5 mm"); // throws while resolving dimensions
    CHECK_THROWS_AS(s.solve(&params), age::Error);
    CHECK(s.variables() == before);
    CHECK(s.constraints()[dim].value == doctest::Approx(10));
}

TEST_CASE("tangent to an arc") {
    Sketch s;
    EntityId ctr = s.addPoint({1, 1});
    EntityId a = s.addPoint({9, 1}), b = s.addPoint({1, 9});
    EntityId arc = s.addArc(ctr, a, b);
    s.addConstraint({ConstraintType::Coincident, ctr, Sketch::kOrigin});
    s.addDimension(ConstraintType::Radius, arc, -1, "10 mm");
    EntityId l = s.addLine({-20, 12}, {20, 13});
    s.addConstraint({ConstraintType::Horizontal, l});
    s.addConstraint({ConstraintType::Tangent, arc, l}); // round entity first
    REQUIRE(s.solve().ok());
    CHECK(s.radius(arc) == doctest::Approx(10));
    CHECK(s.lineStart(l).y == doctest::Approx(10).epsilon(1e-6));
}

TEST_CASE("distance from a point to a line") {
    Sketch s;
    EntityId l = s.addLine({-10, 0}, {10, 0});
    s.fixPoint(s.entity(l).p1);
    s.fixPoint(s.entity(l).p2);
    EntityId p = s.addPoint({3, 7});
    s.addDimension(ConstraintType::Distance, p, l, "5 mm");
    REQUIRE(s.solve().ok());
    CHECK(std::fabs(s.point(p).y) == doctest::Approx(5).epsilon(1e-6));
    CHECK(s.point(p).x == doctest::Approx(3).epsilon(1e-6)); // moved only as far as needed
}

TEST_CASE("angle dimensions wrap past +-180 degrees") {
    Sketch s;
    EntityId h = s.addLine({0, 0}, {10, 0});
    EntityId d = s.addLine(s.entity(h).p1, s.addPoint({-10, -1})); // starts at about -174 deg
    s.addConstraint({ConstraintType::Coincident, s.entity(h).p1, Sketch::kOrigin});
    s.addConstraint({ConstraintType::Horizontal, h});
    s.addDimension(ConstraintType::Angle, h, d, "170 deg");
    REQUIRE(s.solve().ok());
    Vec2 hv = s.lineEnd(h) - s.lineStart(h);
    Vec2 dv = s.lineEnd(d) - s.lineStart(d);
    CHECK(std::atan2(cross(hv, dv), dot(hv, dv)) == doctest::Approx(170 * kPi / 180).epsilon(1e-6));
}

TEST_CASE("negative unsigned dimensions are errors") {
    Sketch s;
    EntityId c = s.addCircle({0, 0}, 5);
    CHECK_THROWS_AS(s.addDimension(ConstraintType::Radius, c, -1, "-1 mm"), age::Error);
    EntityId l = s.addLine({0, 0}, {10, 0});
    CHECK_THROWS_AS(s.addDimension(ConstraintType::Distance, s.entity(l).p1, s.entity(l).p2, "-1"), age::Error);
    // Signed dimensions may be negative.
    s.addDimension(ConstraintType::HorizontalDistance, s.entity(l).p2, s.entity(l).p1, "-10 mm");

    ParameterTable params;
    params.add("r", "-3 mm");
    s.addDimension(ConstraintType::Radius, c, -1, "r");
    CHECK_THROWS_AS(s.solve(&params), age::Error);
}
