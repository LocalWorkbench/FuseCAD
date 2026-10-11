// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A geometry-free Kernel for testing the model layer in isolation. Each shape
// records the operation that produced it plus a nominal "volume", so tests can
// check that the timeline drives the kernel correctly without OCCT.

#include "fuse/core/Error.h"
#include "fuse/kernel/Kernel.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace fuse::testing {

class FakeShape final : public Shape {
public:
    FakeShape(std::string op, double v) : operation(std::move(op)), volume(v) {}
    std::string operation;
    double volume;
};

class FakeKernel final : public Kernel {
public:
    std::vector<std::string> log;

    std::string name() const override { return "fake"; }

    ShapePtr extrude(const Plane&, const std::vector<sketch::Profile>& profiles, double, double distance) override {
        double area = 0;
        for (const auto& p : profiles) area += p.area;
        return make("extrude", area * std::fabs(distance));
    }
    ShapePtr revolve(const Plane&, const std::vector<sketch::Profile>&, Vec2, Vec2, double) override {
        return make("revolve", 1);
    }
    ShapePtr boolean(BooleanOp op, const ShapePtr& a, const ShapePtr& b) override {
        double va = vol(a), vb = vol(b);
        switch (op) {
        case BooleanOp::Join:
            return make("join", va + vb);
        case BooleanOp::Cut:
            return make("cut", va - vb);
        case BooleanOp::Intersect:
            return make("intersect", std::min(va, vb));
        }
        return nullptr;
    }
    ShapePtr fillet(const ShapePtr& s, const EdgeQuery&, double) override { return make("fillet", vol(s)); }
    ShapePtr chamfer(const ShapePtr& s, const EdgeQuery&, double) override { return make("chamfer", vol(s)); }
    ShapePtr importStep(const std::string& path) override { throw Error("cannot import " + path); }
    void exportStep(const std::vector<ShapePtr>&, const std::string&) override {}
    void exportStl(const std::vector<ShapePtr>&, const std::string&, double) override {}
    Mesh tessellate(const ShapePtr&, double) override { return {}; }
    double volume(const ShapePtr& s) override { return vol(s); }
    double surfaceArea(const ShapePtr&) override { return 0; }
    BoundingBox boundingBox(const ShapePtr&) override { return {}; }
    bool isValid(const ShapePtr&) override { return true; }
    int countFaces(const ShapePtr&) override { return 0; }

private:
    ShapePtr make(const char* op, double v) {
        log.emplace_back(op);
        return std::make_shared<FakeShape>(op, v);
    }
    static double vol(const ShapePtr& s) { return dynamic_cast<const FakeShape&>(*s).volume; }
};

} // namespace fuse::testing
