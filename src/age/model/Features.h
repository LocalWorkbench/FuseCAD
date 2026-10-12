// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "age/model/Feature.h"
#include "age/sketch/Sketch.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace age {

enum class SketchPlane { XY, XZ, YZ };

inline constexpr auto kSketchPlanes = makeEnumTable<EnumName<SketchPlane>>({
    {SketchPlane::XY, "XY"},
    {SketchPlane::XZ, "XZ"},
    {SketchPlane::YZ, "YZ"},
});
constexpr const auto& enumTable(SketchPlane) { return kSketchPlanes; }

class SketchFeature final : public FeatureBase<SketchFeature, FeatureType::Sketch> {
public:
    SketchPlane plane = SketchPlane::XY;
    std::string offset = "0"; // length expression, along the plane normal
    sketch::Sketch sketch;

    void execute(RecomputeContext& ctx) override;
    void visitExpressions(const ExpressionVisitor& fn) override;

    // Results of the last execute().
    Plane resolvedPlane;
    std::vector<sketch::Profile> profiles;
    sketch::SolveResult solveResult;
};

// How a new tool body combines with existing geometry.
enum class FeatureOperation { NewBody, Join, Cut, Intersect };

inline constexpr auto kFeatureOperations = makeEnumTable<EnumName<FeatureOperation>>({
    {FeatureOperation::NewBody, "new_body"},
    {FeatureOperation::Join, "join"},
    {FeatureOperation::Cut, "cut"},
    {FeatureOperation::Intersect, "intersect"},
});
constexpr const auto& enumTable(FeatureOperation) { return kFeatureOperations; }

const char* operationName(FeatureOperation op);
std::optional<FeatureOperation> parseOperation(std::string_view name);

// Shared by features that sweep sketch profiles into solids.
struct ProfileSelection {
    int sketchId = 0;
    std::vector<int> profileIndices; // empty = every profile in the sketch
};

struct BodyOperation {
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1; // Join/Cut/Intersect: id of the body to modify (required)
};

class ExtrudeFeature final : public FeatureBase<ExtrudeFeature, FeatureType::Extrude> {
public:
    ProfileSelection profiles;
    std::string distance = "10 mm"; // length; negative extrudes the other way
    bool symmetric = false;         // split the distance evenly about the sketch plane
    BodyOperation op;

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
    void visitExpressions(const ExpressionVisitor& fn) override { fn(distance); }
    int outputBody() const override;
};

class RevolveFeature final : public FeatureBase<RevolveFeature, FeatureType::Revolve> {
public:
    ProfileSelection profiles;
    sketch::EntityId axisLine = -1; // a line in the same sketch
    std::string angle = "360 deg";
    BodyOperation op;

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
    void visitExpressions(const ExpressionVisitor& fn) override { fn(angle); }
    int outputBody() const override;
};

// Fillet and chamfer: modify selected edges of one body by a single length.
class DressUpFeature : public Feature {
public:
    int body = -1; // id of the body to modify (required)
    EdgeQuery edges;
    std::string size = "1 mm"; // fillet radius / chamfer distance

    // What `size` means for this feature ("radius", "distance"); also its file format key.
    virtual const char* sizeName() const = 0;

    void execute(RecomputeContext& ctx) final;
    std::vector<int> dependencies() const final { return {body}; } // body ids are creator feature ids
    void visitExpressions(const ExpressionVisitor& fn) final { fn(size); }
    int outputBody() const final { return body; }

protected:
    DressUpFeature() = default;
    virtual ShapePtr apply(Kernel& kernel, const ShapePtr& shape, double size) const = 0;
};

class FilletFeature final : public FeatureBase<FilletFeature, FeatureType::Fillet, DressUpFeature> {
public:
    const char* sizeName() const override { return "radius"; }

protected:
    ShapePtr apply(Kernel& kernel, const ShapePtr& shape, double size) const override {
        return kernel.fillet(shape, edges, size);
    }
};

class ChamferFeature final : public FeatureBase<ChamferFeature, FeatureType::Chamfer, DressUpFeature> {
public:
    const char* sizeName() const override { return "distance"; }

protected:
    ShapePtr apply(Kernel& kernel, const ShapePtr& shape, double size) const override {
        return kernel.chamfer(shape, edges, size);
    }
};

class ImportStepFeature final : public FeatureBase<ImportStepFeature, FeatureType::ImportStep> {
public:
    std::string path;

    void execute(RecomputeContext& ctx) override;
    void visitExpressions(const ExpressionVisitor&) override {}
    int outputBody() const override { return id; }
};

} // namespace age
