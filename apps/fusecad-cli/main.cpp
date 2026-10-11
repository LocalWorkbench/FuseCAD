// SPDX-License-Identifier: GPL-3.0-or-later
//
// fusecad-cli: headless access to the FuseCAD modelling stack.
//
//   fusecad-cli new <file.fcad> [--sample plate|flange]
//   fusecad-cli info <file.fcad> [-p name=expr]...
//   fusecad-cli set <file.fcad> name=expr...
//   fusecad-cli export <file.fcad> <out.step|out.stp|out.stl> [-p name=expr]...
//
// Exit codes are listed in kUsage.

#include "fuse/core/Error.h"
#include "fuse/model/Document.h"
#include "fuse/model/Samples.h"
#include "fuse/occt/OcctKernel.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kUsage = R"(usage:
  fusecad-cli new <file.fcad> [--sample NAME]      create an empty or sample document
  fusecad-cli info <file.fcad> [-p name=expr]...   recompute and print parameters, features, bodies
  fusecad-cli set <file.fcad> name=expr...         change parameters and save
  fusecad-cli export <file.fcad> <out> [-p ...]    export bodies to STEP (.step/.stp) or STL (.stl)

exit codes:
  0  success
  1  usage error
  2  modelling error: unreadable document, bad expression, failed feature
  3  internal error
)";

enum ExitCode : int { kExitOk = 0, kExitUsage = 1, kExitModel = 2, kExitInternal = 3 };

struct UsageError : fuse::Error {
    using fuse::Error::Error;
};

void applyAssignment(fuse::Document& doc, std::string_view assignment) {
    auto eq = assignment.find('=');
    if (eq == std::string_view::npos || eq == 0)
        throw UsageError("expected name=expression, got '" + std::string(assignment) + "'");
    doc.parameters().setExpression(assignment.substr(0, eq), assignment.substr(eq + 1));
}

// Consumes "-p name=expr" pairs from args[from..].
void applyOverrides(fuse::Document& doc, const std::vector<std::string>& args, size_t from) {
    for (size_t i = from; i < args.size(); ++i) {
        if (args[i] != "-p" || i + 1 >= args.size()) throw UsageError("unexpected argument '" + args[i] + "'");
        applyAssignment(doc, args[++i]);
    }
}

fuse::RecomputeReport recomputeOrWarn(fuse::Document& doc, fuse::Kernel& kernel) {
    fuse::RecomputeReport r = doc.recompute(kernel);
    for (const auto& m : r.messages) std::cerr << "warning: " << m << "\n";
    return r;
}

std::vector<fuse::ShapePtr> allShapes(const fuse::Document& doc) {
    std::vector<fuse::ShapePtr> shapes;
    for (const auto& [id, body] : doc.bodies()) shapes.push_back(body.shape);
    return shapes;
}

int cmdNew(const std::vector<std::string>& args) {
    const bool plain = args.size() == 1;
    const bool withSample = args.size() == 3 && args[1] == "--sample";
    if (!plain && !withSample) throw UsageError("new takes <file.fcad> [--sample plate|flange]");
    fuse::Document doc;
    if (args.size() == 3) {
        if (args[2] == "plate")
            doc = fuse::samples::mountingPlate();
        else if (args[2] == "flange")
            doc = fuse::samples::flange();
        else
            throw UsageError("unknown sample '" + args[2] + "' (available: plate, flange)");
    }
    doc.save(args[0]);
    std::cout << "wrote " << args[0] << "\n";
    return kExitOk;
}

int cmdInfo(const std::vector<std::string>& args) {
    if (args.empty()) throw UsageError("info takes <file.fcad>");
    fuse::Document doc = fuse::Document::load(args[0]);
    applyOverrides(doc, args, 1);
    auto kernel = fuse::occt::createKernel();
    fuse::RecomputeReport report = recomputeOrWarn(doc, *kernel);

    std::cout << "Kernel: " << kernel->name() << "\n\nParameters\n";
    for (const auto& p : doc.parameters().all())
        std::printf("  %-14s = %-20s -> %s\n", p.name.c_str(), p.expression.source().c_str(),
                    fuse::formatValue(p.value, p.unit).c_str());

    std::cout << "\nTimeline\n";
    for (size_t i = 0; i < doc.features().size(); ++i) {
        const auto& f = *doc.features()[i];
        const char* state = i >= doc.marker() ? "rolled back"
                            : f.suppressed    ? "suppressed"
                            : f.healthy()     ? "ok"
                                              : "ERROR";
        std::printf("  %2zu. %-12s %-12s %s%s%s\n", i + 1, f.name.c_str(), fuse::featureTypeName(f.type()), state,
                    f.healthy() ? "" : ": ", f.error.c_str());
    }

    std::cout << "\nBodies\n";
    for (const auto& [id, body] : doc.bodies()) {
        fuse::BoundingBox bb = kernel->boundingBox(body.shape);
        std::printf("  %-8s volume %.4f mm^3, area %.4f mm^2, %d faces, size %.3f x %.3f x %.3f mm%s\n",
                    body.name.c_str(), kernel->volume(body.shape), kernel->surfaceArea(body.shape),
                    kernel->countFaces(body.shape), bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z,
                    kernel->isValid(body.shape) ? "" : "  [INVALID]");
    }
    return report.ok() ? kExitOk : kExitModel;
}

int cmdSet(const std::vector<std::string>& args) {
    if (args.size() < 2) throw UsageError("set takes <file.fcad> name=expr...");
    fuse::Document doc = fuse::Document::load(args[0]);
    for (size_t i = 1; i < args.size(); ++i) applyAssignment(doc, args[i]);
    doc.save(args[0]);
    return kExitOk;
}

int cmdExport(const std::vector<std::string>& args) {
    if (args.size() < 2) throw UsageError("export takes <file.fcad> <out>");
    fuse::Document doc = fuse::Document::load(args[0]);
    applyOverrides(doc, args, 2);
    auto kernel = fuse::occt::createKernel();
    if (!recomputeOrWarn(doc, *kernel).ok()) throw fuse::Error("document has errors; not exporting");
    auto shapes = allShapes(doc);
    if (shapes.empty()) throw fuse::Error("document has no bodies to export");

    const std::string& out = args[1];
    std::string ext = std::filesystem::path(out).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".step" || ext == ".stp")
        kernel->exportStep(shapes, out);
    else if (ext == ".stl")
        kernel->exportStl(shapes, out, fuse::kDefaultStlDeflection);
    else
        throw UsageError("unsupported export format '" + ext + "'");
    std::cout << "wrote " << out << "\n";
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
    throw UsageError("unknown command '" + cmd + "'");
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const UsageError& e) {
        std::cerr << "error: " << e.what() << "\n\n" << kUsage;
        return kExitUsage;
    } catch (const fuse::Error& e) {
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
