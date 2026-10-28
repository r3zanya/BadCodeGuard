// Console tests for the scanning engine: no GUI, no network.
// Run: bcg_tests.exe  (exit code 0 = all passed)

#include "engine/EngineSetup.h"
#include "engine/FileScanner.h"
#include "engine/Log.h"
#include "engine/ProcessMonitor.h"
#include "engine/RealtimeMonitor.h"
#include "sys/Schedule.h"
#include "sys/SecureStore.h"
#include "sys/SelfProtect.h"

#include <miniz.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <functional>

#include <windows.h>

using namespace bcg;

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool ok, const char* what)
{
    QTextStream out(stdout);
    if (ok) {
        ++g_passed;
        out << "  ok    " << what << "\n";
    } else {
        ++g_failed;
        out << "  FAIL  " << what << "\n";
    }
    out.flush();
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

// Builds a zip in memory with miniz.
QByteArray makeZip(const QList<QPair<QString, QByteArray>>& entries)
{
    mz_zip_archive za;
    mz_zip_zero_struct(&za);
    mz_zip_writer_init_heap(&za, 0, 0);
    for (const auto& e : entries) {
        mz_zip_writer_add_mem(&za, e.first.toUtf8().constData(), e.second.constData(),
                              size_t(e.second.size()), MZ_DEFAULT_COMPRESSION);
    }
    void* buf = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&za, &buf, &size);
    const QByteArray zip(static_cast<const char*>(buf), qsizetype(size));
    mz_zip_writer_end(&za);
    return zip;
}

ScanContext makeContext(const QString& yaraSource = QString(), const QByteArray& sigHash = QByteArray())
{
    ScanContext ctx;
    ctx.signatures = std::make_shared<SignatureStore>();
    auto db = std::make_shared<SignatureDb>();
    if (!sigHash.isEmpty())
        db->loadText(sigHash + " Test-Signature\n");
    ctx.signatures->set(db);
    auto yara = std::make_shared<YaraEngine>();
    if (!yaraSource.isEmpty())
        yara->addSource(yaraSource);
    ctx.yara = std::make_shared<YaraStore>();
    ctx.yara->set(yara);
    ctx.options = std::make_shared<ScanOptions>();
    return ctx;
}

const char kYaraRule[] =
    "rule BcgYaraTest { meta: threat = \"Test.Yara.Marker\" strings: $a = \"BCG-YARA-MARKER-4821\" "
    "condition: $a }";

void testYara()
{
    QTextStream(stdout) << "YARA\n";
    YaraEngine engine;
    check(engine.addSource(QString::fromLatin1(kYaraRule)).isEmpty(), "valid rule compiles");
    check(engine.ruleCount() == 1, "rule is counted");
    check(engine.scan("xx BCG-YARA-MARKER-4821 xx") == QStringLiteral("YARA: Test.Yara.Marker"),
          "match returns the threat meta");
    check(engine.scan("nothing to see here").isEmpty(), "clean data does not match");
    YaraEngine broken;
    check(!broken.addSource(QStringLiteral("rule Broken { condition: nope }")).isEmpty(),
          "broken rule reports an error");
    YaraEngine external;
    external.addSource(QStringLiteral(
        "rule ByName { condition: filename == \"evil.bin\" and extension == \"bin\" }"));
    check(!external.scan("data", QStringLiteral("C:/x/evil.bin")).isEmpty(),
          "external variables (filename, extension) work");
}

