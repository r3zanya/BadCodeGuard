#include "ExclusionList.h"

#include <QDir>
#include <QReadLocker>
#include <QWriteLocker>

namespace bcg {

QString ExclusionList::key(const QString& path)
{
    return QDir::fromNativeSeparators(path).toLower();
}

QString ExclusionList::normalizedValue(Exclusion::Type type, const QString& value)
{
    QString v = value.trimmed();
    switch (type) {
    case Exclusion::File:
    case Exclusion::Folder:
        v = QDir::fromNativeSeparators(v);
        while (v.size() > 3 && v.endsWith(QLatin1Char('/')))
            v.chop(1);
        return v;
    case Exclusion::Extension:
        while (v.startsWith(QLatin1Char('*')) || v.startsWith(QLatin1Char('.')))
            v.remove(0, 1);
        return v.toLower();
    case Exclusion::Mask:
        return QDir::fromNativeSeparators(v);
    }
    return v;
}

QList<Exclusion> ExclusionList::items() const
{
    QReadLocker lock(&m_lock);
    return m_items;
}

void ExclusionList::setItems(const QList<Exclusion>& items)
{
    QWriteLocker lock(&m_lock);
    m_items.clear();
    for (Exclusion e : items) {
        e.value = normalizedValue(e.type, e.value);
        if (!e.value.isEmpty() && !m_items.contains(e))
            m_items.append(e);
    }
    rebuild();
}

bool ExclusionList::add(const Exclusion& item)
{
    Exclusion e = item;
    e.value = normalizedValue(e.type, e.value);
    QWriteLocker lock(&m_lock);
    if (e.value.isEmpty() || m_items.contains(e))
        return false;
    m_items.append(e);
    rebuild();
    return true;
}

void ExclusionList::remove(const Exclusion& item)
{
    QWriteLocker lock(&m_lock);
    if (m_items.removeOne(item))
        rebuild();
}

void ExclusionList::rebuild()
{
    m_c = Compiled();
    for (const Exclusion& e : std::as_const(m_items)) {
        switch (e.type) {
        case Exclusion::File:
            m_c.files.insert(key(e.value));
            break;
        case Exclusion::Folder: {
            QString f = key(e.value);
            if (!f.endsWith(QLatin1Char('/')))
                f += QLatin1Char('/');
            m_c.folders.append(f);
            break;
        }
        case Exclusion::Extension:
            m_c.extensions.insert(e.value);
            break;
        case Exclusion::Mask: {
            const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(e.value),
                                        QRegularExpression::CaseInsensitiveOption);
            (e.value.contains(QLatin1Char('/')) ? m_c.pathMasks : m_c.nameMasks).append(re);
            break;
        }
        }
    }
}

bool ExclusionList::folderHit(const QString& lowerPath) const
{
    for (const QString& f : m_c.folders) {
        if (lowerPath.startsWith(f))
            return true;
    }
    return false;
}

bool ExclusionList::matchesFolder(const QString& path) const
{
    QString p = key(path);
    if (!p.endsWith(QLatin1Char('/')))
        p += QLatin1Char('/');
    QReadLocker lock(&m_lock);
    if (folderHit(p))
        return true;
    // A name mask such as "node_modules" may also name a folder.
    const QString name = p.left(p.size() - 1).section(QLatin1Char('/'), -1);
    for (const QRegularExpression& re : m_c.nameMasks) {
        if (re.match(name).hasMatch())
            return true;
    }
    return false;
}

bool ExclusionList::matches(const QString& path) const
{
    const QString p = key(path);
    const QString name = p.section(QLatin1Char('/'), -1);
    const int dot = name.lastIndexOf(QLatin1Char('.'));

    QReadLocker lock(&m_lock);
    if (m_c.files.contains(p) || folderHit(p))
        return true;
    if (dot > 0 && m_c.extensions.contains(name.mid(dot + 1)))
        return true;
    for (const QRegularExpression& re : m_c.nameMasks) {
        if (re.match(name).hasMatch())
            return true;
    }
    for (const QRegularExpression& re : m_c.pathMasks) {
        if (re.match(p).hasMatch())
            return true;
    }
    return false;
}

QStringList ExclusionList::toStrings() const
{
    static const char kTags[] = {'F', 'D', 'E', 'M'};
    QStringList out;
    QReadLocker lock(&m_lock);
    for (const Exclusion& e : m_items)
        out << QStringLiteral("%1|%2").arg(QLatin1Char(kTags[e.type]), e.value);
    return out;
}

QList<Exclusion> ExclusionList::fromStrings(const QStringList& strings)
{
    QList<Exclusion> out;
    for (const QString& s : strings) {
        if (s.size() < 3 || s[1] != QLatin1Char('|'))
            continue;
        const int type = QStringLiteral("FDEM").indexOf(s[0]);
        if (type < 0)
            continue;
        out.append({static_cast<Exclusion::Type>(type), s.mid(2)});
    }
    return out;
}

} // namespace bcg
