// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/core/Error.h"
#include "oc/core/Parameters.h"

#include <doctest/doctest.h>

using oc::ParameterTable;

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
    CHECK_THROWS_AS(t.setExpression("a", "b"), oc::Error);
    CHECK(t.get("a").expression.source() == "10"); // unchanged
    CHECK(t.value("b") == doctest::Approx(11));

    CHECK_THROWS_AS(t.add("c", "nope * 2"), oc::Error);
    CHECK_FALSE(t.contains("c"));
    CHECK_THROWS_AS(t.setExpression("a", "a"), oc::Error);
}

TEST_CASE("names are validated") {
    ParameterTable t;
    CHECK_THROWS_AS(t.add("mm", "1"), oc::Error);
    CHECK_THROWS_AS(t.add("1abc", "1"), oc::Error);
    t.add("x", "1");
    CHECK_THROWS_AS(t.add("x", "2"), oc::Error);
}

TEST_CASE("remove and rename respect references") {
    ParameterTable t;
    t.add("a", "1");
    t.add("b", "a * 2");
    CHECK_THROWS_AS(t.remove("a"), oc::Error);
    CHECK_THROWS_AS(t.rename("a", "z"), oc::Error);
    t.rename("b", "c");
    CHECK(t.value("c") == doctest::Approx(2));
    t.remove("c");
    t.remove("a");
    CHECK(t.all().empty());
}

TEST_CASE("ad-hoc expressions see parameter values") {
    ParameterTable t;
    t.add("w", "30 mm");
    CHECK(t.evaluate("w / 3 + 1 cm") == doctest::Approx(20));
    CHECK(t.dependents("w").empty());
}