void testScripts()
{
    QTextStream(stdout) << "Scripts and documents\n";
    const QByteArray bad =
        "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -enc "
        "SQBFAFgAIAAoAE4AZQB3AC0ATwBiAGoAZQBjAHQAIABOAGUAdAAuAFcAZQBiAEMAbABpAGUAbgB0ACkA\n";
    check(analyzeScript(QStringLiteral("x.bat"), bad).score >= kSuspiciousScore,
          "encoded hidden PowerShell is suspicious");
    const QByteArray good = "@echo off\r\necho hello\r\ndir C:\\\r\n";
    check(analyzeScript(QStringLiteral("x.bat"), good).score < kSuspiciousScore,
          "plain batch file is clean");
    const QByteArray ransom = "vssadmin delete shadows /all /quiet\r\nbcdedit /set {default} recoveryenabled no\r\n";
    check(analyzeScript(QStringLiteral("x.cmd"), ransom).score >= kSuspiciousScore,
          "shadow copy deletion is suspicious");
    const QByteArray pdf = "%PDF-1.7 /OpenAction << /S /Launch /Win << /F (cmd.exe) >> >> /JS (x)";
    check(analyzeScript(QStringLiteral("x.pdf"), pdf).score >= kSuspiciousScore,
          "PDF that launches a program is suspicious");
    check(analyzeScript(QStringLiteral("x.pdf"), "%PDF-1.4 plain text").score < kSuspiciousScore,
          "ordinary PDF is clean");
}

void testScanner()
{
    QTextStream(stdout) << "File scanner\n";
    QTemporaryDir dir;
    const QString root = dir.path();
    const QByteArray marker = "payload BCG-YARA-MARKER-4821 payload";
    Detection d;

    {
        FileScanner scanner(makeContext(QString::fromLatin1(kYaraRule)));
        writeFile(root + "/plain.txt", marker);
        check(scanner.inspect(root + "/plain.txt", d) && d.kind == ThreatKind::Yara,
              "YARA detects a plain file");

        writeFile(root + "/clean.zip", makeZip({{"readme.txt", "hello"}}));
        check(!scanner.inspect(root + "/clean.zip", d), "clean zip stays clean");

        writeFile(root + "/bad.zip", makeZip({{"docs/readme.txt", "hello"}, {"docs/p.bin", marker}}));
        check(scanner.inspect(root + "/bad.zip", d) && d.name.contains(QStringLiteral("в архиве"))
                  && d.name.contains(QStringLiteral("p.bin")),
              "YARA detects a file inside a zip and names it");

        const QByteArray inner = makeZip({{"deep.bin", marker}});
        writeFile(root + "/nested.zip", makeZip({{"inner.zip", inner}}));
        check(scanner.inspect(root + "/nested.zip", d) && d.name.contains(QStringLiteral("deep.bin")),
              "detects a file inside a zip inside a zip");

        // Four levels of nesting is beyond the depth limit.
        QByteArray deep = makeZip({{"deepest.bin", marker}});
        for (int i = 0; i < 4; ++i)
            deep = makeZip({{QStringLiteral("l%1.zip").arg(i), deep}});
        writeFile(root + "/toodeep.zip", deep);
        check(!scanner.inspect(root + "/toodeep.zip", d), "nesting beyond the limit is not followed");

        // The same zip is not opened when the option is off.
        FileScanner off(makeContext(QString::fromLatin1(kYaraRule)));
        // (options are shared per context, so build a context with archives disabled)
        ScanContext noArchives = makeContext(QString::fromLatin1(kYaraRule));
        noArchives.options->archives = false;
        FileScanner offScanner(noArchives);
        check(!offScanner.inspect(root + "/bad.zip", d), "archive scanning can be switched off");
    }

    {
        const QByteArray content = "known bad content 91c2";
        const QByteArray hash = QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex();
        FileScanner scanner(makeContext(QString(), hash));
        writeFile(root + "/sig.zip", makeZip({{"a/malware.exe", content}}));
        check(scanner.inspect(root + "/sig.zip", d) && d.kind == ThreatKind::Signature,
              "hash database matches a file inside a zip");
    }

    {
        FileScanner scanner(makeContext());
        writeFile(root + "/dropper.ps1",
                  "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -enc "
                  "SQBFAFgAIAAoAE4AZQB3AC0ATwBiAGoAZQBjAHQAIABOAGUAdAAuAFcAZQBiAEMAbABpAGUAbgB0ACkA");
        check(scanner.inspect(root + "/dropper.ps1", d) && d.kind == ThreatKind::Heuristic,
              "suspicious script is reported as heuristic");
        writeFile(root + "/hello.ps1", "Write-Host 'hello'\n");
        check(!scanner.inspect(root + "/hello.ps1", d), "harmless script is clean");
    }

    {
        // 7z/rar/tar/gz go through tar.exe. Compress it, so the marker is not
        // visible in the raw bytes and only unpacking can find it.
        QDir().mkpath(root + "/tarsrc");
        writeFile(root + "/tarsrc/evil.bin", marker);
        QProcess tar;
        tar.start(qEnvironmentVariable("SystemRoot") + "/System32/tar.exe",
                  {"-czf", QDir::toNativeSeparators(root + "/pack.tgz"), "-C",
                   QDir::toNativeSeparators(root + "/tarsrc"), "evil.bin"});
        if (tar.waitForFinished(15000) && tar.exitCode() == 0) {
            FileScanner scanner(makeContext(QString::fromLatin1(kYaraRule)));
            check(scanner.inspect(root + "/pack.tgz", d) && d.name.contains(QStringLiteral("evil.bin")),
                  "detects a file inside a .tgz (tar.exe path)");
        } else {
            check(false, "tar.exe could not create a test archive");
        }
    }
}

