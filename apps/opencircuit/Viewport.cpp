// SPDX-License-Identifier: GPL-3.0-or-later
#include "Viewport.h"

#include "Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kFovDeg = 30.0f;

constexpr const char* kShadedVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vNormal;
out vec3 vViewPos;
void main() {
    vec4 viewPos = uView * vec4(aPos, 1.0);
    vViewPos = viewPos.xyz;
    vNormal = mat3(uView) * aNormal;
    gl_Position = uProj * viewPos;
}
)";

// Headlight plus a soft key light, two-sided so open or inverted faces still read.
constexpr const char* kShadedFs = R"(#version 330 core
in vec3 vNormal;
in vec3 vViewPos;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(-vViewPos);
    if (dot(n, v) < 0.0) n = -n;
    vec3 key = normalize(vec3(0.4, 0.6, 1.0));
    float diffuse = 0.55 * max(dot(n, v), 0.0) + 0.35 * max(dot(n, key), 0.0);
    float spec = pow(max(dot(n, normalize(v + key)), 0.0), 60.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.12;
    vec3 c = uColor * (0.32 + diffuse) + vec3(0.22) * spec + vec3(rim);
    fragColor = vec4(c, 1.0);
}
)";

// uFade: xyz = centre, w = radius. When w > 0, alpha falls off with distance
// from the centre (used to fade the ground grid out towards the horizon).
constexpr const char* kLinesVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorld;
void main() {
    vWorld = aPos;
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
)";

constexpr const char* kLinesFs = R"(#version 330 core
in vec3 vWorld;
uniform vec4 uColor;
uniform vec4 uFade;
out vec4 fragColor;
void main() {
    float a = uColor.a;
    if (uFade.w > 0.0) a *= 1.0 - smoothstep(0.25 * uFade.w, uFade.w, distance(vWorld.xy, uFade.xy));
    fragColor = vec4(uColor.rgb, a);
}
)";

// Full-screen gradient from a vertex-less triangle strip.
constexpr const char* kSkyVs = R"(#version 330 core
out float vT;
void main() {
    vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
    vT = p.y * 0.5 + 0.5;
    gl_Position = vec4(p, 0.9999, 1.0);
}
)";

constexpr const char* kSkyFs = R"(#version 330 core
in float vT;
uniform vec3 uTop;
uniform vec3 uBottom;
out vec4 fragColor;
void main() { fragColor = vec4(mix(uBottom, uTop, smoothstep(0.0, 1.0, vT)), 1.0); }
)";

QVector3D rgb(const QColor& c) {
    return {static_cast<float>(c.redF()), static_cast<float>(c.greenF()), static_cast<float>(c.blueF())};
}
QVector4D rgba(const QColor& c, float a = 1.0f) {
    return {static_cast<float>(c.redF()), static_cast<float>(c.greenF()), static_cast<float>(c.blueF()), a};
}

void compile(QOpenGLShaderProgram& program, const char* vs, const char* fs) {
    if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vs) ||
        !program.addShaderFromSourceCode(QOpenGLShader::Fragment, fs) || !program.link())
        qFatal("Viewport shader build failed: %s", qPrintable(program.log()));
}

// "Nice" grid spacing: 1, 2 or 5 times a power of ten.
float niceStep(float approx) {
    const float p = std::pow(10.0f, std::floor(std::log10(std::max(approx, 1e-3f))));
    for (float m : {1.0f, 2.0f, 5.0f})
        if (m * p >= approx) return m * p;
    return 10.0f * p;
}

} // namespace

void Viewport::LineBatch::upload(QOpenGLExtraFunctions& gl, const std::vector<std::vector<float>>& strips) {
    std::vector<float> points;
    ranges.clear();
    for (const auto& s : strips) {
        ranges.emplace_back(static_cast<int>(points.size() / 3), static_cast<int>(s.size() / 3));
        points.insert(points.end(), s.begin(), s.end());
    }
    if (!vao.isCreated()) vao.create();
    vao.bind();
    if (!vbo.isCreated()) vbo.create();
    vbo.bind();
    vbo.allocate(points.data(), static_cast<int>(points.size() * sizeof(float)));
    gl.glEnableVertexAttribArray(0);
    gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    vao.release();
}

