#include "TempCleaner.h"

#include <QDir>

#include <filesystem>
#include <system_error>

namespace bcg::sys {

namespace fs = std::filesystem;

namespace {

qint64 sizeOf(const fs::path& path)
{
    std::error_code ec;
    qint64 total = 0;
    if (fs::is_symlink(path, ec))
        return 0;
    if (!fs::is_directory(path, ec)) {
        const auto size = fs::file_size(path, ec);
        return ec ? 0 : static_cast<qint64>(size);
    }
    fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::directory_iterator(); it.increment(ec))
        total += sizeOf(it->path());
    return total;
}

void removeEntry(const fs::path& path, CleanResult& result)
{
    std::error_code ec;
    const bool isDir = fs::is_directory(path, ec) && !fs::is_symlink(path, ec);
    if (!isDir) {
        const qint64 size = sizeOf(path);
        if (fs::remove(path, ec) && !ec) {
            result.freedBytes += size;
            ++result.removedFiles;
        } else {
            ++result.skippedFiles;
        }
        return;
    }

    std::error_code listEc;
    fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, listEc);
    for (; !listEc && it != fs::directory_iterator(); it.increment(listEc))
        removeEntry(it->path(), result);
    fs::remove(path, ec); // an empty folder goes away; a non-empty one stays
}

} // namespace

QString tempFolder()
{
    return QDir::toNativeSeparators(QDir::tempPath());
}

qint64 folderSize(const QString& path)
{
    return sizeOf(fs::path(path.toStdWString()));
}

CleanResult clearFolder(const QString& path)
{
    CleanResult result;
    const fs::path root(path.toStdWString());
    // Never run on something that is not clearly a temp folder.
    if (path.size() < 8 || root == root.root_path())
        return result;

    std::error_code ec;
    fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::directory_iterator(); it.increment(ec))
        removeEntry(it->path(), result);
    return result;
}

} // namespace bcg::sys