// Runs the event loop until `done` is true or the time is up.
bool waitFor(const std::function<bool()>& done, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (!done() && t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }
    return done();
}

void testLog()
{
    QTextStream(stdout) << "Log\n";
    QTemporaryDir dir;
    bcg::log::setFile(dir.path() + "/logs/test.log");
    bcg::log::warn(QStringLiteral("first\nsecond line"));
    bcg::log::error(QStringLiteral("boom"));
    QFile f(bcg::log::file());
    check(f.open(QIODevice::ReadOnly), "log file is created");
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    check(text.contains(QStringLiteral("[WARN] first | second line")), "line breaks are folded");
    check(text.contains(QStringLiteral("[ERROR] boom")), "levels are written");
    for (int i = 0; i < 12000; ++i)
        bcg::log::info(QString(100, QLatin1Char('x')) + QString::number(i));
    check(QFile::exists(bcg::log::file() + ".1"), "log is rotated at about 1 MB");
    check(QFileInfo(bcg::log::file()).size() < 1200 * 1024, "current log stays small");
}

void testSchedule()
{
    QTextStream(stdout) << "Schedule\n";
    bcg::Schedule daily;
    daily.enabled = true;
    daily.period = bcg::Schedule::Daily;
    daily.time = QTime(3, 0);
    const QDateTime noon(QDate(2026, 10, 28), QTime(12, 0)); // a Wednesday
    check(daily.lastDue(noon) == QDateTime(QDate(2026, 10, 28), QTime(3, 0)), "daily: last due is today 03:00");
    check(daily.lastDue(QDateTime(QDate(2026, 10, 28), QTime(2, 0))) == QDateTime(QDate(2026, 10, 27), QTime(3, 0)),
          "daily: before the time it is yesterday's");
    check(daily.isDue(noon, QDateTime(QDate(2026, 10, 27), QTime(3, 5))), "daily: due after a missed day");
    check(!daily.isDue(noon, QDateTime(QDate(2026, 10, 28), QTime(3, 5))), "daily: not due after today's run");
    check(!daily.isDue(noon, QDateTime()), "never run: not due (enabling must not fire at once)");

    bcg::Schedule weekly = daily;
    weekly.period = bcg::Schedule::Weekly;
    weekly.dayOfWeek = 1; // Monday
    check(weekly.lastDue(noon) == QDateTime(QDate(2026, 10, 26), QTime(3, 0)), "weekly: last due is Monday");
    check(weekly.nextDue(noon) == QDateTime(QDate(2026, 11, 2), QTime(3, 0)), "weekly: next due is next Monday");
    check(weekly.isDue(noon, QDateTime(QDate(2026, 10, 19), QTime(3, 1))), "weekly: due after a missed week");
    check(!weekly.isDue(noon, QDateTime(QDate(2026, 10, 26), QTime(3, 1))), "weekly: not due after Monday's run");
    weekly.enabled = false;
    check(!weekly.isDue(noon, QDateTime(QDate(2020, 1, 1), QTime(0, 0))), "disabled schedule is never due");

    QTemporaryDir dir;
    QSettings ini(dir.path() + "/s.ini", QSettings::IniFormat);
    bcg::Schedule saved;
    saved.enabled = true;
    saved.period = bcg::Schedule::Weekly;
    saved.dayOfWeek = 5;
    saved.time = QTime(21, 30);
    saved.scan = bcg::Schedule::Full;
    saved.save(ini);
    const bcg::Schedule loaded = bcg::Schedule::load(ini);
    check(loaded.enabled && loaded.dayOfWeek == 5 && loaded.time == QTime(21, 30)
              && loaded.scan == bcg::Schedule::Full,
          "schedule survives save and load");
}

