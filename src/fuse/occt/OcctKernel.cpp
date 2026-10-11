// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuse/occt/OcctKernel.h"

#include "fuse/core/Error.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <DESTEP_Parameters.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <exception>
#include <limits>
#include <string>

namespace fuse::occt {

namespace {

// Angular deflection (radians) for face meshing and for discretising display
// edges; the chordal deflection is the caller's choice.
constexpr double kMeshAngularDeflection = 0.5;
constexpr double kEdgeAngularDeflection = 0.1;

// Lengths below this (mm) are zero to the kernel; equal to OCCT's default
// linear precision.
constexpr double kLinearTolerance = 1.0e-7;

constexpr float f32(double v) { return static_cast<float>(v); }

class OcctShape final : public Shape {
public:
    explicit OcctShape(TopoDS_Shape s) : shape(std::move(s)) {}
    TopoDS_Shape shape;
};

ShapePtr wrap(TopoDS_Shape s) { return std::make_shared<OcctShape>(std::move(s)); }

const TopoDS_Shape& unwrap(const ShapePtr& s) {
    auto* p = dynamic_cast<const OcctShape*>(s.get());
    if (!p) throw Error("shape does not belong to the OpenCASCADE kernel");
    return p->shape;
}

// Run an OCCT operation, translating every exception it may raise into
// fuse::Error. OCCT's Standard_Failure does not derive from std::exception, so
// without this it would escape every caller's error handling.
template <class Fn>
auto guarded(const char* what, Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const Error&) {
        throw;
    } catch (const Standard_Failure& e) {
        const char* msg = e.GetMessageString();
        throw Error(std::string(what) + " failed: " + (msg && *msg ? msg : e.DynamicType()->Name()));
    } catch (const std::exception& e) {
        throw Error(std::string(what) + " failed: " + e.what());
    }
}

gp_Pnt pnt(Vec3 v) { return {v.x, v.y, v.z}; }
gp_Dir dir(Vec3 v) { return {v.x, v.y, v.z}; }
gp_Pnt pnt(const Plane& pl, Vec2 p) { return pnt(pl.toWorld(p)); }

gp_Ax2 frameAt(const Plane& pl, Vec2 center) { return {pnt(pl, center), dir(pl.normal()), dir(pl.xDir)}; }

TopoDS_Edge makeEdge(const Plane& pl, const sketch::ProfileEdge& e) {
    using Kind = sketch::ProfileEdge::Kind;
    switch (e.kind) {
    case Kind::Line:
        return BRepBuilderAPI_MakeEdge(pnt(pl, e.start), pnt(pl, e.end)).Edge();
    case Kind::Circle: {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(gp_Circ(frameAt(pl, e.center), e.radius)).Edge();
        return e.ccw ? edge : TopoDS::Edge(edge.Reversed());
    }
    case Kind::Arc: {
        gp_Circ circ(frameAt(pl, e.center), e.radius);
        Vec2 from = e.ccw ? e.start : e.end;
        Vec2 to = e.ccw ? e.end : e.start;
        GC_MakeArcOfCircle arc(circ, pnt(pl, from), pnt(pl, to), Standard_True);
        if (!arc.IsDone()) throw Error("could not build arc edge");
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(arc.Value()).Edge();
        return e.ccw ? edge : TopoDS::Edge(edge.Reversed());
    }
    }
    throw Error("unknown profile edge kind");
}

TopoDS_Wire makeWire(const Plane& pl, const sketch::Loop& loop) {
    BRepBuilderAPI_MakeWire mw;
    for (const auto& e : loop.edges) mw.Add(makeEdge(pl, e));
    if (!mw.IsDone()) {
        if (mw.Error() == BRepBuilderAPI_EmptyWire) throw Error("profile loop has no edges");
        if (mw.Error() == BRepBuilderAPI_NonManifoldWire) throw Error("profile loop crosses or branches on itself");
        throw Error("profile loop has edges that do not connect end to end");
    }
    TopoDS_Wire wire = mw.Wire();
    if (!BRep_Tool::IsClosed(wire)) throw Error("profile loop is not closed");
    return wire;
}

TopoDS_Face makeFace(const Plane& pl, const sketch::Profile& profile) {
    gp_Pln plane(gp_Ax3(pnt(pl.origin), dir(pl.normal()), dir(pl.xDir)));
    BRepBuilderAPI_MakeFace mf(plane, makeWire(pl, profile.outer), Standard_True);
    for (const auto& hole : profile.holes) mf.Add(makeWire(pl, hole));
    if (!mf.IsDone()) throw Error("could not build a face from the sketch profile");
    return mf.Face();
}

int countSubShapes(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(s, type, map);
    return map.Extent();
}

// Accepts `s` as the result of a modelling operation only if it holds at least
// one solid and passes the B-rep checker; otherwise throws `problem`, which
// should tell a CAD user what to change.
TopoDS_Shape requireValidSolid(TopoDS_Shape s, const std::string& problem) {
    if (s.IsNull() || countSubShapes(s, TopAbs_SOLID) == 0 || !BRepCheck_Analyzer(s).IsValid()) throw Error(problem);
    return s;
}

// Meshing stores triangulations on the faces it visits, and faces are shared
// between every shape derived from them. Mesh a topological copy (the geometry
// itself is still shared) so callers' shapes are never modified and the output
// does not depend on what was meshed before, or on another thread.
TopoDS_Shape meshedCopy(const TopoDS_Shape& s, double deflection) {
    TopoDS_Shape copy = BRepBuilderAPI_Copy(s, Standard_False, Standard_False).Shape();
    BRepMesh_IncrementalMesh mesher(copy, deflection, Standard_False, kMeshAngularDeflection, Standard_True);
    if (!mesher.IsDone()) throw Error("could not triangulate the shape");
    return copy;
}

template <class Op>
TopoDS_Shape runBoolean(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    Op op(a, b);
    if (op.HasErrors()) throw Error("the boolean operation could not be computed");
    return op.Shape();
}

TopoDS_Shape fuseAll(const std::vector<TopoDS_Shape>& shapes) {
    if (shapes.empty()) throw Error("nothing to build: no profiles selected");
    TopoDS_Shape acc = shapes.front();
    for (size_t i = 1; i < shapes.size(); ++i) acc = runBoolean<BRepAlgoAPI_Fuse>(acc, shapes[i]);
    return acc;
}

TopoDS_Shape unify(const TopoDS_Shape& s) {
    ShapeUpgrade_UnifySameDomain u(s, Standard_True, Standard_True, Standard_False);
    u.Build();
    return u.Shape();
}

TopoDS_Compound compound(const std::vector<ShapePtr>& shapes) {
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : shapes) b.Add(c, unwrap(s));
    return c;
}

