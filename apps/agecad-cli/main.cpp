// SPDX-License-Identifier: GPL-3.0-or-later
//
// agecad-cli: the AgeCAD command layer from a terminal or a script. Every
// change goes through a command session, exactly as in the app.
//
// Exit codes are listed in kUsage.

#include "age/command/Session.h"
#include "age/core/Error.h"
#include "age/model/Document.h"
#include "age/model/Samples.h"
#include "age/occt/OcctKernel.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using age::cmd::json;

constexpr const char* kUsage = R"(usage:
  agecad-cli new <file.agecad> [--sample NAME]       create an empty or sample document (plate, flange, pillow-block)
  agecad-cli info <file.agecad> [-p name=expr]...    recompute and print parameters, features, bodies
  agecad-cli set <file.agecad> name=expr...          change parameters and save
  agecad-cli export <file.agecad> <out> [-p ...]     export bodies to STEP (.step/.stp) or STL (.stl)
  agecad-cli run <file.agecad> <command> [json] [--agent NAME]
                                                     run one command; edits are saved; prints the JSON result
  agecad-cli commands [command]                      list commands, or print one command's JSON Schema

exit codes:
  0  success
  1  usage error
  2  modelling error: unreadable document, bad arguments, refused or failing command
  3  internal error
)";

enum ExitCode : int { kExitOk = 0, kExitUsage = 1, kExitModel = 2, kExitInternal = 3 };

struct UsageError : age::Error {
    using age::Error::Error;
};

age::Actor cliActor() { return {age::ActorKind::Script, "agecad-cli"}; }

// Owns the kernel a session refers to.
struct Cli {
    std::unique_ptr<age::Kernel> kernel = age::occt::createKernel();
    age::cmd::Session session{*kernel};

    explicit Cli(const std::string& path) { session.execute("document_open", {{"path", path}}, cliActor()); }
    explicit Cli(age::Document doc) : session(*kernel, std::move(doc)) {}
};

void setParameter(age::cmd::Session& s, std::string_view assignment) {
    auto eq = assignment.find('=');
    if (eq == std::string_view::npos || eq == 0)
        throw UsageError("expected name=expression, got '" + std::string(assignment) + "'");
    s.execute("parameter_set",
              {{"name", std::string(assignment.substr(0, eq))}, {"expression", std::string(assignment.substr(eq + 1))}},
              cliActor());
}

// Applies "-p name=expr" pairs from args[from..].
void applyOverrides(age::cmd::Session& s, const std::vector<std::string>& args, size_t from) {
    for (size_t i = from; i < args.size(); ++i) {
        if (args[i] != "-p" || i + 1 >= args.size()) throw UsageError("unexpected argument '" + args[i] + "'");
        setParameter(s, args[++i]);
    }
}

int cmdNew(const std::vector<std::string>& args) {
    const bool plain = args.size() == 1;
    const bool withSample = args.size() == 3 && args[1] == "--sample";
    if (!plain && !withSample) throw UsageError("new takes <file.agecad> [--sample plate|flange|pillow-block]");
    age::Document doc;
    if (withSample) {
        if (args[2] == "plate")
            doc = age::samples::mountingPlate();
        else if (args[2] == "flange")
            doc = age::samples::flange();
        else if (args[2] == "pillow-block")
            doc = age::samples::pillowBlock();
        else
            throw UsageError("unknown sample '" + args[2] + "' (available: plate, flange, pillow-block)");
    }
    Cli cli(std::move(doc));
    cli.session.execute("document_save", {{"path", args[0]}}, cliActor());
    std::cout << "wrote " << args[0] << "\n";
    return kExitOk;
}