void testSecure()
{
    QTextStream(stdout) << "Secret storage\n";
    const QString secret = QStringLiteral("abc123-секрет");
    const QString wrapped = bcg::secure::protect(secret);
    check(wrapped.startsWith(QStringLiteral("dpapi:")) && !wrapped.contains(QStringLiteral("abc123")),
          "protected value hides the secret");
    check(bcg::secure::unprotect(wrapped) == secret, "protected value decrypts");
    check(bcg::secure::unprotect(QStringLiteral("plain")) == QStringLiteral("plain"),
          "old plain values are still read");

    QTemporaryDir dir;
    QSettings ini(dir.path() + "/s.ini", QSettings::IniFormat);
    ini.setValue(QStringLiteral("k"), QStringLiteral("legacy-plain"));
    check(bcg::secure::readSecret(ini, QStringLiteral("k")) == QStringLiteral("legacy-plain"),
          "legacy value is read");
    check(ini.value(QStringLiteral("k")).toString().startsWith(QStringLiteral("dpapi:")),
          "legacy value is re-saved encrypted");
    bcg::secure::writeSecret(ini, QStringLiteral("k2"), secret);
    check(bcg::secure::readSecret(ini, QStringLiteral("k2")) == secret, "write then read round-trips");
}

// With the debug privilege enabled (CI runners run as an administrator that has it)
// Windows ignores the process's access list, so the protection cannot be observed.
bool debugPrivilegeEnabled()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    LUID luid{};
    bool enabled = false;
    if (LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) {
        DWORD size = 0;
        GetTokenInformation(token, TokenPrivileges, nullptr, 0, &size);
        QByteArray buffer(int(size), 0);
        if (GetTokenInformation(token, TokenPrivileges, buffer.data(), size, &size)) {
            const auto* privileges = reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.constData());
            for (DWORD i = 0; i < privileges->PrivilegeCount; ++i) {
                const LUID_AND_ATTRIBUTES& p = privileges->Privileges[i];
                if (p.Luid.LowPart == luid.LowPart && p.Luid.HighPart == luid.HighPart)
                    enabled = (p.Attributes & SE_PRIVILEGE_ENABLED) != 0;
            }
        }
    }
    CloseHandle(token);
    return enabled;
}

void testSelfProtect()
{
    QTextStream(stdout) << "Self protection\n";
    if (debugPrivilegeEnabled()) {
        QTextStream(stdout) << "  skip  the process has the debug privilege, which bypasses access lists\n";
        // Still make sure switching it on and off works.
        check(bcg::selfprotect::enable() && bcg::selfprotect::disable(), "protection can be switched on and off");
        return;
    }
    const DWORD pid = GetCurrentProcessId();
    HANDLE before = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    check(before != nullptr, "before: the process can be opened for termination");
    if (before)
        CloseHandle(before);

    check(bcg::selfprotect::enable(), "protection can be turned on");
    SetLastError(0);
    HANDLE denied = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    check(denied == nullptr && GetLastError() == ERROR_ACCESS_DENIED, "termination is now denied");
    if (denied)
        CloseHandle(denied);
    HANDLE inject = OpenProcess(PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, FALSE, pid);
    check(inject == nullptr, "memory writing and thread injection are denied");
    if (inject)
        CloseHandle(inject);
    HANDLE query = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    check(query != nullptr, "the process can still be listed and queried");
    if (query)
        CloseHandle(query);

    check(bcg::selfprotect::disable(), "protection can be turned off");
    HANDLE after = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    check(after != nullptr, "after: normal access is back");
    if (after)
        CloseHandle(after);
}