std::vector<TopoDS_Edge> selectEdges(const TopoDS_Shape& shape, const EdgeQuery& q) {
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndUniqueAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);

    std::vector<TopoDS_Edge> candidates;
    for (int i = 1; i <= edgeFaces.Extent(); ++i) {
        const TopoDS_Edge& e = TopoDS::Edge(edgeFaces.FindKey(i));
        if (BRep_Tool::Degenerated(e) || edgeFaces(i).Extent() != 2) continue; // seams, free edges
        candidates.push_back(e);
    }

    std::vector<TopoDS_Edge> out;
    switch (q.mode) {
    case EdgeQuery::Mode::All:
        out = std::move(candidates);
        break;
    case EdgeQuery::Mode::ParallelTo: {
        gp_Dir want = dir(q.direction);
        for (const auto& e : candidates) {
            BRepAdaptor_Curve c(e);
            if (c.GetType() == GeomAbs_Line && c.Line().Direction().IsParallel(want, Precision::Angular()))
                out.push_back(e);
        }
        break;
    }
    case EdgeQuery::Mode::NearPoints:
        for (const Vec3& p : q.points) {
            TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(pnt(p)).Vertex();
            double best = std::numeric_limits<double>::max();
            int bestIdx = -1;
            for (size_t i = 0; i < candidates.size(); ++i) {
                BRepExtrema_DistShapeShape d(v, candidates[i]);
                if (d.IsDone() && d.Value() < best) best = d.Value(), bestIdx = static_cast<int>(i);
            }
            if (bestIdx < 0) continue;
            bool dup = false;
            for (const auto& e : out) dup = dup || e.IsSame(candidates[bestIdx]);
            if (!dup) out.push_back(candidates[bestIdx]);
        }
        break;
    }
    if (out.empty()) throw Error("edge selection matched no edges");
    return out;
}

