// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/Parameters.h"
#include "fuse/model/Feature.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fuse {

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

    // Edit parameter values through parameters(); removing or renaming one must
    // go through the document, which also tracks references from features.
    ParameterTable& parameters() { return params_; }
    const ParameterTable& parameters() const { return params_; }
    // Throws if a parameter or feature references `name`.
    void removeParameter(std::string_view name);
    // Renames and rewrites every reference in parameters, features and sketch
    // dimensions. Transactional: on error nothing changes.
    void renameParameter(std::string_view from, std::string to);

    const std::vector<std::unique_ptr<Feature>>& features() const { return features_; }
    Feature* findFeature(int id);
    const Feature* findFeature(int id) const;

    // Inserts a new feature at the timeline marker, assigns an id and a default
    // name like "Extrude2". Returns a reference to the stored feature.
    template <class T>
    T& add() {
        auto feature = std::make_unique<T>();
        T& ref = *feature;
        insert(std::move(feature));
        return ref;
    }
    void removeFeature(int id); // throws if another feature depends on it

    // Id of the body most recently created or modified before the marker
    // (suppressed features ignored), or -1 if there is none.
    int latestBody() const;

    // Rollback marker: features at index >= marker are not executed.
    size_t marker() const { return marker_; }
    void setMarker(size_t position);

    RecomputeReport recompute(Kernel& kernel);
    const std::map<int, Body>& bodies() const { return bodies_; }

    // Native .fcad format (JSON).
    std::string toJson() const;
    static Document fromJson(const std::string& json);
    void save(const std::string& path) const;
    static Document load(const std::string& path);

    static constexpr int kFormatVersion = 2;

private:
    void insert(std::unique_ptr<Feature> f);

    ParameterTable params_;
    std::vector<std::unique_ptr<Feature>> features_;
    size_t marker_ = 0;
    int nextId_ = 1;
    std::map<FeatureType, int> nameCounters_;
    std::map<int, Body> bodies_;
};

// What an edit changed between two states of a document. Feature changes
// compare definitions only (what is saved, minus provenance); a parameter is
// listed when it was added, removed or its expression, unit or comment changed.
struct Changes {
    std::vector<int> created, modified, removed; // feature ids
    std::vector<std::string> parameters;         // parameter names
    bool marker = false;                         // the timeline marker moved

    bool empty() const {
        return created.empty() && modified.empty() && removed.empty() && parameters.empty() && !marker;
    }
};

Changes diff(const Document& before, const Document& after);

// A feature as stored in the .fcad file, as compact JSON text.
std::string featureJson(const Feature& f);

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

} // namespace fuse
