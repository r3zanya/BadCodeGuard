#include "Quarantine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSaveFile>
#include <QUuid>

namespace bcg {

namespace {
constexpr qint64 kChunk = 1 << 20;
constexpr const char* kIndexName = "index.json";
constexpr const char* kDataSuffix = ".qbc";

QString clean(const QString& path)
{
    return QDir::fromNativeSeparators(path).toLower();
}
} // namespace

Quarantine::Quarantine(const QString& rootFolder, QObject* parent)
    : QObject(parent)
    , m_root(QDir::cleanPath(rootFolder))
{
    QDir().mkpath(m_root);
    loadIndex();
}

bool Quarantine::isProtectedPath(const QString& path)
{
    const QString windows = qEnvironmentVariable("SystemRoot");
    if (windows.isEmpty())
        return false;
    return clean(QFileInfo(path).absoluteFilePath()).startsWith(clean(windows) + QLatin1Char('/'));
}

QString Quarantine::dataPath(const QString& id) const
{
    return m_root + QLatin1Char('/') + id + QLatin1String(kDataSuffix);
}

// Reversible scrambling: not secrecy, only neutralisation of the content.
void Quarantine::scramble(QByteArray& chunk, qint64 offset)
{
    static const unsigned char key[16] = {0x5A, 0xC3, 0x91, 0x2E, 0x77, 0xB8, 0x04, 0xD6,
                                          0x3F, 0xA1, 0x68, 0xE5, 0x1B, 0x9C, 0x42, 0xF0};
    char* p = chunk.data();
    for (qsizetype i = 0; i < chunk.size(); ++i)
        p[i] = static_cast<char>(static_cast<unsigned char>(p[i]) ^ key[(offset + i) & 15]);
}

bool Quarantine::loadIndex()
{
    QFile f(m_root + QLatin1Char('/') + QLatin1String(kIndexName));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    m_entries.clear();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        QuarantineEntry e;
        e.id = o[QLatin1String("id")].toString();
        e.originalPath = o[QLatin1String("path")].toString();
        e.threat = o[QLatin1String("threat")].toString();
        e.kind = o[QLatin1String("kind")].toInt();
        e.when = QDateTime::fromString(o[QLatin1String("when")].toString(), Qt::ISODate);
        e.size = o[QLatin1String("size")].toVariant().toLongLong();
        e.sha256 = o[QLatin1String("sha256")].toString().toLatin1();
        if (!e.id.isEmpty() && QFile::exists(dataPath(e.id)))
            m_entries.append(e);
    }
    return true;
}

bool Quarantine::saveIndex() const
{
    QJsonArray arr;
    for (const QuarantineEntry& e : m_entries) {
        QJsonObject o;
        o[QLatin1String("id")] = e.id;
        o[QLatin1String("path")] = e.originalPath;
        o[QLatin1String("threat")] = e.threat;
        o[QLatin1String("kind")] = e.kind;
        o[QLatin1String("when")] = e.when.toString(Qt::ISODate);
        o[QLatin1String("size")] = e.size;
        o[QLatin1String("sha256")] = QString::fromLatin1(e.sha256);
        arr.append(o);
    }
    QSaveFile f(m_root + QLatin1Char('/') + QLatin1String(kIndexName));
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return f.commit();
}

QList<QuarantineEntry> Quarantine::entries() const
{
    QMutexLocker lock(&m_mutex);
    return m_entries;
}

bool Quarantine::contains(const QString& id) const
{
    QMutexLocker lock(&m_mutex);
    for (const QuarantineEntry& e : m_entries) {
        if (e.id == id)
            return true;
    }
    return false;
}

