#include "ArchiveReader.h"

#include <miniz.h>

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace bcg {

bool isArchiveName(const QString& path)
{
    static const char* const kExtensions[] = {
        // zip family (opened in memory)
        "zip", "jar", "war", "apk", "nupkg", "vsix", "whl", "docx", "docm", "xlsx", "xlsm",
        "pptx", "pptm", "odt", "ods",
        // everything else goes through tar.exe
        "7z", "rar", "tar", "gz", "tgz", "bz2", "tbz2", "xz", "txz", "lzma", "zst", "cab", "iso",
        "lzh", "lha", "cpio"};
    const QString ext = QFileInfo(path).suffix();
    for (const char* e : kExtensions) {
        if (ext.compare(QLatin1String(e), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool looksLikeZip(const QByteArray& data)
{
    return data.size() >= 4 && data.startsWith("PK\x03\x04");
}

ArchiveStatus visitZip(const QByteArray& zip, const EntryVisitor& visit, const ArchiveLimits& limits,
                       ArchiveBudget& budget)
{
    mz_zip_archive za;
    mz_zip_zero_struct(&za);
    if (!mz_zip_reader_init_mem(&za, zip.constData(), size_t(zip.size()), 0))
        return ArchiveStatus::Unreadable;

    ArchiveStatus status = ArchiveStatus::Ok;
    const mz_uint count = mz_zip_reader_get_num_files(&za);
    for (mz_uint i = 0; i < count; ++i) {
        if (budget.entries >= limits.maxEntries) {
            status = ArchiveStatus::Truncated;
            break;
        }
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, i, &st) || st.m_is_directory)
            continue;
        ++budget.entries;
        if (st.m_is_encrypted) {
            if (status == ArchiveStatus::Ok)
                status = ArchiveStatus::Encrypted;
            continue;
        }
        if (!st.m_is_supported)
            continue;
        if (qint64(st.m_uncomp_size) > limits.maxEntrySize
            || budget.bytes + qint64(st.m_uncomp_size) > limits.maxTotal) {
            status = ArchiveStatus::Truncated;
            continue;
        }
        size_t size = 0;
        void* heap = mz_zip_reader_extract_to_heap(&za, i, &size, 0);
        if (!heap)
            continue;
        const QByteArray data(static_cast<const char*>(heap), qsizetype(size));
        mz_free(heap);
        budget.bytes += qint64(size);
        if (visit(QString::fromUtf8(st.m_filename), data))
            break;
    }
    mz_zip_reader_end(&za);
    return status;
}

namespace {

qint64 folderSize(const QString& dir)
{
    qint64 total = 0;
    QDirIterator it(dir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // namespace

ArchiveStatus visitExternal(const QString& path, const EntryVisitor& visit,
                            const ArchiveLimits& limits, ArchiveBudget& budget)
{
    const QString tar = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))
        + QStringLiteral("/System32/tar.exe");
    if (!QFileInfo::exists(tar))
        return ArchiveStatus::Unreadable;

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return ArchiveStatus::Unreadable;

    QProcess process;
    process.setProgram(tar);
    process.setArguments({QStringLiteral("-xf"), QDir::toNativeSeparators(path),
                          QStringLiteral("-C"), QDir::toNativeSeparators(tmp.path())});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(5000))
        return ArchiveStatus::Unreadable;

    ArchiveStatus status = ArchiveStatus::Ok;
    QElapsedTimer clock;
    clock.start();
    while (!process.waitForFinished(500)) {
        if (clock.elapsed() > qint64(limits.externalTimeoutSec) * 1000
            || folderSize(tmp.path()) > limits.maxTotal - budget.bytes) {
            process.kill();
            process.waitForFinished(3000);
            status = ArchiveStatus::Truncated;
            break;
        }
    }

    bool any = false;
    QDirIterator it(tmp.path(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        if (budget.entries >= limits.maxEntries) {
            status = ArchiveStatus::Truncated;
            break;
        }
        const qint64 size = it.fileInfo().size();
        if (size <= 0)
            continue;
        ++budget.entries;
        any = true;
        if (size > limits.maxEntrySize || budget.bytes + size > limits.maxTotal) {
            status = ArchiveStatus::Truncated;
            continue;
        }
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = f.readAll();
        budget.bytes += data.size();
        if (visit(QDir(tmp.path()).relativeFilePath(file), data))
            break;
    }
    if (!any && process.exitCode() != 0)
        return ArchiveStatus::Unreadable; // damaged, unsupported, or password protected
    return status;
}

} // namespace bcg
