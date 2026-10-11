// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/kernel/Kernel.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fuse {
class Document;
class SketchFeature;
} // namespace fuse
struct ViewState;

// Turns the document's derived geometry into display data for the Viewport.
namespace scene {

// A body's display mesh. Meshes are shared and immutable, so the viewport can
// recognise one it has already uploaded by pointer.
struct BodyMesh {
    int id = 0;
    std::shared_ptr<const fuse::Mesh> mesh;
};

// Tessellates bodies, caching meshes by shape so bodies whose shape did not
// change across a recompute are neither re-tessellated nor re-uploaded.
class MeshCache {
public:
    // Meshes for every body in `doc`. Bodies that fail to tessellate are
    // skipped and reported in `problems` as "<body>: <error>".
    std::vector<BodyMesh> update(fuse::Kernel& kernel, const fuse::Document& doc, std::vector<std::string>& problems);

private:
    struct Entry {
        fuse::ShapePtr shape; // keeps the key's address from being reused
        std::shared_ptr<const fuse::Mesh> mesh;
    };
    std::unordered_map<const fuse::Shape*, Entry> entries_;
};

// World-space polylines (xyz strips) of a sketch's non-construction geometry.
std::vector<std::vector<float>> sketchPolylines(const fuse::SketchFeature& sketch);

// Polylines of every healthy, active sketch that `view` shows.
std::vector<std::vector<float>> visibleSketchPolylines(const fuse::Document& doc, const ViewState& view);

} // namespace scene
