#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace bcg {

struct HeuristicReport {
    int score = 0;
    QStringList reasons;
};

// Static heuristics over a Windows executable: header oddities, packing,
// process-injection imports, disguised names. Pure function, thread-safe.
// A score of kSuspiciousScore or more is worth reporting.
constexpr int kSuspiciousScore = 60;

// `data` holds the beginning of the file, `fileSize` its real size.
HeuristicReport analyzeFile(const QString& path, const QByteArray& data, qint64 fileSize);

// True when the file carries a valid Authenticode signature (no network).
bool hasTrustedSignature(const QString& path);

// Shannon entropy in bits per byte (0..8).
double entropy(const QByteArray& bytes);

} // namespace bcg
