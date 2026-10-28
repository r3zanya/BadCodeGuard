#include "ProcessMonitor.h"

#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <unordered_map>

namespace bcg {

namespace {

constexpr int kMaxCache = 20000;

quint64 fileTimeToU64(const FILETIME& ft)
{
    return (quint64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Opens a process and reads its start time and image path. Returns false when
// the process is protected or gone.
bool describe(DWORD pid, quint64& startTime, QString& image)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return false;
    FILETIME created, exited, kernel, user;
    const bool timed = GetProcessTimes(h, &created, &exited, &kernel, &user);
    wchar_t buffer[32768];
    DWORD len = DWORD(std::size(buffer));
    const bool named = QueryFullProcessImageNameW(h, 0, buffer, &len);
    CloseHandle(h);
    if (!timed || !named)
        return false;
    startTime = fileTimeToU64(created);
    image = QDir::fromNativeSeparators(QString::fromWCharArray(buffer, int(len)));
    return true;
}

} // namespace

ProcessMonitor::ProcessMonitor(ScanContext context, QObject* parent)
    : QObject(parent)
    , m_ctx(context)
    , m_scanner(context)
{
}

ProcessMonitor::~ProcessMonitor()
{
    stop();
}

void ProcessMonitor::start()
{
    if (m_running)
        return;
    m_running = true;
    m_thread = std::thread([this] { loop(); });
}

void ProcessMonitor::stop()
{
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

void ProcessMonitor::loop()
{
    const DWORD self = GetCurrentProcessId();
    std::unordered_map<DWORD, quint64> known; // pid -> start time of the process seen under that pid
    bool first = true;

    while (m_running) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        std::unordered_map<DWORD, quint64> now;
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Process32FirstW(snap, &entry); ok && m_running;
             ok = Process32NextW(snap, &entry)) {
            const DWORD pid = entry.th32ProcessID;
            if (pid == 0 || pid == 4 || pid == self)
                continue;
            const auto old = known.find(pid);
            quint64 startTime = 0;
            QString image;
            if (old != known.end()) {
                now[pid] = old->second; // seen before: its start time cannot change
                continue;
            }
            if (!describe(pid, startTime, image)) {
                now[pid] = 0; // protected or already gone: do not retry every round
                continue;
            }
            now[pid] = startTime;
            handle(pid, startTime, image, first);
        }
        CloseHandle(snap);
        known.swap(now);
        first = false;

        for (int waited = 0; waited < m_intervalMs && m_running; waited += 50)
            Sleep(50);
    }
}

void ProcessMonitor::handle(quint32 pid, quint64 startTime, const QString& image, bool atStartup)
{
    const QFileInfo info(image);
    if (!info.isFile() || info.size() <= 0)
        return;
    if (m_ctx.exclusions && m_ctx.exclusions->matches(image))
        return;
    // Windows' own programs are not examined; they are the platform, not a guess.
    if (Quarantine::isProtectedPath(image))
        return;

    const Stamp stamp{info.size(), info.lastModified().toMSecsSinceEpoch()};
    if (m_clean.contains(image) && m_clean.value(image) == stamp)
        return;
    const bool wasReported = m_reported.contains(image) && m_reported.value(image) == stamp;
    if (wasReported && atStartup)
        return;

    Detection detection;
    if (!m_scanner.inspect(image, detection)) {
        if (m_clean.size() >= kMaxCache)
            m_clean.clear();
        m_clean.insert(image, stamp);
        return;
    }

    bool terminated = false;
    const bool certain = detection.kind != ThreatKind::Heuristic;
    if (wasReported && !certain)
        return; // a suspicious program that was already reported: do not repeat
    if (certain && !atStartup) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                               FALSE, pid);
        if (h) {
            // The pid may have been reused while the file was being checked.
            FILETIME created, exited, kernel, user;
            if (GetProcessTimes(h, &created, &exited, &kernel, &user)
                && fileTimeToU64(created) == startTime && TerminateProcess(h, 1)) {
                terminated = WaitForSingleObject(h, 3000) == WAIT_OBJECT_0;
            }
            CloseHandle(h);
        }
        if (!terminated)
            log::warn(QStringLiteral("Не удалось завершить процесс %1 (%2)").arg(pid).arg(image));
    }

    QString quarantineId;
    QString quarantineError;
    // Programs that were already running when we started are only reported: a
    // running file is locked anyway, and they were there before the guard.
    const bool autoQuarantine = m_ctx.quarantine && m_ctx.options && m_ctx.options->autoQuarantine
        && certain && !atStartup;
    if (autoQuarantine) {
        // The file is released a moment after the process exits.
        for (int attempt = 0; attempt < 5; ++attempt) {
            const QuarantineResult r = m_ctx.quarantine->add(image, detection.name, int(detection.kind));
            if (r.ok) {
                quarantineId = r.entry.id;
                quarantineError.clear();
                break;
            }
            quarantineError = r.error;
            Sleep(300);
        }
    }

    if (m_reported.size() >= kMaxCache)
        m_reported.clear();
    m_reported.insert(image, stamp);
    if (wasReported)
        return; // stopped again, but already told the user once
    log::info(QStringLiteral("Запуск программы: %1 (%2)%3")
                  .arg(detection.name, image, terminated ? QStringLiteral(", процесс завершён") : QString()));
    emit threatFound(image, detection.name, int(detection.kind), quarantineId, quarantineError,
                     terminated, pid);
}

} // namespace bcg
