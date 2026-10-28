#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace bcg {

// Downloads the MalwareBazaar "recent SHA-256" export into the signatures
// folder. The abuse.ch Auth-Key is part of the URL; it is never logged or
// shown in error messages.
class DatabaseUpdater : public QObject {
    Q_OBJECT
public:
    explicit DatabaseUpdater(QObject* parent = nullptr);

    // Accepts a bare key or a full export link and returns the key; empty if
    // the text does not look like either.
    static QString extractKey(const QString& text);
    // File inside signaturesFolder() that this class writes.
    static QString targetFile();

    bool busy() const { return m_reply != nullptr; }

public slots:
    void update(const QString& keyOrUrl);

signals:
    // `hashes` is the number of valid hashes in the file that was saved.
    void finished(bool ok, const QString& message, int hashes);

private:
    void onReplyFinished();
    void fail(const QString& message);

    QNetworkAccessManager* m_net;
    QNetworkReply* m_reply = nullptr;
    QString m_key;
};

} // namespace bcg
