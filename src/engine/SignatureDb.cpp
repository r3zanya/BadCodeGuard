#include "SignatureDb.h"

#include <QDir>
#include <QFile>

namespace bcg {

namespace {
bool isHex64(const QByteArray& s)
{
    if (s.size() != 64)
        return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    }
    return true;
}
} // namespace

int SignatureDb::loadText(const QByteArray& text)
{
    int added = 0;
    const QList<QByteArray> lines = text.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        qsizetype sep = 0;
        while (sep < line.size() && line[sep] != ' ' && line[sep] != '\t' && line[sep] != ':'
               && line[sep] != ',' && line[sep] != ';')
            ++sep;
        const QByteArray hash = line.left(sep).toLower().replace("\"", "");
        if (!isHex64(hash))
            continue;
        QString name = QString::fromUtf8(line.mid(sep + 1)).trimmed();
        if (name.isEmpty())
            name = QStringLiteral("Known-Malware");
        if (!m_hashes.contains(hash))
            ++added;
        m_hashes.insert(hash, name);
    }
    return added;
}

int SignatureDb::loadFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? loadText(f.readAll()) : 0;
}

int SignatureDb::loadFolder(const QString& folder)
{
    int added = 0;
    const QDir dir(folder);
    for (const QFileInfo& fi : dir.entryInfoList({QStringLiteral("*.txt"), QStringLiteral("*.sha256")},
                                                 QDir::Files))
        added += loadFile(fi.absoluteFilePath());
    return added;
}

QString SignatureDb::lookup(const QByteArray& sha256) const
{
    if (m_hashes.isEmpty())
        return {};
    const QByteArray hex = sha256.size() == 32 ? sha256.toHex() : sha256.toLower();
    return m_hashes.value(hex);
}

} // namespace bcg
