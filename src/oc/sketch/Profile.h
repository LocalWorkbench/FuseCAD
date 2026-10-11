// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/core/Math.h"

#include <vector>

namespace oc::sketch {

// Kernel-independent description of a closed planar region in sketch
// coordinates. This is what the sketcher hands to the geometry kernel.
struct ProfileEdge {
    enum class Kind { Line, Arc, Circle } kind = Kind::Line;
    Vec2 start, end; // Circle: start == end, a point on the circle
    Vec2 center;     // Arc / Circle
    double radius = 0;
    bool ccw = true; // Arc / Circle: direction of travel from start to end
};

struct Loop {
    std::vector<ProfileEdge> edges; // consecutive: edges[i].end == edges[i+1].start
    double signedArea = 0;          // > 0 counter-clockwise
};

struct Profile {
    Loop outer;              // counter-clockwise
    std::vector<Loop> holes; // clockwise
    double area = 0;         // net area (outer minus holes)
};

// Point samples along a loop (arcs discretised), used for area and containment.
std::vector<Vec2> sampleLoop(const Loop& loop, int segmentsPerFullCircle = 64);
double signedArea(const std::vector<Vec2>& polygon);
bool pointInPolygon(Vec2 p, const std::vector<Vec2>& polygon);

} // namespace oc::sketch
