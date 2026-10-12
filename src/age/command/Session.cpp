// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/command/Session.h"

#include "age/core/Error.h"

#include <exception>
#include <utility>

namespace age::cmd {

namespace {

std::vector<Issue> issuesOf(const Document& doc) {
    std::vector<Issue> out;
    for (const auto& f : doc.features())
        if (!f->healthy()) out.push_back({f->id, f->name, f->error});
    return out;
}

json changesToJson(const Changes& c) {
    return {{"created", c.created},
            {"modified", c.modified},
            {"removed", c.removed},
            {"parameters", c.parameters},
            {"marker", c.marker}};
}

} // namespace

json Result::toJson() const {
    return {
        {"output", output}, {"changes", changesToJson(changes)}, {"issues", Codec<std::vector<Issue>>::write(issues)}};
}

Session::Session(Kernel& kernel, Document doc, const Registry& registry)
    : kernel_(kernel), registry_(registry), undo_(doc) {
    reset(std::move(doc), {});
}

void Session::reset(Document doc, std::string path) {
    doc_ = std::move(doc);
    doc_.recompute(kernel_);
    undo_ = UndoStack(doc_);
    path_ = std::move(path);
    savedJson_ = doc_.toJson();
}

Session::Applied Session::apply(const CommandEntry& e, const json& args, const Actor& actor) {
    Applied a{doc_, {}};
    EditContext ctx{a.doc, kernel_, actor};
    a.result.output = e.edit(ctx, args);
    // Diff before recomputing: a recompute re-solves sketches, which rewrites
    // their stored solver state without the command having changed them.
    a.result.changes = diff(doc_, a.doc);
    a.doc.recompute(kernel_);
    for (int id : a.result.changes.created) {
        Feature* f = a.doc.findFeature(id);
        f->createdBy = f->modifiedBy = actor;
    }
    for (int id : a.result.changes.modified) a.doc.findFeature(id)->modifiedBy = actor;
    a.result.issues = issuesOf(a.doc);
    return a;
}

void Session::checkAccepted(const Document& doc, const Changes& changes) const {
    for (int id : changes.created) {
        const Feature* f = doc.findFeature(id);
        if (!f->healthy()) throw Error(f->name + ": " + f->error);
    }
    for (int id : changes.modified) {
        const Feature* f = doc.findFeature(id);
        const Feature* before = doc_.findFeature(id);
        if (!f->healthy() && before->healthy()) throw Error(f->name + ": " + f->error);
    }
}

void Session::record(const CommandEntry& e, const json& args, const Actor& actor, std::string error) {
    journal_.push_back({std::chrono::system_clock::now(), actor, e.info.id, args, std::move(error)});
}

void Session::requireNoTransaction(std::string_view action) const {
    if (txn_)
        throw Error("cannot " + std::string(action) + " while '" + txn_->entry->info.title +
                    "' is in progress; commit or cancel it first");
}

Result Session::execute(std::string_view command, const json& args, const Actor& actor) {
    const CommandEntry& e = registry_.get(command);
    if (e.kind == CommandKind::Query) {
        QueryContext ctx{document(), kernel_};
        return {e.query(ctx, args), {}, {}};
    }
    try {
        Result result;
        if (e.kind == CommandKind::Session) {
            result.output = e.session(*this, args);
        } else {
            requireNoTransaction(e.info.title);
            Applied a = apply(e, args, actor);
            checkAccepted(a.doc, a.result.changes);
            if (!a.result.changes.empty()) {
                doc_ = std::move(a.doc);
                undo_.commit(doc_, e.info.title);
            }
            result = std::move(a.result);
        }
        record(e, args, actor, {});
        return result;
    } catch (const std::exception& err) {
        record(e, args, actor, err.what());
        throw;
    }
}

json Session::call(std::string_view command, const json& args, const Actor& actor) {
    try {
        json out = execute(command, args, actor).toJson();
        out["ok"] = true;
        return out;
    } catch (const Error& e) {
        return {{"ok", false}, {"error", e.what()}};
    } catch (const std::exception& e) {
        return {{"ok", false}, {"error", std::string("internal error: ") + e.what()}};
    } catch (...) {
        return {{"ok", false}, {"error", "internal error"}};
    }
}

Result Session::begin(std::string_view command, const json& args, const Actor& actor) {
    const CommandEntry& e = registry_.get(command);
    if (e.kind != CommandKind::Edit) throw Error("'" + std::string(command) + "' does not edit the document");
    requireNoTransaction(e.info.title);
    Applied a = apply(e, args, actor);
    txn_ = Transaction{&e, actor, args, std::move(a.doc), std::move(a.result)};
    return txn_->result;
}

Result Session::update(const json& args) {
    if (!txn_) throw Error("no command is in progress");
    Transaction& t = *txn_;
    Applied a = apply(*t.entry, args, t.actor); // on error the previous preview stays
    t.args = args;
    t.doc = std::move(a.doc);
    t.result = std::move(a.result);
    return t.result;
}

Result Session::commit() {
    if (!txn_) throw Error("no command is in progress");
    checkAccepted(txn_->doc, txn_->result.changes);
    Transaction t = std::move(*txn_);
    txn_.reset();
    if (!t.result.changes.empty()) {
        doc_ = std::move(t.doc);
        undo_.commit(doc_, t.entry->info.title);
    }
    record(*t.entry, t.args, t.actor, {});
    return std::move(t.result);
}

void Session::cancel() { txn_.reset(); }

std::string Session::undo() {
    requireNoTransaction("undo");
    std::string label = undo_.canUndo() ? undo_.undoLabel() : std::string();
    doc_ = undo_.undo();
    doc_.recompute(kernel_);
    return label;
}

std::string Session::redo() {
    requireNoTransaction("redo");
    std::string label = undo_.canRedo() ? undo_.redoLabel() : std::string();
    doc_ = undo_.redo();
    doc_.recompute(kernel_);
    return label;
}

void Session::newDocument() {
    requireNoTransaction("start a new document");
    reset({}, {});
}

void Session::open(const std::string& path) {
    requireNoTransaction("open a document");
    reset(Document::load(path), path);
}

std::string Session::save(const std::string& path) {
    const std::string target = path.empty() ? path_ : path;
    if (target.empty()) throw Error("the document has no file yet; give a path to save to");
    doc_.save(target);
    path_ = target;
    savedJson_ = doc_.toJson();
    return target;
}

} // namespace age::cmd
