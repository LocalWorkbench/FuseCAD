// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/core/Parameters.h"
#include "oc/model/Feature.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oc {

struct RecomputeReport {
    int executed = 0;
    int failed = 0;
    std::vector<std::string> messages; // "<feature>: <error>"
    bool ok() const { return failed == 0; }
};

// A parametric design: user parameters plus an ordered feature timeline.
// Geometry (bodies) is derived state, rebuilt by recompute().
class Document {
public:
    Document() = default;
    Document(const Document& other);
    Document& operator=(const Document& other);
    Document(Document&&) noexcept = default;
    Document& operator=(Document&&) noexcept = default;

    ParameterTable& parameters() { return params_; }
    const ParameterTable& parameters() const { return params_; }

    const std::vector<std::unique_ptr<Feature>>& features() const { return features_; }
    Feature* findFeature(int id);
    const Feature* findFeature(int id) const;
    template <class T>
    T* get(int id) {
        return featureCast<T>(findFeature(id));
    }

    // Inserts at the timeline marker, assigns an id and (if empty) a default
    // name like "Extrude2". Returns a reference to the stored feature.
    template <class T>
    T& add(std::unique_ptr<T> feature) {
        T* raw = feature.get();
        insert(std::move(feature));
        return *raw;
    }
    template <class T>
    T& add() {
        return add(std::make_unique<T>());
    }
    void removeFeature(int id); // throws if another feature depends on it

    // Rollback marker: features at index >= marker are not executed.
    size_t marker() const { return marker_; }
    void setMarker(size_t position);

    RecomputeReport recompute(Kernel& kernel);
    const std::map<int, Body>& bodies() const { return bodies_; }

    // Native .ocd format (JSON).
    std::string toJson() const;
    static Document fromJson(const std::string& json);
    void save(const std::string& path) const;
    static Document load(const std::string& path);

    static constexpr int kFormatVersion = 1;

private:
    void insert(std::unique_ptr<Feature> f);

    ParameterTable params_;
    std::vector<std::unique_ptr<Feature>> features_;
    size_t marker_ = 0;
    int nextId_ = 1;
    std::map<FeatureType, int> nameCounters_;
    std::map<int, Body> bodies_;
};

// Snapshot-based undo. The feature tree is small (geometry is derived), so
// storing serialised snapshots is simple and robust.
class UndoStack {
public:
    explicit UndoStack(const Document& initial, size_t limit = 200);

    void commit(const Document& doc, std::string label);
    bool canUndo() const { return index_ > 0; }
    bool canRedo() const { return index_ + 1 < states_.size(); }
    const std::string& undoLabel() const { return states_[index_].label; }
    const std::string& redoLabel() const { return states_[index_ + 1].label; }
    Document undo();
    Document redo();

private:
    struct State {
        std::string json;
        std::string label;
    };
    std::vector<State> states_;
    size_t index_ = 0;
    size_t limit_;
};

} // namespace oc
