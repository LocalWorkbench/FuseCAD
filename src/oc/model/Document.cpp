// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/model/Document.h"

#include "oc/core/Error.h"
#include "oc/model/Features.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace oc {

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
    int n = ++nameCounters_[f->type()];
    if (f->name.empty()) {
        std::string base = featureTypeName(f->type());
        base[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[0])));
        if (f->type() == FeatureType::ImportStep) base = "Import";
        f->name = base + std::to_string(n);
    }
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

} // namespace oc
