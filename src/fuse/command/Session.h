// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/command/Command.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fuse::cmd {

// A feature that is in error after a command ran.
struct Issue {
    int feature = 0;
    std::string name;
    std::string error;
    static constexpr auto fields = std::tuple{
        field("feature", &Issue::feature, "Id of the failing feature", Format::FeatureId),
        field("name", &Issue::name, "Its name on the timeline"),
        field("error", &Issue::error, "Why it failed"),
    };
};

struct Result {
    json output;               // the command's Output, as JSON
    Changes changes;           // edits only: what changed in the document
    std::vector<Issue> issues; // edits only: every feature in error afterwards

    // {"output": ..., "changes": {...}, "issues": [...]}
    json toJson() const;
};

// One command that ran (or was refused) through Session::execute or commit().
struct JournalEntry {
    std::chrono::system_clock::time_point time;
    Actor actor;
    std::string command;
    json args;
    std::string error; // empty when the command succeeded
};

// The one place a document is changed. Owns the document, its undo history
// and the journal of who did what; the interface, scripts and agents all call
// execute() with a command id and JSON arguments, so they share one code path.
//
// Edits run on a copy of the document, which is then recomputed. The edit is
// refused, leaving everything unchanged, if it throws, creates a feature that
// fails, or breaks a feature that was working. Edits that change nothing add
// no undo step. Features record which actor created and last modified them.
//
// A transaction previews one edit: begin() applies it, update() re-applies it
// with new arguments (as when dragging a handle), commit() keeps the result as
// a single undo step and cancel() drops it. While one is open, document()
// returns the preview and other edits are refused.
//
// Not thread-safe: use a session from one thread.
class Session {
public:
    explicit Session(Kernel& kernel, Document doc = {}, const Registry& registry = Registry::builtin());

    const Document& document() const { return txn_ ? txn_->doc : doc_; }
    const Registry& registry() const { return registry_; }
    Kernel& kernel() { return kernel_; }

    // Runs a command. Throws fuse::Error (ArgumentError for bad arguments).
    Result execute(std::string_view command, const json& args = json::object(), const Actor& actor = {});

    // For transports: reports failure in the result instead of throwing.
    // {"ok": true, ...Result::toJson()} or {"ok": false, "error": "..."}.
    json call(std::string_view command, const json& args, const Actor& actor);

    // Typed form for C++ callers; goes through execute() like everyone else.
    template <Command C>
    C::Output run(const C& command, const Actor& actor = {}) {
        const Result r = execute(C::info.id, Codec<C>::write(command), actor);
        return Codec<typename C::Output>::read(r.output, "");
    }

    Result begin(std::string_view command, const json& args, const Actor& actor = {});
    Result update(const json& args);
    Result commit(); // throws, keeping the transaction open, if the preview is refused
    void cancel();
    bool inTransaction() const { return txn_.has_value(); }

    bool canUndo() const { return undo_.canUndo(); }
    bool canRedo() const { return undo_.canRedo(); }
    const std::string& undoLabel() const { return undo_.undoLabel(); }
    const std::string& redoLabel() const { return undo_.redoLabel(); }

    const std::string& path() const { return path_; } // empty until opened or saved
    bool modified() const { return doc_.toJson() != savedJson_; }

    const std::vector<JournalEntry>& journal() const { return journal_; }

private:
    friend struct SessionAccess; // the session commands (document_open, history_undo, ...)

    struct Applied {
        Document doc;
        Result result;
    };
    struct Transaction {
        const CommandEntry* entry;
        Actor actor;
        json args;
        Document doc;
        Result result;
    };

    Applied apply(const CommandEntry& e, const json& args, const Actor& actor);
    void checkAccepted(const Document& doc, const Changes& changes) const;
    void record(const CommandEntry& e, const json& args, const Actor& actor, std::string error);
    void requireNoTransaction(std::string_view action) const;
    void reset(Document doc, std::string path);

    std::string undo();
    std::string redo();
    void newDocument();
    void open(const std::string& path);
    std::string save(const std::string& path); // empty: the current path

    Kernel& kernel_;
    const Registry& registry_;
    Document doc_;
    UndoStack undo_;
    std::optional<Transaction> txn_;
    std::vector<JournalEntry> journal_;
    std::string path_;
    std::string savedJson_;
};

} // namespace fuse::cmd
