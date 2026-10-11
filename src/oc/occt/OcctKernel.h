// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "oc/kernel/Kernel.h"

#include <memory>

namespace oc::occt {

// Geometry kernel backed by Open CASCADE Technology (LGPL-2.1).
std::unique_ptr<Kernel> createKernel();

} // namespace oc::occt
