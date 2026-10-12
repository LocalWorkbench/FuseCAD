// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/model/Document.h"

#include "age/core/Error.h"
#include "age/model/Features.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace age {

namespace {

// An expression that does not parse already fails its feature, and holds no
// usable reference; leave it as the user wrote it.
std::optional<Expression> tryParse(const std::string& source) {
    try {
        return Expression::parse(source);
    } catch (const Error&) {
        return std::nullopt;
    }
}

} // namespace

Document::Document(const Document& other)
    : params_(other.params_), marker_(other.marker_), nextId_(other.nextId_), nameCounters_(other.nameCounters_),
      bodies_(other.bodies_) {
    features_.reserve(other.features_.size());
    for (const auto& f : other.features_) features_.push_back(f->clone());
}

Document& Document::operator=(const Document& other) {
    if (this != &other) {
        Document copy(other);
        *this = std::move(copy);
    }
    return *this;
}

const Feature* Document::findFeature(int id) const {
    auto it = std::find_if(features_.begin(), features_.end(), [id](const auto& f) { return f->id == id; });
    return it == features_.end() ? nullptr : it->get();
}

Feature* Document::findFeature(int id) { return const_cast<Feature*>(std::as_const(*this).findFeature(id)); }

void Document::insert(std::unique_ptr<Feature> f) {
    if (!f) throw Error("cannot add a null feature");
    f->id = nextId_++;
    f->name = featureDisplayName(f->type()) + std::to_string(++nameCounters_[f->type()]);
    features_.insert(features_.begin() + static_cast<std::ptrdiff_t>(marker_), std::move(f));
    ++marker_;
}

void Document::removeFeature(int id) {
    auto it = std::find_if(features_.begin(), features_.end(), [id](const auto& f) { return f->id == id; });
    if (it == features_.end()) throw Error("feature " + std::to_string(id) + " does not exist");
    for (const auto& f : features_) {
        auto deps = f->dependencies();
        if (f->id != id && std::find(deps.begin(), deps.end(), id) != deps.end())
            throw Error("cannot delete '" + (*it)->name + "': '" + f->name + "' depends on it");
    }
    if (std::cmp_less(it - features_.begin(), marker_)) --marker_;
    features_.erase(it);
}

void Document::removeParameter(std::string_view name) {
    for (const auto& f : features_) {
        bool used = false;
        f->visitExpressions([&](const std::string& source) {
            if (auto e = tryParse(source)) {
                const auto& refs = e->references();
                used = used || std::find(refs.begin(), refs.end(), name) != refs.end();
            }
        });
        if (used) throw Error("cannot delete '" + std::string(name) + "': referenced by '" + f->name + "'");
    }
    params_.remove(name);
}

void Document::renameParameter(std::string_view from, std::string to) {
    const std::string target = to;
    params_.rename(from, std::move(to)); // validates; the feature rewrite below cannot fail
    for (const auto& f : features_)
        f->visitExpressions([&](std::string& source) {
            if (auto e = tryParse(source)) source = e->renamed(from, target).source();
        });
}

int Document::latestBody() const {
    for (size_t i = std::min(marker_, features_.size()); i-- > 0;) {
        const Feature& f = *features_[i];
        if (!f.suppressed && f.outputBody() >= 0) return f.outputBody();
    }
    return -1;
}

void Document::setMarker(size_t position) { marker_ = std::min(position, features_.size()); }

RecomputeReport Document::recompute(Kernel& kernel) {
    RecomputeContext ctx{kernel, params_};
    RecomputeReport report;
    for (size_t i = 0; i < features_.size(); ++i) {
        Feature& f = *features_[i];
        f.error.clear();
        if (i >= marker_ || f.suppressed) continue;
        try {
            f.execute(ctx);
            ++report.executed;
        } catch (const Error& e) {
            f.error = e.what();
            ++report.failed;
            report.messages.push_back(f.name + ": " + f.error);
        }
        // Failed features stay visible so dependents report a clear error.
        ctx.executed.push_back(&f);
    }
    bodies_ = std::move(ctx.bodies);
    return report;
}

void Document::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("cannot open " + path + " for writing");
    out << toJson();
    if (!out) throw Error("failed writing " + path);
}

Document Document::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error("cannot open " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return fromJson(ss.str());
}

UndoStack::UndoStack(const Document& initial, size_t limit) : limit_(std::max<size_t>(limit, 2)) {
    states_.push_back({initial.toJson(), "Initial state"});
}

void UndoStack::commit(const Document& doc, std::string label) {
    states_.resize(index_ + 1); // drop redo history
    states_.push_back({doc.toJson(), std::move(label)});
    if (states_.size() > limit_) states_.erase(states_.begin());
    index_ = states_.size() - 1;
}

Document UndoStack::undo() {
    if (!canUndo()) throw Error("nothing to undo");
    return Document::fromJson(states_[--index_].json);
}

Document UndoStack::redo() {
    if (!canRedo()) throw Error("nothing to redo");
    return Document::fromJson(states_[++index_].json);
}

} // namespace age
