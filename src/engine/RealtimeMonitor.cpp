#include "RealtimeMonitor.h"

#include "Log.h"

#include <QDateTime>
#include <QSet>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bcg {

namespace {

constexpr qint64 kSettleMs = 1000;     // a file must be quiet this long before it is checked
constexpr int kMaxAttempts = 60;       // ~ a minute of waiting for a locked file
constexpr int kMaxSeen = 20000;
constexpr DWORD kBufferSize = 64 * 1024;

QString key(const QString& path)
{
    return QDir::fromNativeSeparators(path).toLower();
}

bool isTransient(const QString& path)
{
    // Browsers write to these while downloading and rename to the real name
    // when done; the rename is what we check.
    static const char* const kSuffixes[] = {".crdownload", ".part", ".partial", ".download",
                                            ".opdownload"};
    for (const char* s : kSuffixes) {
        if (path.endsWith(QLatin1String(s), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace

RealtimeMonitor::RealtimeMonitor(ScanContext context, QObject* parent)
    : QObject(parent)
    , m_ctx(context)
    , m_scanner(context)
{
}

RealtimeMonitor::~RealtimeMonitor()
{
    stop();
}

bool RealtimeMonitor::isRiskyFile(const QString& path)
{
    static const QSet<QString> kRisky = {
        QStringLiteral("exe"),  QStringLiteral("dll"),  QStringLiteral("scr"),  QStringLiteral("com"),
        QStringLiteral("cpl"),  QStringLiteral("msi"),  QStringLiteral("msp"),  QStringLiteral("sys"),
        QStringLiteral("ocx"),  QStringLiteral("bat"),  QStringLiteral("cmd"),  QStringLiteral("ps1"),
        QStringLiteral("psm1"), QStringLiteral("vbs"),  QStringLiteral("vbe"),  QStringLiteral("js"),
        QStringLiteral("jse"),  QStringLiteral("wsf"),  QStringLiteral("wsh"),  QStringLiteral("hta"),
        QStringLiteral("lnk"),  QStringLiteral("jar"),  QStringLiteral("zip"),  QStringLiteral("rar"),
        QStringLiteral("7z"),   QStringLiteral("cab"),  QStringLiteral("iso"),  QStringLiteral("doc"),
        QStringLiteral("docx"), QStringLiteral("docm"), QStringLiteral("xls"),  QStringLiteral("xlsx"),
        QStringLiteral("xlsm"), QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("pptm"),
        QStringLiteral("pdf")};
    return kRisky.contains(QFileInfo(path).suffix().toLower());
}

int RealtimeMonitor::start(const QStringList& folders)
{
    QList<WatchFolder> list;
    for (const QString& f : folders)
        list.append({f, false});
    return start(list);
}

int RealtimeMonitor::start(const QList<WatchFolder>& folders)
{
    stop();

    m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); // manual reset
    if (!m_stopEvent)
        return 0;

    int watched = 0;
    m_running = true;
    m_alive = 0;
    for (const WatchFolder& folder : folders) {
        if (!QDir(folder.path).exists())
            continue;
        ++watched;
        m_watchers.emplace_back(
            [this, path = folder.path, risky = folder.onlyRisky] { watchFolder(path, risky); });
    }
    if (watched == 0) {
        m_running = false;
        CloseHandle(static_cast<HANDLE>(m_stopEvent));
        m_stopEvent = nullptr;
        return 0;
    }
    m_checked = 0;
    m_checker = std::thread([this] { checkerLoop(); });
    return watched;
}

void RealtimeMonitor::stop()
{
    if (!m_running && !m_stopEvent)
        return;
    m_running = false;
    if (m_stopEvent)
        SetEvent(static_cast<HANDLE>(m_stopEvent));
    {
        QMutexLocker lock(&m_mutex);
        m_wake.wakeAll();
    }
    for (std::thread& t : m_watchers) {
        if (t.joinable())
            t.join();
    }
    m_watchers.clear();
    if (m_checker.joinable())
        m_checker.join();
    if (m_stopEvent) {
        CloseHandle(static_cast<HANDLE>(m_stopEvent));
        m_stopEvent = nullptr;
    }
    QMutexLocker lock(&m_mutex);
    m_pending.clear();
    m_seen.clear();
}

void RealtimeMonitor::enqueue(const QString& path)
{
    if (isTransient(path))
        return;
    QMutexLocker lock(&m_mutex);
    Pending& p = m_pending[path];
    p.lastEventMs = QDateTime::currentMSecsSinceEpoch();
    p.attempts = 0;
    m_wake.wakeOne();
}

// Blocks, reporting changed files, until stop() sets the stop event.
void RealtimeMonitor::watchFolder(QString folder, bool onlyRisky)
{
    const QString root = QDir::toNativeSeparators(QDir(folder).absolutePath());
    HANDLE dir = CreateFileW(reinterpret_cast<LPCWSTR>(root.utf16()), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                             nullptr);
    if (dir == INVALID_HANDLE_VALUE) {
        log::warn(QStringLiteral("Не удалось начать слежение за папкой: %1").arg(root));
        return;
    }
    ++m_alive;

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    alignas(DWORD) BYTE buffer[kBufferSize];
    HANDLE waitOn[2] = {ov.hEvent, static_cast<HANDLE>(m_stopEvent)};

    while (m_running) {
        ResetEvent(ov.hEvent);
        if (!ReadDirectoryChangesW(dir, buffer, kBufferSize, TRUE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE
                                       | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                   nullptr, &ov, nullptr))
            break;
        if (WaitForMultipleObjects(2, waitOn, FALSE, INFINITE) != WAIT_OBJECT_0) {
            CancelIoEx(dir, &ov);
            DWORD ignored = 0;
            GetOverlappedResult(dir, &ov, &ignored, TRUE);
            break;
        }
        DWORD bytes = 0;
        if (!GetOverlappedResult(dir, &ov, &bytes, FALSE))
            break;
        if (bytes == 0)
            continue; // buffer overflowed: events were lost, keep listening

        const BYTE* cursor = buffer;
        for (;;) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
            if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_MODIFIED
                || info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                const QString name = QString::fromWCharArray(info->FileName,
                                                             int(info->FileNameLength / 2));
                const QString full = QDir::cleanPath(root + QLatin1Char('\\') + name);
                if (!onlyRisky || isRiskyFile(full))
                    enqueue(full);
            }
            if (info->NextEntryOffset == 0)
                break;
            cursor += info->NextEntryOffset;
        }
    }
    CloseHandle(ov.hEvent);
    CloseHandle(dir);
    --m_alive;
    if (m_running)
        log::warn(QStringLiteral("Слежение за папкой остановилось: %1").arg(root));
}

void RealtimeMonitor::checkerLoop()
{
    while (m_running) {
        QStringList ready;
        {
            QMutexLocker lock(&m_mutex);
            if (m_pending.isEmpty())
                m_wake.wait(&m_mutex, 1000);
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
                if (now - it->lastEventMs >= kSettleMs)
                    ready << it.key();
            }
        }
        if (ready.isEmpty()) {
            QMutexLocker lock(&m_mutex);
            if (m_running)
                m_wake.wait(&m_mutex, 300);
            continue;
        }

        for (const QString& path : ready) {
            if (!m_running)
                return;
            bool retry = false;
            process(path, retry);
            QMutexLocker lock(&m_mutex);
            auto it = m_pending.find(path);
            if (it == m_pending.end())
                continue;
            if (retry && ++it->attempts < kMaxAttempts)
                it->lastEventMs = QDateTime::currentMSecsSinceEpoch(); // try again later
            else
                m_pending.erase(it);
        }
    }
}

// Checks one settled file. Sets `retry` when it could not be read yet.
void RealtimeMonitor::process(const QString& path, bool& retry)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0)
        return;
    if (m_ctx.exclusions && m_ctx.exclusions->matches(path))
        return;
    if (m_ctx.quarantine
        && (key(path) + QLatin1Char('/')).startsWith(key(m_ctx.quarantine->root()) + QLatin1Char('/')))
        return;

    // A file still held by its writer cannot be opened for reading yet.
    {
        QFile probe(path);
        if (!probe.open(QIODevice::ReadOnly)) {
            retry = true;
            return;
        }
    }

    const FileStamp stamp{info.size(), info.lastModified().toMSecsSinceEpoch()};
    {
        QMutexLocker lock(&m_mutex);
        const auto seen = m_seen.constFind(path);
        if (seen != m_seen.cend() && seen->size == stamp.size && seen->modifiedMs == stamp.modifiedMs)
            return; // unchanged since it was found clean
    }

    Detection detection;
    const bool threat = m_scanner.inspect(path, detection);
    emit checkedChanged(++m_checked);
    {
        // Remember the verdict so repeated change events do not re-check (or
        // re-report) a file that has not changed.
        QMutexLocker lock(&m_mutex);
        if (m_seen.size() >= kMaxSeen)
            m_seen.clear();
        m_seen.insert(path, stamp);
    }
    if (!threat)
        return;

    QString quarantineId;
    QString quarantineError;
    const bool autoQuarantine = m_ctx.quarantine && m_ctx.options && m_ctx.options->autoQuarantine
        && detection.kind != ThreatKind::Heuristic;
    if (autoQuarantine) {
        const QuarantineResult r = m_ctx.quarantine->add(path, detection.name, int(detection.kind));
        if (r.ok)
            quarantineId = r.entry.id;
        else
            quarantineError = r.error;
    }
    emit threatFound(path, detection.name, int(detection.kind), quarantineId, quarantineError);
}

} // namespace bcg