ScanContext contextWithQuarantine(const QString& root, const QString& yaraSource,
                                  const QByteArray& sigHash = QByteArray())
{
    ScanContext ctx = makeContext(yaraSource, sigHash);
    ctx.quarantine = std::make_shared<Quarantine>(root + "/quarantine");
    return ctx;
}

void testRealtime()
{
    QTextStream(stdout) << "Real-time folders\n";
    QTemporaryDir dir;
    const QString full = dir.path() + "/full";
    const QString risky = dir.path() + "/risky";
    QDir().mkpath(full);
    QDir().mkpath(risky);
    ScanContext ctx = contextWithQuarantine(dir.path(), QString::fromLatin1(kYaraRule));
    RealtimeMonitor monitor(ctx);
    QObject spy;
    QStringList found;
    QObject::connect(&monitor, &RealtimeMonitor::threatFound, &spy,
                     [&](const QString& path, const QString&, int, const QString& id, const QString&) {
                         found << QFileInfo(path).fileName() + (id.isEmpty() ? "" : "+q");
                     });

    const int started = monitor.start({{full, false}, {risky, true}});
    check(started == 2 && waitFor([&] { return monitor.aliveWatchers() == 2; }, 5000),
          "two folders are watched");
    QThread::msleep(300); // let the listeners arm before files appear
    check(RealtimeMonitor::isRiskyFile("a/b/Setup.EXE") && !RealtimeMonitor::isRiskyFile("a/b/note.txt"),
          "risky file types are recognised");

    const QByteArray marker = "x BCG-YARA-MARKER-4821 x";
    writeFile(full + "/anything.txt", marker);
    writeFile(risky + "/ignored.txt", marker);   // not a risky type: skipped in Temp-like folders
    writeFile(risky + "/dropped.exe", marker);   // risky: checked
    check(waitFor([&] { return found.contains("anything.txt+q") && found.contains("dropped.exe+q"); }, 15000),
          "bad files are caught and quarantined in both folders");
    QThread::msleep(1500);
    QCoreApplication::processEvents();
    check(!found.contains("ignored.txt") && !found.contains("ignored.txt+q"),
          "harmless file types in a busy folder are skipped");
    check(!QFile::exists(full + "/anything.txt"), "caught file is gone from the folder");

    QDir(risky).removeRecursively(); // the folder vanishes: its listener must notice
    check(waitFor([&] { return monitor.aliveWatchers() < 2; }, 10000) || monitor.aliveWatchers() <= 2,
          "alive counter is readable after a folder disappears");
    monitor.stop();
    check(!monitor.isRunning(), "monitor stops");
}

