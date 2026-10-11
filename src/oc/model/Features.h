// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/model/Feature.h"
#include "oc/sketch/Sketch.h"

#include <string>
#include <vector>

namespace oc {

enum class SketchPlane { XY, XZ, YZ };

class SketchFeature final : public FeatureBase<SketchFeature, FeatureType::Sketch> {
public:
    SketchPlane plane = SketchPlane::XY;
    std::string offset = "0"; // expression, along the plane normal
    sketch::Sketch sketch;

    void execute(RecomputeContext& ctx) override;

    // Results of the last execute().
    Plane resolvedPlane;
    std::vector<sketch::Profile> profiles;
    sketch::SolveResult solveResult;
};

// How a new tool body combines with existing geometry.
enum class FeatureOperation { NewBody, Join, Cut, Intersect };

const char* operationName(FeatureOperation op);
bool parseOperation(const std::string& name, FeatureOperation& out);

// Shared by features that sweep sketch profiles into solids.
struct ProfileSelection {
    int sketchId = 0;
    std::vector<int> profileIndices; // empty = every profile in the sketch
};

struct BodyOperation {
    FeatureOperation operation = FeatureOperation::NewBody;
    int targetBody = -1; // Join/Cut/Intersect; -1 = most recent body
};

class ExtrudeFeature final : public FeatureBase<ExtrudeFeature, FeatureType::Extrude> {
public:
    ProfileSelection profiles;
    std::string distance = "10 mm"; // negative extrudes the other way
    bool symmetric = false;         // split the distance evenly about the sketch plane
    BodyOperation op;

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
};

class RevolveFeature final : public FeatureBase<RevolveFeature, FeatureType::Revolve> {
public:
    ProfileSelection profiles;
    sketch::EntityId axisLine = -1; // a line in the same sketch
    std::string angle = "360 deg";
    BodyOperation op;

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
};

// Fillet and chamfer share their shape: edges of a body plus one size.
class FilletFeature final : public FeatureBase<FilletFeature, FeatureType::Fillet> {
public:
    int body = -1; // -1 = most recent body
    EdgeQuery edges;
    std::string radius = "1 mm";

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
};

class ChamferFeature final : public FeatureBase<ChamferFeature, FeatureType::Chamfer> {
public:
    int body = -1;
    EdgeQuery edges;
    std::string distance = "1 mm";

    void execute(RecomputeContext& ctx) override;
    std::vector<int> dependencies() const override;
};

class ImportStepFeature final : public FeatureBase<ImportStepFeature, FeatureType::ImportStep> {
public:
    std::string path;

    void execute(RecomputeContext& ctx) override;
};

} // namespace oc
