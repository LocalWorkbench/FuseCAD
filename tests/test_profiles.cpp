// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/sketch/Sketch.h"

#include <doctest/doctest.h>

#include <algorithm>

using namespace oc;
using namespace oc::sketch;

namespace {

std::vector<double> areas(const std::vector<Profile>& ps) {
    std::vector<double> a;
    a.reserve(ps.size());
    for (const auto& p : ps) a.push_back(p.area);
    std::sort(a.begin(), a.end());
    return a;
}

} // namespace

TEST_CASE("a rectangle is one profile") {
    Sketch s;
    s.addRectangle({0, 0}, {40, 25});
    auto ps = s.profiles();
    REQUIRE(ps.size() == 1);
    CHECK(ps[0].area == doctest::Approx(1000));
    CHECK(ps[0].outer.signedArea > 0);
    CHECK(ps[0].outer.edges.size() == 4);
    CHECK(ps[0].holes.empty());
}

TEST_CASE("a circle inside a rectangle yields a holed region and a disc") {
    Sketch s;
    s.addRectangle({0, 0}, {40, 25});
    s.addCircle({20, 12.5}, 5);
    auto ps = s.profiles();
    REQUIRE(ps.size() == 2);
    auto a = areas(ps);
    CHECK(a[0] == doctest::Approx(kPi * 25).epsilon(1e-3));
    CHECK(a[1] == doctest::Approx(1000 - kPi * 25).epsilon(1e-3));
    auto holed = std::find_if(ps.begin(), ps.end(), [](const Profile& p) { return !p.holes.empty(); });
    REQUIRE(holed != ps.end());
    CHECK(holed->holes.size() == 1);
    CHECK(holed->holes[0].signedArea < 0);
}

TEST_CASE("nested islands attach to their nearest container") {
    Sketch s;
    s.addRectangle({0, 0}, {100, 100});
    s.addCircle({50, 50}, 30);
    s.addCircle({50, 50}, 10);
    auto ps = s.profiles();
    REQUIRE(ps.size() == 3);
    for (const auto& p : ps) CHECK(p.holes.size() <= 1);
    auto a = areas(ps);
    CHECK(a[0] == doctest::Approx(kPi * 100).epsilon(1e-3));
    CHECK(a[1] == doctest::Approx(kPi * (900 - 100)).epsilon(1e-3));
    CHECK(a[2] == doctest::Approx(10000 - kPi * 900).epsilon(1e-3));
}

TEST_CASE("a shared edge splits two regions") {
    Sketch s;
    EntityId a = s.addPoint({0, 0}), b = s.addPoint({10, 0}), c = s.addPoint({20, 0});
    EntityId d = s.addPoint({20, 10}), e = s.addPoint({10, 10}), f = s.addPoint({0, 10});
    s.addLine(a, b);
    s.addLine(b, c);
    s.addLine(c, d);
    s.addLine(d, e);
    s.addLine(e, f);
    s.addLine(f, a);
    s.addLine(b, e); // divider
    auto ps = s.profiles();
    REQUIRE(ps.size() == 2);
    CHECK(ps[0].area == doctest::Approx(100));
    CHECK(ps[1].area == doctest::Approx(100));
}

TEST_CASE("dangling and construction geometry is ignored") {
    Sketch s;
    auto lines = s.addRectangle({0, 0}, {10, 10});
    s.addLine(s.entity(lines[0]).p2, s.addPoint({20, -5})); // dangling spur
    EntityId diag = s.addLine(s.entity(lines[0]).p1, s.entity(lines[1]).p2);
    s.setConstruction(diag, true);
    auto ps = s.profiles();
    REQUIRE(ps.size() == 1);
    CHECK(ps[0].area == doctest::Approx(100));
    CHECK(ps[0].outer.edges.size() == 4);

    Sketch open;
    open.addLine({0, 0}, {10, 0});
    open.addLine({10, 0}, {10, 10});
    CHECK(open.profiles().empty());
}

TEST_CASE("a slot made of lines and arcs") {
    // Two semicircles of radius 5 joined by 20 mm straight sides.
    Sketch s;
    EntityId p1 = s.addPoint({0, -5}), p2 = s.addPoint({20, -5});
    EntityId p3 = s.addPoint({20, 5}), p4 = s.addPoint({0, 5});
    EntityId c1 = s.addPoint({20, 0}), c2 = s.addPoint({0, 0});
    s.addLine(p1, p2);
    s.addArc(c1, p2, p3);
    s.addLine(p3, p4);
    s.addArc(c2, p4, p1);
    auto ps = s.profiles();
    REQUIRE(ps.size() == 1);
    CHECK(ps[0].area == doctest::Approx(200 + kPi * 25).epsilon(1e-3));
    CHECK(ps[0].outer.edges.size() == 4);
}

TEST_CASE("geometry helpers") {
    std::vector<Vec2> sq{{0, 0}, {2, 0}, {2, 2}, {0, 2}};
    CHECK(signedArea(sq) == doctest::Approx(4));
    CHECK(pointInPolygon({1, 1}, sq));
    CHECK_FALSE(pointInPolygon({3, 1}, sq));
}