void Viewport::LineBatch::destroy() {
    vbo.destroy();
    vao.destroy();
    ranges.clear();
}

Viewport::Viewport(QWidget* parent) : QOpenGLWidget(parent), animation_(new QVariantAnimation(this)) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 240);
    animation_->setDuration(320);
    animation_->setEasingCurve(QEasingCurve::InOutCubic);
}

Viewport::~Viewport() {
    makeCurrent();
    releaseGpu();
    for (LineBatch* b : {&sketch_, &gridMinor_, &gridMajor_, &originAxes_, &originPlanes_}) b->destroy();
    skyVao_.destroy();
    doneCurrent();
}

void Viewport::setBodies(std::vector<BodyMesh> bodies) {
    pendingBodies_ = std::move(bodies);
    bodiesDirty_ = true;

    constexpr float big = std::numeric_limits<float>::max();
    QVector3D lo(big, big, big);
    QVector3D hi = -lo;
    bool any = false;
    for (const auto& b : pendingBodies_) {
        const auto& pos = b.mesh.positions;
        for (size_t i = 0; i + 2 < pos.size(); i += 3) {
            lo = QVector3D(std::min(lo.x(), pos[i]), std::min(lo.y(), pos[i + 1]), std::min(lo.z(), pos[i + 2]));
            hi = QVector3D(std::max(hi.x(), pos[i]), std::max(hi.y(), pos[i + 1]), std::max(hi.z(), pos[i + 2]));
            any = true;
        }
    }
    const float oldRadius = sceneRadius_;
    if (any) {
        sceneCenter_ = (lo + hi) * 0.5f;
        sceneRadius_ = std::max((hi - lo).length() * 0.5f, 1.0f);
    } else {
        sceneCenter_ = {};
        sceneRadius_ = 50.0f;
    }
    if (std::abs(oldRadius - sceneRadius_) > 1e-3f) gridDirty_ = true;
    update();
}

void Viewport::setSketchCurves(std::vector<std::vector<float>> polylines) {
    pendingSketch_ = std::move(polylines);
    sketchDirty_ = true;
    update();
}

void Viewport::setViewState(const ViewState& state) {
    state_ = state;
    update();
}

void Viewport::fitParameters(QVector3D& target, float& distance) const {
    target = sceneCenter_;
    distance = sceneRadius_ / std::sin(qDegreesToRadians(kFovDeg) * 0.5f) * 1.05f;
}

void Viewport::fitAll() {
    QVector3D t;
    float d = 0;
    fitParameters(t, d);
    animateTo(yawDeg_, pitchDeg_, t, d);
}

void Viewport::homeView() {
    const auto o = ViewCube::orientation(ViewCube::Home);
    setView(o.yawDeg, o.pitchDeg);
}

void Viewport::setView(float yawDeg, float pitchDeg) {
    QVector3D t;
    float d = 0;
    fitParameters(t, d);
    animateTo(yawDeg, std::clamp(pitchDeg, -89.0f, 89.0f), t, d);
}

void Viewport::animateTo(float yaw, float pitch, QVector3D target, float distance) {
    animation_->stop();
    // Take the short way round.
    const float dyaw = std::fmod(yaw - yawDeg_ + 540.0f, 360.0f) - 180.0f;
    const float y0 = yawDeg_, p0 = pitchDeg_, d0 = distance_;
    const QVector3D t0 = target_;
    if (!isVisible()) {
        yawDeg_ = y0 + dyaw;
        pitchDeg_ = pitch;
        distance_ = distance;
        target_ = target;
        update();
        return;
    }
    animation_->disconnect(this);
    animation_->setStartValue(0.0);
    animation_->setEndValue(1.0);
    connect(animation_, &QVariantAnimation::valueChanged, this, [=, this](const QVariant& v) {
        const float s = v.toFloat();
        yawDeg_ = y0 + dyaw * s;
        pitchDeg_ = p0 + (pitch - p0) * s;
        distance_ = d0 + (distance - d0) * s;
        target_ = t0 + (target - t0) * s;
        update();
    });
    animation_->start();
}

