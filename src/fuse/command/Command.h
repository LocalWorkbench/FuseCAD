// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A command is one user action: the unit the interface, scripts and agents
// all drive FuseCAD with. Each is a struct holding its arguments, described
// by a `fields` list (Codec.h), with static `info`, an `Output` type and a
// `run` member whose parameter decides the command's kind:
//
//   Output run(EditContext&) const     edit: changes the document, one undo step
//   Output run(QueryContext&) const    query: reads the document
//   Output run(Session&) const         session: files and history
//
// Commands are reached by id through a Registry, which also produces the JSON
// Schema of every command's arguments and output.

#include "fuse/command/Codec.h"
#include "fuse/model/Document.h"

#include <concepts>
#include <functional>
#include <string_view>
#include <tuple>
#include <vector>

namespace fuse::cmd {

class Session;

struct CommandInfo {
    const char* id;          // stable name, [a-z][a-z0-9_]*: "sketch_add_line"
    const char* title;       // label in the interface and on the undo stack: "Add Line"
    const char* description; // what it does, written for people and agents alike
};

enum class CommandKind { Edit, Query, Session };

const char* commandKindName(CommandKind kind); // "edit", "query", "session"

struct EditContext {
    Document& doc; // a working copy; discarded if the command throws
    Kernel& kernel;
    const Actor& actor;
};

struct QueryContext {
    const Document& doc;
    Kernel& kernel;
};

template <class C>
concept EditCommand = requires(const C& c, EditContext& ctx) { c.run(ctx); };
template <class C>
concept QueryCommand = requires(const C& c, QueryContext& ctx) { c.run(ctx); };
template <class C>
concept SessionCommand = requires(const C& c, Session& s) { c.run(s); };

template <class C>
concept Command =
    requires {
        { C::info } -> std::convertible_to<CommandInfo>;
        typename C::Output;
    } && Described<C> && Described<typename C::Output> &&
    (static_cast<int>(EditCommand<C>) + static_cast<int>(QueryCommand<C>) + static_cast<int>(SessionCommand<C>)) == 1;

consteval bool isValidCommandId(const char* id) {
    auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
    auto digit = [](char c) { return c >= '0' && c <= '9'; };
    if (id == nullptr || !lower(*id)) return false;
    for (const char* p = id; *p != '\0'; ++p)
        if (!lower(*p) && !digit(*p) && *p != '_') return false;
    return true;
}

// Output of commands that return nothing.
struct NoOutput {
    static constexpr std::tuple<> fields{};
};

// Output of commands that add a feature to the timeline.
struct FeatureRef {
    int feature = 0;
    static constexpr auto fields = std::tuple{
        field("feature", &FeatureRef::feature, "Id of the new feature", Format::FeatureId),
    };
};

struct CommandEntry {
    CommandInfo info;
    CommandKind kind;
    json inputSchema;
    json outputSchema;
    // Exactly one is set, matching `kind`. Each parses the arguments (throwing
    // ArgumentError), runs the command and returns its output as JSON.
    std::function<json(EditContext&, const json&)> edit;
    std::function<json(QueryContext&, const json&)> query;
    std::function<json(Session&, const json&)> session;
};

class Registry {
public:
    template <Command C>
    void add() {
        static_assert(isValidCommandId(C::info.id), "command ids are lower snake_case: [a-z][a-z0-9_]*");
        using Out = C::Output;
        CommandEntry e{C::info, CommandKind::Edit, Codec<C>::schema(), Codec<Out>::schema(), {}, {}, {}};
        if constexpr (EditCommand<C>) {
            e.edit = [](EditContext& ctx, const json& args) {
                return Codec<Out>::write(Codec<C>::read(args, "").run(ctx));
            };
        } else if constexpr (QueryCommand<C>) {
            e.kind = CommandKind::Query;
            e.query = [](QueryContext& ctx, const json& args) {
                return Codec<Out>::write(Codec<C>::read(args, "").run(ctx));
            };
        } else {
            e.kind = CommandKind::Session;
            e.session = [](Session& s, const json& args) { return Codec<Out>::write(Codec<C>::read(args, "").run(s)); };
        }
        insert(std::move(e));
    }

    const CommandEntry* find(std::string_view id) const;
    const CommandEntry& get(std::string_view id) const;               // throws Error for an unknown id
    const std::vector<CommandEntry>& all() const { return entries_; } // sorted by id

    // Every command as {name, title, description, kind, input_schema, output_schema}.
    json describe() const;
    static json describe(const CommandEntry& e);

    // All commands FuseCAD ships with.
    static const Registry& builtin();

private:
    void insert(CommandEntry e); // keeps entries sorted; throws std::logic_error on a duplicate id

    std::vector<CommandEntry> entries_;
};

} // namespace fuse::cmd
