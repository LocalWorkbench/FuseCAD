// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// High-level, fully parametric modelling commands. Each one appends ordinary
// features (a dimensioned sketch plus an extrude, or a dress-up feature), so
// the result is editable through parameters and survives save/load exactly
// like hand-built history. All sizes are length expressions ("width / 2", "1 in").
//
// Body references given as kLatestBody (the default) are resolved when the
// feature is created, to the body most recently created or modified before the
// timeline marker (Document::latestBody); the stored feature always names a
// concrete body. Builders throw age::Error before touching the document if
// an expression is invalid or there is no body to operate on.

#include "age/model/Document.h"
#include "age/model/Features.h"

#include <string>

namespace age::build {

constexpr int kLatestBody = -1;

struct Placement {
    std::string x = "0", y = "0"; // centre on the XY plane
    std::string z = "0";          // base height
};

struct BoxSpec {
    std::string width = "40 mm";  // X
    std::string depth = "30 mm";  // Y
    std::string height = "20 mm"; // Z (negative builds downwards)
    Placement at;
    BodyOperation op; // targetBody may be kLatestBody
};

struct CylinderSpec {
    std::string diameter = "20 mm";
    std::string height = "20 mm";
    Placement at;
    BodyOperation op; // targetBody may be kLatestBody
};

// Returns the extrude feature (its id is the new body's id for NewBody).
ExtrudeFeature& addBox(Document& doc, const BoxSpec& spec);
ExtrudeFeature& addCylinder(Document& doc, const CylinderSpec& spec);

FilletFeature& addFillet(Document& doc, EdgeQuery edges, std::string radius, int body = kLatestBody);
ChamferFeature& addChamfer(Document& doc, EdgeQuery edges, std::string distance, int body = kLatestBody);

} // namespace age::build
