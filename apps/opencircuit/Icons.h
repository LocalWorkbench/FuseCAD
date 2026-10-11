// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QString>

// OpenCircuit's icon set, drawn procedurally with QPainter: no binary assets,
// crisp at any device pixel ratio, and trivially recoloured by the theme.
// Unknown names fall back to a neutral placeholder glyph.
namespace icons {

QIcon get(const QString& name);

} // namespace icons
