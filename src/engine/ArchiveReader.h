#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

namespace bcg {

// Hard caps that keep a hostile archive (zip bomb, endless nesting) harmless.
struct ArchiveLimits {
    int maxEntries = 5000;                      // across the whole archive tree
    qint64 maxEntrySize = 64LL * 1024 * 1024;   // one unpacked file
    qint64 maxTotal = 512LL * 1024 * 1024;      // all unpacked data of one scan
    int maxDepth = 3;                           // archives inside archives
    int externalTimeoutSec = 60;
};

// Shared by every archive opened for one file, so nesting cannot multiply the work.
struct ArchiveBudget {
    qint64 bytes = 0;
    int entries = 0;
};

enum class ArchiveStatus {
    Ok,
    Unreadable, // not an archive we can open (damaged, unsupported, needs a password)
    Encrypted,  // some entries are password protected and were skipped
    Truncated,  // a limit was reached; the rest was not examined
};

// Called for every unpacked file; return true to stop early.
using EntryVisitor = std::function<bool(const QString& name, const QByteArray& data)>;

// By file name: .zip, .jar, Office Open XML, .7z, .rar, .tar, .gz, ...
bool isArchiveName(const QString& path);
// "PK\x03\x04" at the start.
bool looksLikeZip(const QByteArray& data);

// .zip and friends, unpacked in memory (miniz).
ArchiveStatus visitZip(const QByteArray& zip, const EntryVisitor& visit, const ArchiveLimits& limits,
                       ArchiveBudget& budget);

// 7z, rar, tar, gz, ...: unpacked with the tar.exe that ships with Windows into
// a temporary folder that is removed afterwards.
ArchiveStatus visitExternal(const QString& path, const EntryVisitor& visit,
                            const ArchiveLimits& limits, ArchiveBudget& budget);

} // namespace bcg
