// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/command/Command.h"
#include "fuse/model/Features.h"

#include <optional>
#include <string>
#include <vector>

namespace fuse::cmd {

template <>
struct FieldsOf<EdgeQuery> {
    static constexpr auto fields = std::tuple{
        defaulted("mode", &EdgeQuery::mode,
                  "all: every edge; parallel_to: straight edges parallel to `direction`; near_points: the edge "
                  "closest to each of `points`"),
        defaulted("direction", &EdgeQuery::direction, "parallel_to: direction [x, y, z]"),
        defaulted("points", &EdgeQuery::points, "near_points: one [x, y, z] point (mm) per edge"),
    };
};

inline constexpr const char* kTargetBodyHelp =
    "Body to join, cut or intersect with; -1 = the most recently created or modified body";

struct CreateExtrude {
    static constexpr CommandInfo info{
        "extrude_create", "Extrude",
        "Extrude closed sketch profiles along the sketch plane's normal into a new body, or join, cut or intersect "
        "them with an existing one."};
    int sketchId = 0;
    std::vector<int> profiles;
    std::string distance = "10 mm";
    bool symmetric = false;
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1;
    static constexpr auto fields = std::tuple{
        field("sketch", &CreateExtrude::sketchId, "The sketch feature", Format::FeatureId),
        defaulted("profiles", &CreateExtrude::profiles, "Profile indices (see sketch_describe); empty = all"),
        defaulted("distance", &CreateExtrude::distance, "Length; negative extrudes the other way", Format::Length),
        defaulted("symmetric", &CreateExtrude::symmetric, "Split the distance evenly about the sketch plane"),
        defaulted("operation", &CreateExtrude::operation, "What to do with the extruded solid"),
        defaulted("target_body", &CreateExtrude::targetBody, kTargetBodyHelp, Format::BodyId),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct EditExtrude {
    static constexpr CommandInfo info{"extrude_edit", "Edit Extrude",
                                      "Change an extrude. Only the arguments given are changed."};
    int feature = 0;
    std::optional<int> sketchId;
    std::optional<std::vector<int>> profiles;
    std::optional<std::string> distance;
    std::optional<bool> symmetric;
    std::optional<FeatureOperation> operation;
    std::optional<int> targetBody;
    static constexpr auto fields = std::tuple{
        field("feature", &EditExtrude::feature, "The extrude feature", Format::FeatureId),
        field("sketch", &EditExtrude::sketchId, "The sketch feature", Format::FeatureId),
        field("profiles", &EditExtrude::profiles, "Profile indices; empty = all"),
        field("distance", &EditExtrude::distance, "Length; negative extrudes the other way", Format::Length),
        field("symmetric", &EditExtrude::symmetric, "Split the distance evenly about the sketch plane"),
        field("operation", &EditExtrude::operation, "What to do with the extruded solid"),
        field("target_body", &EditExtrude::targetBody, "Body to join, cut or intersect with", Format::BodyId),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct CreateRevolve {
    static constexpr CommandInfo info{
        "revolve_create", "Revolve",
        "Revolve closed sketch profiles about a line in the same sketch into a new body, or join, cut or "
        "intersect them with an existing one."};
    int sketchId = 0;
    sketch::EntityId axis = -1;
    std::vector<int> profiles;
    std::string angle = "360 deg";
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1;
    static constexpr auto fields = std::tuple{
        field("sketch", &CreateRevolve::sketchId, "The sketch feature", Format::FeatureId),
        field("axis", &CreateRevolve::axis, "A line in the sketch to revolve about", Format::EntityId),
        defaulted("profiles", &CreateRevolve::profiles, "Profile indices (see sketch_describe); empty = all"),
        defaulted("angle", &CreateRevolve::angle, "Sweep angle", Format::Angle),
        defaulted("operation", &CreateRevolve::operation, "What to do with the revolved solid"),
        defaulted("target_body", &CreateRevolve::targetBody, kTargetBodyHelp, Format::BodyId),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct EditRevolve {
    static constexpr CommandInfo info{"revolve_edit", "Edit Revolve",
                                      "Change a revolve. Only the arguments given are changed."};
    int feature = 0;
    std::optional<int> sketchId;
    std::optional<sketch::EntityId> axis;
    std::optional<std::vector<int>> profiles;
    std::optional<std::string> angle;
    std::optional<FeatureOperation> operation;
    std::optional<int> targetBody;
    static constexpr auto fields = std::tuple{
        field("feature", &EditRevolve::feature, "The revolve feature", Format::FeatureId),
        field("sketch", &EditRevolve::sketchId, "The sketch feature", Format::FeatureId),
        field("axis", &EditRevolve::axis, "A line in the sketch to revolve about", Format::EntityId),
        field("profiles", &EditRevolve::profiles, "Profile indices; empty = all"),
        field("angle", &EditRevolve::angle, "Sweep angle", Format::Angle),
        field("operation", &EditRevolve::operation, "What to do with the revolved solid"),
        field("target_body", &EditRevolve::targetBody, "Body to join, cut or intersect with", Format::BodyId),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct CreateFillet {
    static constexpr CommandInfo info{"fillet_create", "Fillet", "Round edges of a body with a constant radius."};
    int body = -1;
    EdgeQuery edges;
    std::string radius;
    static constexpr auto fields = std::tuple{
        defaulted("body", &CreateFillet::body, "The body; -1 = the most recently created or modified body",
                  Format::BodyId),
        defaulted("edges", &CreateFillet::edges, "Which edges to round"),
        field("radius", &CreateFillet::radius, "Fillet radius", Format::Length),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct EditFillet {
    static constexpr CommandInfo info{"fillet_edit", "Edit Fillet",
                                      "Change a fillet. Only the arguments given are changed."};
    int feature = 0;
    std::optional<int> body;
    std::optional<EdgeQuery> edges;
    std::optional<std::string> radius;
    static constexpr auto fields = std::tuple{
        field("feature", &EditFillet::feature, "The fillet feature", Format::FeatureId),
        field("body", &EditFillet::body, "The body", Format::BodyId),
        field("edges", &EditFillet::edges, "Which edges to round"),
        field("radius", &EditFillet::radius, "Fillet radius", Format::Length),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct CreateChamfer {
    static constexpr CommandInfo info{"chamfer_create", "Chamfer",
                                      "Bevel edges of a body by an equal distance on both faces."};
    int body = -1;
    EdgeQuery edges;
    std::string distance;
    static constexpr auto fields = std::tuple{
        defaulted("body", &CreateChamfer::body, "The body; -1 = the most recently created or modified body",
                  Format::BodyId),
        defaulted("edges", &CreateChamfer::edges, "Which edges to bevel"),
        field("distance", &CreateChamfer::distance, "Setback along each face", Format::Length),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct EditChamfer {
    static constexpr CommandInfo info{"chamfer_edit", "Edit Chamfer",
                                      "Change a chamfer. Only the arguments given are changed."};
    int feature = 0;
    std::optional<int> body;
    std::optional<EdgeQuery> edges;
    std::optional<std::string> distance;
    static constexpr auto fields = std::tuple{
        field("feature", &EditChamfer::feature, "The chamfer feature", Format::FeatureId),
        field("body", &EditChamfer::body, "The body", Format::BodyId),
        field("edges", &EditChamfer::edges, "Which edges to bevel"),
        field("distance", &EditChamfer::distance, "Setback along each face", Format::Length),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct ImportStep {
    static constexpr CommandInfo info{"step_import", "Import STEP",
                                      "Add the solids of a STEP file as a new body. The file is read again on "
                                      "every recompute."};
    std::string path;
    static constexpr auto fields = std::tuple{
        field("path", &ImportStep::path, "The .step or .stp file", Format::FilePath),
    };
    using Output = FeatureRef;
    Output run(EditContext& ctx) const;
};

struct CreateBox {
    static constexpr CommandInfo info{
        "box_create", "Box",
        "Add a box: a dimensioned rectangle sketch centred at (x, y) on a plane at height z, extruded by height. "
        "All sizes are expressions, so the box follows parameters."};
    std::string width = "40 mm", depth = "30 mm", height = "20 mm";
    std::string x = "0 mm", y = "0 mm", z = "0 mm";
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1;
    static constexpr auto fields = std::tuple{
        defaulted("width", &CreateBox::width, "Size along X", Format::Length),
        defaulted("depth", &CreateBox::depth, "Size along Y", Format::Length),
        defaulted("height", &CreateBox::height, "Size along Z; negative builds downwards", Format::Length),
        defaulted("x", &CreateBox::x, "Centre X", Format::Length),
        defaulted("y", &CreateBox::y, "Centre Y", Format::Length),
        defaulted("z", &CreateBox::z, "Base height", Format::Length),
        defaulted("operation", &CreateBox::operation, "What to do with the box"),
        defaulted("target_body", &CreateBox::targetBody, kTargetBodyHelp, Format::BodyId),
    };
    using Output = FeatureRef; // the extrude
    Output run(EditContext& ctx) const;
};

struct CreateCylinder {
    static constexpr CommandInfo info{
        "cylinder_create", "Cylinder",
        "Add an upright cylinder: a dimensioned circle sketch centred at (x, y) on a plane at height z, extruded "
        "by height."};
    std::string diameter = "20 mm", height = "20 mm";
    std::string x = "0 mm", y = "0 mm", z = "0 mm";
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1;
    static constexpr auto fields = std::tuple{
        defaulted("diameter", &CreateCylinder::diameter, "Diameter", Format::Length),
        defaulted("height", &CreateCylinder::height, "Height; negative builds downwards", Format::Length),
        defaulted("x", &CreateCylinder::x, "Centre X", Format::Length),
        defaulted("y", &CreateCylinder::y, "Centre Y", Format::Length),
        defaulted("z", &CreateCylinder::z, "Base height", Format::Length),
        defaulted("operation", &CreateCylinder::operation, "What to do with the cylinder"),
        defaulted("target_body", &CreateCylinder::targetBody, kTargetBodyHelp, Format::BodyId),
    };
    using Output = FeatureRef; // the extrude
    Output run(EditContext& ctx) const;
};

struct RenameFeature {
    static constexpr CommandInfo info{"feature_rename", "Rename", "Rename a feature on the timeline."};
    int feature = 0;
    std::string name;
    static constexpr auto fields = std::tuple{
        field("feature", &RenameFeature::feature, "The feature", Format::FeatureId),
        field("name", &RenameFeature::name, "Its new name"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct SuppressFeature {
    static constexpr CommandInfo info{
        "feature_suppress", "Suppress",
        "Suppress a feature (skip it during recompute without deleting it), or restore it."};
    int feature = 0;
    bool suppressed = true;
    static constexpr auto fields = std::tuple{
        field("feature", &SuppressFeature::feature, "The feature", Format::FeatureId),
        defaulted("suppressed", &SuppressFeature::suppressed, "false restores the feature"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct DeleteFeature {
    static constexpr CommandInfo info{"feature_delete", "Delete",
                                      "Delete a feature. Refused while other features depend on it."};
    int feature = 0;
    static constexpr auto fields = std::tuple{
        field("feature", &DeleteFeature::feature, "The feature", Format::FeatureId),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct SetTimelineMarker {
    static constexpr CommandInfo info{
        "timeline_set_marker", "Roll Timeline",
        "Move the timeline marker: features from this position on are not computed, and new features are "
        "inserted here. 0 = before the first feature; the number of features = after the last."};
    int position = 0;
    static constexpr auto fields = std::tuple{
        field("position", &SetTimelineMarker::position, "Number of features before the marker"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

} // namespace fuse::cmd
