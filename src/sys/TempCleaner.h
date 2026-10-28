#pragma once

#include <QString>

namespace bcg::sys {

struct CleanResult {
    qint64 freedBytes = 0;
    int removedFiles = 0;
    int skippedFiles = 0; // in use or access denied
};

// The current user's TEMP folder.
QString tempFolder();

// Blocking: total size of all files below the folder.
qint64 folderSize(const QString& path);

// Blocking: deletes everything it can below the folder; the folder itself
// stays. Files that are in use are skipped.
CleanResult clearFolder(const QString& path);

} // namespace bcg::sys
