#pragma once

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>

#include <memory>

namespace bcg {

// Known-bad SHA-256 hashes. Text format, one entry per line:
//   <sha256 hex>  <threat name>     (separator: space, tab or ':')
//   <sha256 hex>                    (name defaults to "Known-Malware")
// '#' starts a comment. Plain hash lists such as MalwareBazaar exports work.
// Load everything before the first scan; lookups are read-only afterwards.
class SignatureDb {
public:
    // Returns the number of entries added.
    int loadText(const QByteArray& text);
    int loadFile(const QString& path);
    // Loads every *.txt / *.sha256 file of a folder.
    int loadFolder(const QString& folder);

    // `sha256` is the raw digest (32 bytes) or its hex form. Empty = clean.
    QString lookup(const QByteArray& sha256) const;
    qsizetype size() const { return m_hashes.size(); }

private:
    QHash<QByteArray, QString> m_hashes; // lower-case hex -> name
};

// Holder that lets the UI swap in a freshly loaded database (after an update)
// while scanner threads keep reading the previous one.
class SignatureStore {
public:
    std::shared_ptr<const SignatureDb> get() const
    {
        QMutexLocker lock(&m_mutex);
        return m_db;
    }
    void set(std::shared_ptr<const SignatureDb> db)
    {
        QMutexLocker lock(&m_mutex);
        m_db = std::move(db);
    }

private:
    mutable QMutex m_mutex;
    std::shared_ptr<const SignatureDb> m_db;
};

} // namespace bcg
