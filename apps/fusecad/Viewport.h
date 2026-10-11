// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SceneBuilder.h"
#include "ViewCube.h"
#include "ViewState.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

#include <memory>
#include <vector>

class QVariantAnimation;

// 3D view of the design: gradient backdrop, ground grid, shaded bodies with
// B-rep edges, sketch curves, origin planes and the ViewCube.
//
// Navigation (Z up): left-drag orbit, middle-drag (or shift+left / right-drag)
// pan, wheel zoom. A right-click without dragging requests the marking menu.
//
// Display data is kept on the CPU side; GL objects are derived from it and
// rebuilt whenever the GL context is replaced (e.g. on re-parenting).
// NOLINTNEXTLINE(misc-multiple-inheritance): Qt's documented QOpenGLWidget + GL functions pattern
class Viewport final : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT

public:
    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    // Meshes already uploaded (same shared mesh) are reused, not re-uploaded.
    void setBodies(std::vector<scene::BodyMesh> bodies);
    void setSketchCurves(std::vector<std::vector<float>> polylines); // world-space xyz strips
    void setViewState(const ViewState& state);
    bool isAnimating() const;

    void fitAll();
    void homeView();
    void setView(float yawDeg, float pitchDeg); // animated, then fits the scene

signals:
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct GpuMesh {
        int id = 0;
        std::shared_ptr<const fuse::Mesh> source; // identifies the upload and keeps its address unique
        QOpenGLVertexArrayObject faceVao;
        QOpenGLBuffer faceVbo{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer faceIbo{QOpenGLBuffer::IndexBuffer};
        int indexCount = 0;
        QOpenGLVertexArrayObject edgeVao;
        QOpenGLBuffer edgeVbo{QOpenGLBuffer::VertexBuffer};
        std::vector<std::pair<int, int>> edgeStrips; // (first vertex, count)
        void destroy();
    };
    // A batch of line strips (or triangle strips) in one buffer.
    struct LineBatch {
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
        std::vector<std::pair<int, int>> ranges;
        void upload(QOpenGLExtraFunctions& gl, const std::vector<std::vector<float>>& strips);
        void destroy();
    };

    std::unique_ptr<GpuMesh> upload(const scene::BodyMesh& body);
    void uploadBodies();
    void rebuildGrid();
    void releaseGl(); // destroys every GL object and marks all display data for re-upload
    void animateTo(float yaw, float pitch, QVector3D target, float distance);
    void fitParameters(QVector3D& target, float& distance) const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    QVector3D eye() const;
    QRect cubeRect() const;

    // GL objects (valid only while the current context lives).
    std::unique_ptr<QOpenGLShaderProgram> shaded_, lines_, sky_;
    QOpenGLVertexArrayObject skyVao_;
    std::vector<std::unique_ptr<GpuMesh>> gpu_;
    LineBatch sketch_, gridMinor_, gridMajor_, originAxes_, originPlanes_;

    // Display data (CPU side).
    std::vector<scene::BodyMesh> bodies_;
    bool bodiesDirty_ = false;
    std::vector<std::vector<float>> sketchCurves_;
    bool sketchDirty_ = false;
    bool gridDirty_ = true;
    ViewState state_;

    QVector3D sceneCenter_{0, 0, 0};
    float sceneRadius_ = 50.0f;

    // Orbit camera.
    QVector3D target_{0, 0, 0};
    float distance_ = 200.0f;
    float yawDeg_ = ViewCube::kHomeOrientation.yawDeg;
    float pitchDeg_ = ViewCube::kHomeOrientation.pitchDeg;
    QVariantAnimation* animation_ = nullptr;

    // Mouse.
    QPoint lastMouse_, pressPos_;
    bool rightDragged_ = false;
    ViewCube cube_;
    ViewCube::Target cubeHover_ = ViewCube::None;
};
