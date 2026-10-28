#include "FileScanner.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <thread>

namespace bcg {

namespace {
constexpr qint64 kProgressIntervalMs = 50;
constexpr qint64 kReadLimit = 16 * 1024 * 1024;       // bytes examined by rules and heuristics
constexpr qint64 kHashLimit = 512LL * 1024 * 1024;    // larger files are not hashed
constexpr qint64 kChunk = 1 << 20;
constexpr int kArchiveSuspiciousScore = 75;           // heuristics inside archives need more evidence

QString key(const QString& path)
{
    return QDir::fromNativeSeparators(path).toLower();
}

// Where inspect() collects remarks about the file it is working on (set by the caller).
thread_local QStringList* t_notes = nullptr;

void noteArchive(const QString& name, ArchiveStatus status)
{
    if (!t_notes)
        return;
    switch (status) {
    case ArchiveStatus::Ok:
        break;
    case ArchiveStatus::Encrypted:
        *t_notes << QStringLiteral("Архив защищён паролем, часть файлов не проверена: %1").arg(name);
        break;
    case ArchiveStatus::Truncated:
        *t_notes << QStringLiteral("Архив слишком большой или сложный, проверена только часть: %1").arg(name);
        break;
    case ArchiveStatus::Unreadable:
        *t_notes << QStringLiteral("Архив не удалось открыть (повреждён, с паролем или формат не поддерживается): %1")
                        .arg(name);
        break;
    }
}
} // namespace

FileScanner::FileScanner(ScanContext context, QObject* parent)
    : IScanner(parent)
    , m_ctx(std::move(context))
{
}

bool FileScanner::inspect(const QString& path, Detection& out, QStringList* notes) const
{
    struct NotesScope {
        explicit NotesScope(QStringList* n) { t_notes = n; }
        ~NotesScope() { t_notes = nullptr; }
    } notesScope(notes);

    QFile file(path);
    const qint64 size = file.size();
    if (size <= 0 || !file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray head = file.read(kReadLimit);
    if (head.isEmpty())
        return false;

    // 1. Known-bad hash.
    const std::shared_ptr<const SignatureDb> signatures =
        m_ctx.signatures ? m_ctx.signatures->get() : nullptr;
    if (signatures && signatures->size() > 0 && size <= kHashLimit) {
        QByteArray digest;
        if (size <= head.size()) {
            digest = QCryptographicHash::hash(head, QCryptographicHash::Sha256);
        } else {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(head);
            while (!file.atEnd()) {
                if (m_cancel)
                    return false;
                hash.addData(file.read(kChunk));
            }
            digest = hash.result();
        }
        const QString name = signatures->lookup(digest);
        if (!name.isEmpty()) {
            out = {name, ThreatKind::Signature};
            return true;
        }
    }

    // 2. Rules, YARA and heuristics over the content.
    if (inspectContent(path, head, size, path, out))
        return true;

    // 3. Inside archives.
    if (m_ctx.options && m_ctx.options->archives && isArchiveName(path) && !m_cancel) {
        ArchiveBudget budget;
        return scanArchiveFile(path, budget, out);
    }
    return false;
}

bool FileScanner::inspectContent(const QString& name, const QByteArray& data, qint64 size,
                                 const QString& realPath, Detection& out) const
{
    // Pattern rules (own .bcg format).
    if (m_ctx.rules && m_ctx.rules->size() > 0) {
        const QString hit = m_ctx.rules->match(data, size);
        if (!hit.isEmpty()) {
            out = {hit, ThreatKind::Rule};
            return true;
        }
    }

    // YARA.
    if (m_ctx.yara) {
        const std::shared_ptr<const YaraEngine> yara = m_ctx.yara->get();
        if (yara && !yara->empty()) {
            const QString hit = yara->scan(data, name);
            if (!hit.isEmpty()) {
                out = {hit, ThreatKind::Yara};
                return true;
            }
        }
    }

    // Heuristics. Windows' own files and validly signed programs are trusted.
    const bool inArchive = realPath.isEmpty();
    if (!m_ctx.options || !m_ctx.options->heuristics)
        return false;
    if (!inArchive && Quarantine::isProtectedPath(realPath))
        return false;

    if (isScriptOrDocument(name, data)) {
        const HeuristicReport report = analyzeScript(name, data);
        if (report.score >= kSuspiciousScore) {
            out = {QStringLiteral("Heur:Script (%1) — %2")
                       .arg(report.score)
                       .arg(report.reasons.join(QStringLiteral("; "))),
                   ThreatKind::Heuristic};
            return true;
        }
        return false;
    }

    // Packed installers inside archives look much like packed malware: ask for more evidence.
    const int threshold = inArchive ? kArchiveSuspiciousScore : kSuspiciousScore;
    const HeuristicReport report = analyzeFile(name, data, size);
    if (report.score >= threshold && (inArchive || !hasTrustedSignature(realPath))) {
        out = {QStringLiteral("Heur:Suspicious (%1) — %2")
                   .arg(report.score)
                   .arg(report.reasons.join(QStringLiteral("; "))),
               ThreatKind::Heuristic};
        return true;
    }
    return false;
}

bool FileScanner::inspectEntry(const QString& chain, const QByteArray& data, int depth,
                               ArchiveBudget& budget, Detection& out) const
{
    if (data.isEmpty())
        return false;
    const auto tag = [&](Detection& d) {
        d.name = QStringLiteral("%1 [в архиве: %2]").arg(d.name, chain);
    };

    const std::shared_ptr<const SignatureDb> signatures =
        m_ctx.signatures ? m_ctx.signatures->get() : nullptr;
    if (signatures && signatures->size() > 0) {
        const QString hit = signatures->lookup(QCryptographicHash::hash(data, QCryptographicHash::Sha256));
        if (!hit.isEmpty()) {
            out = {hit, ThreatKind::Signature};
            tag(out);
            return true;
        }
    }

    const QByteArray head = data.size() > kReadLimit ? data.left(kReadLimit) : data;
    if (inspectContent(chain, head, data.size(), QString(), out)) {
        tag(out);
        return true;
    }

    // A zip inside a zip, up to the depth limit.
    if (depth < m_limits.maxDepth && looksLikeZip(data))
        return scanZip(data, chain, depth + 1, budget, out);
    return false;
}

bool FileScanner::scanZip(const QByteArray& zip, const QString& chain, int depth,
                          ArchiveBudget& budget, Detection& out) const
{
    bool found = false;
    const ArchiveStatus status = visitZip(
        zip,
        [&](const QString& entry, const QByteArray& data) {
            if (m_cancel)
                return true;
            found = inspectEntry(chain + QLatin1Char('/') + entry, data, depth, budget, out);
            return found;
        },
        m_limits, budget);
    if (!found)
        noteArchive(chain, status);
    return found;
}

bool FileScanner::scanArchiveFile(const QString& path, ArchiveBudget& budget, Detection& out) const
{
    QFile file(path);
    if (file.size() <= 0 || file.size() > m_limits.maxTotal || !file.open(QIODevice::ReadOnly))
        return false;
    const QString chain = QFileInfo(path).fileName();

    const QByteArray magic = file.peek(4);
    if (looksLikeZip(magic))
        return scanZip(file.readAll(), chain, 1, budget, out);
    file.close();

    bool found = false;
    const ArchiveStatus status = visitExternal(
        path,
        [&](const QString& entry, const QByteArray& data) {
            if (m_cancel)
                return true;
            found = inspectEntry(chain + QLatin1Char('/') + entry, data, 1, budget, out);
            return found;
        },
        m_limits, budget);
    if (!found)
        noteArchive(chain, status);
    return found;
}

void FileScanner::start(const QStringList& paths)
{
    m_cancel = false;
    qint64 scanned = 0;
    qint64 threats = 0;
    qint64 skipped = 0; // excluded files and folders
    QElapsedTimer sinceProgress;
    sinceProgress.start();

    const QString quarantineRoot = m_ctx.quarantine ? key(m_ctx.quarantine->root()) + QLatin1Char('/')
                                                    : QString();
    auto excludedFile = [&](const QString& file) {
        return m_ctx.exclusions && m_ctx.exclusions->matches(file);
    };
    auto excludedFolder = [&](const QString& dir) {
        if (!quarantineRoot.isEmpty() && (key(dir) + QLatin1Char('/')).startsWith(quarantineRoot))
            return true; // never scan our own vault
        return m_ctx.exclusions && m_ctx.exclusions->matchesFolder(dir);
    };

    // The same walk serves the scan and the background counter that makes the
    // progress bar exact. An own walk (not a recursive iterator) lets an excluded
    // folder be skipped together with everything below it.
    auto walk = [&](const auto& onFile, const auto& onSkippedFolder) {
        for (const QString& path : paths) {
            if (m_cancel)
                break;
            const QFileInfo info(path);
            if (info.isFile()) {
                onFile(info.absoluteFilePath());
                continue;
            }
            QStringList pending{info.absoluteFilePath()};
            while (!pending.isEmpty() && !m_cancel) {
                const QString dir = pending.takeLast();
                if (excludedFolder(dir)) {
                    onSkippedFolder();
                    continue;
                }
                QDirIterator it(dir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden
                                         | QDir::NoSymLinks);
                while (it.hasNext() && !m_cancel) {
                    const QString entry = it.next();
                    if (it.fileInfo().isDir())
                        pending.append(entry);
                    else
                        onFile(entry);
                }
            }
        }
    };

    std::atomic<qint64> total{0};
    std::atomic_bool totalDone{false};
    std::thread counter([&] {
        qint64 n = 0;
        walk(
            [&](const QString& file) {
                if (excludedFile(file))
                    return;
                if (++n % 256 == 0)
                    total.store(n);
            },
            [] {});
        total.store(n);
        totalDone = true;
    });
    qint64 reportedTotal = -1;
    bool reportedDone = false;
    auto reportTotal = [&] {
        const qint64 t = total.load();
        const bool d = totalDone.load();
        if (t != reportedTotal || d != reportedDone) {
            reportedTotal = t;
            reportedDone = d;
            emit totalChanged(t, d);
        }
    };

    auto visit = [&](const QString& file) {
        if (excludedFile(file)) {
            ++skipped;
            return;
        }
        ++scanned;
        Detection detection;
        QStringList notes;
        const bool bad = inspect(file, detection, &notes);
        for (const QString& n : std::as_const(notes))
            emit note(n);
        if (bad) {
            ++threats;
            QString quarantineId;
            QString quarantineError;
            const bool auto_ = m_ctx.quarantine && m_ctx.options && m_ctx.options->autoQuarantine
                && detection.kind != ThreatKind::Heuristic;
            if (auto_) {
                const QuarantineResult r = m_ctx.quarantine->add(file, detection.name,
                                                                 int(detection.kind));
                if (r.ok)
                    quarantineId = r.entry.id;
                else
                    quarantineError = r.error;
            }
            emit threatFound(file, detection.name, int(detection.kind), quarantineId, quarantineError);
        }
        if (sinceProgress.elapsed() >= kProgressIntervalMs) {
            emit progress(scanned, file);
            reportTotal();
            sinceProgress.restart();
        }
    };

    walk(visit, [&] { ++skipped; });

    counter.join(); // the walk stops on cancel, so this returns promptly
    reportTotal();
    emit progress(scanned, QString());
    emit finished(m_cancel, scanned, threats, skipped);
}

} // namespace bcg
