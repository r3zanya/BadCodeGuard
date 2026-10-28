#pragma once

#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

namespace bcg {

// Downloads extra detection data into the signatures folder:
//  - further SHA-256 hash lists (any https link to a plain list, one hash per line);
//  - the community YARA rule set (Yara-Rules/rules, "malware" folder).
// Only https links are accepted. Files are validated before they replace anything.
class FeedUpdater : public QObject {
    Q_OBJECT
public:
    enum Kind { HashLists, YaraRules };

    explicit FeedUpdater(QObject* parent = nullptr);

    bool busy() const { return m_busy; }

    // Where the community rule set comes from; shown in the UI.
    static QString yaraSourceUrl();

public slots:
    void updateHashLists(const QStringList& urls);
    void updateYaraRules();

signals:
    void finished(int kind, bool ok, const QString& message);

private:
    void nextHashList();
    void finish(Kind kind, bool ok, const QString& message);
    QNetworkReply* get(const QString& url, qint64 maxBytes);
    static QString fileForUrl(const QString& url);

    QNetworkAccessManager* m_net;
    bool m_busy = false;

    QStringList m_urls;
    int m_index = 0;
    int m_loaded = 0;
    int m_hashes = 0;
    QStringList m_failed;
};

} // namespace bcg
