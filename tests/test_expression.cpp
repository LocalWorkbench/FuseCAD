// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/core/Error.h"
#include "oc/core/Expression.h"
#include "oc/core/Math.h"

#include <doctest/doctest.h>

#include <map>
#include <string>

using oc::Expression;

namespace {

double eval(const char* src, const std::map<std::string, double>& vars = {}) {
    return Expression::parse(src).evaluate([&](std::string_view name) {
        auto it = vars.find(std::string(name));
        if (it == vars.end()) throw oc::Error("unknown " + std::string(name));
        return it->second;
    });
}

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
    CHECK(eval("10 mm") == doctest::Approx(10));
    CHECK(eval("1 in") == doctest::Approx(25.4));
    CHECK(eval("2cm + 5 mm") == doctest::Approx(25));
    CHECK(eval("180 deg") == doctest::Approx(oc::kPi));
    CHECK(eval("(1 + 1) in") == doctest::Approx(50.8));
    CHECK(eval("1 m / 4") == doctest::Approx(250));
}

TEST_CASE("functions and constants") {
    CHECK(eval("sin(90 deg)") == doctest::Approx(1));
    CHECK(eval("sqrt(16) + abs(-2)") == doctest::Approx(6));
    CHECK(eval("max(3, 7) - min(3, 7)") == doctest::Approx(4));
    CHECK(eval("2 * pi") == doctest::Approx(2 * oc::kPi));
}

TEST_CASE("identifiers resolve through lookup") {
    CHECK(eval("width / 2 + 3 mm", {{"width", 80}}) == doctest::Approx(43));
    auto e = Expression::parse("a * b + a");
    REQUIRE(e.references().size() == 2);
    CHECK(e.references()[0] == "a");
    CHECK(e.references()[1] == "b");
}

TEST_CASE("malformed expressions are rejected") {
    CHECK_THROWS_AS(Expression::parse(""), oc::Error);
    CHECK_THROWS_AS(Expression::parse("1 +"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("(1 + 2"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("1 2"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("foo(1)"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("sin(1, 2)"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("mm"), oc::Error);
    CHECK_THROWS_AS(eval("1 / 0"), oc::Error);
    CHECK_THROWS_AS(eval("sqrt(-1)"), oc::Error);
    CHECK_THROWS_AS(Expression::parse("x").evaluate(), oc::Error);
}

TEST_CASE("identifier rules") {
    CHECK(oc::isValidIdentifier("width_2"));
    CHECK(oc::isValidIdentifier("_x"));
    CHECK_FALSE(oc::isValidIdentifier("2x"));
    CHECK_FALSE(oc::isValidIdentifier("a-b"));
    CHECK(oc::isReservedName("mm"));
    CHECK(oc::isReservedName("sin"));
    CHECK(oc::isReservedName("pi"));
    CHECK_FALSE(oc::isReservedName("width"));
}
