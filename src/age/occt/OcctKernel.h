// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "age/kernel/Kernel.h"

#include <memory>

namespace age::occt {

// Geometry kernel backed by Open CASCADE Technology (LGPL-2.1).
std::unique_ptr<Kernel> createKernel();

} // namespace age::occt
