// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/Units.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fuse {

// A parsed arithmetic expression such as "width / 2 + 3 mm" or "sin(30 deg) * r".
//
// Grammar:
//   expr    := term (('+' | '-') term)*
//   term    := unary (('*' | '/') unary)*
//   unary   := ('-' | '+') unary | power
//   power   := primary ('^' unary)?
//   primary := number [unit] | ident '(' args ')' | ident | '(' expr ')' [unit]
//
// Every value has a kind: length, angle or unitless. Literals followed by a
// unit ("10 mm", "1.5in", "45 deg") are converted to base units (mm, rad) and
// take the unit's kind. Identifiers resolve through a caller-supplied lookup
// (user parameters), which supplies their kind; the constants pi and e are unitless.
//
// Bare numbers: a number without a unit has no kind of its own and adopts the
// kind its context expects, read in that kind's display unit (mm or deg):
//   - as a whole result, the kind the caller asks for: "10" for a length is
//     10 mm, "45" for an angle is 45 deg (not radians), "2" for a unitless value is 2;
//   - next to a dimensioned operand of + - min max, that operand's kind:
//     "10 + 5 mm" is 15 mm, "90 - 30 deg" is 60 deg;
//   - inside sin/cos/tan, an angle: "sin(30)" is sin(30 deg);
//   - as a factor, divisor, exponent or unit-suffixed group, unitless:
//     "w * 2", "w / 4", "2 ^ 3", "(1 + 1) in".
// Arithmetic between bare numbers stays bare ("2 * 3" for a length is 6 mm).
//
// Dimension rules (violations throw fuse::Error):
//   + - min max     operands of the same kind
//   *               unitless * x = x; length * length (area) and other products are errors
//   /               x / unitless = x, x / x = unitless; anything else is an error
//   ^               unitless base and exponent
//   sin cos tan     angle -> unitless
//   asin acos atan  unitless -> angle
//   atan2(y, x)     two lengths or two unitless values -> angle
//   sqrt            unitless -> unitless
//   abs             any kind, preserved
//   floor ceil round  length or unitless, preserved (lengths round in mm)
class Expression {
public:
    using Lookup = std::function<Quantity(std::string_view name)>;

    Expression();
    static Expression parse(std::string_view source); // throws fuse::Error

    // Evaluate to a value of kind `expected`, in base units. Throws fuse::Error
    // on unknown identifiers, dimension errors (including a result of another
    // kind than `expected`) and non-finite results.
    double evaluate(UnitKind expected, const Lookup& lookup) const;
    double evaluate(UnitKind expected) const; // throws if the expression references any identifier

    // Parameter names referenced (functions and constants excluded), deduplicated.
    const std::vector<std::string>& references() const { return refs_; }
    const std::string& source() const { return source_; }

    // This expression with every reference to parameter `from` renamed to `to`
    // (whole identifiers only; the rest of the source text is kept verbatim).
    Expression renamed(std::string_view from, std::string_view to) const;

    struct Node;

private:
    std::shared_ptr<const Node> root_;
    std::string source_;
    std::vector<std::string> refs_;
};

// True if `name` is reserved (unit, function, or constant) and so cannot be used
// as a parameter name.
bool isReservedName(std::string_view name);
bool isValidIdentifier(std::string_view name);

} // namespace fuse
