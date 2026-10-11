// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>

class QApplication;

// Central palette and stylesheet. Widgets reference these tokens instead of
// hard-coding colours, so a dark theme is a matter of swapping values.
namespace theme {

inline const QColor kAccent{0x1f, 0x8f, 0xe0};     // selection, active tab, primary buttons
inline const QColor kAccentSoft{0xd6, 0xec, 0xfb}; // hover / selected backgrounds
inline const QColor kChrome{0xf5, 0xf6, 0xf7};     // ribbon, timeline, bars
inline const QColor kPanel{0xff, 0xff, 0xff};      // floating panels
inline const QColor kBorder{0xd5, 0xd8, 0xdc};
inline const QColor kText{0x2b, 0x2f, 0x36};
inline const QColor kTextMuted{0x7a, 0x80, 0x89};
inline const QColor kWarning{0xe0, 0x8a, 0x1e};
inline const QColor kError{0xd9, 0x3f, 0x3f};

// Viewport.
inline const QColor kSkyTop{0xfd, 0xfe, 0xff};
inline const QColor kSkyBottom{0xd9, 0xdf, 0xe6};
inline const QColor kGridMinor{0xc9, 0xd0, 0xd8};
inline const QColor kGridMajor{0xae, 0xb7, 0xc2};
inline const QColor kBody{0xa9, 0xb4, 0xc2};
inline const QColor kEdge{0x23, 0x27, 0x2e};
inline const QColor kSketch{0x2a, 0x6e, 0xd8};

void apply(QApplication& app);

} // namespace theme
