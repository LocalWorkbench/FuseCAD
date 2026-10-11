// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/core/Units.h"

#include "oc/core/Math.h"

#include <array>
#include <cstdio>
#include <utility>

namespace oc {

namespace {

constexpr std::array<std::pair<std::string_view, UnitInfo>, 9> kUnits{{
    {"mm", {UnitKind::Length, 1.0}},
    {"cm", {UnitKind::Length, 10.0}},
    {"m", {UnitKind::Length, 1000.0}},
    {"um", {UnitKind::Length, 0.001}},
    {"in", {UnitKind::Length, 25.4}},
    {"ft", {UnitKind::Length, 304.8}},
    {"mil", {UnitKind::Length, 0.0254}},
    {"deg", {UnitKind::Angle, kPi / 180.0}},
    {"rad", {UnitKind::Angle, 1.0}},
}};

} // namespace

std::optional<UnitInfo> lookupUnit(std::string_view name) {
    for (const auto& [n, info] : kUnits)
        if (n == name) return info;
    return std::nullopt;
}

std::string_view unitKindName(UnitKind kind) {
    switch (kind) {
    case UnitKind::Length:
        return "length";
    case UnitKind::Angle:
        return "angle";
    case UnitKind::Unitless:
        return "unitless";
    }
    return "unitless";
}

std::optional<UnitKind> parseUnitKind(std::string_view name) {
    if (name == "length") return UnitKind::Length;
    if (name == "angle") return UnitKind::Angle;
    if (name == "unitless") return UnitKind::Unitless;
    return std::nullopt;
}

std::string formatValue(double baseValue, UnitKind kind) {
    char buf[64];
    switch (kind) {
    case UnitKind::Length:
        std::snprintf(buf, sizeof buf, "%.6g mm", baseValue);
        break;
    case UnitKind::Angle:
        std::snprintf(buf, sizeof buf, "%.6g deg", baseValue * 180.0 / kPi);
        break;
    case UnitKind::Unitless:
        std::snprintf(buf, sizeof buf, "%.6g", baseValue);
        break;
    }
    return buf;
}

} // namespace oc
