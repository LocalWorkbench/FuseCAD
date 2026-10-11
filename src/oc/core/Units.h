// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace oc {

// Internal base units: millimetres for length, radians for angle.
enum class UnitKind { Length, Angle, Unitless };

struct UnitInfo {
    UnitKind kind;
    double toBase; // multiply a value in this unit by toBase to get base units
};

std::optional<UnitInfo> lookupUnit(std::string_view name);

std::string_view unitKindName(UnitKind kind);
std::optional<UnitKind> parseUnitKind(std::string_view name);

// Format a base-unit value for display ("12.5 mm", "45 deg").
std::string formatValue(double baseValue, UnitKind kind);

} // namespace oc
