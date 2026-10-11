// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/core/Error.h"
#include "fuse/core/Expression.h"
#include "fuse/core/Math.h"

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using fuse::Expression;
using fuse::Quantity;
using fuse::UnitKind;

namespace {

double eval(const char* src, UnitKind expected = UnitKind::Unitless, const std::map<std::string, Quantity>& vars = {}) {
    return Expression::parse(src).evaluate(expected, [&](std::string_view name) {
        auto it = vars.find(std::string(name));
        if (it == vars.end()) throw fuse::Error("unknown " + std::string(name));
        return it->second;
    });
}

double length(const char* src, const std::map<std::string, Quantity>& vars = {}) {
    return eval(src, UnitKind::Length, vars);
}

double angle(const char* src, const std::map<std::string, Quantity>& vars = {}) {
    return eval(src, UnitKind::Angle, vars);
}

constexpr double kDeg = fuse::kPi / 180;

} // namespace

TEST_CASE("arithmetic and precedence") {
    CHECK(eval("1 + 2 * 3") == doctest::Approx(7));
    CHECK(eval("(1 + 2) * 3") == doctest::Approx(9));
    CHECK(eval("2 ^ 3 ^ 2") == doctest::Approx(512)); // right-associative
    CHECK(eval("-2 ^ 2") == doctest::Approx(-4));
    CHECK(eval("10 / 4") == doctest::Approx(2.5));
    CHECK(eval("1.5e2") == doctest::Approx(150));
    CHECK(eval("--3") == doctest::Approx(3));
}

TEST_CASE("units convert to base units") {
    CHECK(length("10 mm") == doctest::Approx(10));
    CHECK(length("1 in") == doctest::Approx(25.4));
    CHECK(length("2cm + 5 mm") == doctest::Approx(25));
    CHECK(angle("180 deg") == doctest::Approx(fuse::kPi));
    CHECK(angle("1 rad") == doctest::Approx(1));
    CHECK(length("(1 + 1) in") == doctest::Approx(50.8));
    CHECK(length("1 m / 4") == doctest::Approx(250));
}

TEST_CASE("bare numbers adopt the kind their context expects") {
    CHECK(length("10") == doctest::Approx(10));        // mm
    CHECK(angle("45") == doctest::Approx(45 * kDeg));  // degrees, not radians
    CHECK(length("10 + 5 mm") == doctest::Approx(15)); // 10 adopts length
    CHECK(angle("90 - 30 deg") == doctest::Approx(60 * kDeg));
    CHECK(angle("2 * 3") == doctest::Approx(6 * kDeg)); // arithmetic between bare numbers stays bare
    CHECK(eval("sin(30)") == doctest::Approx(0.5));     // trig arguments are angles
    CHECK(length("w * 2", {{"w", {4, UnitKind::Length}}}) == doctest::Approx(8));
    CHECK(length("1 in * 2") == doctest::Approx(50.8));
    CHECK(angle("2 * 45 deg") == doctest::Approx(90 * kDeg));
}

TEST_CASE("dimension rules") {
    CHECK(eval("10 mm / 5 mm") == doctest::Approx(2));
    CHECK(eval("90 deg / 45 deg") == doctest::Approx(2));
    CHECK(length("10 mm / 2") == doctest::Approx(5));
    CHECK(angle("asin(1)") == doctest::Approx(90 * kDeg));
    CHECK(angle("atan2(1 mm, 1 mm)") == doctest::Approx(45 * kDeg));
    CHECK(angle("atan2(1, 1)") == doctest::Approx(45 * kDeg));
    CHECK(length("abs(-3 mm)") == doctest::Approx(3));
    CHECK(length("floor(12.7 mm)") == doctest::Approx(12));
    CHECK(length("max(1 in, 20)") == doctest::Approx(25.4));
    CHECK(eval("pi") == doctest::Approx(fuse::kPi));

    CHECK_THROWS_AS(eval("10 mm + 5 deg"), fuse::Error);
    CHECK_THROWS_AS(eval("10 mm * 5 mm"), fuse::Error); // areas are not supported
    CHECK_THROWS_AS(eval("10 mm * 5 deg"), fuse::Error);
    CHECK_THROWS_AS(eval("10 mm / 5 deg"), fuse::Error);
    CHECK_THROWS_AS(eval("1 / 5 mm"), fuse::Error);
    CHECK_THROWS_AS(eval("(10 mm) ^ 2"), fuse::Error);
    CHECK_THROWS_AS(eval("2 ^ (1 mm)"), fuse::Error);
    CHECK_THROWS_AS(eval("(1 mm) in"), fuse::Error);
    CHECK_THROWS_AS(eval("sin(1 mm)"), fuse::Error);
    CHECK_THROWS_AS(eval("sin(pi)"), fuse::Error); // pi is unitless; write sin(180 deg)
    CHECK_THROWS_AS(angle("asin(1 mm)"), fuse::Error);
    CHECK(angle("atan2(1 mm, 1)") == doctest::Approx(45 * kDeg)); // the bare 1 adopts length
    CHECK_THROWS_AS(angle("atan2(1 mm, pi)"), fuse::Error);
    CHECK_THROWS_AS(angle("atan2(1 deg, 1 deg)"), fuse::Error);
    CHECK_THROWS_AS(eval("sqrt(4 mm)"), fuse::Error);
    CHECK_THROWS_AS(angle("round(10 deg)"), fuse::Error);
    CHECK_THROWS_AS(eval("min(1 mm, 1 deg)"), fuse::Error);
}