QuarantineResult Quarantine::add(const QString& path, const QString& threat, int kind)
{
    QuarantineResult result;
    const QFileInfo info(path);
    if (!info.isFile()) {
        result.error = QStringLiteral("Файл не найден");
        return result;
    }
    if (isProtectedPath(path)) {
        result.error = QStringLiteral("Файлы Windows не помещаются в карантин");
        return result;
    }

    {
        QMutexLocker lock(&m_mutex);
        QuarantineEntry e;
        e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        e.originalPath = info.absoluteFilePath();
        e.threat = threat;
        e.kind = kind;
        e.when = QDateTime::currentDateTime();

        const QString finalPath = dataPath(e.id);
        const QString partPath = finalPath + QLatin1String(".part");
        {
            QFile src(path);
            QFile dst(partPath);
            if (!src.open(QIODevice::ReadOnly)) {
                result.error = QStringLiteral("Не удалось прочитать файл: %1").arg(src.errorString());
                return result;
            }
            if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                result.error = QStringLiteral("Не удалось создать файл карантина: %1").arg(dst.errorString());
                return result;
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            qint64 offset = 0;
            bool ok = true;
            while (!src.atEnd() && ok) {
                QByteArray chunk = src.read(kChunk);
                if (chunk.isEmpty())
                    break;
                hash.addData(chunk);
                scramble(chunk, offset);
                offset += chunk.size();
                ok = dst.write(chunk) == chunk.size();
            }
            ok = ok && src.error() == QFile::NoError && dst.flush();
            dst.close();
            if (!ok) {
                QFile::remove(partPath);
                result.error = QStringLiteral("Ошибка копирования в карантин");
                return result;
            }
            e.size = offset;
            e.sha256 = hash.result().toHex();
        }
        if (!QFile::rename(partPath, finalPath)) {
            QFile::remove(partPath);
            result.error = QStringLiteral("Не удалось сохранить файл карантина");
            return result;
        }

        m_entries.append(e);
        if (!saveIndex()) {
            m_entries.removeLast();
            QFile::remove(finalPath);
            result.error = QStringLiteral("Не удалось записать индекс карантина");
            return result;
        }

        // Only now is it safe to take the original away.
        QFile original(path);
        bool removed = original.remove();
        if (!removed) {
            original.setPermissions(original.permissions() | QFileDevice::WriteUser);
            removed = original.remove();
        }
        if (!removed) {
            m_entries.removeLast();
            saveIndex();
            QFile::remove(finalPath);
            result.error = QStringLiteral("Не удалось удалить исходный файл (возможно, он используется)");
            return result;
        }
        result.entry = e;
        result.ok = true;
    }
    emit changed();
    return result;
}

QuarantineResult Quarantine::restore(const QString& id, bool overwrite, const QString& destination)
{
    QuarantineResult result;
    {
        QMutexLocker lock(&m_mutex);
        int index = -1;
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == id)
                index = i;
        }
        if (index < 0) {
            result.error = QStringLiteral("Запись не найдена");
            return result;
        }
        const QuarantineEntry e = m_entries[index];
        const QString target = destination.isEmpty() ? e.originalPath : destination;

        if (QFileInfo::exists(target) && !overwrite) {
            result.targetExists = true;
            result.error = QStringLiteral("По этому пути уже есть файл");
            return result;
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
            result.error = QStringLiteral("Не удалось создать папку назначения");
            return result;
        }

        const QString partPath = target + QLatin1String(".bcgrestore");
        {
            QFile src(dataPath(id));
            QFile dst(partPath);
            if (!src.open(QIODevice::ReadOnly)) {
                result.error = QStringLiteral("Файл карантина повреждён или отсутствует");
                return result;
            }
            if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                result.error = QStringLiteral("Не удалось записать в место назначения: %1").arg(dst.errorString());
                return result;
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            qint64 offset = 0;
            bool ok = true;
            while (!src.atEnd() && ok) {
                QByteArray chunk = src.read(kChunk);
                if (chunk.isEmpty())
                    break;
                scramble(chunk, offset);
                offset += chunk.size();
                hash.addData(chunk);
                ok = dst.write(chunk) == chunk.size();
            }
            ok = ok && dst.flush();
            dst.close();
            if (!ok || hash.result().toHex() != e.sha256) {
                QFile::remove(partPath);
                result.error = ok ? QStringLiteral("Контрольная сумма не совпала, файл не восстановлен")
                                  : QStringLiteral("Ошибка записи при восстановлении");
                return result;
            }
        }
        if (QFileInfo::exists(target) && !QFile::remove(target)) {
            QFile::remove(partPath);
            result.error = QStringLiteral("Не удалось заменить существующий файл");
            return result;
        }
        if (!QFile::rename(partPath, target)) {
            QFile::remove(partPath);
            result.error = QStringLiteral("Не удалось завершить восстановление");
            return result;
        }

        QFile::remove(dataPath(id));
        m_entries.removeAt(index);
        saveIndex();
        result.entry = e;
        result.entry.originalPath = target;
        result.ok = true;
    }
    emit changed();
    return result;
}

bool Quarantine::remove(const QString& id)
{
    {
        QMutexLocker lock(&m_mutex);
        int index = -1;
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == id)
                index = i;
        }
        if (index < 0)
            return false;
        QFile::remove(dataPath(id));
        m_entries.removeAt(index);
        saveIndex();
    }
    emit changed();
    return true;
}

int Quarantine::removeAll()
{
    int count = 0;
    {
        QMutexLocker lock(&m_mutex);
        count = int(m_entries.size());
        for (const QuarantineEntry& e : std::as_const(m_entries))
            QFile::remove(dataPath(e.id));
        m_entries.clear();
        saveIndex();
    }
    if (count)
        emit changed();
    return count;
}

} // namespace bcg
