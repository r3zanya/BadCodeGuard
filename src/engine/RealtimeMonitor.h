#pragma once

#include "FileScanner.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QWaitCondition>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace bcg {

// Watches folders and checks every file that appears or changes in them, with
// the same engine the manual scan uses. One thread per folder listens for
// changes (ReadDirectoryChangesW); one checker thread waits until a file stops
// changing, then inspects it. Signals may be emitted from those threads: connect
// with a receiver in the UI thread (queued) as usual.
class RealtimeMonitor : public QObject {
    Q_OBJECT
public:
    // `onlyRisky` limits a busy folder (Temp) to file types that can do harm:
    // programs, scripts, shortcuts, archives, documents.
    struct WatchFolder {
        QString path;
        bool onlyRisky = false;
    };

    explicit RealtimeMonitor(ScanContext context, QObject* parent = nullptr);
    ~RealtimeMonitor() override;

    // Starts (or restarts) watching. Folders that do not exist are skipped.
    // Returns the number of folders actually watched.
    int start(const QList<WatchFolder>& folders);
    int start(const QStringList& folders);
    void stop();
    bool isRunning() const { return m_running; }
    // Folders whose listener thread is still alive; lower than the number that
    // were started means a folder disappeared or a listener failed.
    int aliveWatchers() const { return m_alive; }

    static bool isRiskyFile(const QString& path);

signals:
    void threatFound(const QString& path, const QString& threatName, int kind,
                     const QString& quarantineId, const QString& quarantineError);
    // Total number of files inspected since start().
    void checkedChanged(qint64 total);

private:
    struct Pending {
        qint64 lastEventMs = 0;
        int attempts = 0;
    };
    struct FileStamp {
        qint64 size = 0;
        qint64 modifiedMs = 0;
    };

    void watchFolder(QString folder, bool onlyRisky);
    void checkerLoop();
    void enqueue(const QString& path);
    void process(const QString& path, bool& retry);

    ScanContext m_ctx;
    FileScanner m_scanner;

    std::atomic_bool m_running{false};
    std::atomic<int> m_alive{0};
    void* m_stopEvent = nullptr; // Win32 event; wakes the watcher threads
    std::vector<std::thread> m_watchers;
    std::thread m_checker;

    QMutex m_mutex;
    QWaitCondition m_wake;
    QHash<QString, Pending> m_pending;
    QHash<QString, FileStamp> m_seen; // files already found clean, by path
    std::atomic<qint64> m_checked{0};
};

} // namespace bcg
