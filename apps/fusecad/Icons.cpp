// SPDX-License-Identifier: GPL-3.0-or-later
#include "Icons.h"

#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>

#include <cmath>
#include <functional>

namespace icons {

namespace {

// All glyphs are drawn on a 64 x 64 canvas.
using Painter = std::function<void(QPainter&)>;

const QColor kSolid(0x6f, 0xa6, 0xde);  // body blue
const QColor kOrange(0xf2, 0x93, 0x2c); // "affected geometry" highlight
const QColor kGreen(0x34, 0xa8, 0x53);  // create / add
const QColor kAmber(0xf0, 0xb4, 0x3c);  // construction geometry
const QColor kRed(0xd9, 0x3f, 0x3f);
const QColor kInk(0x3d, 0x44, 0x4f); // line work
const QColor kPaper(0xff, 0xff, 0xff);
const QColor kGray(0x9a, 0xa3, 0xae);

void poly(QPainter& p, std::initializer_list<QPointF> pts, const QColor& fill, const QColor& line) {
    QPolygonF poly(pts);
    p.setPen(QPen(line, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(fill);
    p.drawPolygon(poly);
}

// Isometric box: (cx, top) is the far corner of the top face.
void isoBox(QPainter& p, double cx, double top, double w, double h, QColor base = kSolid) {
    const double q = w * 0.28;
    const QPointF T(cx, top), R(cx + w / 2, top + q), B(cx, top + 2 * q), L(cx - w / 2, top + q);
    const QPointF dh(0, h);
    const QColor line = base.darker(190);
    poly(p, {L, B, B + dh, L + dh}, base, line);
    poly(p, {B, R, R + dh, B + dh}, base.darker(135), line);
    poly(p, {T, R, B, L}, base.lighter(135), line);
}

void cylinder(QPainter& p, double cx, double top, double w, double h, QColor base = kSolid) {
    const double rx = w / 2, ry = w * 0.2;
    QLinearGradient g(cx - rx, 0, cx + rx, 0);
    g.setColorAt(0, base.lighter(115));
    g.setColorAt(0.55, base);
    g.setColorAt(1, base.darker(150));
    QPainterPath body;
    body.moveTo(cx - rx, top + ry);
    body.lineTo(cx - rx, top + ry + h);
    body.arcTo(QRectF(cx - rx, top + h, w, 2 * ry), 180, 180);
    body.lineTo(cx + rx, top + ry);
    body.closeSubpath();
    p.setPen(QPen(base.darker(190), 1.6));
    p.setBrush(g);
    p.drawPath(body);
    p.setBrush(base.lighter(140));
    p.drawEllipse(QRectF(cx - rx, top, w, 2 * ry));
}

void sphere(QPainter& p, double cx, double cy, double r, QColor base = kSolid) {
    QRadialGradient g(cx - r * 0.35, cy - r * 0.4, r * 1.4);
    g.setColorAt(0, base.lighter(160));
    g.setColorAt(0.5, base);
    g.setColorAt(1, base.darker(170));
    p.setPen(QPen(base.darker(190), 1.4));
    p.setBrush(g);
    p.drawEllipse(QPointF(cx, cy), r, r);
}

void arrow(QPainter& p, QPointF from, QPointF to, const QColor& c, double width = 3.2) {
    p.setPen(QPen(c, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(from, to);
    const QPointF d = to - from;
    const double len = std::hypot(d.x(), d.y());
    if (len <= 0) return;
    const QPointF u = d / len, n(-u.y(), u.x());
    const double s = width * 2.6;
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(QPolygonF({to + u * s * 0.6, to - u * s + n * s * 0.75, to - u * s - n * s * 0.75}));
}

void plusBadge(QPainter& p, double cx, double cy, const QColor& c = kGreen) {
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(cx, cy), 10, 10);
    p.setPen(QPen(Qt::white, 3.2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cx - 5, cy), QPointF(cx + 5, cy));
    p.drawLine(QPointF(cx, cy - 5), QPointF(cx, cy + 5));
}

void plane(QPainter& p, QPointF a, QPointF b, QPointF c, QPointF d, QColor base = kAmber) {
    QColor fill = base;
    fill.setAlpha(200);
    poly(p, {a, b, c, d}, fill, base.darker(160));
}

void stroke(QPainter& p, const QColor& c, double w = 3.0) {
    p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
}

void eye(QPainter& p, bool open) {
    stroke(p, open ? kInk : kGray, 3.4);
    QPainterPath lid;
    lid.moveTo(8, 32);
    lid.quadTo(32, 8, 56, 32);
    lid.quadTo(32, 56, 8, 32);
    p.drawPath(lid);
    if (open) {
        p.setBrush(kInk);
        p.drawEllipse(QPointF(32, 32), 7, 7);
    } else {
        p.drawLine(QPointF(12, 52), QPointF(52, 12));
    }
}

void picture(QPainter& p) {
    p.setPen(QPen(kInk, 2));
    p.setBrush(kPaper);
    p.drawRoundedRect(QRectF(8, 12, 48, 40), 4, 4);
    poly(p, {QPointF(12, 48), QPointF(26, 28), QPointF(36, 40), QPointF(42, 33), QPointF(52, 48)},
         QColor(0x4c, 0xaf, 0x6e), QColor(0x2e, 0x7d, 0x4a));
    p.setPen(Qt::NoPen);
    p.setBrush(kAmber);
    p.drawEllipse(QPointF(44, 22), 5, 5);
}

const QHash<QString, Painter>& painters() {
    static const QHash<QString, Painter> table = {
        // --- Create --------------------------------------------------------
        {"sketch",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawRoundedRect(QRectF(8, 10, 40, 40), 3, 3);
             stroke(p, QColor(0x2a, 0x6e, 0xd8), 2.6);
             p.drawRect(QRectF(15, 26, 18, 16));
             p.drawEllipse(QPointF(33, 22), 7, 7);
             plusBadge(p, 48, 48);
         }},
        {"extrude",
         [](QPainter& p) {
             isoBox(p, 32, 30, 44, 12);
             arrow(p, QPointF(32, 34), QPointF(32, 6), kOrange);
         }},
        {"revolve",
         [](QPainter& p) {
             cylinder(p, 32, 18, 34, 28);
             stroke(p, kOrange, 3.2);
             p.drawArc(QRectF(6, 30, 52, 22), 200 * 16, 220 * 16);
             arrow(p, QPointF(50, 36), QPointF(54, 41), kOrange);
         }},
        {"sweep",
         [](QPainter& p) {
             QPainterPath path;
             path.moveTo(10, 50);
             path.cubicTo(14, 18, 44, 44, 54, 14);
             p.setPen(QPen(kSolid.darker(150), 12, Qt::SolidLine, Qt::RoundCap));
             p.drawPath(path);
             p.setPen(QPen(kSolid.lighter(130), 7, Qt::SolidLine, Qt::RoundCap));
             p.drawPath(path);
         }},
        {"loft",
         [](QPainter& p) {
             poly(p, {QPointF(14, 18), QPointF(50, 18), QPointF(42, 50), QPointF(22, 50)}, kSolid, kSolid.darker(190));
             p.setPen(QPen(kOrange, 2.6));
             p.setBrush(kSolid.lighter(140));
             p.drawEllipse(QRectF(14, 12, 36, 12));
             p.drawEllipse(QRectF(22, 46, 20, 8));
         }},
        {"hole",
         [](QPainter& p) {
             isoBox(p, 32, 14, 48, 20);
             p.setPen(QPen(kSolid.darker(220), 1.6));
             p.setBrush(QColor(0x2a, 0x34, 0x42));
             p.drawEllipse(QRectF(22, 21, 20, 11));
         }},
        {"pattern",
         [](QPainter& p) {
             isoBox(p, 20, 8, 20, 8);
             isoBox(p, 44, 8, 20, 8);
             isoBox(p, 20, 34, 20, 8);
             isoBox(p, 44, 34, 20, 8, kOrange);
         }},
        {"mirror",
         [](QPainter& p) {
             poly(p, {QPointF(10, 50), QPointF(28, 14), QPointF(28, 50)}, kSolid, kSolid.darker(190));
             poly(p, {QPointF(54, 50), QPointF(36, 14), QPointF(36, 50)}, kOrange, kOrange.darker(160));
             p.setPen(QPen(kInk, 2, Qt::DashLine));
             p.drawLine(QPointF(32, 8), QPointF(32, 56));
         }},
        {"box", [](QPainter& p) { isoBox(p, 32, 8, 46, 26); }},
        {"cylinder", [](QPainter& p) { cylinder(p, 32, 8, 38, 36); }},
        {"sphere", [](QPainter& p) { sphere(p, 32, 32, 22); }},
        {"torus",
         [](QPainter& p) {
             p.setPen(QPen(kSolid.darker(160), 13));
             p.setBrush(Qt::NoBrush);
             p.drawEllipse(QRectF(10, 20, 44, 26));
             p.setPen(QPen(kSolid.lighter(125), 8));
             p.drawEllipse(QRectF(10, 20, 44, 26));
         }},
        {"coil",
         [](QPainter& p) {
             stroke(p, kSolid.darker(140), 5);
             for (int i = 0; i < 4; ++i) p.drawArc(QRectF(12, 8 + i * 11, 40, 14), 0, -180 * 16);
             stroke(p, kSolid.lighter(120), 5);
             for (int i = 0; i < 4; ++i) p.drawArc(QRectF(12, 8 + i * 11, 40, 14), 0, 180 * 16);
         }},
        {"thread",
         [](QPainter& p) {
             cylinder(p, 32, 6, 30, 44, kGray);
             stroke(p, kInk, 2);
             for (int i = 0; i < 6; ++i) p.drawLine(QPointF(17, 18 + i * 6), QPointF(47, 14 + i * 6));
         }},
        {"rib",
         [](QPainter& p) {
             isoBox(p, 32, 34, 48, 10);
             poly(p, {QPointF(26, 14), QPointF(38, 20), QPointF(38, 42), QPointF(26, 36)}, kOrange,
                  kOrange.darker(160));
         }},
        {"emboss",
         [](QPainter& p) {
             isoBox(p, 32, 20, 48, 16);
             p.setPen(QPen(kOrange.darker(130), 1));
             p.setBrush(kOrange);
             QFont f("Helvetica", 18, QFont::Black);
             p.setFont(f);
             p.drawText(QRectF(14, 18, 36, 24), Qt::AlignCenter, "A");
         }},

        // --- Modify --------------------------------------------------------
        {"presspull",
         [](QPainter& p) {
             isoBox(p, 30, 24, 40, 18);
             arrow(p, QPointF(50, 30), QPointF(50, 6), kOrange, 3);
             arrow(p, QPointF(50, 34), QPointF(50, 58), kOrange, 3);
         }},
        {"fillet",
         [](QPainter& p) {
             isoBox(p, 32, 10, 46, 26);
             stroke(p, kOrange, 5);
             p.drawLine(QPointF(32, 37), QPointF(32, 62));
         }},
        {"chamfer",
         [](QPainter& p) {
             isoBox(p, 32, 10, 46, 26);
             poly(p, {QPointF(26, 34), QPointF(38, 34), QPointF(38, 60), QPointF(26, 60)}, kOrange,
                  kOrange.darker(150));
         }},
        {"shell",
         [](QPainter& p) {
             isoBox(p, 32, 10, 46, 26);
             poly(p, {QPointF(32, 16), QPointF(47, 23), QPointF(32, 30), QPointF(17, 23)}, QColor(0x3a, 0x4a, 0x5e),
                  kSolid.darker(190));
         }},
        {"combine",
         [](QPainter& p) {
             isoBox(p, 24, 16, 30, 18);
             isoBox(p, 40, 26, 30, 18, kOrange);
         }},
        {"move",
         [](QPainter& p) {
             arrow(p, QPointF(32, 32), QPointF(32, 6), kInk, 3);
             arrow(p, QPointF(32, 32), QPointF(32, 58), kInk, 3);
             arrow(p, QPointF(32, 32), QPointF(6, 32), kInk, 3);
             arrow(p, QPointF(32, 32), QPointF(58, 32), kInk, 3);
         }},
        {"align",
         [](QPainter& p) {
             isoBox(p, 20, 30, 22, 14);
             isoBox(p, 46, 10, 22, 14, kOrange);
             arrow(p, QPointF(40, 40), QPointF(46, 30), kInk, 2.4);
         }},
        {"split",
         [](QPainter& p) {
             isoBox(p, 32, 10, 46, 26);
             p.setPen(QPen(kRed, 2.6, Qt::DashLine));
             p.drawLine(QPointF(10, 22), QPointF(54, 46));
         }},
        {"scale",
         [](QPainter& p) {
             isoBox(p, 24, 26, 24, 14);
             p.setPen(QPen(kInk, 1.6, Qt::DashLine));
             p.setBrush(Qt::NoBrush);
             p.drawRect(QRectF(8, 8, 48, 48));
             arrow(p, QPointF(38, 26), QPointF(54, 10), kOrange, 2.6);
         }},
        {"delete",
         [](QPainter& p) {
             stroke(p, kRed, 6);
             p.drawLine(QPointF(16, 16), QPointF(48, 48));
             p.drawLine(QPointF(48, 16), QPointF(16, 48));
         }},
        {"parameters",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawRoundedRect(QRectF(6, 10, 52, 44), 6, 6);
             QFont f("Times New Roman", 24, QFont::Bold);
             f.setItalic(true);
             p.setFont(f);
             p.setPen(QColor(0x2a, 0x6e, 0xd8));
             p.drawText(QRectF(6, 10, 52, 44), Qt::AlignCenter, "fx");
         }},
        {"appearance",
         [](QPainter& p) {
             QConicalGradient g(32, 32, 0);
             g.setColorAt(0, QColor(0xe9, 0x4b, 0x3c));
             g.setColorAt(0.33, QColor(0xf2, 0xc1, 0x2e));
             g.setColorAt(0.66, QColor(0x2f, 0x9e, 0x5b));
             g.setColorAt(1, QColor(0xe9, 0x4b, 0x3c));
             p.setPen(QPen(kInk, 1.4));
             p.setBrush(g);
             p.drawEllipse(QPointF(32, 32), 22, 22);
             p.setBrush(QColor(255, 255, 255, 120));
             p.setPen(Qt::NoPen);
             p.drawEllipse(QPointF(24, 24), 7, 7);
         }},
        {"material", [](QPainter& p) { sphere(p, 32, 32, 22, QColor(0xb8, 0xbe, 0xc6)); }},
        {"compute",
         [](QPainter& p) {
             stroke(p, kGreen, 5);
             p.drawArc(QRectF(12, 12, 40, 40), 30 * 16, 290 * 16);
             arrow(p, QPointF(46, 16), QPointF(50, 22), kGreen, 4);
         }},

        // --- Assemble ------------------------------------------------------
        {"component",
         [](QPainter& p) {
             isoBox(p, 30, 14, 40, 22, kGray);
             plusBadge(p, 48, 48);
         }},
        {"joint",
         [](QPainter& p) {
             isoBox(p, 22, 10, 26, 16);
             isoBox(p, 42, 30, 26, 16, kOrange);
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawEllipse(QPointF(32, 32), 7, 7);
         }},
        {"motion",
         [](QPainter& p) {
             stroke(p, kInk, 3);
             p.drawArc(QRectF(10, 10, 44, 44), 30 * 16, 120 * 16);
             arrow(p, QPointF(17, 22), QPointF(13, 28), kInk, 3);
             p.drawArc(QRectF(10, 10, 44, 44), 210 * 16, 120 * 16);
             arrow(p, QPointF(47, 42), QPointF(51, 36), kInk, 3);
         }},

        // --- Construct (amber planes) --------------------------------------
        {"plane-offset",
         [](QPainter& p) {
             plane(p, QPointF(8, 44), QPointF(36, 52), QPointF(56, 40), QPointF(28, 32), kGray);
             plane(p, QPointF(8, 24), QPointF(36, 32), QPointF(56, 20), QPointF(28, 12));
             arrow(p, QPointF(32, 42), QPointF(32, 24), kInk, 2.4);
         }},
        {"plane-angle",
         [](QPainter& p) {
             plane(p, QPointF(8, 46), QPointF(36, 54), QPointF(56, 42), QPointF(28, 34), kGray);
             plane(p, QPointF(18, 48), QPointF(26, 12), QPointF(46, 8), QPointF(40, 44));
         }},
        {"midplane",
         [](QPainter& p) {
             plane(p, QPointF(8, 14), QPointF(8, 50), QPointF(20, 56), QPointF(20, 20), kGray);
             plane(p, QPointF(44, 14), QPointF(44, 50), QPointF(56, 56), QPointF(56, 20), kGray);
             plane(p, QPointF(26, 10), QPointF(26, 48), QPointF(38, 54), QPointF(38, 16));
         }},
        {"axis",
         [](QPainter& p) {
             cylinder(p, 32, 18, 26, 26, kGray);
             p.setPen(QPen(kAmber.darker(120), 3.4, Qt::DashDotLine));
             p.drawLine(QPointF(32, 4), QPointF(32, 62));
         }},
        {"point",
         [](QPainter& p) {
             stroke(p, kGray, 2);
             p.drawLine(QPointF(12, 32), QPointF(52, 32));
             p.drawLine(QPointF(32, 12), QPointF(32, 52));
             p.setPen(QPen(kAmber.darker(160), 2));
             p.setBrush(kAmber);
             p.drawEllipse(QPointF(32, 32), 9, 9);
         }},

        // --- Inspect / Insert / Select ------------------------------------
        {"measure",
         [](QPainter& p) {
             p.save();
             p.translate(32, 32);
             p.rotate(-30);
             p.setPen(QPen(QColor(0x9a, 0x6b, 0x12), 1.6));
             p.setBrush(QColor(0xf5, 0xc8, 0x4a));
             p.drawRect(QRectF(-26, -8, 52, 16));
             for (int i = -22; i <= 22; i += 6) p.drawLine(QPointF(i, -8), QPointF(i, i % 12 == 2 ? 2 : -2));
             p.restore();
         }},
        {"interference",
         [](QPainter& p) {
             isoBox(p, 24, 12, 30, 20);
             isoBox(p, 40, 22, 30, 20, kGray);
             p.setPen(Qt::NoPen);
             p.setBrush(QColor(0xd9, 0x3f, 0x3f, 190));
             p.drawEllipse(QPointF(32, 34), 8, 8);
         }},
        {"section",
         [](QPainter& p) {
             isoBox(p, 26, 12, 34, 26);
             plane(p, QPointF(32, 4), QPointF(56, 16), QPointF(56, 60), QPointF(32, 48), QColor(0x5d, 0xb2, 0xf0));
         }},
        {"zebra",
         [](QPainter& p) {
             sphere(p, 32, 32, 22, QColor(0xe8, 0xea, 0xee));
             p.save();
             QPainterPath clip;
             clip.addEllipse(QPointF(32, 32), 21, 21);
             p.setClipPath(clip);
             p.setPen(QPen(kInk, 4));
             for (int i = -2; i <= 2; ++i) p.drawLine(QPointF(4, 32 + i * 10 - 8), QPointF(60, 32 + i * 10 + 8));
             p.restore();
         }},
        {"decal",
         [](QPainter& p) {
             picture(p);
             poly(p, {QPointF(44, 52), QPointF(56, 40), QPointF(56, 52)}, QColor(0xdd, 0xe2, 0xe8), kInk);
         }},
        {"canvas",
         [](QPainter& p) {
             p.setPen(QPen(QColor(0x8a, 0x5a, 0x2b), 4));
             p.setBrush(Qt::NoBrush);
             p.drawRect(QRectF(8, 10, 48, 44));
             picture(p);
         }},
        {"insert-mesh",
         [](QPainter& p) {
             stroke(p, kSolid.darker(150), 2);
             const QPointF a(10, 50), b(32, 8), c(54, 50), d(32, 36);
             p.setBrush(kSolid.lighter(120));
             p.drawPolygon(QPolygonF({a, b, c}));
             p.drawLine(a, d);
             p.drawLine(b, d);
             p.drawLine(c, d);
         }},
        {"import",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawPolygon(
                 QPolygonF({QPointF(14, 6), QPointF(40, 6), QPointF(50, 16), QPointF(50, 58), QPointF(14, 58)}));
             QFont f("Helvetica", 10, QFont::Bold);
             p.setFont(f);
             p.setPen(kSolid.darker(150));
             p.drawText(QRectF(14, 20, 36, 14), Qt::AlignCenter, "STEP");
             arrow(p, QPointF(32, 36), QPointF(32, 54), kGreen, 3.4);
         }},
        {"export",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawPolygon(
                 QPolygonF({QPointF(14, 6), QPointF(40, 6), QPointF(50, 16), QPointF(50, 58), QPointF(14, 58)}));
             arrow(p, QPointF(32, 50), QPointF(32, 24), kOrange, 3.4);
         }},
        {"select",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2, Qt::DashLine));
             p.setBrush(QColor(0x1f, 0x8f, 0xe0, 40));
             p.drawRect(QRectF(6, 6, 40, 34));
             poly(p,
                  {QPointF(30, 24), QPointF(54, 44), QPointF(43, 45), QPointF(49, 58), QPointF(44, 60), QPointF(38, 47),
                   QPointF(30, 54)},
                  kPaper, kInk);
         }},

        // --- Surface / Mesh / Sheet metal / Plastic / Utilities ----------
        {"patch",
         [](QPainter& p) {
             QPainterPath s;
             s.moveTo(8, 40);
             s.cubicTo(20, 20, 40, 52, 56, 24);
             s.lineTo(56, 44);
             s.cubicTo(40, 64, 20, 36, 8, 56);
             s.closeSubpath();
             p.setPen(QPen(kSolid.darker(170), 1.6));
             p.setBrush(kSolid.lighter(120));
             p.drawPath(s);
         }},
        {"trim",
         [](QPainter& p) {
             icons::get("patch").paint(&p, QRect(0, 0, 64, 64));
             p.setPen(QPen(kRed, 3, Qt::DashLine));
             p.drawLine(QPointF(34, 10), QPointF(30, 60));
         }},
        {"stitch",
         [](QPainter& p) {
             poly(p, {QPointF(6, 20), QPointF(30, 14), QPointF(30, 50), QPointF(6, 56)}, kSolid.lighter(120),
                  kSolid.darker(170));
             poly(p, {QPointF(34, 14), QPointF(58, 20), QPointF(58, 56), QPointF(34, 50)}, kSolid, kSolid.darker(170));
             stroke(p, kOrange, 2.6);
             for (int y = 18; y < 50; y += 8) p.drawLine(QPointF(27, y), QPointF(37, y + 4));
         }},
        {"mesh",
         [](QPainter& p) {
             sphere(p, 32, 32, 22, QColor(0x9b, 0xc4, 0x8a));
             stroke(p, QColor(0x3f, 0x6b, 0x33), 1.4);
             p.drawEllipse(QRectF(10, 24, 44, 16));
             p.drawEllipse(QRectF(24, 10, 16, 44));
             p.drawLine(QPointF(14, 20), QPointF(50, 44));
             p.drawLine(QPointF(14, 44), QPointF(50, 20));
         }},
        {"flange-sm",
         [](QPainter& p) {
             poly(p, {QPointF(8, 40), QPointF(36, 52), QPointF(56, 42), QPointF(28, 30)}, kGray.lighter(115), kInk);
             poly(p, {QPointF(28, 30), QPointF(56, 42), QPointF(56, 12), QPointF(28, 2)}, kOrange, kOrange.darker(160));
         }},
        {"print3d",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2));
             p.setBrush(QColor(0xe6, 0xe9, 0xed));
             p.drawRoundedRect(QRectF(8, 6, 48, 52), 4, 4);
             p.setBrush(kPaper);
             p.drawRect(QRectF(14, 14, 36, 34));
             isoBox(p, 32, 30, 18, 8, kOrange);
             p.setBrush(kInk);
             p.drawRect(QRectF(28, 14, 8, 6));
         }},
        {"scripts",
         [](QPainter& p) {
             stroke(p, QColor(0x2a, 0x6e, 0xd8), 4.4);
             p.drawPolyline(QPolygonF({QPointF(22, 16), QPointF(8, 32), QPointF(22, 48)}));
             p.drawPolyline(QPolygonF({QPointF(42, 16), QPointF(56, 32), QPointF(42, 48)}));
             stroke(p, kInk, 4.4);
             p.drawLine(QPointF(36, 12), QPointF(28, 52));
         }},
        {"settings",
         [](QPainter& p) {
             p.save();
             p.translate(32, 32);
             p.setPen(Qt::NoPen);
             p.setBrush(kInk);
             for (int i = 0; i < 8; ++i) {
                 p.drawRoundedRect(QRectF(-5, -26, 10, 14), 2, 2);
                 p.rotate(45);
             }
             p.drawEllipse(QPointF(0, 0), 18, 18);
             p.setBrush(kPaper);
             p.drawEllipse(QPointF(0, 0), 7, 7);
             p.restore();
         }},

        // --- Navigation bar ------------------------------------------------
        {"orbit",
         [](QPainter& p) {
             stroke(p, kInk, 3.2);
             p.drawEllipse(QRectF(8, 20, 48, 24));
             arrow(p, QPointF(46, 21), QPointF(52, 24), kInk, 3.2);
             p.setBrush(kInk);
             p.drawEllipse(QPointF(32, 32), 5, 5);
         }},
        {"pan",
         [](QPainter& p) {
             QPainterPath hand;
             hand.addRoundedRect(QRectF(16, 28, 32, 28), 10, 10);
             for (int i = 0; i < 4; ++i)
                 hand.addRoundedRect(QRectF(17 + i * 8, 10 + (i == 0 || i == 3 ? 6 : 0), 7, 26), 3.5, 3.5);
             p.setPen(QPen(kInk, 2.6));
             p.setBrush(kPaper);
             p.drawPath(hand.simplified());
         }},
        {"zoom",
         [](QPainter& p) {
             stroke(p, kInk, 4);
             p.drawEllipse(QPointF(27, 27), 15, 15);
             p.drawLine(QPointF(38, 38), QPointF(54, 54));
             p.drawLine(QPointF(20, 27), QPointF(34, 27));
             p.drawLine(QPointF(27, 20), QPointF(27, 34));
         }},
        {"fit",
         [](QPainter& p) {
             stroke(p, kInk, 4);
             p.drawEllipse(QPointF(27, 27), 15, 15);
             p.drawLine(QPointF(38, 38), QPointF(54, 54));
             stroke(p, kInk, 2.4);
             p.drawRect(QRectF(20, 21, 14, 12));
         }},
        {"lookat",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.4));
             p.setBrush(kPaper);
             p.drawRoundedRect(QRectF(6, 22, 34, 26), 4, 4);
             poly(p, {QPointF(40, 30), QPointF(56, 20), QPointF(56, 50), QPointF(40, 40)}, kPaper, kInk);
         }},
        {"display",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.6));
             p.setBrush(QColor(0xdd, 0xec, 0xfa));
             p.drawRoundedRect(QRectF(6, 10, 52, 34), 3, 3);
             p.drawLine(QPointF(32, 44), QPointF(32, 52));
             p.drawLine(QPointF(20, 54), QPointF(44, 54));
             isoBox(p, 32, 16, 18, 10);
         }},
        {"grid",
         [](QPainter& p) {
             stroke(p, kInk, 2.2);
             for (int i = 0; i < 4; ++i) {
                 p.drawLine(QPointF(10 + i * 14.6, 10), QPointF(10 + i * 14.6, 54));
                 p.drawLine(QPointF(10, 10 + i * 14.6), QPointF(54, 10 + i * 14.6));
             }
         }},
        {"viewports",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.6));
             p.setBrush(kPaper);
             for (int r = 0; r < 2; ++r)
                 for (int c = 0; c < 2; ++c) p.drawRect(QRectF(8 + c * 25, 10 + r * 23, 22, 20));
         }},

        // --- Browser -------------------------------------------------------
        {"eye", [](QPainter& p) { eye(p, true); }},
        {"eye-off", [](QPainter& p) { eye(p, false); }},
        {"folder",
         [](QPainter& p) {
             poly(p,
                  {QPointF(6, 16), QPointF(24, 16), QPointF(30, 22), QPointF(58, 22), QPointF(58, 52), QPointF(6, 52)},
                  QColor(0xc4, 0xcb, 0xd4), QColor(0x7d, 0x86, 0x92));
             poly(p, {QPointF(6, 28), QPointF(58, 28), QPointF(58, 52), QPointF(6, 52)}, QColor(0xdb, 0xe0, 0xe6),
                  QColor(0x7d, 0x86, 0x92));
         }},
        {"doc",
         [](QPainter& p) {
             isoBox(p, 32, 10, 42, 22, kGray);
             p.setPen(QPen(kInk, 2));
             p.setBrush(kPaper);
             p.drawEllipse(QPointF(46, 46), 9, 9);
         }},
        {"body", [](QPainter& p) { isoBox(p, 32, 8, 44, 26, QColor(0xa9, 0xb4, 0xc2)); }},
        {"origin-plane",
         [](QPainter& p) { plane(p, QPointF(6, 40), QPointF(32, 54), QPointF(58, 26), QPointF(32, 12)); }},
        {"named-view",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.4));
             p.setBrush(QColor(0xdd, 0xe2, 0xe8));
             p.drawRoundedRect(QRectF(8, 20, 48, 30), 5, 5);
             p.setBrush(kPaper);
             p.drawEllipse(QPointF(32, 35), 10, 10);
             p.drawRect(QRectF(16, 14, 12, 6));
         }},
        {"warning",
         [](QPainter& p) {
             poly(p, {QPointF(32, 6), QPointF(60, 56), QPointF(4, 56)}, kAmber, kAmber.darker(170));
             stroke(p, kInk, 5);
             p.drawLine(QPointF(32, 24), QPointF(32, 40));
             p.drawPoint(QPointF(32, 48));
         }},

        // --- Timeline transport -------------------------------------------
        {"tl-first",
         [](QPainter& p) {
             poly(p, {QPointF(50, 14), QPointF(50, 50), QPointF(22, 32)}, kInk, kInk);
             p.drawRect(QRectF(12, 14, 5, 36));
         }},
        {"tl-prev", [](QPainter& p) { poly(p, {QPointF(44, 14), QPointF(44, 50), QPointF(16, 32)}, kInk, kInk); }},
        {"tl-play", [](QPainter& p) { poly(p, {QPointF(20, 12), QPointF(20, 52), QPointF(52, 32)}, kInk, kInk); }},
        {"tl-next",
         [](QPainter& p) {
             poly(p, {QPointF(20, 14), QPointF(20, 50), QPointF(48, 32)}, kInk, kInk);
             p.drawRect(QRectF(48, 14, 4, 36));
         }},
        {"tl-last",
         [](QPainter& p) {
             poly(p, {QPointF(14, 14), QPointF(14, 50), QPointF(42, 32)}, kInk, kInk);
             p.drawRect(QRectF(47, 14, 5, 36));
         }},

        // --- Top bar -------------------------------------------------------
        {"apps",
         [](QPainter& p) {
             p.setPen(Qt::NoPen);
             p.setBrush(kInk);
             for (int r = 0; r < 3; ++r)
                 for (int c = 0; c < 3; ++c) p.drawRoundedRect(QRectF(10 + c * 16, 10 + r * 16, 11, 11), 2, 2);
         }},
        {"file",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.6));
             p.setBrush(kPaper);
             p.drawPolygon(
                 QPolygonF({QPointF(14, 6), QPointF(38, 6), QPointF(50, 18), QPointF(50, 58), QPointF(14, 58)}));
             p.drawPolyline(QPolygonF({QPointF(38, 6), QPointF(38, 18), QPointF(50, 18)}));
         }},
        {"save",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 2.6));
             p.setBrush(QColor(0xdd, 0xe2, 0xe8));
             p.drawRoundedRect(QRectF(8, 8, 48, 48), 4, 4);
             p.setBrush(kPaper);
             p.drawRect(QRectF(16, 8, 30, 16));
             p.drawRect(QRectF(16, 34, 32, 22));
         }},
        {"undo",
         [](QPainter& p) {
             stroke(p, kInk, 4.4);
             QPainterPath a;
             a.moveTo(18, 26);
             a.cubicTo(30, 14, 56, 18, 52, 44);
             p.drawPath(a);
             arrow(p, QPointF(24, 22), QPointF(14, 30), kInk, 4.4);
         }},
        {"redo",
         [](QPainter& p) {
             stroke(p, kInk, 4.4);
             QPainterPath a;
             a.moveTo(46, 26);
             a.cubicTo(34, 14, 8, 18, 12, 44);
             p.drawPath(a);
             arrow(p, QPointF(40, 22), QPointF(50, 30), kInk, 4.4);
         }},
        {"plus",
         [](QPainter& p) {
             stroke(p, kInk, 4.4);
             p.drawLine(QPointF(32, 12), QPointF(32, 52));
             p.drawLine(QPointF(12, 32), QPointF(52, 32));
         }},
        {"bell",
         [](QPainter& p) {
             QPainterPath b;
             b.moveTo(14, 46);
             b.cubicTo(20, 40, 16, 14, 32, 12);
             b.cubicTo(48, 14, 44, 40, 50, 46);
             b.closeSubpath();
             p.setPen(QPen(kInk, 3));
             p.setBrush(Qt::NoBrush);
             p.drawPath(b);
             p.drawArc(QRectF(26, 44, 12, 10), 180 * 16, 180 * 16);
         }},
        {"help",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 3));
             p.setBrush(Qt::NoBrush);
             p.drawEllipse(QPointF(32, 32), 24, 24);
             QFont f("Helvetica", 26, QFont::Bold);
             p.setFont(f);
             p.drawText(QRectF(8, 8, 48, 48), Qt::AlignCenter, "?");
         }},
        {"user",
         [](QPainter& p) {
             p.setPen(Qt::NoPen);
             p.setBrush(QColor(0x5b, 0x6b, 0x7f));
             p.drawEllipse(QPointF(32, 32), 26, 26);
             p.setBrush(kPaper);
             p.drawEllipse(QPointF(32, 25), 9, 9);
             p.drawChord(QRectF(14, 36, 36, 30), 0, 180 * 16);
         }},
        {"jobs",
         [](QPainter& p) {
             p.setPen(QPen(kInk, 3));
             p.setBrush(Qt::NoBrush);
             p.drawEllipse(QPointF(32, 32), 22, 22);
             p.drawLine(QPointF(32, 32), QPointF(32, 18));
             p.drawLine(QPointF(32, 32), QPointF(42, 38));
         }},
    };
    return table;
}

QPixmap render(const Painter& paint, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.scale(size / 64.0, size / 64.0);
    paint(p);
    return pm;
}

void placeholder(QPainter& p) {
    p.setPen(QPen(kGray, 2.4, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(10, 10, 44, 44), 6, 6);
}

} // namespace

QIcon get(const QString& name) {
    static QHash<QString, QIcon> cache;
    if (auto it = cache.constFind(name); it != cache.constEnd()) return *it;
    const auto& table = painters();
    const auto found = table.constFind(name);
    const Painter paint = found != table.constEnd() ? *found : Painter(placeholder);
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 96, 128}) icon.addPixmap(render(paint, size));
    cache.insert(name, icon);
    return icon;
}

} // namespace icons
