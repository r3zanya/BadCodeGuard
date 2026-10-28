#pragma once

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

namespace bcg {

struct QuarantineEntry {
    QString id;
    QString originalPath;
    QString threat;
    int kind = 0; // ThreatKind
    QDateTime when;
    qint64 size = 0;
    QByteArray sha256; // hex, of the original content
};

struct QuarantineResult {
    bool ok = false;
    QString error;
    bool targetExists = false; // restore refused because the target is there
    QuarantineEntry entry;
};

// Isolates files: the content is scrambled (so it can no longer run or be
// re-detected), stored under a random name and indexed with its original
// path. Thread-safe; changed() fires after every modification.
class Quarantine : public QObject {
    Q_OBJECT
public:
    explicit Quarantine(const QString& rootFolder, QObject* parent = nullptr);

    QString root() const { return m_root; }
    QList<QuarantineEntry> entries() const;
    bool contains(const QString& id) const;

    // Files inside the Windows folder are never quarantined.
    static bool isProtectedPath(const QString& path);

    QuarantineResult add(const QString& path, const QString& threat, int kind);
    // Puts the file back. An existing target is only replaced with `overwrite`;
    // `destination` overrides the original location.
    QuarantineResult restore(const QString& id, bool overwrite = false,
                             const QString& destination = QString());
    bool remove(const QString& id);
    int removeAll();

signals:
    void changed();

private:
    QString dataPath(const QString& id) const;
    bool loadIndex();
    bool saveIndex() const;
    static void scramble(QByteArray& chunk, qint64 offset);

    QString m_root;
    mutable QMutex m_mutex;
    QList<QuarantineEntry> m_entries;
};

} // namespace bcg