class OcctKernel final : public Kernel {
public:
    std::string name() const override { return "OpenCASCADE " OCC_VERSION_COMPLETE; }

    ShapePtr extrude(const Plane& pl, const std::vector<sketch::Profile>& profiles, double start,
                     double distance) override {
        if (std::fabs(distance) < kLinearTolerance) throw Error("extrude distance must not be zero");
        return guarded("extrude", [&] {
            Vec3 n = pl.normal();
            gp_Trsf shift;
            shift.SetTranslation(gp_Vec(n.x * start, n.y * start, n.z * start));
            std::vector<TopoDS_Shape> solids;
            for (const auto& p : profiles) {
                TopoDS_Shape face = makeFace(pl, p);
                if (start != 0) face = BRepBuilderAPI_Transform(face, shift, Standard_True).Shape();
                BRepPrimAPI_MakePrism prism(face, gp_Vec(n.x * distance, n.y * distance, n.z * distance));
                if (!prism.IsDone()) throw Error("prism construction failed");
                solids.push_back(prism.Shape());
            }
            return wrap(requireValidSolid(fuseAll(solids), "the extrusion did not produce a valid solid"));
        });
    }

    ShapePtr revolve(const Plane& pl, const std::vector<sketch::Profile>& profiles, Vec2 axisOrigin, Vec2 axisDirection,
                     double angle) override {
        if (std::fabs(angle) < Precision::Angular()) throw Error("revolve angle must not be zero");
        return guarded("revolve", [&] {
            gp_Ax1 axis(pnt(pl, axisOrigin), dir(pl.dirToWorld(axisDirection.normalized())));
            std::vector<TopoDS_Shape> solids;
            for (const auto& p : profiles) {
                BRepPrimAPI_MakeRevol rev(makeFace(pl, p), axis,
                                          std::min(std::fabs(angle), 2 * kPi) * (angle < 0 ? -1 : 1));
                if (!rev.IsDone()) throw Error("revolution failed");
                solids.push_back(rev.Shape());
            }
            return wrap(requireValidSolid(fuseAll(solids),
                                          "the revolution did not produce a valid solid; check that the profile does "
                                          "not cross the axis"));
        });
    }

    ShapePtr boolean(BooleanOp op, const ShapePtr& target, const ShapePtr& tool) override {
        return guarded("boolean", [&] {
            const TopoDS_Shape& a = unwrap(target);
            const TopoDS_Shape& b = unwrap(tool);
            TopoDS_Shape result;
            const char* empty = nullptr;
            switch (op) {
            case BooleanOp::Join:
                result = runBoolean<BRepAlgoAPI_Fuse>(a, b);
                empty = "joining the bodies left no solid";
                break;
            case BooleanOp::Cut:
                result = runBoolean<BRepAlgoAPI_Cut>(a, b);
                empty = "the cut removes the whole body";
                break;
            case BooleanOp::Intersect:
                result = runBoolean<BRepAlgoAPI_Common>(a, b);
                empty = "the bodies do not overlap, so their intersection is empty";
                break;
            }
            if (result.IsNull() || countSubShapes(result, TopAbs_SOLID) == 0) throw Error(empty);
            return wrap(requireValidSolid(unify(result), "the boolean operation produced invalid geometry"));
        });
    }