TEST_CASE("the result must have the expected kind") {
    CHECK_THROWS_AS(length("45 deg"), fuse::Error);
    CHECK_THROWS_AS(angle("10 mm"), fuse::Error);
    CHECK_THROWS_AS(eval("10 mm"), fuse::Error);
    CHECK_THROWS_AS(length("pi"), fuse::Error);
    CHECK_THROWS_AS(length("a", {{"a", {1, UnitKind::Angle}}}), fuse::Error);
    CHECK(eval("a / b", UnitKind::Unitless, {{"a", {6, UnitKind::Length}}, {"b", {3, UnitKind::Length}}}) ==
          doctest::Approx(2));
}

TEST_CASE("functions and constants") {
    CHECK(eval("sin(90 deg)") == doctest::Approx(1));
    CHECK(eval("sqrt(16) + abs(-2)") == doctest::Approx(6));
    CHECK(eval("max(3, 7) - min(3, 7)") == doctest::Approx(4));
    CHECK(eval("2 * pi") == doctest::Approx(2 * fuse::kPi));
}

TEST_CASE("identifiers resolve through lookup") {
    CHECK(length("width / 2 + 3 mm", {{"width", {80, UnitKind::Length}}}) == doctest::Approx(43));
    auto e = Expression::parse("a * b + a");
    REQUIRE(e.references().size() == 2);
    CHECK(e.references()[0] == "a");
    CHECK(e.references()[1] == "b");
}

TEST_CASE("malformed expressions are rejected") {
    CHECK_THROWS_AS(Expression::parse(""), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("1 +"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("(1 + 2"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("1 2"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("foo(1)"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("sin(1, 2)"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("mm"), fuse::Error);
    CHECK_THROWS_AS(eval("1 / 0"), fuse::Error);
    CHECK_THROWS_AS(eval("sqrt(-1)"), fuse::Error);
    CHECK_THROWS_AS(Expression::parse("x").evaluate(UnitKind::Length), fuse::Error);
}

TEST_CASE("renaming rewrites whole identifiers only") {
    auto e = Expression::parse("w + w2 * w - max(w, 1 mm)");
    CHECK(e.renamed("w", "width").source() == "width + w2 * width - max(width, 1 mm)");
    CHECK(e.renamed("w", "width").references() == std::vector<std::string>{"width", "w2"});
    CHECK(e.renamed("other", "x").source() == e.source());
    CHECK(Expression::parse("1e5 + e").renamed("e5", "x").source() == "1e5 + e");
}

TEST_CASE("identifier rules") {
    CHECK(fuse::isValidIdentifier("width_2"));
    CHECK(fuse::isValidIdentifier("_x"));
    CHECK_FALSE(fuse::isValidIdentifier("2x"));
    CHECK_FALSE(fuse::isValidIdentifier("a-b"));
    CHECK(fuse::isReservedName("mm"));
    CHECK(fuse::isReservedName("sin"));
    CHECK(fuse::isReservedName("pi"));
    CHECK_FALSE(fuse::isReservedName("width"));
}
