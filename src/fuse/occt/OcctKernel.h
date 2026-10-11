// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/kernel/Kernel.h"

#include <memory>

namespace fuse::occt {

// Geometry kernel backed by Open CASCADE Technology (LGPL-2.1).
std::unique_ptr<Kernel> createKernel();

} // namespace fuse::occt
