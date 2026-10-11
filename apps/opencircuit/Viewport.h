// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ViewCube.h"
#include "ViewState.h"
#include "oc/kernel/Kernel.h"

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
class Viewport final : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT

public:
    struct BodyMesh {
        int id;
        oc::Mesh mesh;
    };

    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    void setBodies(std::vector<BodyMesh> bodies);
    void setSketchCurves(std::vector<std::vector<float>> polylines); // world-space xyz strips
    void setViewState(const ViewState& state);

public slots:
    void fitAll();
    void homeView();
    void setView(float yawDeg, float pitchDeg); // animated, then fits the scene

signals:
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct GpuMesh {
        int id = 0;
        QOpenGLVertexArrayObject faceVao;
        QOpenGLBuffer faceVbo{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer faceIbo{QOpenGLBuffer::IndexBuffer};
        int indexCount = 0;
        QOpenGLVertexArrayObject edgeVao;
        QOpenGLBuffer edgeVbo{QOpenGLBuffer::VertexBuffer};
        std::vector<std::pair<int, int>> edgeStrips; // (first vertex, count)
    };
    // A batch of line strips (or triangle strips) in one buffer.
    struct LineBatch {
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
        std::vector<std::pair<int, int>> ranges;
        void upload(QOpenGLExtraFunctions& gl, const std::vector<std::vector<float>>& strips);
        void destroy();
    };

    void uploadBodies();
    void rebuildGrid();
    void releaseGpu();
    void animateTo(float yaw, float pitch, QVector3D target, float distance);
    void fitParameters(QVector3D& target, float& distance) const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    QVector3D eye() const;
    QRect cubeRect() const;

    QOpenGLShaderProgram shaded_, lines_, sky_;
    QOpenGLVertexArrayObject skyVao_;
    std::vector<std::unique_ptr<GpuMesh>> gpu_;
    std::vector<BodyMesh> pendingBodies_;
    bool bodiesDirty_ = false;
    LineBatch sketch_, gridMinor_, gridMajor_, originAxes_, originPlanes_;
    std::vector<std::vector<float>> pendingSketch_;
    bool sketchDirty_ = false;
    bool gridDirty_ = true;
    ViewState state_;

    QVector3D sceneCenter_{0, 0, 0};
    float sceneRadius_ = 50.0f;

    // Orbit camera.
    QVector3D target_{0, 0, 0};
    float distance_ = 200.0f;
    float yawDeg_ = -60.0f;
    float pitchDeg_ = 30.0f;
    QVariantAnimation* animation_ = nullptr;

    // Mouse.
    QPoint lastMouse_, pressPos_;
    bool rightDragged_ = false;
    ViewCube cube_;
    ViewCube::Target cubeHover_ = ViewCube::None;
};
