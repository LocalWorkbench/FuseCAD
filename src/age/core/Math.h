// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <numbers>

namespace age {

constexpr double kPi = std::numbers::pi;

struct Vec2 {
    double x = 0, y = 0;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(double s) const { return {x / s, y / s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr bool operator==(const Vec2&) const = default;

    double length() const { return std::hypot(x, y); }
    Vec2 normalized() const {
        double l = length();
        return l > 0 ? *this / l : Vec2{};
    }
    // Rotate +90 degrees (counter-clockwise).
    constexpr Vec2 perp() const { return {-y, x}; }
};

constexpr double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

struct Vec3 {
    double x = 0, y = 0, z = 0;

    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr bool operator==(const Vec3&) const = default;

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        double l = length();
        return l > 0 ? *this / l : Vec3{};
    }
};

constexpr double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

// A right-handed planar coordinate frame. Sketches live in a Plane; 2D sketch
// coordinates (u, v) map to origin + u * xDir + v * yDir.
struct Plane {
    Vec3 origin{0, 0, 0};
    Vec3 xDir{1, 0, 0};
    Vec3 yDir{0, 1, 0};

    Vec3 normal() const { return cross(xDir, yDir).normalized(); }
    Vec3 toWorld(Vec2 p) const { return origin + xDir * p.x + yDir * p.y; }
    Vec3 dirToWorld(Vec2 d) const { return xDir * d.x + yDir * d.y; }

    static Plane XY(double offset = 0) { return {{0, 0, offset}, {1, 0, 0}, {0, 1, 0}}; }
    static Plane XZ(double offset = 0) { return {{0, offset, 0}, {1, 0, 0}, {0, 0, 1}}; }
    static Plane YZ(double offset = 0) { return {{offset, 0, 0}, {0, 1, 0}, {0, 0, 1}}; }
};

// Wrap an angle to (-pi, pi].
inline double wrapAngle(double a) {
    a = std::fmod(a + kPi, 2 * kPi);
    if (a <= 0) a += 2 * kPi;
    return a - kPi;
}

} // namespace age
