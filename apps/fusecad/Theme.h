// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>

class QApplication;

// Central palette and stylesheet. Widgets, the viewport and the ViewCube
// reference these tokens instead of hard-coding colours, so a dark theme is a
// matter of swapping values. (The procedural icon set keeps its own palette;
// see Icons.h.)
namespace theme {

// Chrome.
inline const QColor kAccent{0x1f, 0x8f, 0xe0};       // selection, active tab, primary buttons
inline const QColor kAccentSoft{0xd6, 0xec, 0xfb};   // hover / selected backgrounds
inline const QColor kAccentBorder{0xb8, 0xdc, 0xf6}; // hovered button outline
inline const QColor kAccentPressed{0xbf, 0xe0, 0xf7};
inline const QColor kHover{0xee, 0xf6, 0xfd};  // hovered tree rows
inline const QColor kChrome{0xf5, 0xf6, 0xf7}; // ribbon, timeline, bars
inline const QColor kTab{0xe9, 0xeb, 0xee};    // inactive document tabs
inline const QColor kPanel{0xff, 0xff, 0xff};  // floating panels, dialogs, menus
inline const QColor kPanelAlt{0xf7, 0xf8, 0xf9};
inline const QColor kButton{0xfb, 0xfb, 0xfc};
inline const QColor kBorder{0xd5, 0xd8, 0xdc};
inline const QColor kText{0x2b, 0x2f, 0x36};
inline const QColor kTextMuted{0x7a, 0x80, 0x89};
inline const QColor kTextInverse{0xff, 0xff, 0xff}; // on accent or ink backgrounds
inline const QColor kInk{0x3d, 0x44, 0x4f};         // markers and handles drawn over content
inline const QColor kError{0xd9, 0x3f, 0x3f};
inline const QColor kShadow{0, 0, 0, 45};
inline const QColor kOverlayFill{0xff, 0xff, 0xff, 200}; // small controls drawn over the 3D view
inline const QColor kToast{0x2b, 0x2f, 0x36, 230};
constexpr int kPanelAlpha = 236; // floating panels are slightly see-through

// Viewport.
inline const QColor kSkyTop{0xfd, 0xfe, 0xff};
inline const QColor kSkyBottom{0xd9, 0xdf, 0xe6};
inline const QColor kGridMinor{0xc9, 0xd0, 0xd8};
inline const QColor kGridMajor{0xae, 0xb7, 0xc2};
inline const QColor kBody{0xa9, 0xb4, 0xc2};
inline const QColor kEdge{0x23, 0x27, 0x2e};
inline const QColor kSketch{0x2a, 0x6e, 0xd8};
inline const QColor kOriginPlane{0xf2, 0xb5, 0x3d, 56};
inline const QColor kAxisX{0xd9, 0x3f, 0x3f};
inline const QColor kAxisY{0x34, 0xa8, 0x53};
inline const QColor kAxisZ{0x2a, 0x6e, 0xd8};

// ViewCube.
inline const QColor kCubeFaceLight{0xfb, 0xfc, 0xfd};
inline const QColor kCubeFaceDark{0xe2, 0xe6, 0xeb};
inline const QColor kCubeOutline{0x9a, 0xa3, 0xae};
inline const QColor kCubeLabel{0x55, 0x5d, 0x68};
inline const QColor kCubeHome{0x6b, 0x72, 0x7c};

void apply(QApplication& app);

} // namespace theme