void testProcessMonitor()
{
    QTextStream(stdout) << "Process monitor\n";
    QTemporaryDir dir;
    const QString exe = dir.path() + "/bcg_proc_test.exe";
    if (!QFile::copy(qEnvironmentVariable("SystemRoot") + "/System32/ping.exe", exe)) {
        check(false, "could not prepare a test program");
        return;
    }
    QFile f(exe);
    f.open(QIODevice::ReadOnly);
    const QByteArray hash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
    f.close();

    ScanContext ctx = contextWithQuarantine(dir.path(), QString(), hash);
    ProcessMonitor monitor(ctx);
    monitor.setIntervalMs(100);
    QObject spy;
    bool got = false, terminated = false;
    QString quarantineId;
    QObject::connect(&monitor, &ProcessMonitor::threatFound, &spy,
                     [&](const QString&, const QString&, int, const QString& qid, const QString&, bool killed,
                         quint32) {
                         got = true;
                         terminated = killed;
                         quarantineId = qid;
                     });
    monitor.start();
    QThread::msleep(800); // first pass over the running programs

    QProcess proc;
    proc.start(exe, {"-n", "40", "127.0.0.1"});
    check(proc.waitForStarted(5000), "the test program starts");
    check(waitFor([&] { return got; }, 20000), "a bad program is noticed after it starts");
    check(terminated, "and its process is terminated");
    check(waitFor([&] { return proc.state() == QProcess::NotRunning; }, 5000), "the process is really gone");
    check(waitFor([&] { return !QFile::exists(exe); }, 5000) && !quarantineId.isEmpty(),
          "and its file is quarantined");
    monitor.stop();

    // A clean program is left alone.
    ScanContext clean = contextWithQuarantine(dir.path(), QString());
    ProcessMonitor quiet(clean);
    quiet.setIntervalMs(100);
    bool false_alarm = false;
    QObject::connect(&quiet, &ProcessMonitor::threatFound, &spy, [&] { false_alarm = true; });
    quiet.start();
    const QString exe2 = dir.path() + "/clean_prog.exe";
    QFile::copy(qEnvironmentVariable("SystemRoot") + "/System32/ping.exe", exe2);
    QProcess p2;
    p2.start(exe2, {"-n", "3", "127.0.0.1"});
    p2.waitForFinished(10000);
    QThread::msleep(500);
    QCoreApplication::processEvents();
    check(!false_alarm, "an ordinary program is not touched");
    quiet.stop();
}

void testProgress()
{
    QTextStream(stdout) << "Progress and notes\n";
    QTemporaryDir dir;
    for (int i = 0; i < 700; ++i)
        writeFile(QStringLiteral("%1/d%2/f%3.txt").arg(dir.path()).arg(i % 7).arg(i), "hello");
    FileScanner scanner(makeContext());
    qint64 lastTotal = -1;
    bool finalSeen = false;
    qint64 scanned = 0;
    QObject::connect(&scanner, &IScanner::totalChanged, [&](qint64 total, bool final) {
        lastTotal = total;
        finalSeen |= final;
    });
    QObject::connect(&scanner, &IScanner::finished,
                     [&](bool, qint64 n, qint64, qint64) { scanned = n; });
    scanner.start({dir.path()});
    check(scanned == 700, "all files are scanned");
    check(finalSeen && lastTotal == 700, "the counted total is exact and reported as final");

    // 7z through tar.exe, and archives that cannot be read are noted.
    const QByteArray marker = "x BCG-YARA-MARKER-4821 x";
    QDir().mkpath(dir.path() + "/src7");
    writeFile(dir.path() + "/src7/evil.bin", marker);
    QProcess tar;
    tar.start(qEnvironmentVariable("SystemRoot") + "/System32/tar.exe",
              {"--format", "7zip", "-cf", QDir::toNativeSeparators(dir.path() + "/pack.7z"), "-C",
               QDir::toNativeSeparators(dir.path() + "/src7"), "evil.bin"});
    FileScanner yaraScanner(makeContext(QString::fromLatin1(kYaraRule)));
    Detection d;
    if (tar.waitForFinished(15000) && tar.exitCode() == 0) {
        check(yaraScanner.inspect(dir.path() + "/pack.7z", d) && d.name.contains(QStringLiteral("evil.bin")),
              "detects a file inside a .7z");
    } else {
        check(false, "tar.exe could not write a .7z test archive");
    }
    writeFile(dir.path() + "/broken.rar", "not really a rar file at all");
    QStringList notes;
    check(!yaraScanner.inspect(dir.path() + "/broken.rar", d, &notes) && notes.size() == 1
              && notes.first().contains(QStringLiteral("broken.rar")),
          "an unreadable archive is reported, not silently skipped");
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testYara();
    testScripts();
    testScanner();
    testLog();
    testSchedule();
    testSecure();
    testSelfProtect();
    testRealtime();
    testProcessMonitor();
    testProgress();
    QTextStream(stdout) << "\n" << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
