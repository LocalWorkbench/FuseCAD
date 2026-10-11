// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char** argv) {
    // Must be set before the QApplication is created so every GL context
    // (including the viewport's) gets a core profile. 4.1 is the macOS ceiling.
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName("FuseCAD");
    QApplication::setOrganizationName("FuseCAD");
    QApplication::setApplicationVersion(FUSE_VERSION_STRING);
    QApplication::setWindowIcon(QIcon(":/fusecad.png"));
    theme::apply(app);

    QCommandLineParser parser;
    parser.setApplicationDescription("Open-source parametric CAD");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "FuseCAD document (.fcad) to open");
    const QCommandLineOption screenshot("screenshot", "Save an image of the window to <png> and exit.", "png");
    parser.addOption(screenshot);
    parser.process(app);

    MainWindow window;
    const QStringList files = parser.positionalArguments();
    window.openInitial(files.isEmpty() ? QString() : files.front());
    // Screenshot runs are automated: never steal keyboard focus from the user.
    if (parser.isSet(screenshot)) window.setAttribute(Qt::WA_ShowWithoutActivating);
    window.show();
    if (parser.isSet(screenshot)) {
        auto capture = [&window, path = parser.value(screenshot)] {
            const bool ok = window.snapshot().save(path);
            QApplication::exit(ok ? 0 : 1);
        };
        // Capture once a settled frame is on screen; the timeout covers
        // platforms that never present one (e.g. an offscreen session).
        constexpr int kScreenshotTimeoutMs = 10000;
        window.whenViewSettled(capture);
        QTimer::singleShot(kScreenshotTimeoutMs, &window, capture);
    }
    return QApplication::exec();
}
