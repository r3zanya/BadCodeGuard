#include "ReputationService.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>

namespace bcg {

namespace {
constexpr int kMinGapMs = 16000;       // free tier: 4 requests per minute
constexpr int kTimeoutMs = 30000;
} // namespace

ReputationService::ReputationService(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    qRegisterMetaType<bcg::ReputationResult>();
}

QString ReputationService::summary(const ReputationResult& r)
{
    switch (r.state) {
    case ReputationResult::Known: {
        const int bad = r.malicious + r.suspicious;
        if (bad == 0)
            return QStringLiteral("VirusTotal: никто из %1 антивирусов не считает файл опасным")
                .arg(r.total());
        return QStringLiteral("VirusTotal: %1 из %2 антивирусов считают файл опасным")
            .arg(bad)
            .arg(r.total());
    }
    case ReputationResult::Unknown:
        return QStringLiteral("VirusTotal: файл ещё не встречался");
    case ReputationResult::Error:
        break;
    }
    return r.message;
}

void ReputationService::lookupFile(const QString& path, qint64 token)
{
    QPointer<ReputationService> self(this);
    QThreadPool::globalInstance()->start([self, path, token] {
        QFile f(path);
        QString hash;
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash h(QCryptographicHash::Sha256);
            h.addData(&f);
            hash = QString::fromLatin1(h.result().toHex());
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, hash, path, token] {
                if (!self)
                    return;
                if (hash.isEmpty()) {
                    ReputationResult r;
                    r.token = token;
                    r.path = path;
                    r.message = QStringLiteral("Не удалось прочитать файл для проверки");
                    emit self->result(r);
                    return;
                }
                self->lookupHash(hash, path, token);
            },
            Qt::QueuedConnection);
    });
}

void ReputationService::lookupHash(const QString& sha256, const QString& path, qint64 token)
{
    Job job;
    job.sha256 = sha256;
    job.path = path;
    job.token = token;
    m_queue.append(job);
    pump();
}

void ReputationService::pump()
{
    if (m_reply || m_timerArmed || m_queue.isEmpty())
        return;
    const qint64 gap = m_sinceLast.isValid() ? kMinGapMs - m_sinceLast.elapsed() : 0;
    if (gap > 0) {
        m_timerArmed = true;
        QTimer::singleShot(int(gap), this, [this] {
            m_timerArmed = false;
            pump();
        });
        return;
    }
    startRequest(m_queue.takeFirst());
}

void ReputationService::startRequest(const Job& job)
{
    if (m_key.isEmpty()) {
        ReputationResult r;
        r.token = job.token;
        r.path = job.path;
        r.sha256 = job.sha256;
        r.message = QStringLiteral("Не задан ключ VirusTotal");
        emit result(r);
        pump();
        return;
    }
    QNetworkRequest request(
        QUrl(QStringLiteral("https://www.virustotal.com/api/v3/files/%1").arg(job.sha256)));
    request.setRawHeader("x-apikey", m_key.toUtf8());
    request.setRawHeader("accept", "application/json");
    request.setTransferTimeout(kTimeoutMs);
    m_sinceLast.restart();
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, job] { onFinished(job, m_reply); });
}

void ReputationService::onFinished(const Job& job, QNetworkReply* reply)
{
    m_reply = nullptr;
    reply->deleteLater();

    ReputationResult r;
    r.token = job.token;
    r.path = job.path;
    r.sha256 = job.sha256;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 200) {
        const QJsonObject stats = QJsonDocument::fromJson(reply->readAll())
                                      .object()[QLatin1String("data")]
                                      .toObject()[QLatin1String("attributes")]
                                      .toObject()[QLatin1String("last_analysis_stats")]
                                      .toObject();
        r.state = ReputationResult::Known;
        r.malicious = stats[QLatin1String("malicious")].toInt();
        r.suspicious = stats[QLatin1String("suspicious")].toInt();
        r.harmless = stats[QLatin1String("harmless")].toInt();
        r.undetected = stats[QLatin1String("undetected")].toInt();
    } else if (status == 404) {
        r.state = ReputationResult::Unknown;
    } else if (status == 401 || status == 403) {
        r.message = QStringLiteral("VirusTotal отклонил ключ (HTTP %1)").arg(status);
    } else if (status == 429) {
        r.message = QStringLiteral("VirusTotal: превышен лимит запросов, попробуйте позже");
    } else {
        r.message = QStringLiteral("VirusTotal недоступен: %1").arg(reply->errorString());
    }
    emit result(r);
    pump();
}

} // namespace bcg
