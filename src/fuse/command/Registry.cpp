// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/command/Command.h"
#include "fuse/core/EnumTable.h"
#include "fuse/core/Error.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace fuse::cmd {

namespace {

constexpr auto kFormats = makeEnumTable<EnumName<Format>>({
    {Format::None, ""},
    {Format::Expression, "expression"},
    {Format::Length, "length-expression"},
    {Format::Angle, "angle-expression"},
    {Format::FeatureId, "feature-id"},
    {Format::BodyId, "body-id"},
    {Format::EntityId, "sketch-entity-id"},
    {Format::ConstraintId, "sketch-constraint-id"},
    {Format::FilePath, "file-path"},
});

constexpr auto kCommandKinds = makeEnumTable<EnumName<CommandKind>>({
    {CommandKind::Edit, "edit"},
    {CommandKind::Query, "query"},
    {CommandKind::Session, "session"},
});

} // namespace

const char* formatName(Format f) { return kFormats.name(f); }

const char* commandKindName(CommandKind kind) { return kCommandKinds.name(kind); }

void Registry::insert(CommandEntry e) {
    const std::string_view id = e.info.id;
    auto it = std::lower_bound(entries_.begin(), entries_.end(), id,
                               [](const CommandEntry& a, std::string_view b) { return a.info.id < b; });
    if (it != entries_.end() && it->info.id == id) throw std::logic_error("duplicate command id " + std::string(id));
    entries_.insert(it, std::move(e));
}

const CommandEntry* Registry::find(std::string_view id) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), id,
                               [](const CommandEntry& a, std::string_view b) { return a.info.id < b; });
    return it != entries_.end() && it->info.id == id ? &*it : nullptr;
}

const CommandEntry& Registry::get(std::string_view id) const {
    if (const CommandEntry* e = find(id)) return *e;
    throw Error("unknown command '" + std::string(id) + "'");
}

json Registry::describe(const CommandEntry& e) {
    return {{"name", e.info.id},
            {"title", e.info.title},
            {"description", e.info.description},
            {"kind", commandKindName(e.kind)},
            {"input_schema", e.inputSchema},
            {"output_schema", e.outputSchema}};
}

json Registry::describe() const {
    json out = json::array();
    for (const auto& e : entries_) out.push_back(describe(e));
    return out;
}

} // namespace fuse::cmd
