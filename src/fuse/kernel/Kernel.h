// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/core/EnumTable.h"
#include "fuse/core/Math.h"
#include "fuse/sketch/Profile.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fuse {

// Opaque handle to a kernel-owned solid. Shapes are immutable: every modelling
// operation returns a new one and no query (meshing and export included)
// modifies its input, so a shape can be shared between documents, undo states
// and caches without copying.
class Shape {
public:
    virtual ~Shape() = default;
};
using ShapePtr = std::shared_ptr<const Shape>;

// Chordal deflection (mm) for STL export when the user has not chosen one.
inline constexpr double kDefaultStlDeflection = 0.05;

enum class BooleanOp { Join, Cut, Intersect };

// Triangulated display mesh. Positions/normals are xyz triplets.
struct Mesh {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<std::uint32_t> indices;
    // B-rep edges as polylines (xyz triplets), for drawing the wireframe overlay.
    std::vector<std::vector<float>> edges;
};

// How a feature picks edges of a body. Interim answer to the topological
// naming problem: selections are re-resolved geometrically on every recompute.
struct EdgeQuery {
    enum class Mode {
        All,        // every edge bounded by two distinct faces
        ParallelTo, // straight edges parallel to `direction`
        NearPoints, // the edge closest to each point in `points`
    } mode = Mode::All;
    Vec3 direction{0, 0, 1};
    std::vector<Vec3> points;
};

inline constexpr auto kEdgeQueryModes = makeEnumTable<EnumName<EdgeQuery::Mode>>({
    {EdgeQuery::Mode::All, "all"},
    {EdgeQuery::Mode::ParallelTo, "parallel_to"},
    {EdgeQuery::Mode::NearPoints, "near_points"},
});
constexpr const auto& enumTable(EdgeQuery::Mode) { return kEdgeQueryModes; }

struct BoundingBox {
    Vec3 min, max;
};

class Kernel {
public:
    virtual ~Kernel() = default;

    virtual std::string name() const = 0;

    // Extrude profiles along the plane normal, from `start` to `start + distance`
    // (both signed, in base units).
    virtual ShapePtr extrude(const Plane& plane, const std::vector<sketch::Profile>& profiles, double start,
                             double distance) = 0;
    // Revolve profiles about an axis given in sketch coordinates.
    virtual ShapePtr revolve(const Plane& plane, const std::vector<sketch::Profile>& profiles, Vec2 axisOrigin,
                             Vec2 axisDirection, double angle) = 0;
    virtual ShapePtr boolean(BooleanOp op, const ShapePtr& target, const ShapePtr& tool) = 0;
    virtual ShapePtr fillet(const ShapePtr& shape, const EdgeQuery& edges, double radius) = 0;
    virtual ShapePtr chamfer(const ShapePtr& shape, const EdgeQuery& edges, double distance) = 0;

    virtual ShapePtr importStep(const std::string& path) = 0;
    virtual void exportStep(const std::vector<ShapePtr>& shapes, const std::string& path) = 0;
    virtual void exportStl(const std::vector<ShapePtr>& shapes, const std::string& path, double deflection) = 0;

    virtual Mesh tessellate(const ShapePtr& shape, double deflection) = 0;
    virtual double volume(const ShapePtr& shape) = 0;
    virtual double surfaceArea(const ShapePtr& shape) = 0;
    virtual BoundingBox boundingBox(const ShapePtr& shape) = 0;
    virtual bool isValid(const ShapePtr& shape) = 0;
    virtual int countFaces(const ShapePtr& shape) = 0;
};

} // namespace fuse
