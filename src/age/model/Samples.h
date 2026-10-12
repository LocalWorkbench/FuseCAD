// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "age/model/Document.h"

namespace age::samples {

// A filleted mounting plate with a centred through-hole, fully driven by the
// user parameters width, height, thickness, corner_r and hole_d. Exercises
// sketch dimensions bound to parameters, new-body/cut extrudes and fillets.
Document mountingPlate();

// A revolved hub-and-flange with a bore and four bolt holes on a bolt circle.
// Parameters: flange_d, flange_t, hub_d, hub_h, bore_d, bolt_circle, bolt_d.
Document flange();

// A pillow-block bearing housing: filleted base with counterbored bolt holes,
// a tapered web up to a horizontal boss with a chamfered bore, and a drilled
// lubrication boss on top. Edge selections for the fillets and chamfers are
// placed for the default parameters (see EdgeQuery).
Document pillowBlock();

} // namespace age::samples
