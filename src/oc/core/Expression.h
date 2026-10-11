// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace oc {

// A parsed arithmetic expression such as "width / 2 + 3 mm" or "sin(30 deg) * r".
//
// Grammar:
//   expr    := term (('+' | '-') term)*
//   term    := unary (('*' | '/') unary)*
//   unary   := ('-' | '+') unary | power
//   power   := primary ('^' unary)?
//   primary := number [unit] | ident '(' args ')' | ident | '(' expr ')' [unit]
//
// Literals followed by a unit ("10 mm", "1.5in", "45 deg") are converted to base
// units (mm, rad). Bare numbers are taken to already be in base units.
// Identifiers resolve through a caller-supplied lookup (user parameters).
class Expression {
public:
    using Lookup = std::function<double(std::string_view name)>;

    Expression();
    static Expression parse(std::string_view source); // throws oc::Error

    double evaluate(const Lookup& lookup) const;
    double evaluate() const; // throws if the expression references any identifier

    // Parameter names referenced (functions and constants excluded), deduplicated.
    const std::vector<std::string>& references() const { return refs_; }
    const std::string& source() const { return source_; }

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

} // namespace oc
