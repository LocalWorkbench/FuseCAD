// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/core/Expression.h"
#include "oc/core/Units.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oc {

struct Parameter {
    std::string name;
    Expression expression;
    UnitKind unit = UnitKind::Length;
    std::string comment;
    double value = 0; // cached result of the last evaluate(), in base units
};

// The user parameter table ("Change Parameters" in the app). Parameters may
// reference each other; evaluation order is resolved topologically and cycles
// are rejected. Every mutating call is transactional: on error the table is
// left unchanged.
class ParameterTable {
public:
    void add(std::string name, std::string_view expression, UnitKind unit = UnitKind::Length, std::string comment = {});
    void setExpression(std::string_view name, std::string_view expression);
    void rename(std::string_view from, std::string to);
    void remove(std::string_view name);

    bool contains(std::string_view name) const;
    const Parameter& get(std::string_view name) const;
    double value(std::string_view name) const { return get(name).value; }
    const std::vector<Parameter>& all() const { return params_; }

    // Evaluate an ad-hoc expression (e.g. a sketch dimension) against current values.
    double evaluate(const Expression& expr) const;
    double evaluate(std::string_view expr) const { return evaluate(Expression::parse(expr)); }

    // Names that directly reference `name`.
    std::vector<std::string> dependents(std::string_view name) const;

private:
    void recompute(); // throws on cycles / unknown references
    void reindex();

    std::vector<Parameter> params_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace oc
