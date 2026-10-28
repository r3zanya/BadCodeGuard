#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace bcg {

struct ReputationResult {
    enum State { Known, Unknown, Error };

    State state = Error;
    qint64 token = 0;       // whatever the caller passed to lookup()
    QString path;
    QString sha256;
    int malicious = 0;      // engines that flag the file
    int suspicious = 0;
    int harmless = 0;
    int undetected = 0;
    QString message;        // short text for the UI

    int total() const { return malicious + suspicious + harmless + undetected; }
};

// Asks VirusTotal what it knows about a file. Only the SHA-256 hash leaves the
// computer, never the file. Requests are spaced out to stay inside the free
// tier (4 per minute); results arrive through result().
class ReputationService : public QObject {
    Q_OBJECT
public:
    explicit ReputationService(QObject* parent = nullptr);

    void setApiKey(const QString& key) { m_key = key.trimmed(); }
    bool hasKey() const { return !m_key.isEmpty(); }
    int pending() const { return int(m_queue.size()) + (m_reply ? 1 : 0); }

    // Hashes the file on a worker thread, then queues the lookup.
    void lookupFile(const QString& path, qint64 token);
    void lookupHash(const QString& sha256, const QString& path, qint64 token);

    // Short Russian summary: "VirusTotal: 12 из 70 антивирусов считают файл опасным".
    static QString summary(const ReputationResult& r);

signals:
    void result(const ReputationResult& result);

private:
    struct Job {
        QString sha256;
        QString path;
        qint64 token = 0;
    };

    void pump();
    void startRequest(const Job& job);
    void onFinished(const Job& job, QNetworkReply* reply);

    QNetworkAccessManager* m_net;
    QNetworkReply* m_reply = nullptr;
    QList<Job> m_queue;
    QElapsedTimer m_sinceLast;
    bool m_timerArmed = false;
    QString m_key;
};

} // namespace bcg

Q_DECLARE_METATYPE(bcg::ReputationResult)
