// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <vector>

namespace oc::sketch {

struct SolverOutput {
    bool converged = false;
    int iterations = 0;
    double residual = 0; // max |f_i|
    int equations = 0;
    int rank = 0;                 // rank of the Jacobian at the solution
    std::vector<bool> determined; // per variable: fixed by the constraints (not in the null space)
};

using ResidualFn = std::function<void(const std::vector<double>& x, std::vector<double>& f)>;

// Damped least-squares (Levenberg) solve of f(x) = 0, starting from and
// updating x. Steps stay in the row space of the Jacobian, so variables the
// constraints do not touch are left where they are -- this is what makes
// dragging an under-constrained sketch feel natural.
SolverOutput solveLeastSquares(const ResidualFn& f, std::vector<double>& x, double tolerance = 1e-9,
                               int maxIterations = 200);

} // namespace oc::sketch
