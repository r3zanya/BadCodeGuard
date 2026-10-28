#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace bcg {

// How a threat was recognised. Heuristic hits are guesses: the engine never
// quarantines them on its own.
enum class ThreatKind { Signature, Rule, Heuristic, Yara };

// Contract between the UI and any scanning engine. The UI moves the scanner
// to a worker thread, calls start() through a queued call and cancel() directly.
class IScanner : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IScanner() override = default;

    // Thread-safe: may be called from any thread while start() is running.
    virtual void cancel() = 0;

public slots:
    // Scans files and folders recursively; emits finished() exactly once.
    virtual void start(const QStringList& paths) = 0;

signals:
    void progress(qint64 scanned, const QString& currentFile);
    // quarantineId is empty when the file was left in place; quarantineError
    // explains a failed automatic quarantine.
    void threatFound(const QString& path, const QString& threatName, int kind,
                     const QString& quarantineId, const QString& quarantineError);
    void finished(bool cancelled, qint64 scanned, qint64 threats, qint64 skipped);
    // Files to check, counted in the background while the scan runs. `final` is
    // true once counting is done, so the figure is exact.
    void totalChanged(qint64 total, bool final);
    // Something worth a line in the log (an archive that could not be opened, ...).
    void note(const QString& text);
};

} // namespace bcg