    ShapePtr fillet(const ShapePtr& shape, const EdgeQuery& q, double radius) override {
        if (radius <= 0) throw Error("fillet radius must be positive");
        return guarded("fillet", [&] {
            const TopoDS_Shape& s = unwrap(shape);
            BRepFilletAPI_MakeFillet mk(s);
            for (const auto& e : selectEdges(s, q)) mk.Add(radius, e);
            mk.Build();
            const char* tooLarge = "fillet radius is too large for the selected edges";
            if (!mk.IsDone()) throw Error(tooLarge);
            return wrap(requireValidSolid(mk.Shape(), tooLarge));
        });
    }

    ShapePtr chamfer(const ShapePtr& shape, const EdgeQuery& q, double distance) override {
        if (distance <= 0) throw Error("chamfer distance must be positive");
        return guarded("chamfer", [&] {
            const TopoDS_Shape& s = unwrap(shape);
            BRepFilletAPI_MakeChamfer mk(s);
            for (const auto& e : selectEdges(s, q)) mk.Add(distance, e);
            mk.Build();
            const char* tooLarge = "chamfer distance is too large for the selected edges";
            if (!mk.IsDone()) throw Error(tooLarge);
            return wrap(requireValidSolid(mk.Shape(), tooLarge));
        });
    }

