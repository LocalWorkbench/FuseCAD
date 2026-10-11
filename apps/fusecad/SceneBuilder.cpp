// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneBuilder.h"

#include "ViewState.h"
#include "fuse/core/Error.h"
#include "fuse/model/Document.h"
#include "fuse/model/Features.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace scene {

namespace {

constexpr double kDisplayDeflection = 0.05; // mm
constexpr int kSegmentsPerTurn = 72;        // circle and arc display resolution
constexpr int kMinArcSegments = 8;

} // namespace

std::vector<BodyMesh> MeshCache::update(fuse::Kernel& kernel, const fuse::Document& doc,
                                        std::vector<std::string>& problems) {
    std::unordered_map<const fuse::Shape*, Entry> next;
    std::vector<BodyMesh> meshes;
    for (const auto& [id, body] : doc.bodies()) {
        const fuse::Shape* key = body.shape.get();
        if (auto it = next.find(key); it != next.end()) {
            meshes.push_back({id, it->second.mesh});
            continue;
        }
        if (auto it = entries_.find(key); it != entries_.end()) {
            meshes.push_back({id, it->second.mesh});
            next.insert(entries_.extract(it));
            continue;
        }
        try {
            auto mesh = std::make_shared<const fuse::Mesh>(kernel.tessellate(body.shape, kDisplayDeflection));
            meshes.push_back({id, mesh});
            next.emplace(key, Entry{body.shape, std::move(mesh)});
        } catch (const fuse::Error& e) {
            problems.push_back(body.name + ": " + e.what());
        }
    }
    entries_ = std::move(next);
    return meshes;
}

std::vector<std::vector<float>> sketchPolylines(const fuse::SketchFeature& f) {
    using fuse::sketch::EntityType;
    const fuse::sketch::Sketch& s = f.sketch;
    const fuse::Plane& pl = f.resolvedPlane;
    std::vector<std::vector<float>> out;
    auto push = [&](std::vector<float>& strip, fuse::Vec2 p) {
        const fuse::Vec3 w = pl.toWorld(p);
        strip.insert(strip.end(), {static_cast<float>(w.x), static_cast<float>(w.y), static_cast<float>(w.z)});
    };
    auto arc = [&](fuse::Vec2 c, double r, double a0, double sweep) {
        std::vector<float> strip;
        const int n = std::max(kMinArcSegments,
                               static_cast<int>(std::ceil(std::fabs(sweep) / (2 * fuse::kPi) * kSegmentsPerTurn)));
        for (int i = 0; i <= n; ++i) {
            const double a = a0 + sweep * i / n;
            push(strip, {c.x + r * std::cos(a), c.y + r * std::sin(a)});
        }
        out.push_back(std::move(strip));
    };
    for (const auto& e : s.entities()) {
        if (e.construction) continue;
        switch (e.type) {
        case EntityType::Point:
            break;
        case EntityType::Line: {
            std::vector<float> strip;
            push(strip, s.point(e.p1));
            push(strip, s.point(e.p2));
            out.push_back(std::move(strip));
            break;
        }
        case EntityType::Circle:
            arc(s.point(e.p1), s.variables()[static_cast<size_t>(e.var)], 0, 2 * fuse::kPi);
            break;
        case EntityType::Arc: {
            const fuse::Vec2 c = s.point(e.p1), a = s.point(e.p2), b = s.point(e.p3);
            const double a0 = std::atan2(a.y - c.y, a.x - c.x);
            double sweep = std::atan2(b.y - c.y, b.x - c.x) - a0;
            if (sweep <= 0) sweep += 2 * fuse::kPi;
            arc(c, (a - c).length(), a0, sweep);
            break;
        }
        }
    }
    return out;
}

std::vector<std::vector<float>> visibleSketchPolylines(const fuse::Document& doc, const ViewState& view) {
    std::vector<std::vector<float>> curves;
    for (size_t i = 0; i < doc.features().size() && i < doc.marker(); ++i) {
        const auto* sk = fuse::featureCast<fuse::SketchFeature>(doc.features()[i].get());
        if (!sk || !sk->healthy() || !view.sketchVisible(sk->id)) continue;
        auto lines = sketchPolylines(*sk);
        curves.insert(curves.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
    }
    return curves;
}

} // namespace scene
