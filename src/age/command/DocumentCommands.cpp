// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/command/DocumentCommands.h"

#include "age/command/Session.h"
#include "age/core/Error.h"
#include "age/model/Features.h"

#include <cctype>
#include <filesystem>

namespace age::cmd {

// The session commands' way in to Session's private file and history operations.
struct SessionAccess {
    static void newDocument(Session& s) { s.newDocument(); }
    static void open(Session& s, const std::string& path) { s.open(path); }
    static std::string save(Session& s, const std::string& path) { return s.save(path); }
    static std::string undo(Session& s) { return s.undo(); }
    static std::string redo(Session& s) { return s.redo(); }
};

DescribeDocument::Output DescribeDocument::run(QueryContext& ctx) const {
    const Document& doc = ctx.doc;
    Output out;
    for (const auto& p : doc.parameters().all())
        out.parameters.push_back(
            {p.name, p.expression.source(), p.unit, p.value, formatValue(p.value, p.unit), p.comment});

    for (size_t i = 0; i < doc.features().size(); ++i) {
        const age::Feature& f = *doc.features()[i];
        Feature item{f.id,        f.name,       featureTypeName(f.type()),  f.suppressed, i >= doc.marker(), {},
                     f.createdBy, f.modifiedBy, json::parse(featureJson(f))};
        if (!f.healthy()) item.error = f.error;
        item.definition.erase("created_by");
        item.definition.erase("modified_by");
        out.features.push_back(std::move(item));
    }
    out.marker = static_cast<int>(doc.marker());

    for (const auto& [id, body] : doc.bodies())
        out.bodies.push_back({id, body.name, ctx.kernel.volume(body.shape), ctx.kernel.surfaceArea(body.shape),
                              ctx.kernel.countFaces(body.shape), ctx.kernel.isValid(body.shape),
                              ctx.kernel.boundingBox(body.shape)});
    return out;
}

NoOutput ExportDocument::run(QueryContext& ctx) const {
    for (const auto& f : ctx.doc.features())
        if (!f->healthy()) throw Error("'" + f->name + "' has errors; fix them before exporting");
    std::vector<ShapePtr> shapes;
    for (const auto& [id, body] : ctx.doc.bodies()) shapes.push_back(body.shape);
    if (shapes.empty()) throw Error("the document has no bodies to export");

    std::string ext = std::filesystem::path(path).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".step" || ext == ".stp") {
        ctx.kernel.exportStep(shapes, path);
    } else if (ext == ".stl") {
        if (deflection <= 0) throw Error("deflection must be positive");
        ctx.kernel.exportStl(shapes, path, deflection);
    } else {
        throw Error("cannot export to '" + ext + "' files; use .step, .stp or .stl");
    }
    return {};
}

NoOutput NewDocument::run(Session& session) const {
    SessionAccess::newDocument(session);
    return {};
}

NoOutput OpenDocument::run(Session& session) const {
    SessionAccess::open(session, path);
    return {};
}

SaveDocument::Output SaveDocument::run(Session& session) const {
    return {SessionAccess::save(session, path.value_or(""))};
}

HistoryStep Undo::run(Session& session) const { return {SessionAccess::undo(session)}; }

HistoryStep Redo::run(Session& session) const { return {SessionAccess::redo(session)}; }

} // namespace age::cmd
