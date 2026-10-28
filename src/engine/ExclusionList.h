#pragma once

#include <QList>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

namespace bcg {

struct Exclusion {
    enum Type { File, Folder, Extension, Mask };

    Type type = File;
    // File/Folder: absolute path. Extension: "ext" without dot. Mask: wildcard pattern.
    QString value;

    bool operator==(const Exclusion& o) const { return type == o.type && value == o.value; }
};

// Paths the engine must neither scan, report nor touch. Thread-safe: the UI
// edits the list while the scanner thread asks matches().
class ExclusionList {
public:
    QList<Exclusion> items() const;
    void setItems(const QList<Exclusion>& items);

    // Returns false when the entry is empty or already present.
    bool add(const Exclusion& item);
    void remove(const Exclusion& item);

    // True when the file, any of its parent folders, its extension or its
    // name is excluded. Case-insensitive, accepts / and \ separators.
    bool matches(const QString& path) const;
    // True when the folder itself (or one of its parents) is excluded, so the
    // scanner can skip the whole subtree.
    bool matchesFolder(const QString& path) const;

    // Persistence: "F|C:/x", "D|C:/dir", "E|tmp", "M|*.log".
    QStringList toStrings() const;
    static QList<Exclusion> fromStrings(const QStringList& strings);

    // Normalised form of an entry's value (separators, trailing slash, dot).
    static QString normalizedValue(Exclusion::Type type, const QString& value);

private:
    struct Compiled {
        QSet<QString> files;
        QStringList folders; // lower-case, forward slashes, trailing '/'
        QSet<QString> extensions;
        QList<QRegularExpression> nameMasks; // match the file name
        QList<QRegularExpression> pathMasks; // match the whole path
    };
    static QString key(const QString& path);
    void rebuild();
    bool folderHit(const QString& lowerPath) const;

    mutable QReadWriteLock m_lock;
    QList<Exclusion> m_items;
    Compiled m_c;
};

} // namespace bcg
