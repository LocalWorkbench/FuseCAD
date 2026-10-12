// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "age/command/Command.h"

#include <optional>
#include <string>
#include <vector>

namespace age::cmd {

template <>
struct FieldsOf<Actor> {
    static constexpr auto fields = std::tuple{
        field("kind", &Actor::kind, "person, script or agent"),
        defaulted("name", &Actor::name, "Which script or agent; empty for the person"),
    };
};

template <>
struct FieldsOf<BoundingBox> {
    static constexpr auto fields = std::tuple{
        field("min", &BoundingBox::min, "Minimum corner [x, y, z] in mm"),
        field("max", &BoundingBox::max, "Maximum corner [x, y, z] in mm"),
    };
};

struct DescribeDocument {
    static constexpr CommandInfo info{
        "document_describe", "Inspect Document",
        "Report the parameters, the feature timeline (with each feature's definition, errors and who made it) and "
        "the resulting bodies with their volume, area and bounding box."};
    static constexpr std::tuple<> fields{};

    struct Parameter {
        std::string name, expression;
        UnitKind unit = UnitKind::Length;
        double value = 0;
        std::string display, comment;
        static constexpr auto fields = std::tuple{
            field("name", &Parameter::name, "Name"),
            field("expression", &Parameter::expression, "Expression as entered", Format::Expression),
            field("unit", &Parameter::unit, "What it measures"),
            field("value", &Parameter::value, "Current value in mm, radians or unitless"),
            field("display", &Parameter::display, "Current value formatted for people"),
            field("comment", &Parameter::comment, "Note"),
        };
    };
    struct Feature {
        int id = 0;
        std::string name, type;
        bool suppressed = false, rolledBack = false;
        std::optional<std::string> error;
        Actor createdBy, modifiedBy;
        json definition;
        static constexpr auto fields = std::tuple{
            field("id", &Feature::id, "Feature id", Format::FeatureId),
            field("name", &Feature::name, "Name on the timeline"),
            field("type", &Feature::type, "sketch, extrude, revolve, fillet, chamfer or import_step"),
            field("suppressed", &Feature::suppressed, "Skipped during recompute"),
            field("rolled_back", &Feature::rolledBack, "After the timeline marker, so not computed"),
            field("error", &Feature::error, "Why the feature failed, if it did"),
            field("created_by", &Feature::createdBy, "Who added it"),
            field("modified_by", &Feature::modifiedBy, "Who last changed it"),
            field("definition", &Feature::definition, "Its full definition, as saved in the .agecad file"),
        };
    };
    struct Body {
        int id = 0;
        std::string name;
        double volume = 0, area = 0;
        int faces = 0;
        bool valid = true;
        BoundingBox boundingBox;
        static constexpr auto fields = std::tuple{
            field("id", &Body::id, "Body id (the id of the feature that created it)", Format::BodyId),
            field("name", &Body::name, "Name"),
            field("volume", &Body::volume, "Volume in mm^3"),
            field("area", &Body::area, "Surface area in mm^2"),
            field("faces", &Body::faces, "Number of faces"),
            field("valid", &Body::valid, "Passes the kernel's validity check"),
            field("bounding_box", &Body::boundingBox, "Axis-aligned bounds"),
        };
    };
    struct Output {
        std::vector<Parameter> parameters;
        std::vector<Feature> features;
        int marker = 0;
        std::vector<Body> bodies;
        static constexpr auto fields = std::tuple{
            field("parameters", &Output::parameters, "User parameters"),
            field("features", &Output::features, "The timeline, in order"),
            field("marker", &Output::marker, "Number of features before the timeline marker"),
            field("bodies", &Output::bodies, "Solids after recompute"),
        };
    };
    Output run(QueryContext& ctx) const;
};

struct ExportDocument {
    static constexpr CommandInfo info{
        "document_export", "Export",
        "Write every body to a STEP (.step, .stp) or STL (.stl) file, chosen by the extension. Refused while any "
        "feature has errors."};
    std::string path;
    double deflection = kDefaultStlDeflection;
    static constexpr auto fields = std::tuple{
        field("path", &ExportDocument::path, "File to write", Format::FilePath),
        defaulted("deflection", &ExportDocument::deflection, "STL only: maximum chordal deviation in mm"),
    };
    using Output = NoOutput;
    Output run(QueryContext& ctx) const;
};

struct NewDocument {
    static constexpr CommandInfo info{"document_new", "New", "Replace the open document with an empty one."};
    static constexpr std::tuple<> fields{};
    using Output = NoOutput;
    Output run(Session& session) const;
};

struct OpenDocument {
    static constexpr CommandInfo info{"document_open", "Open", "Open a .agecad document, replacing the current one."};
    std::string path;
    static constexpr auto fields = std::tuple{
        field("path", &OpenDocument::path, "The .agecad file", Format::FilePath),
    };
    using Output = NoOutput;
    Output run(Session& session) const;
};

struct SaveDocument {
    static constexpr CommandInfo info{"document_save", "Save",
                                      "Save the document, to its current file or to a new path."};
    std::optional<std::string> path;
    static constexpr auto fields = std::tuple{
        field("path", &SaveDocument::path, "Where to save; omit to save to the current file", Format::FilePath),
    };
    struct Output {
        std::string path;
        static constexpr auto fields = std::tuple{
            field("path", &Output::path, "The file written", Format::FilePath),
        };
    };
    Output run(Session& session) const;
};

// Output of undo and redo.
struct HistoryStep {
    std::string label;
    static constexpr auto fields = std::tuple{
        field("label", &HistoryStep::label, "The command that was undone or redone"),
    };
};

struct Undo {
    static constexpr CommandInfo info{"history_undo", "Undo", "Undo the last edit."};
    static constexpr std::tuple<> fields{};
    using Output = HistoryStep;
    Output run(Session& session) const;
};

struct Redo {
    static constexpr CommandInfo info{"history_redo", "Redo", "Redo the last undone edit."};
    static constexpr std::tuple<> fields{};
    using Output = HistoryStep;
    Output run(Session& session) const;
};

} // namespace age::cmd
