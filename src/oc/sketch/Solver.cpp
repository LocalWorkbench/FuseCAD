// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/sketch/Solver.h"

#include <algorithm>
#include <cmath>

namespace oc::sketch {

namespace {

using Matrix = std::vector<double>; // row-major

double maxAbs(const std::vector<double>& v) {
    double m = 0;
    for (double x : v) m = std::max(m, std::fabs(x));
    return m;
}

double sumSquares(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x * x;
    return s;
}

// Central-difference Jacobian, m x n row-major.
Matrix jacobian(const ResidualFn& f, std::vector<double> x, size_t m) {
    const size_t n = x.size();
    Matrix J(m * n);
    std::vector<double> fp, fm;
    for (size_t j = 0; j < n; ++j) {
        const double orig = x[j];
        const double h = 1e-7 * std::max(1.0, std::fabs(orig));
        x[j] = orig + h;
        f(x, fp);
        x[j] = orig - h;
        f(x, fm);
        x[j] = orig;
        for (size_t i = 0; i < m; ++i) J[i * n + j] = (fp[i] - fm[i]) / (2 * h);
    }
    return J;
}

// Solve (A) y = b in place for symmetric positive definite A (n x n).
bool cholesky(Matrix A, std::vector<double>& b, size_t n) {
    for (size_t j = 0; j < n; ++j) {
        double d = A[j * n + j];
        for (size_t k = 0; k < j; ++k) d -= A[j * n + k] * A[j * n + k];
        if (!(d > 0)) return false;
        d = std::sqrt(d);
        A[j * n + j] = d;
        for (size_t i = j + 1; i < n; ++i) {
            double s = A[i * n + j];
            for (size_t k = 0; k < j; ++k) s -= A[i * n + k] * A[j * n + k];
            A[i * n + j] = s / d;
        }
    }
    for (size_t i = 0; i < n; ++i) {
        double s = b[i];
        for (size_t k = 0; k < i; ++k) s -= A[i * n + k] * b[k];
        b[i] = s / A[i * n + i];
    }
    for (size_t i = n; i-- > 0;) {
        double s = b[i];
        for (size_t k = i + 1; k < n; ++k) s -= A[k * n + i] * b[k];
        b[i] = s / A[i * n + i];
    }
    return true;
}

// Reduced row echelon form of J (m x n). Returns rank and, per variable, whether
// it is orthogonal to the null space (i.e. uniquely determined).
int analyze(Matrix J, size_t m, size_t n, std::vector<bool>& determined) {
    const double scale = std::max(1.0, [&] {
        double s = 0;
        for (double v : J) s = std::max(s, std::fabs(v));
        return s;
    }());
    const double tol = 1e-8 * scale;

    std::vector<int> pivotCol;
    size_t row = 0;
    for (size_t col = 0; col < n && row < m; ++col) {
        size_t best = row;
        for (size_t i = row + 1; i < m; ++i)
            if (std::fabs(J[i * n + col]) > std::fabs(J[best * n + col])) best = i;
        if (std::fabs(J[best * n + col]) <= tol) continue;
        if (best != row)
            for (size_t k = 0; k < n; ++k) std::swap(J[row * n + k], J[best * n + k]);
        const double p = J[row * n + col];
        for (size_t k = 0; k < n; ++k) J[row * n + k] /= p;
        for (size_t i = 0; i < m; ++i) {
            if (i == row) continue;
            const double factor = J[i * n + col];
            if (factor == 0) continue;
            for (size_t k = 0; k < n; ++k) J[i * n + k] -= factor * J[row * n + k];
        }
        pivotCol.push_back(static_cast<int>(col));
        ++row;
    }

    // Null-space basis: one vector per free column f, with v_f = 1 and
    // v_pivot(r) = -R[r][f]. A pivot variable is determined iff all of those
    // components vanish; a free variable never is.
    determined.assign(n, false);
    std::vector<bool> isPivot(n, false);
    for (int c : pivotCol) isPivot[c] = true;
    for (size_t r = 0; r < pivotCol.size(); ++r) {
        bool det = true;
        for (size_t f = 0; f < n && det; ++f)
            if (!isPivot[f] && std::fabs(J[r * n + f]) > 1e-7) det = false;
        determined[pivotCol[r]] = det;
    }
    return static_cast<int>(pivotCol.size());
}

} // namespace

SolverOutput solveLeastSquares(const ResidualFn& f, std::vector<double>& x, double tolerance, int maxIterations) {
    SolverOutput out;
    const size_t n = x.size();
    std::vector<double> F;
    f(x, F);
    const size_t m = F.size();
    out.equations = static_cast<int>(m);

    if (m == 0 || n == 0) {
        out.converged = maxAbs(F) <= tolerance;
        out.residual = maxAbs(F);
        out.determined.assign(n, false);
        return out;
    }

    double cost = sumSquares(F);
    double lambda = 1e-3;
    std::vector<double> trial, Ftrial;

    for (; out.iterations < maxIterations; ++out.iterations) {
        if (maxAbs(F) <= tolerance) break;

        Matrix J = jacobian(f, x, m);
        Matrix A(n * n, 0.0);
        std::vector<double> g(n, 0.0);
        double maxDiag = 0;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i; j < n; ++j) {
                double s = 0;
                for (size_t k = 0; k < m; ++k) s += J[k * n + i] * J[k * n + j];
                A[i * n + j] = A[j * n + i] = s;
            }
            for (size_t k = 0; k < m; ++k) g[i] += J[k * n + i] * F[k];
            maxDiag = std::max(maxDiag, A[i * n + i]);
        }
        const double floor = 1e-10 * (1 + maxDiag);

        bool improved = false;
        while (lambda < 1e12) {
            Matrix M = A;
            for (size_t i = 0; i < n; ++i) M[i * n + i] += std::max(lambda, floor);
            std::vector<double> dx(g.size());
            for (size_t i = 0; i < n; ++i) dx[i] = -g[i];
            if (cholesky(std::move(M), dx, n)) {
                trial = x;
                for (size_t i = 0; i < n; ++i) trial[i] += dx[i];
                f(trial, Ftrial);
                double c = sumSquares(Ftrial);
                if (std::isfinite(c) && c < cost) {
                    x.swap(trial);
                    F.swap(Ftrial);
                    cost = c;
                    lambda = std::max(lambda / 3, 1e-15);
                    improved = true;
                    break;
                }
            }
            lambda *= 4;
        }
        if (!improved) break; // stuck in a local minimum with non-zero residual
    }

    out.residual = maxAbs(F);
    out.converged = out.residual <= tolerance;
    out.rank = analyze(jacobian(f, x, m), m, n, out.determined);
    return out;
}

} // namespace oc::sketch
