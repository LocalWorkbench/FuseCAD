// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/core/Parameters.h"

#include "age/core/Error.h"

#include <algorithm>
#include <functional>

namespace age {

namespace {

void checkName(std::string_view name) {
    if (!isValidIdentifier(name)) throw Error("invalid parameter name '" + std::string(name) + "'");
    if (isReservedName(name)) throw Error("'" + std::string(name) + "' is a reserved name");
}

} // namespace

void ParameterTable::add(std::string name, std::string_view expression, UnitKind unit, std::string comment) {
    checkName(name);
    if (contains(name)) throw Error("parameter '" + name + "' already exists");
    ParameterTable next = *this;
    next.params_.push_back({std::move(name), Expression::parse(expression), unit, std::move(comment), 0});
    next.reindex();
    next.recompute();
    *this = std::move(next);
}

void ParameterTable::setExpression(std::string_view name, std::string_view expression) {
    ParameterTable next = *this;
    auto it = next.index_.find(std::string(name));
    if (it == next.index_.end()) throw Error("unknown parameter '" + std::string(name) + "'");
    next.params_[it->second].expression = Expression::parse(expression);
    next.recompute();
    *this = std::move(next);
}

void ParameterTable::rename(std::string_view from, std::string to) {
    checkName(to);
    if (contains(to)) throw Error("parameter '" + to + "' already exists");
    ParameterTable next = *this;
    auto it = next.index_.find(std::string(from));
    if (it == next.index_.end()) throw Error("unknown parameter '" + std::string(from) + "'");
    for (auto& p : next.params_) p.expression = p.expression.renamed(from, to);
    next.params_[it->second].name = std::move(to);
    next.reindex();
    *this = std::move(next);
}

void ParameterTable::remove(std::string_view name) {
    auto deps = dependents(name);
    if (!deps.empty()) throw Error("cannot delete '" + std::string(name) + "': referenced by '" + deps.front() + "'");
    auto it = index_.find(std::string(name));
    if (it == index_.end()) throw Error("unknown parameter '" + std::string(name) + "'");
    params_.erase(params_.begin() + static_cast<std::ptrdiff_t>(it->second));
    reindex();
}

bool ParameterTable::contains(std::string_view name) const { return index_.count(std::string(name)) != 0; }

const Parameter& ParameterTable::get(std::string_view name) const {
    auto it = index_.find(std::string(name));
    if (it == index_.end()) throw Error("unknown parameter '" + std::string(name) + "'");
    return params_[it->second];
}

double ParameterTable::evaluate(const Expression& expr, UnitKind expected) const {
    return expr.evaluate(expected, [this](std::string_view name) {
        const Parameter& p = get(name);
        return Quantity{p.value, p.unit};
    });
}

std::vector<std::string> ParameterTable::dependents(std::string_view name) const {
    std::vector<std::string> out;
    for (const auto& p : params_) {
        const auto& refs = p.expression.references();
        if (std::find(refs.begin(), refs.end(), name) != refs.end()) out.push_back(p.name);
    }
    return out;
}

void ParameterTable::reindex() {
    index_.clear();
    for (size_t i = 0; i < params_.size(); ++i) index_[params_[i].name] = i;
}

void ParameterTable::recompute() {
    enum class Mark { None, Visiting, Done };
    std::vector<Mark> marks(params_.size(), Mark::None);

    std::function<void(size_t)> visit = [&](size_t i) {
        if (marks[i] == Mark::Done) return;
        if (marks[i] == Mark::Visiting) throw Error("circular reference involving parameter '" + params_[i].name + "'");
        marks[i] = Mark::Visiting;
        for (const auto& ref : params_[i].expression.references()) {
            auto it = index_.find(ref);
            if (it == index_.end())
                throw Error("parameter '" + params_[i].name + "' references unknown parameter '" + ref + "'");
            visit(it->second);
        }
        params_[i].value = params_[i].expression.evaluate(params_[i].unit, [this](std::string_view n) {
            const Parameter& p = params_[index_.at(std::string(n))];
            return Quantity{p.value, p.unit};
        });
        marks[i] = Mark::Done;
    };

    for (size_t i = 0; i < params_.size(); ++i) visit(i);
}

} // namespace age