void Viewport::initializeGL() {
    initializeOpenGLFunctions();
    compile(shaded_, kShadedVs, kShadedFs);
    compile(lines_, kLinesVs, kLinesFs);
    compile(sky_, kSkyVs, kSkyFs);
    skyVao_.create();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Viewport::resizeGL(int, int) {}

void Viewport::releaseGpu() {
    for (auto& g : gpu_) {
        g->faceVbo.destroy();
        g->faceIbo.destroy();
        g->faceVao.destroy();
        g->edgeVbo.destroy();
        g->edgeVao.destroy();
    }
    gpu_.clear();
}

void Viewport::uploadBodies() {
    releaseGpu();
    for (const auto& body : pendingBodies_) {
        const oc::Mesh& m = body.mesh;
        auto g = std::make_unique<GpuMesh>();
        g->id = body.id;

        std::vector<float> interleaved;
        interleaved.reserve(m.positions.size() * 2);
        for (size_t i = 0; i + 2 < m.positions.size(); i += 3)
            interleaved.insert(interleaved.end(), {m.positions[i], m.positions[i + 1], m.positions[i + 2], m.normals[i],
                                                   m.normals[i + 1], m.normals[i + 2]});
        g->faceVao.create();
        g->faceVao.bind();
        g->faceVbo.create();
        g->faceVbo.bind();
        g->faceVbo.allocate(interleaved.data(), static_cast<int>(interleaved.size() * sizeof(float)));
        g->faceIbo.create();
        g->faceIbo.bind();
        g->faceIbo.allocate(m.indices.data(), static_cast<int>(m.indices.size() * sizeof(std::uint32_t)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        g->faceVao.release();
        g->indexCount = static_cast<int>(m.indices.size());

        std::vector<float> edgePoints;
        for (const auto& strip : m.edges) {
            g->edgeStrips.emplace_back(static_cast<int>(edgePoints.size() / 3), static_cast<int>(strip.size() / 3));
            edgePoints.insert(edgePoints.end(), strip.begin(), strip.end());
        }
        g->edgeVao.create();
        g->edgeVao.bind();
        g->edgeVbo.create();
        g->edgeVbo.bind();
        g->edgeVbo.allocate(edgePoints.data(), static_cast<int>(edgePoints.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        g->edgeVao.release();

        gpu_.push_back(std::move(g));
    }
    pendingBodies_.clear();
    bodiesDirty_ = false;
}

void Viewport::rebuildGrid() {
    // Grid on the XY plane sized to the scene, with a major line every 5 steps.
    const float step = niceStep(sceneRadius_ / 6.0f);
    const int half = 30;
    const float extent = step * static_cast<float>(half);
    std::vector<std::vector<float>> minor, major;
    for (int i = -half; i <= half; ++i) {
        const float c = static_cast<float>(i) * step;
        auto& dst = (i % 5 == 0) ? major : minor;
        dst.push_back({c, -extent, 0, c, extent, 0});
        dst.push_back({-extent, c, 0, extent, c, 0});
    }
    gridMinor_.upload(*this, minor);
    gridMajor_.upload(*this, major);

    const float a = sceneRadius_ * 0.9f;
    originAxes_.upload(*this, {{0, 0, 0, a, 0, 0}, {0, 0, 0, 0, a, 0}, {0, 0, 0, 0, 0, a}});
    const float s = sceneRadius_ * 0.7f;
    // One 4-vertex triangle strip per origin plane.
    originPlanes_.upload(*this, {{0, 0, 0, s, 0, 0, 0, s, 0, s, s, 0},
                                 {0, 0, 0, s, 0, 0, 0, 0, s, s, 0, s},
                                 {0, 0, 0, 0, s, 0, 0, 0, s, 0, s, s}});
    gridDirty_ = false;
}

QVector3D Viewport::eye() const {
    const float yaw = qDegreesToRadians(yawDeg_);
    const float pitch = qDegreesToRadians(pitchDeg_);
    const QVector3D dir(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
    return target_ + dir * distance_;
}

QMatrix4x4 Viewport::viewMatrix() const {
    QMatrix4x4 v;
    v.lookAt(eye(), target_, QVector3D(0, 0, 1));
    return v;
}

QMatrix4x4 Viewport::projectionMatrix() const {
    QMatrix4x4 p;
    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    const float reach = (eye() - sceneCenter_).length() + sceneRadius_ * 40.0f;
    p.perspective(kFovDeg, aspect, std::max(distance_ * 0.01f, 0.01f), reach * 2.0f);
    return p;
}

QRect Viewport::cubeRect() const { return {width() - 140, 14, 120, 120}; }

void Viewport::paintGL() {
    if (bodiesDirty_) uploadBodies();
    if (sketchDirty_) {
        sketch_.upload(*this, pendingSketch_);
        sketchDirty_ = false;
    }
    if (gridDirty_) rebuildGrid();

    // QPainter (the ViewCube overlay) leaves its own GL state behind; reset
    // everything this pass depends on. Depth writes must be on for glClear.
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Backdrop.
    glDepthMask(GL_FALSE);
    sky_.bind();
    sky_.setUniformValue("uTop", rgb(theme::kSkyTop));
    sky_.setUniformValue("uBottom", rgb(theme::kSkyBottom));
    skyVao_.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    skyVao_.release();
    sky_.release();

    const QMatrix4x4 view = viewMatrix();
    const QMatrix4x4 proj = projectionMatrix();

    lines_.bind();
    lines_.setUniformValue("uView", view);
    lines_.setUniformValue("uProj", proj);
    auto drawBatch = [this](LineBatch& b, GLenum mode, const QVector4D& color) {
        if (b.ranges.empty()) return;
        lines_.setUniformValue("uColor", color);
        b.vao.bind();
        for (const auto& [first, count] : b.ranges) glDrawArrays(mode, first, count);
        b.vao.release();
    };

    // Ground grid: drawn without depth writes so it never occludes bodies.
    lines_.setUniformValue("uFade", QVector4D(0, 0, 0, 0));
    if (state_.showGrid) {
        glEnable(GL_BLEND);
        lines_.setUniformValue("uFade", QVector4D(sceneCenter_.x(), sceneCenter_.y(), 0, sceneRadius_ * 6.0f));
        drawBatch(gridMinor_, GL_LINES, rgba(theme::kGridMinor, 0.45f));
        drawBatch(gridMajor_, GL_LINES, rgba(theme::kGridMajor, 0.7f));
        lines_.setUniformValue("uFade", QVector4D(0, 0, 0, 0));
        glDisable(GL_BLEND);
    }
    glDepthMask(GL_TRUE);
    lines_.release();

    // Bodies.
    const bool faces = state_.style != ViewState::Style::Wireframe;
    const bool edges = state_.style != ViewState::Style::Shaded;
    if (faces) {
        // Push faces back so edges lying exactly on them win the depth test.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        shaded_.bind();
        shaded_.setUniformValue("uView", view);
        shaded_.setUniformValue("uProj", proj);
        shaded_.setUniformValue("uColor", rgb(theme::kBody));
        for (auto& g : gpu_) {
            if (!state_.bodyVisible(g->id)) continue;
            g->faceVao.bind();
            glDrawElements(GL_TRIANGLES, g->indexCount, GL_UNSIGNED_INT, nullptr);
            g->faceVao.release();
        }
        shaded_.release();
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    lines_.bind();
    if (edges) {
        lines_.setUniformValue("uColor", rgba(theme::kEdge));
        for (auto& g : gpu_) {
            if (!state_.bodyVisible(g->id)) continue;
            g->edgeVao.bind();
            for (const auto& [first, count] : g->edgeStrips) glDrawArrays(GL_LINE_STRIP, first, count);
            g->edgeVao.release();
        }
    }
    drawBatch(sketch_, GL_LINE_STRIP, rgba(theme::kSketch));

    if (state_.showOrigin) {
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        drawBatch(originPlanes_, GL_TRIANGLE_STRIP, QVector4D(0.95f, 0.71f, 0.24f, 0.22f));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        const QVector4D axisColors[] = {{0.85f, 0.25f, 0.25f, 1}, {0.2f, 0.66f, 0.33f, 1}, {0.16f, 0.43f, 0.85f, 1}};
        originAxes_.vao.bind();
        for (int i = 0; i < 3; ++i) {
            lines_.setUniformValue("uColor", axisColors[i]);
            glDrawArrays(GL_LINES, i * 2, 2);
        }
        originAxes_.vao.release();
    }
    lines_.release();

    // 2D overlay.
    QPainter painter(this);
    cube_.paint(painter, cubeRect(), view, cubeHover_);
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    lastMouse_ = pressPos_ = event->position().toPoint();
    rightDragged_ = false;
    if (event->button() == Qt::LeftButton) {
        const auto hit = cube_.hitTest(pressPos_, cubeRect(), viewMatrix());
        if (hit == ViewCube::Home) {
            homeView();
            return;
        }
        if (hit != ViewCube::None) {
            const auto o = ViewCube::orientation(hit);
            setView(o.yawDeg, o.pitchDeg);
            return;
        }
    }
    animation_->stop();
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - lastMouse_;
    lastMouse_ = pos;

    if (event->buttons() == Qt::NoButton) {
        const auto hover = cube_.hitTest(pos, cubeRect(), viewMatrix());
        if (hover != cubeHover_) {
            cubeHover_ = hover;
            setCursor(hover == ViewCube::None ? Qt::ArrowCursor : Qt::PointingHandCursor);
            update();
        }
        return;
    }

    const bool right = event->buttons() & Qt::RightButton;
    if (right && (pos - pressPos_).manhattanLength() > 4) rightDragged_ = true;
    if (right && !rightDragged_) return;

    const bool pan = (event->buttons() & Qt::MiddleButton) || right ||
                     ((event->buttons() & Qt::LeftButton) && (event->modifiers() & Qt::ShiftModifier));
    if (pan) {
        const QMatrix4x4 inv = viewMatrix().inverted();
        const QVector3D rightDir = inv.mapVector({1, 0, 0}).normalized();
        const QVector3D up = inv.mapVector({0, 1, 0}).normalized();
        const float unitsPerPixel =
            2.0f * distance_ * std::tan(qDegreesToRadians(kFovDeg) * 0.5f) / static_cast<float>(std::max(height(), 1));
        target_ += (-rightDir * static_cast<float>(delta.x()) + up * static_cast<float>(delta.y())) * unitsPerPixel;
    } else if (event->buttons() & Qt::LeftButton) {
        yawDeg_ -= static_cast<float>(delta.x()) * 0.4f;
        pitchDeg_ = std::clamp(pitchDeg_ + static_cast<float>(delta.y()) * 0.4f, -89.0f, 89.0f);
    }
    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton && !rightDragged_)
        emit contextMenuRequested(event->globalPosition().toPoint());
}

void Viewport::wheelEvent(QWheelEvent* event) {
    animation_->stop();
    const float steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    distance_ = std::clamp(distance_ * std::pow(0.87f, steps), sceneRadius_ * 0.02f, sceneRadius_ * 200.0f);
    update();
}

void Viewport::leaveEvent(QEvent*) {
    if (cubeHover_ != ViewCube::None) {
        cubeHover_ = ViewCube::None;
        update();
    }
}
