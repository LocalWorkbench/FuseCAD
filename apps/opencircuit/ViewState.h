// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>

// Per-session display state that is not part of the design itself
// (per-item visibility toggles, display style, grid).
struct ViewState {
    enum class Style { Shaded, ShadedWithEdges, Wireframe };

    bool showBodies = true;
    bool showSketches = true;
    bool showOrigin = false;
    QSet<int> hiddenBodies;    // body ids
    QSet<int> visibleSketches; // sketch feature ids (sketches are hidden by default once consumed)
    Style style = Style::ShadedWithEdges;
    bool showGrid = true;

    bool bodyVisible(int id) const { return showBodies && !hiddenBodies.contains(id); }
    bool sketchVisible(int id) const { return showSketches && visibleSketches.contains(id); }
};
