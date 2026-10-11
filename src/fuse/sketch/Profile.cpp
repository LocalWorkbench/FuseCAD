// SPDX-License-Identifier: GPL-3.0-or-later
//
// Profile detection: find the bounded faces of the planar graph formed by the
// sketch's non-construction curves, then attach nested loops as holes.
//
// Faces are traced with the usual half-edge rule: arriving at a vertex, leave
// along the outgoing edge immediately clockwise of the one we came in on. That
// keeps the face on the left, so bounded faces come out counter-clockwise and
// the outer boundary of each connected component comes out clockwise.
//
// Limitation: curves must meet at shared/coincident endpoints. Crossing curves
// are not split at their intersections yet (see docs/ROADMAP.md).

#include "fuse/sketch/Profile.h"

#include "fuse/sketch/Sketch.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>

namespace fuse::sketch {

namespace {

// Curve endpoints closer than this are the same vertex; circles smaller than
// this are ignored.
constexpr double kMergeTol = 1e-6;
// Traced loops with less area than this are slivers (e.g. a doubled-back edge).
constexpr double kMinLoopArea = 1e-9;
// Containment polygons: segments per full turn, and at least this many per arc.
constexpr int kSegmentsPerFullCircle = 64;
constexpr int kMinArcSegments = 4;

// Start angle and signed sweep of an arc or circle edge (full turn for circles).
struct ArcSpan {
    double start, sweep;
};

ArcSpan arcSpan(const ProfileEdge& e) {
    const double a0 = std::atan2(e.start.y - e.center.y, e.start.x - e.center.x);
    if (e.kind == ProfileEdge::Kind::Circle) return {a0, e.ccw ? 2 * kPi : -2 * kPi};
    double sweep = std::atan2(e.end.y - e.center.y, e.end.x - e.center.x) - a0;
    if (e.ccw && sweep <= 0) sweep += 2 * kPi;
    if (!e.ccw && sweep >= 0) sweep -= 2 * kPi;
    return {a0, sweep};
}

// Point samples along a loop (arcs discretised), used for containment tests.
std::vector<Vec2> sampleLoop(const Loop& loop) {
    std::vector<Vec2> pts;
    for (const auto& e : loop.edges) {
        if (e.kind == ProfileEdge::Kind::Line) {
            pts.push_back(e.start);
            continue;
        }
        const auto [a0, sweep] = arcSpan(e);
        const int segs = std::max(kMinArcSegments,
                                  static_cast<int>(std::ceil(std::fabs(sweep) / (2 * kPi) * kSegmentsPerFullCircle)));
        for (int i = 0; i < segs; ++i) {
            double a = a0 + sweep * i / segs;
            pts.push_back({e.center.x + e.radius * std::cos(a), e.center.y + e.radius * std::sin(a)});
        }
    }
    return pts;
}

bool pointInPolygon(Vec2 p, const std::vector<Vec2>& poly) {
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            inside = !inside;
    }
    return inside;
}

struct GraphEdge {
    int v0, v1;
    ProfileEdge forward; // v0 -> v1
    bool alive = true;
};

ProfileEdge reversed(const ProfileEdge& e) {
    ProfileEdge r = e;
    std::swap(r.start, r.end);
    r.ccw = !e.ccw;
    return r;
}

// Unit tangent leaving e.start, plus signed curvature (left-turning > 0) used
// to order curves that leave a vertex in the same direction.
std::pair<double, double> departure(const ProfileEdge& e) {
    Vec2 t;
    double k = 0;
    if (e.kind == ProfileEdge::Kind::Line) {
        t = e.end - e.start;
    } else {
        Vec2 radial = e.start - e.center;
        t = e.ccw ? radial.perp() : -radial.perp();
        k = (e.ccw ? 1.0 : -1.0) / e.radius;
    }
    return {std::atan2(t.y, t.x), k};
}

struct Face {
    Loop loop;
    std::vector<Vec2> polygon;
    int component = -1;
};

// Exact signed area via Green's theorem, A = 1/2 * closed-integral (x dy - y dx).
// A line contributes cross(start, end) / 2; an arc of radius r about c sweeping
// angle s from a0 to a1 contributes (r * [cx sin t - cy cos t]_{a0}^{a1} + r^2 s) / 2.
double edgeArea(const ProfileEdge& e) {
    if (e.kind == ProfileEdge::Kind::Line) return cross(e.start, e.end) / 2;
    const auto [a0, sweep] = arcSpan(e);
    const double a1 = a0 + sweep;
    const Vec2 c = e.center;
    const double r = e.radius;
    double linear = r * ((c.x * std::sin(a1) - c.y * std::cos(a1)) - (c.x * std::sin(a0) - c.y * std::cos(a0)));
    return (linear + r * r * sweep) / 2;
}

Loop makeLoop(std::vector<ProfileEdge> edges) {
    Loop l;
    l.edges = std::move(edges);
    for (const auto& e : l.edges) l.signedArea += edgeArea(e);
    return l;
}

Loop reverseLoop(const Loop& l) {
    Loop r;
    for (const auto& e : std::views::reverse(l.edges)) r.edges.push_back(reversed(e));
    r.signedArea = -l.signedArea;
    return r;
}

} // namespace