int cmdInfo(const std::vector<std::string>& args) {
    if (args.empty()) throw UsageError("info takes <file.agecad>");
    Cli cli(args[0]);
    applyOverrides(cli.session, args, 1);
    const json doc = cli.session.execute("document_describe").output;

    std::cout << "Kernel: " << cli.kernel->name() << "\n\nParameters\n";
    for (const auto& p : doc["parameters"])
        std::printf("  %-14s = %-20s -> %s\n", p["name"].get<std::string>().c_str(),
                    p["expression"].get<std::string>().c_str(), p["display"].get<std::string>().c_str());

    std::cout << "\nTimeline\n";
    bool failed = false;
    int n = 0;
    for (const auto& f : doc["features"]) {
        const std::string error = f.value("error", "");
        failed = failed || !error.empty();
        const char* state = f["rolled_back"].get<bool>()  ? "rolled back"
                            : f["suppressed"].get<bool>() ? "suppressed"
                            : error.empty()               ? "ok"
                                                          : "ERROR";
        std::printf("  %2d. %-12s %-12s %s%s%s\n", ++n, f["name"].get<std::string>().c_str(),
                    f["type"].get<std::string>().c_str(), state, error.empty() ? "" : ": ", error.c_str());
    }

    std::cout << "\nBodies\n";
    for (const auto& b : doc["bodies"]) {
        const auto& lo = b["bounding_box"]["min"];
        const auto& hi = b["bounding_box"]["max"];
        auto size = [&](int i) { return hi[i].get<double>() - lo[i].get<double>(); };
        std::printf("  %-8s volume %.4f mm^3, area %.4f mm^2, %d faces, size %.3f x %.3f x %.3f mm%s\n",
                    b["name"].get<std::string>().c_str(), b["volume"].get<double>(), b["area"].get<double>(),
                    b["faces"].get<int>(), size(0), size(1), size(2), b["valid"].get<bool>() ? "" : "  [INVALID]");
    }
    return failed ? kExitModel : kExitOk;
}

int cmdSet(const std::vector<std::string>& args) {
    if (args.size() < 2) throw UsageError("set takes <file.agecad> name=expr...");
    Cli cli(args[0]);
    for (size_t i = 1; i < args.size(); ++i) setParameter(cli.session, args[i]);
    cli.session.execute("document_save", json::object(), cliActor());
    return kExitOk;
}

int cmdExport(const std::vector<std::string>& args) {
    if (args.size() < 2) throw UsageError("export takes <file.agecad> <out>");
    Cli cli(args[0]);
    applyOverrides(cli.session, args, 2);
    cli.session.execute("document_export", {{"path", args[1]}}, cliActor());
    std::cout << "wrote " << args[1] << "\n";
    return kExitOk;
}

int cmdRun(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    age::Actor actor = cliActor();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--agent") {
            if (i + 1 >= args.size()) throw UsageError("--agent takes a name");
            actor = {age::ActorKind::Agent, args[++i]};
        } else {
            positional.push_back(args[i]);
        }
    }
    if (positional.size() < 2 || positional.size() > 3)
        throw UsageError("run takes <file.agecad> <command> [json arguments]");
    json arguments = json::object();
    if (positional.size() == 3) {
        try {
            arguments = json::parse(positional[2]);
        } catch (const json::exception& e) {
            throw UsageError(std::string("arguments are not valid JSON: ") + e.what());
        }
    }

    Cli cli(positional[0]);
    const auto& entry = cli.session.registry().get(positional[1]);
    json result = cli.session.call(positional[1], arguments, actor);
    if (result["ok"] == true && entry.kind == age::cmd::CommandKind::Edit)
        cli.session.execute("document_save", json::object(), cliActor());
    std::cout << result.dump(2) << "\n";
    return result["ok"] == true ? kExitOk : kExitModel;
}

int cmdCommands(const std::vector<std::string>& args) {
    const auto& registry = age::cmd::Registry::builtin();
    if (args.size() > 1) throw UsageError("commands takes at most one command name");
    if (args.size() == 1) {
        std::cout << age::cmd::Registry::describe(registry.get(args[0])).dump(2) << "\n";
        return kExitOk;
    }
    for (const auto& e : registry.all())
        std::printf("  %-24s %-8s %s\n", e.info.id, age::cmd::commandKindName(e.kind), e.info.title);
    return kExitOk;
}

int run(std::vector<std::string> args) {
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        std::cout << kUsage;
        return args.empty() ? kExitUsage : kExitOk;
    }
    const std::string cmd = args.front();
    args.erase(args.begin());
    if (cmd == "new") return cmdNew(args);
    if (cmd == "info") return cmdInfo(args);
    if (cmd == "set") return cmdSet(args);
    if (cmd == "export") return cmdExport(args);
    if (cmd == "run") return cmdRun(args);
    if (cmd == "commands") return cmdCommands(args);
    throw UsageError("unknown command '" + cmd + "'");
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const UsageError& e) {
        std::cerr << "error: " << e.what() << "\n\n" << kUsage;
        return kExitUsage;
    } catch (const age::Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitModel;
    } catch (const std::exception& e) {
        std::cerr << "internal error: " << e.what() << "\n";
        return kExitInternal;
    } catch (...) {
        std::cerr << "internal error: unknown exception\n";
        return kExitInternal;
    }
}
