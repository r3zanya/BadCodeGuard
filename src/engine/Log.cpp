#include "Log.h"

#include "EngineSetup.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

namespace bcg::log {

namespace {

constexpr qint64 kMaxBytes = 1024 * 1024;

QMutex& mutex()
{
    static QMutex m;
    return m;
}

QString& overridePath()
{
    static QString p;
    return p;
}

void write(const char* level, const QString& text)
{
    QMutexLocker lock(&mutex());
    const QString path = overridePath().isEmpty() ? dataFolder() + QStringLiteral("/logs/badcodeguard.log")
                                                  : overridePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (QFileInfo(path).size() > kMaxBytes) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    // One event per line: fold line breaks.
    QString line = text;
    line.replace(QLatin1Char('\r'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QStringLiteral(" | "));
    f.write(QStringLiteral("%1 [%2] %3\n")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                     QString::fromLatin1(level), line)
                .toUtf8());
}

} // namespace

void info(const QString& text) { write("INFO", text); }
void warn(const QString& text) { write("WARN", text); }
void error(const QString& text) { write("ERROR", text); }

QString file()
{
    QMutexLocker lock(&mutex());
    return overridePath().isEmpty() ? dataFolder() + QStringLiteral("/logs/badcodeguard.log")
                                    : overridePath();
}

void setFile(const QString& path)
{
    QMutexLocker lock(&mutex());
    overridePath() = path;
}

} // namespace bcg::log