std::vector<Profile> Sketch::profiles() const {
    // 1. Vertices: merge curve endpoints that coincide.
    std::vector<Vec2> verts;
    auto vertexFor = [&](EntityId pointId) {
        Vec2 p = point(pointId);
        for (size_t i = 0; i < verts.size(); ++i)
            if ((verts[i] - p).length() < kMergeTol) return static_cast<int>(i);
        verts.push_back(p);
        return static_cast<int>(verts.size() - 1);
    };

    std::vector<GraphEdge> edges;
    std::vector<Face> faces; // circles contribute faces directly
    int componentCount = 0;

    for (const Entity& e : entities_) {
        if (e.construction) continue;
        if (e.type == EntityType::Line) {
            int a = vertexFor(e.p1), b = vertexFor(e.p2);
            if (a == b) continue;
            ProfileEdge pe{ProfileEdge::Kind::Line, verts[a], verts[b]};
            edges.push_back({a, b, pe});
        } else if (e.type == EntityType::Arc) {
            int a = vertexFor(e.p2), b = vertexFor(e.p3);
            if (a == b) continue;
            Vec2 c = point(e.p1);
            ProfileEdge pe{ProfileEdge::Kind::Arc, verts[a], verts[b], c, (verts[a] - c).length(), true};
            edges.push_back({a, b, pe});
        } else if (e.type == EntityType::Circle) {
            double r = vars_[e.var];
            if (r <= kMergeTol) continue;
            Vec2 c = point(e.p1);
            ProfileEdge pe{ProfileEdge::Kind::Circle, c + Vec2{r, 0}, c + Vec2{r, 0}, c, r, true};
            Loop ccw = makeLoop({pe});
            int comp = componentCount++;
            faces.push_back({ccw, sampleLoop(ccw), comp});
            Loop cw = reverseLoop(ccw);
            faces.push_back({cw, sampleLoop(cw), comp});
        }
    }

    // 2. Drop dangling chains (edges with an endpoint of degree 1).
    for (bool changed = true; changed;) {
        changed = false;
        std::vector<int> degree(verts.size(), 0);
        for (const auto& e : edges)
            if (e.alive) ++degree[e.v0], ++degree[e.v1];
        for (auto& e : edges)
            if (e.alive && (degree[e.v0] < 2 || degree[e.v1] < 2)) e.alive = false, changed = true;
    }

    // 3. Half-edges (2i = forward, 2i+1 = reverse) and connected components.
    const int H = static_cast<int>(edges.size() * 2);
    auto halfStart = [&](int h) { return h % 2 == 0 ? edges[h / 2].v0 : edges[h / 2].v1; };
    auto halfEnd = [&](int h) { return h % 2 == 0 ? edges[h / 2].v1 : edges[h / 2].v0; };
    auto halfCurve = [&](int h) { return h % 2 == 0 ? edges[h / 2].forward : reversed(edges[h / 2].forward); };

    std::vector<int> parent(verts.size());
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](int v) {
        while (parent[v] != v) v = parent[v] = parent[parent[v]];
        return v;
    };
    for (const auto& e : edges)
        if (e.alive) parent[find(e.v0)] = find(e.v1);

    // Outgoing half-edges per vertex, sorted counter-clockwise.
    std::vector<std::vector<int>> outgoing(verts.size());
    std::vector<std::pair<double, double>> key(H);
    for (int h = 0; h < H; ++h) {
        if (!edges[h / 2].alive) continue;
        key[h] = departure(halfCurve(h));
        outgoing[halfStart(h)].push_back(h);
    }
    for (auto& list : outgoing) std::sort(list.begin(), list.end(), [&](int a, int b) { return key[a] < key[b]; });

    // 4. Trace faces.
    std::vector<int> compId(verts.size(), -1);
    std::vector<bool> used(H, false);
    for (int h0 = 0; h0 < H; ++h0) {
        if (used[h0] || !edges[h0 / 2].alive) continue;
        std::vector<ProfileEdge> loopEdges;
        int h = h0;
        while (!used[h]) {
            used[h] = true;
            loopEdges.push_back(halfCurve(h));
            int v = halfEnd(h);
            const auto& out = outgoing[v];
            int twin = h % 2 == 0 ? h + 1 : h - 1;
            auto it = std::find(out.begin(), out.end(), twin);
            int idx = static_cast<int>(it - out.begin());
            h = out[(idx + static_cast<int>(out.size()) - 1) % out.size()];
        }
        Loop loop = makeLoop(std::move(loopEdges));
        if (std::fabs(loop.signedArea) < kMinLoopArea) continue;
        int root = find(halfStart(h0));
        if (compId[root] < 0) compId[root] = componentCount++;
        faces.push_back({loop, sampleLoop(loop), compId[root]});
    }

    // 5. Bounded faces are CCW. Each component's CW face is its outer boundary;
    //    it becomes a hole of the smallest CCW face of another component that contains it.
    std::vector<Profile> result;
    std::vector<int> faceToProfile(faces.size(), -1);
    for (size_t i = 0; i < faces.size(); ++i) {
        if (faces[i].loop.signedArea <= 0) continue;
        faceToProfile[i] = static_cast<int>(result.size());
        result.push_back({faces[i].loop, {}, faces[i].loop.signedArea});
    }
    for (size_t i = 0; i < faces.size(); ++i) {
        if (faces[i].loop.signedArea >= 0) continue;
        Vec2 probe = faces[i].polygon.front();
        int best = -1;
        for (size_t j = 0; j < faces.size(); ++j) {
            if (faceToProfile[j] < 0 || faces[j].component == faces[i].component) continue;
            if (!pointInPolygon(probe, faces[j].polygon)) continue;
            if (best < 0 || faces[j].loop.signedArea < faces[best].loop.signedArea) best = static_cast<int>(j);
        }
        if (best < 0) continue; // outermost boundary: unbounded face
        Profile& p = result[faceToProfile[best]];
        p.holes.push_back(faces[i].loop);
        p.area += faces[i].loop.signedArea;
    }
    return result;
}

} // namespace fuse::sketch
