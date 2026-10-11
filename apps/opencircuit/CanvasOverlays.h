// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QLabel>
#include <QStringList>
#include <QWidget>

class CommandRegistry;
class QMenu;
class QTimer;

// Floating navigation toolbar at the bottom of the canvas.
class NavigationBar final : public QFrame {
    Q_OBJECT
public:
    // `displayMenu` holds the visual-style choices; `ids` are command ids or "|" for a separator.
    NavigationBar(CommandRegistry& commands, const QStringList& ids, QMenu* displayMenu, QWidget* parent);
};

// A transient message bubble ("Extrude is coming soon").
class Toast final : public QLabel {
    Q_OBJECT
public:
    explicit Toast(QWidget* parent);
    void showMessage(const QString& html, int ms = 2600);

private:
    QTimer* timer_;
};

// Radial "marking menu" opened by right-clicking the canvas: up to eight
// commands around the cursor plus an overflow list below.
class MarkingMenu final : public QWidget {
    Q_OBJECT
public:
    // ring: eight ids clockwise from north ("" leaves a slot empty); list: overflow ids, "-" for separators.
    MarkingMenu(CommandRegistry& commands, const QStringList& ring, const QStringList& list, QWidget* parent);
    void popupAt(const QPoint& globalCenter);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint center_;
};

// The 3D canvas plus the panels that float over it.
class Canvas final : public QWidget {
    Q_OBJECT
public:
    Canvas(QWidget* viewport, QWidget* browser, QWidget* navBar, QWidget* parent);
    void toast(const QString& html) { toast_->showMessage(html); }
    void setInfo(const QString& text);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutOverlays();

    QWidget* viewport_;
    QWidget* browser_;
    QWidget* navBar_;
    QFrame* comments_;
    Toast* toast_;
    QLabel* info_;
};
