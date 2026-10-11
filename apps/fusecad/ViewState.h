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
    QSet<int> visibleSketches; // sketch feature ids (sketches start hidden until toggled on)
    Style style = Style::ShadedWithEdges;
    bool showGrid = true;

    // An item's own toggle, ignoring whether its folder is shown.
    bool bodyToggledOn(int id) const { return !hiddenBodies.contains(id); }
    bool sketchToggledOn(int id) const { return visibleSketches.contains(id); }
    // Whether the item is actually drawn.
    bool bodyVisible(int id) const { return showBodies && bodyToggledOn(id); }
    bool sketchVisible(int id) const { return showSketches && sketchToggledOn(id); }
};
