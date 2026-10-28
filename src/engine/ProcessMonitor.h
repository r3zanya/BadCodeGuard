#pragma once

#include "FileScanner.h"

#include <QHash>
#include <QMutex>
#include <QObject>

#include <atomic>
#include <thread>

namespace bcg {

// Looks at programs right after they start. Every new process is noticed within
// a fraction of a second; its image file is checked with the same engine as a
// manual scan. A threat found by hash, rule or YARA is terminated and its file
// quarantined; a merely suspicious one is only reported. Programs that were
// already running at start-up are checked once and only reported.
//
// This is detection shortly after launch, not a blocking filter: a very short
// lived program can finish before it is seen.
class ProcessMonitor : public QObject {
    Q_OBJECT
public:
    explicit ProcessMonitor(ScanContext context, QObject* parent = nullptr);
    ~ProcessMonitor() override;

    void start();
    void stop();
    bool isRunning() const { return m_running; }

    // How often the process list is read.
    void setIntervalMs(int ms) { m_intervalMs = ms; }

signals:
    // Emitted from the monitor thread; connect with a receiver in the UI thread.
    void threatFound(const QString& path, const QString& threatName, int kind,
                      const QString& quarantineId, const QString& quarantineError, bool terminated,
                      quint32 pid);

private:
    struct Stamp {
        qint64 size = 0;
        qint64 modifiedMs = 0;
        bool operator==(const Stamp& o) const { return size == o.size && modifiedMs == o.modifiedMs; }
    };

    void loop();
    void handle(quint32 pid, quint64 startTime, const QString& image, bool atStartup);

    ScanContext m_ctx;
    FileScanner m_scanner;
    std::atomic_bool m_running{false};
    std::atomic<int> m_intervalMs{300};
    std::thread m_thread;

    QHash<QString, Stamp> m_clean;    // images found clean, by path
    QHash<QString, Stamp> m_reported; // images already reported, so a repeat start is silent
};

} // namespace bcg
