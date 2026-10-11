// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/Expression.h"
#include "fuse/core/Units.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fuse {

struct Parameter {
    std::string name;
    Expression expression;
    UnitKind unit = UnitKind::Length; // the expression must evaluate to this kind
    std::string comment;
    double value = 0; // current value in base units, kept up to date by the table
};

// The user parameter table ("Change Parameters" in the app). Parameters may
// reference each other; evaluation order is resolved topologically and cycles
// are rejected. Every mutating call is transactional: on error the table is
// left unchanged.
class ParameterTable {
public:
    void add(std::string name, std::string_view expression, UnitKind unit = UnitKind::Length, std::string comment = {});
    void setExpression(std::string_view name, std::string_view expression);
    // Renames and rewrites references in other parameters' expressions. Features
    // also reference parameters: use Document::renameParameter / removeParameter.
    void rename(std::string_view from, std::string to);
    void remove(std::string_view name); // throws if another parameter references it

    bool contains(std::string_view name) const;
    const Parameter& get(std::string_view name) const;
    double value(std::string_view name) const { return get(name).value; }
    const std::vector<Parameter>& all() const { return params_; }

    // Evaluate an ad-hoc expression (e.g. a sketch dimension) against current
    // values; the result must be of kind `expected`.
    double evaluate(const Expression& expr, UnitKind expected) const;
    double evaluate(std::string_view expr, UnitKind expected) const {
        return evaluate(Expression::parse(expr), expected);
    }

    // Names that directly reference `name`.
    std::vector<std::string> dependents(std::string_view name) const;

private:
    void recompute(); // throws on cycles / unknown references
    void reindex();

    std::vector<Parameter> params_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace fuse
