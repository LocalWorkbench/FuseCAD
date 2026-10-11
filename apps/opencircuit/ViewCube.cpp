// SPDX-License-Identifier: GPL-3.0-or-later
#include "ViewCube.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QVector3D>

#include <array>

namespace {

struct Face {
    ViewCube::Target target;
    const char* label;
    QVector3D normal;
    // Corners as seen from outside: bottom-left, bottom-right, top-right, top-left.
    std::array<QVector3D, 4> corners;
};

const std::array<Face, 6> kFaces{{
    {ViewCube::Top, "TOP", {0, 0, 1}, {{{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}}},
    {ViewCube::Bottom, "BOTTOM", {0, 0, -1}, {{{-1, 1, -1}, {1, 1, -1}, {1, -1, -1}, {-1, -1, -1}}}},
    {ViewCube::Front, "FRONT", {0, -1, 0}, {{{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}}},
    {ViewCube::Back, "BACK", {0, 1, 0}, {{{1, 1, -1}, {-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}}}},
    {ViewCube::Right, "RIGHT", {1, 0, 0}, {{{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}}}},
    {ViewCube::Left, "LEFT", {-1, 0, 0}, {{{-1, 1, -1}, {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}}}},
}};

// Rotation part of the view matrix (camera orientation only).
QMatrix4x4 rotationOnly(const QMatrix4x4& view) {
    QMatrix4x4 r = view;
    r.setColumn(3, QVector4D(0, 0, 0, 1));
    return r;
}

struct Projector {
    QPointF center;
    float scale;
    QMatrix4x4 rot;
    QPointF operator()(const QVector3D& v) const {
        QVector3D r = rot.map(v);
        return center + QPointF(double(r.x()), -double(r.y())) * double(scale);
    }
};

Projector projector(const QRect& area, const QMatrix4x4& view) {
    return {QPointF(area.center()) + QPointF(0, 6), float(area.width()) * 0.26f, rotationOnly(view)};
}

QRectF homeRect(const QRect& area) { return {double(area.left()), double(area.top()), 18.0, 18.0}; }

} // namespace

ViewCube::Orientation ViewCube::orientation(Target t) {
    switch (t) {
    case Top: return {-90.0f, 89.0f};
    case Bottom: return {-90.0f, -89.0f};
    case Front: return {-90.0f, 0.0f};
    case Back: return {90.0f, 0.0f};
    case Right: return {0.0f, 0.0f};
    case Left: return {180.0f, 0.0f};
    case Home:
    case None: break;
    }
    return {-60.0f, 30.0f};
}

void ViewCube::paint(QPainter& p, const QRect& area, const QMatrix4x4& view, Target hover) const {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const Projector proj = projector(area, view);

    // Axis triad from the cube's origin corner.
    const QVector3D o(-1, -1, -1);
    const struct {
        QVector3D dir;
        QColor color;
        const char* name;
    } axes[] = {{{1, 0, 0}, QColor(0xd9, 0x3f, 0x3f), "X"},
                {{0, 1, 0}, QColor(0x34, 0xa8, 0x53), "Y"},
                {{0, 0, 1}, QColor(0x2a, 0x6e, 0xd8), "Z"}};
    QFont small = p.font();
    small.setPixelSize(10);
    small.setBold(true);
    p.setFont(small);
    for (const auto& a : axes) {
        const QPointF from = proj(o), to = proj(o + a.dir * 2.8f);
        p.setPen(QPen(a.color, 2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(from, to);
        p.drawText(QRectF(to - QPointF(6, 6), QSizeF(12, 12)), Qt::AlignCenter, a.name);
    }

    // Visible faces (the cube is convex, so no sorting is needed).
    QFont font = p.font();
    font.setPixelSize(20);
    font.setBold(true);
    for (const Face& f : kFaces) {
        if (proj.rot.mapVector(f.normal).z() <= 1e-3f) continue;
        QPolygonF quad;
        for (const auto& c : f.corners) quad << proj(c);
        const bool hot = hover == f.target;
        QLinearGradient g(quad.boundingRect().topLeft(), quad.boundingRect().bottomRight());
        g.setColorAt(0, hot ? theme::kAccentSoft : QColor(0xfb, 0xfc, 0xfd));
        g.setColorAt(1, hot ? theme::kAccentSoft.darker(106) : QColor(0xe2, 0xe6, 0xeb));
        p.setPen(QPen(hot ? theme::kAccent : QColor(0x9a, 0xa3, 0xae), 1.2));
        p.setBrush(g);
        p.drawPolygon(quad);

        // Label mapped onto the face: text rect corners (TL, TR, BR, BL) -> quad (TL, TR, BR, BL).
        QTransform t;
        const QPolygonF textRect({QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100)});
        const QPolygonF faceQuad({quad[3], quad[2], quad[1], quad[0]});
        if (QTransform::quadToQuad(textRect, faceQuad, t)) {
            p.save();
            p.setTransform(t, true);
            p.setFont(font);
            p.setPen(hot ? theme::kAccent : QColor(0x55, 0x5d, 0x68));
            p.drawText(QRectF(0, 0, 100, 100), Qt::AlignCenter, f.label);
            p.restore();
        }
    }

    // Home button.
    const QRectF h = homeRect(area);
    p.setPen(QPen(hover == Home ? theme::kAccent : QColor(0x6b, 0x72, 0x7c), 1.6));
    p.setBrush(hover == Home ? theme::kAccentSoft : QColor(255, 255, 255, 200));
    QPainterPath house;
    house.moveTo(h.center().x(), h.top() + 2);
    house.lineTo(h.right() - 2, h.top() + 9);
    house.lineTo(h.right() - 4, h.top() + 9);
    house.lineTo(h.right() - 4, h.bottom() - 2);
    house.lineTo(h.left() + 4, h.bottom() - 2);
    house.lineTo(h.left() + 4, h.top() + 9);
    house.lineTo(h.left() + 2, h.top() + 9);
    house.closeSubpath();
    p.drawPath(house);
    p.restore();
}

ViewCube::Target ViewCube::hitTest(const QPoint& pos, const QRect& area, const QMatrix4x4& view) const {
    if (homeRect(area).contains(pos)) return Home;
    const Projector proj = projector(area, view);
    for (const Face& f : kFaces) {
        if (proj.rot.mapVector(f.normal).z() <= 1e-3f) continue;
        QPolygonF quad;
        for (const auto& c : f.corners) quad << proj(c);
        if (quad.containsPoint(pos, Qt::OddEvenFill)) return f.target;
    }
    return None;
}
