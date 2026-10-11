// SPDX-License-Identifier: GPL-3.0-or-later
#include "DocumentController.h"

#include "fuse/core/Error.h"
#include "fuse/occt/OcctKernel.h"

#include <QElapsedTimer>
#include <QFileInfo>

namespace {

QString errorText(const std::exception& e) { return QString::fromStdString(e.what()); }

} // namespace

DocumentController::DocumentController(QObject* parent)
    : QObject(parent), kernel_(fuse::occt::createKernel()), undo_(std::make_unique<fuse::UndoStack>(doc_)) {}

DocumentController::~DocumentController() = default;

QString DocumentController::documentName() const {
    return filePath_.isEmpty() ? tr("Untitled") : QFileInfo(filePath_).completeBaseName();
}

void DocumentController::setDocument(fuse::Document doc, const QString& filePath) {
    doc_ = std::move(doc);
    undo_ = std::make_unique<fuse::UndoStack>(doc_);
    filePath_ = filePath;
    modified_ = false;
    emit documentReset();
    recompute();
    emit historyChanged();
    emit fileStateChanged();
}

bool DocumentController::open(const QString& path) {
    try {
        setDocument(fuse::Document::load(path.toStdString()), path);
        return true;
    } catch (const fuse::Error& e) {
        emit failed(tr("Open Failed"), tr("Could not open %1:\n%2").arg(path, errorText(e)), Severity::Critical);
        return false;
    }
}

bool DocumentController::save() {
    Q_ASSERT(!filePath_.isEmpty());
    try {
        doc_.save(filePath_.toStdString());
    } catch (const fuse::Error& e) {
        emit failed(tr("Save Failed"), errorText(e), Severity::Critical);
        return false;
    }
    modified_ = false;
    emit fileStateChanged();
    return true;
}

bool DocumentController::saveAs(const QString& path) {
    filePath_ = path;
    return save();
}

bool DocumentController::exportShapes(const std::vector<fuse::ShapePtr>& shapes, const QString& path,
                                      ExportFormat format) {
    try {
        if (format == ExportFormat::Stl)
            kernel_->exportStl(shapes, path.toStdString(), fuse::kDefaultStlDeflection);
        else
            kernel_->exportStep(shapes, path.toStdString());
        return true;
    } catch (const fuse::Error& e) {
        emit failed(tr("Export Failed"), errorText(e), Severity::Critical);
        return false;
    }
}

bool DocumentController::runCommand(const QString& label, const std::function<void(fuse::Document&)>& edit,
                                    const QString& failureTitle) {
    fuse::Document before = doc_;
    try {
        edit(doc_);
    } catch (const fuse::Error& e) {
        doc_ = std::move(before);
        emit failed(failureTitle.isEmpty() ? label : failureTitle, errorText(e), Severity::Warning);
        return false;
    }
    undo_->commit(doc_, label.toStdString());
    setModified(true);
    recompute();
    emit historyChanged();
    return true;
}

void DocumentController::recompute() {
    QElapsedTimer timer;
    timer.start();
    const fuse::RecomputeReport report = doc_.recompute(*kernel_);
    emit recomputed(report, timer.elapsed());
}

void DocumentController::undo() {
    if (canUndo()) restore(undo_->undo());
}

void DocumentController::redo() {
    if (canRedo()) restore(undo_->redo());
}

void DocumentController::restore(fuse::Document doc) {
    doc_ = std::move(doc);
    setModified(true);
    recompute();
    emit historyChanged();
}

void DocumentController::setModified(bool modified) {
    if (modified_ == modified) return;
    modified_ = modified;
    emit fileStateChanged();
}