    ShapePtr importStep(const std::string& path) override {
        return guarded("STEP import", [&] {
            STEPControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) throw Error("cannot read STEP file " + path);
            if (reader.TransferRoots() == 0) throw Error("STEP file " + path + " contains no shapes");
            return wrap(requireValidSolid(reader.OneShape(), "STEP file " + path +
                                                                 " contains no valid solid (only surfaces, curves or "
                                                                 "damaged geometry)"));
        });
    }

    void exportStep(const std::vector<ShapePtr>& shapes, const std::string& path) override {
        guarded("STEP export", [&] {
            DESTEP_Parameters params; // defaults, not the process-wide Interface_Static settings
            params.WriteUnit = UnitsMethods_LengthUnit_Millimeter;
            STEPControl_Writer writer;
            for (const auto& s : shapes)
                if (writer.Transfer(unwrap(s), STEPControl_AsIs, params) != IFSelect_RetDone)
                    throw Error("could not translate shape to STEP");
            if (writer.Write(path.c_str()) != IFSelect_RetDone) throw Error("cannot write " + path);
        });
    }

    void exportStl(const std::vector<ShapePtr>& shapes, const std::string& path, double deflection) override {
        guarded("STL export", [&] {
            TopoDS_Shape meshed = meshedCopy(compound(shapes), deflection);
            StlAPI_Writer writer;
            writer.ASCIIMode() = Standard_False;
            if (!writer.Write(meshed, path.c_str())) throw Error("cannot write " + path);
        });
    }

    Mesh tessellate(const ShapePtr& shape, double deflection) override {
        return guarded("tessellation", [&] {
            const TopoDS_Shape s = meshedCopy(unwrap(shape), deflection);
            Mesh m;
            for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next()) {
                const TopoDS_Face& face = TopoDS::Face(ex.Current());
                TopLoc_Location loc;
                Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
                if (tri.IsNull()) continue;
                const gp_Trsf tr = loc.Transformation();
                const bool reversed = face.Orientation() == TopAbs_REVERSED;
                const auto base = static_cast<std::uint32_t>(m.positions.size() / 3);
                const int nodes = tri->NbNodes();
                for (int i = 1; i <= nodes; ++i) {
                    gp_Pnt p = tri->Node(i).Transformed(tr);
                    m.positions.insert(m.positions.end(), {f32(p.X()), f32(p.Y()), f32(p.Z())});
                }
                // Smooth normals within a face; faces stay sharp against each other.
                std::vector<double> nrm(static_cast<size_t>(nodes) * 3, 0.0);
                for (int t = 1; t <= tri->NbTriangles(); ++t) {
                    int a, b, c;
                    tri->Triangle(t).Get(a, b, c);
                    if (reversed) std::swap(b, c);
                    gp_Pnt pa = tri->Node(a).Transformed(tr), pb = tri->Node(b).Transformed(tr),
                           pc = tri->Node(c).Transformed(tr);
                    gp_Vec fn = gp_Vec(pa, pb).Crossed(gp_Vec(pa, pc)); // area-weighted
                    for (int v : {a, b, c}) {
                        const auto at = static_cast<size_t>(v - 1) * 3;
                        nrm[at] += fn.X();
                        nrm[at + 1] += fn.Y();
                        nrm[at + 2] += fn.Z();
                    }
                    m.indices.insert(m.indices.end(), {base + static_cast<std::uint32_t>(a - 1),
                                                       base + static_cast<std::uint32_t>(b - 1),
                                                       base + static_cast<std::uint32_t>(c - 1)});
                }
                for (int i = 0; i < nodes; ++i) {
                    double* v = &nrm[static_cast<size_t>(i) * 3];
                    double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                    if (len > 0) v[0] /= len, v[1] /= len, v[2] /= len;
                    m.normals.insert(m.normals.end(), {f32(v[0]), f32(v[1]), f32(v[2])});
                }
            }

            // Display edges: skip degenerate edges and seams (an edge bounded by a
            // single face on both sides, e.g. the parametric seam of a cylinder).
            TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
            TopExp::MapShapesAndUniqueAncestors(s, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
            for (int i = 1; i <= edgeFaces.Extent(); ++i) {
                const TopoDS_Edge& e = TopoDS::Edge(edgeFaces.FindKey(i));
                if (BRep_Tool::Degenerated(e)) continue;
                if (edgeFaces(i).Extent() == 1 && BRep_Tool::IsClosed(e, TopoDS::Face(edgeFaces(i).First()))) continue;
                BRepAdaptor_Curve curve(e);
                GCPnts_TangentialDeflection disc(curve, kEdgeAngularDeflection, deflection);
                std::vector<float> line;
                for (int k = 1; k <= disc.NbPoints(); ++k) {
                    gp_Pnt p = disc.Value(k);
                    line.insert(line.end(), {f32(p.X()), f32(p.Y()), f32(p.Z())});
                }
                if (line.size() >= 6) m.edges.push_back(std::move(line));
            }
            return m;
        });
    }

    double volume(const ShapePtr& shape) override {
        return guarded("volume", [&] {
            GProp_GProps props;
            BRepGProp::VolumeProperties(unwrap(shape), props);
            return props.Mass();
        });
    }

    double surfaceArea(const ShapePtr& shape) override {
        return guarded("surface area", [&] {
            GProp_GProps props;
            BRepGProp::SurfaceProperties(unwrap(shape), props);
            return props.Mass();
        });
    }

    BoundingBox boundingBox(const ShapePtr& shape) override {
        return guarded("bounding box", [&] {
            Bnd_Box box;
            BRepBndLib::Add(unwrap(shape), box);
            BoundingBox b;
            if (!box.IsVoid()) box.Get(b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z);
            return b;
        });
    }

    bool isValid(const ShapePtr& shape) override {
        return guarded("validity check", [&] { return BRepCheck_Analyzer(unwrap(shape)).IsValid() == Standard_True; });
    }

    int countFaces(const ShapePtr& shape) override {
        return guarded("face count", [&] { return countSubShapes(unwrap(shape), TopAbs_FACE); });
    }
};

} // namespace

std::unique_ptr<Kernel> createKernel() { return std::make_unique<OcctKernel>(); }

} // namespace fuse::occt
