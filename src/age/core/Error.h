// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdexcept>
#include <string>

namespace age {

// Base exception for all user-facing modelling errors (bad expressions,
// unsolvable sketches, failed kernel operations, ...).
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace age
