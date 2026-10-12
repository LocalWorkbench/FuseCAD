// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/EnumTable.h"

#include <optional>
#include <string>
#include <string_view>

namespace fuse {

// Internal base units: millimetres for length, radians for angle.
enum class UnitKind { Length, Angle, Unitless };

// A value in base units together with its dimension.
struct Quantity {
    double value = 0;
    UnitKind kind = UnitKind::Unitless;
};

struct UnitInfo {
    UnitKind kind;
    double toBase; // multiply a value in this unit by toBase to get base units
};

std::optional<UnitInfo> lookupUnit(std::string_view name);

inline constexpr auto kUnitKinds = makeEnumTable<EnumName<UnitKind>>({
    {UnitKind::Length, "length"},
    {UnitKind::Angle, "angle"},
    {UnitKind::Unitless, "unitless"},
});
constexpr const auto& enumTable(UnitKind) { return kUnitKinds; }

const char* unitKindName(UnitKind kind); // "length", "angle", "unitless"
std::optional<UnitKind> parseUnitKind(std::string_view name);

// Format a base-unit value for display ("12.5 mm", "45 deg").
std::string formatValue(double baseValue, UnitKind kind);

} // namespace fuse
