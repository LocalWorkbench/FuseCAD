// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMatrix4x4>
#include <QRect>

class QPainter;

// Orientation widget drawn in the viewport corner. Faces are labelled and
// clickable; clicking one snaps the camera to that standard view.
class ViewCube {
public:
    enum Target { None = -1, Top, Bottom, Front, Back, Right, Left, Home };

    struct Orientation {
        float yawDeg = 0;
        float pitchDeg = 0;
    };
    // The default three-quarter view; also the viewport's initial camera.
    static constexpr Orientation kHomeOrientation{-60.0f, 30.0f};

    // `area` is the square the cube is drawn into; `view` the camera's view matrix.
    void paint(QPainter& p, const QRect& area, const QMatrix4x4& view, Target hover) const;
    Target hitTest(const QPoint& pos, const QRect& area, const QMatrix4x4& view) const;

    static Orientation orientation(Target t);
};
