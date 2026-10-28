#include "FeedUpdater.h"

#include "engine/ArchiveReader.h"
#include "engine/EngineSetup.h"

#include <miniz.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace bcg {

namespace {

constexpr qint64 kMaxHashList = 256LL * 1024 * 1024;
constexpr qint64 kMaxYaraZip = 128LL * 1024 * 1024;
constexpr int kTimeoutMs = 120 * 1000;
constexpr int kMaxRuleFiles = 3000;
constexpr qint64 kMaxRuleFile = 4LL * 1024 * 1024;
const char kYaraZip[] = "https://github.com/Yara-Rules/rules/archive/refs/heads/master.zip";

int countHashes(const QByteArray& data)
{
    int n = 0;
    for (const QByteArray& line : data.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        // The hash is the first token; the engine accepts " ", tab, ":", "," and ";" after it.
        qsizetype end = 0;
        while (end < trimmed.size() && trimmed[end] != ' ' && trimmed[end] != '\t'
               && trimmed[end] != ':' && trimmed[end] != ',' && trimmed[end] != ';')
            ++end;
        if (end != 64)
            continue;
        const QByteArray t = trimmed.left(64);
        bool hex = true;
        for (char c : t)
            hex &= (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        n += hex;
    }
    return n;
}

} // namespace

FeedUpdater::FeedUpdater(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString FeedUpdater::yaraSourceUrl()
{
    return QString::fromLatin1(kYaraZip);
}

QString FeedUpdater::fileForUrl(const QString& url)
{
    const QByteArray id =
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex().left(10);
    return signaturesFolder() + QStringLiteral("/feed_") + QString::fromLatin1(id)
        + QStringLiteral(".txt");
}

QNetworkReply* FeedUpdater::get(const QString& url, qint64 maxBytes)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BadCodeGuard"));
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply, maxBytes](qint64 got, qint64) {
        if (got > maxBytes)
            reply->abort();
    });
    return reply;
}

void FeedUpdater::finish(Kind kind, bool ok, const QString& message)
{
    m_busy = false;
    emit finished(kind, ok, message);
}

void FeedUpdater::updateHashLists(const QStringList& urls)
{
    if (m_busy)
        return;
    m_busy = true;
    m_urls.clear();
    for (const QString& u : urls) {
        const QString t = u.trimmed();
        if (!t.isEmpty())
            m_urls << t;
    }
    m_index = 0;
    m_loaded = 0;
    m_hashes = 0;
    m_failed.clear();

    // Forget lists whose link was removed from the settings.
    QDir().mkpath(signaturesFolder());
    QStringList keep;
    for (const QString& u : m_urls)
        keep << QFileInfo(fileForUrl(u)).fileName();
    const QDir dir(signaturesFolder());
    for (const QString& f : dir.entryList({QStringLiteral("feed_*.txt")}, QDir::Files)) {
        if (!keep.contains(f))
            QFile::remove(dir.filePath(f));
    }

    if (m_urls.isEmpty()) {
        finish(HashLists, true, QStringLiteral("Дополнительных списков нет."));
        return;
    }
    nextHashList();
}

void FeedUpdater::nextHashList()
{
    if (m_index >= m_urls.size()) {
        const QString text = QStringLiteral("Списков загружено: %1, хэшей в них: %2.")
                                 .arg(m_loaded)
                                 .arg(m_hashes);
        if (m_failed.isEmpty())
            finish(HashLists, true, text);
        else
            finish(HashLists, m_loaded > 0,
                   text + QStringLiteral(" Не удалось: %1.").arg(m_failed.join(QStringLiteral("; "))));
        return;
    }

    const QString url = m_urls.at(m_index);
    const QUrl parsed(url);
    if (parsed.scheme() != QLatin1String("https") || parsed.host().isEmpty()) {
        m_failed << QStringLiteral("%1 (нужна ссылка https)").arg(parsed.host().isEmpty() ? url : parsed.host());
        ++m_index;
        nextHashList();
        return;
    }

    QNetworkReply* reply = get(url, kMaxHashList);
    connect(reply, &QNetworkReply::finished, this, [this, reply, parsed] {
        reply->deleteLater();
        const QString host = parsed.host();
        if (reply->error() != QNetworkReply::NoError) {
            m_failed << QStringLiteral("%1 (%2)").arg(host, reply->errorString().left(80));
        } else {
            const QByteArray data = reply->readAll();
            const int n = countHashes(data);
            QSaveFile out(fileForUrl(m_urls.at(m_index)));
            if (n == 0) {
                m_failed << QStringLiteral("%1 (в ответе нет хэшей)").arg(host);
            } else if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()
                       || !out.commit()) {
                m_failed << QStringLiteral("%1 (не удалось сохранить)").arg(host);
            } else {
                ++m_loaded;
                m_hashes += n;
            }
        }
        ++m_index;
        nextHashList();
    });
}

void FeedUpdater::updateYaraRules()
{
    if (m_busy)
        return;
    m_busy = true;
    QNetworkReply* reply = get(yaraSourceUrl(), kMaxYaraZip);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(YaraRules, false,
                   QStringLiteral("Не удалось скачать правила: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray zip = reply->readAll();

        // Unpack into a fresh folder next to the live one, then swap.
        const QString live = yaraFolder() + QStringLiteral("/community");
        const QString fresh = yaraFolder() + QStringLiteral("/community.new");
        QDir(fresh).removeRecursively();
        QDir().mkpath(fresh);

        int written = 0;
        ArchiveBudget budget;
        ArchiveLimits limits;
        limits.maxEntries = 100000;
        limits.maxEntrySize = kMaxRuleFile;
        limits.maxTotal = 512LL * 1024 * 1024;
        visitZip(
            zip,
            [&](const QString& name, const QByteArray& data) {
                const QString lower = name.toLower();
                if (!lower.contains(QLatin1String("/malware/")))
                    return false;
                if (!lower.endsWith(QLatin1String(".yar")) && !lower.endsWith(QLatin1String(".yara")))
                    return false;
                if (lower.contains(QLatin1String("index")))
                    return false; // index files only `include` the others
                // Only the file name is used, so the archive cannot write elsewhere.
                QSaveFile out(fresh + QLatin1Char('/') + QString::number(written) + QLatin1Char('_')
                              + QFileInfo(name).fileName());
                if (out.open(QIODevice::WriteOnly) && out.write(data) == data.size() && out.commit())
                    ++written;
                return written >= kMaxRuleFiles;
            },
            limits, budget);

        if (written == 0) {
            QDir(fresh).removeRecursively();
            finish(YaraRules, false, QStringLiteral("В скачанном архиве нет файлов правил."));
            return;
        }
        QDir(live).removeRecursively();
        if (!QDir().rename(fresh, live)) {
            finish(YaraRules, false, QStringLiteral("Не удалось сохранить правила."));
            return;
        }
        finish(YaraRules, true, QStringLiteral("Скачано файлов с правилами: %1.").arg(written));
    });
}

} // namespace bcg
