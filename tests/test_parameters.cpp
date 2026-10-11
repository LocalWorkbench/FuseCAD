// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/core/Error.h"
#include "fuse/core/Math.h"
#include "fuse/core/Parameters.h"

#include <doctest/doctest.h>

using fuse::ParameterTable;

TEST_CASE("parameters evaluate in dependency order") {
    ParameterTable t;
    t.add("width", "80 mm");
    t.add("half", "width / 2");
    t.add("quarter", "half / 2");
    CHECK(t.value("quarter") == doctest::Approx(20));

    t.setExpression("width", "4 in");
    CHECK(t.value("half") == doctest::Approx(50.8));
    CHECK(t.value("quarter") == doctest::Approx(25.4));
}

TEST_CASE("a parameter may reference one defined later") {
    ParameterTable t;
    t.add("a", "1");
    t.add("b", "2");
    t.setExpression("a", "b * 10");
    CHECK(t.value("a") == doctest::Approx(20));
}

TEST_CASE("cycles and unknown references are rejected transactionally") {
    ParameterTable t;
    t.add("a", "10");
    t.add("b", "a + 1");
    CHECK_THROWS_AS(t.setExpression("a", "b"), fuse::Error);
    CHECK(t.get("a").expression.source() == "10"); // unchanged
    CHECK(t.value("b") == doctest::Approx(11));

    CHECK_THROWS_AS(t.add("c", "nope * 2"), fuse::Error);
    CHECK_FALSE(t.contains("c"));
    CHECK_THROWS_AS(t.setExpression("a", "a"), fuse::Error);
}

TEST_CASE("names are validated") {
    ParameterTable t;
    CHECK_THROWS_AS(t.add("mm", "1"), fuse::Error);
    CHECK_THROWS_AS(t.add("1abc", "1"), fuse::Error);
    t.add("x", "1");
    CHECK_THROWS_AS(t.add("x", "2"), fuse::Error);
}

TEST_CASE("remove respects references; rename rewrites them") {
    ParameterTable t;
    t.add("a", "1");
    t.add("ab", "5");
    t.add("b", "a * 2 + ab");
    CHECK_THROWS_AS(t.remove("a"), fuse::Error);
    t.rename("a", "z");
    CHECK(t.get("b").expression.source() == "z * 2 + ab");
    CHECK(t.value("b") == doctest::Approx(7));
    CHECK_THROWS_AS(t.rename("z", "ab"), fuse::Error);  // taken
    CHECK_THROWS_AS(t.rename("z", "sin"), fuse::Error); // reserved
    CHECK_THROWS_AS(t.rename("nope", "y"), fuse::Error);
    t.rename("b", "c");
    CHECK(t.value("c") == doctest::Approx(7));
    t.remove("c");
    t.remove("z");
    t.remove("ab");
    CHECK(t.all().empty());
}

TEST_CASE("parameter units are enforced") {
    ParameterTable t;
    t.add("w", "30");                           // bare number: 30 mm
    t.add("tilt", "45", fuse::UnitKind::Angle); // bare number: 45 deg
    t.add("n", "4", fuse::UnitKind::Unitless);
    CHECK(t.value("w") == doctest::Approx(30));
    CHECK(t.value("tilt") == doctest::Approx(fuse::kPi / 4));
    t.add("pitch", "w / n");
    CHECK(t.value("pitch") == doctest::Approx(7.5));
    t.add("ratio", "w / pitch", fuse::UnitKind::Unitless);
    CHECK(t.value("ratio") == doctest::Approx(4));

    CHECK_THROWS_AS(t.add("bad", "10 deg"), fuse::Error);
    CHECK_THROWS_AS(t.add("bad", "w", fuse::UnitKind::Angle), fuse::Error);
    CHECK_THROWS_AS(t.add("bad", "w * w"), fuse::Error);
    CHECK_THROWS_AS(t.setExpression("n", "2 mm"), fuse::Error);
    CHECK(t.value("n") == doctest::Approx(4));
    CHECK_THROWS_AS(t.setExpression("w", "30 deg"), fuse::Error);
}

TEST_CASE("ad-hoc expressions see parameter values") {
    ParameterTable t;
    t.add("w", "30 mm");
    CHECK(t.evaluate("w / 3 + 1 cm", fuse::UnitKind::Length) == doctest::Approx(20));
    CHECK_THROWS_AS(t.evaluate("w", fuse::UnitKind::Angle), fuse::Error);
    CHECK(t.dependents("w").empty());
}
