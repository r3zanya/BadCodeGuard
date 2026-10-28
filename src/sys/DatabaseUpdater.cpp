#include "DatabaseUpdater.h"

#include "engine/EngineSetup.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

namespace bcg {

namespace {
constexpr qint64 kMaxDownload = 256LL * 1024 * 1024;
constexpr int kTimeoutMs = 60 * 1000;
} // namespace

DatabaseUpdater::DatabaseUpdater(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString DatabaseUpdater::extractKey(const QString& text)
{
    const QString trimmed = text.trimmed();
    static const QRegularExpression link(QStringLiteral("/exports/([A-Za-z0-9_-]{8,128})/"));
    const QRegularExpressionMatch m = link.match(trimmed);
    if (m.hasMatch())
        return m.captured(1);
    static const QRegularExpression bare(QStringLiteral("^[A-Za-z0-9_-]{8,128}$"));
    return bare.match(trimmed).hasMatch() ? trimmed : QString();
}

QString DatabaseUpdater::targetFile()
{
    return signaturesFolder() + QStringLiteral("/malwarebazaar_recent.txt");
}

void DatabaseUpdater::fail(const QString& message)
{
    QString safe = message;
    if (!m_key.isEmpty())
        safe.replace(m_key, QStringLiteral("***"));
    emit finished(false, safe, 0);
}

void DatabaseUpdater::update(const QString& keyOrUrl)
{
    if (m_reply)
        return;
    m_key = extractKey(keyOrUrl);
    if (m_key.isEmpty()) {
        fail(QStringLiteral("Введите ключ abuse.ch или ссылку на экспорт MalwareBazaar."));
        return;
    }

    QNetworkRequest request(
        QUrl(QStringLiteral("https://mb-api.abuse.ch/v2/files/exports/%1/sha256_recent.txt")
                 .arg(m_key)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BadCodeGuard"));

    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
        if (m_reply && received > kMaxDownload)
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::finished, this, &DatabaseUpdater::onReplyFinished);
}

void DatabaseUpdater::onReplyFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 || status == 403 || status == 404)
            fail(QStringLiteral("Сервер отклонил ключ (HTTP %1). Проверьте ключ abuse.ch.").arg(status));
        else
            fail(QStringLiteral("Не удалось скачать базу: %1").arg(reply->errorString()));
        m_key.clear();
        return;
    }

    const QByteArray data = reply->readAll();
    // Count lines that are a bare 64-character hex hash; anything else (an HTML
    // page from a failed login, for example) is not a hash list.
    int hashes = 0;
    for (const QByteArray& line : data.split('\n')) {
        const QByteArray t = line.trimmed();
        if (t.size() != 64)
            continue;
        bool hex = true;
        for (char c : t)
            hex &= (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (hex)
            ++hashes;
    }
    if (hashes == 0) {
        fail(QStringLiteral("Сервер вернул не список хэшей. Проверьте ключ abuse.ch."));
        m_key.clear();
        return;
    }

    QDir().mkpath(signaturesFolder());
    QSaveFile out(targetFile());
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        fail(QStringLiteral("Не удалось сохранить файл базы: %1").arg(out.errorString()));
        m_key.clear();
        return;
    }
    m_key.clear();
    emit finished(true, QStringLiteral("База обновлена: %1 хэшей в списке.").arg(hashes), hashes);
}

} // namespace bcg
