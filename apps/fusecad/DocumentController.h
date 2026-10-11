// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fuse/kernel/Kernel.h"
#include "fuse/model/Document.h"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

// Owns the open design: the geometry kernel, the document, its undo history
// and the file it lives in. Every edit goes through here, so recompute, undo
// and the modified flag stay consistent; views react to the signals.
class DocumentController final : public QObject {
    Q_OBJECT

public:
    enum class Severity { Warning, Critical };
    enum class ExportFormat { Step, Stl };

    explicit DocumentController(QObject* parent = nullptr);
    ~DocumentController() override;

    fuse::Kernel& kernel() { return *kernel_; }
    const fuse::Document& document() const { return doc_; }
    const QString& filePath() const { return filePath_; }
    QString documentName() const; // file base name, or "Untitled"
    bool isModified() const { return modified_; }

    // Replace the document and start a fresh history.
    void setDocument(fuse::Document doc, const QString& filePath = {});
    bool open(const QString& path);
    bool save(); // to filePath(), which must be set
    bool saveAs(const QString& path);
    bool exportShapes(const std::vector<fuse::ShapePtr>& shapes, const QString& path, ExportFormat format);

    // Apply an edit and commit it as one undo step. If the edit throws
    // fuse::Error the document is restored, failed() is emitted (titled
    // `failureTitle`, or `label` when empty) and false returned.
    bool runCommand(const QString& label, const std::function<void(fuse::Document&)>& edit,
                    const QString& failureTitle = {});
    void recompute();

    bool canUndo() const { return undo_->canUndo(); }
    bool canRedo() const { return undo_->canRedo(); }
    QString undoLabel() const { return QString::fromStdString(undo_->undoLabel()); }
    QString redoLabel() const { return QString::fromStdString(undo_->redoLabel()); }
    void undo();
    void redo();

signals:
    void documentReset(); // a different design was loaded (before it is recomputed)
    void recomputed(const fuse::RecomputeReport& report, qint64 elapsedMs);
    void historyChanged();   // undo/redo availability or labels
    void fileStateChanged(); // file path or modified flag
    void failed(const QString& title, const QString& message, DocumentController::Severity severity);

private:
    void restore(fuse::Document doc); // from the undo history
    void setModified(bool modified);

    std::unique_ptr<fuse::Kernel> kernel_;
    fuse::Document doc_;
    std::unique_ptr<fuse::UndoStack> undo_;
    QString filePath_;
    bool modified_ = false;
};
