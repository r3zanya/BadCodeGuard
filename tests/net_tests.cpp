// Manual check of the downloaders (needs internet; touches the real data folder).
// Build target bcg_nettests (not part of the default build).

#include "engine/EngineSetup.h"
#include "engine/FileScanner.h"
#include "sys/FeedUpdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QTimer>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("BadCodeGuard"));
    QCoreApplication::setApplicationName(QStringLiteral("BadCodeGuard"));
    QTextStream out(stdout);

    bcg::FeedUpdater feeds;
    QObject::connect(&feeds, &bcg::FeedUpdater::finished, [&](int kind, bool ok, const QString& msg) {
        out << "feed kind=" << kind << " ok=" << ok << " : " << msg << "\n";
        out.flush();
        app.quit();
    });
    feeds.updateYaraRules();
    QTimer::singleShot(180000, &app, &QCoreApplication::quit);
    app.exec();

    QElapsedTimer t;
    t.start();
    bcg::YaraEngine engine;
    const bcg::YaraLoadReport r = engine.loadFolder(bcg::yaraFolder());
    out << "yara: files=" << r.files << " failed=" << r.failed << " rules=" << r.rules << " in "
        << t.elapsed() << " ms\n";
    for (const QString& e : r.errors)
        out << "  error sample: " << e << "\n";

    // False-positive check on files that ship with Windows.
    const QStringList samples = {"C:/Windows/System32/notepad.exe", "C:/Windows/System32/cmd.exe",
                                 "C:/Windows/explorer.exe", "C:/Windows/System32/kernel32.dll",
                                 "C:/Windows/System32/powershell.exe", "C:/Windows/System32/svchost.exe",
                                 "C:/Windows/System32/ntdll.dll", "C:/Windows/System32/mshta.exe"};
    int hits = 0;
    t.restart();
    for (const QString& s : samples) {
        QFile f(s);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString hit = engine.scan(f.read(16 * 1024 * 1024), s);
        out << "  " << s << " -> " << (hit.isEmpty() ? "clean" : hit) << "\n";
        hits += !hit.isEmpty();
    }
    out << "false positives on system files: " << hits << " of " << samples.size() << ", "
        << t.elapsed() << " ms total\n";
    return 0;
}
