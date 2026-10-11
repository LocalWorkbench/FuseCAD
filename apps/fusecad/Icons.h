// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QString>

// FuseCAD's icon set, drawn procedurally with QPainter: no binary assets and
// crisp at any device pixel ratio. Glyph colours are part of each icon's
// design (a fixed palette in Icons.cpp), not theme tokens.
// Unknown names fall back to a neutral placeholder glyph.
namespace icons {

QIcon get(const QString& name);

} // namespace icons
