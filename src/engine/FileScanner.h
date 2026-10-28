#pragma once

#include "ArchiveReader.h"
#include "ExclusionList.h"
#include "IScanner.h"
#include "PeAnalyzer.h"
#include "Quarantine.h"
#include "RuleEngine.h"
#include "ScriptAnalyzer.h"
#include "SignatureDb.h"
#include "YaraEngine.h"

#include <atomic>
#include <memory>

namespace bcg {

// User-adjustable switches, readable from the scanner thread.
struct ScanOptions {
    std::atomic_bool autoQuarantine{true}; // signature and rule hits only
    std::atomic_bool heuristics{true};
    std::atomic_bool archives{true};       // look inside zip, 7z, rar, ...
};

// Everything the scanner consults; shared with the UI.
struct ScanContext {
    std::shared_ptr<ExclusionList> exclusions;
    std::shared_ptr<SignatureStore> signatures;
    std::shared_ptr<const RuleEngine> rules;
    std::shared_ptr<YaraStore> yara;
    std::shared_ptr<Quarantine> quarantine;
    std::shared_ptr<ScanOptions> options;
};

struct Detection {
    QString name;
    ThreatKind kind = ThreatKind::Signature;
};

// Walks the file tree and checks every file: exclusions, SHA-256 database,
// pattern rules, then PE heuristics. Detections are quarantined automatically
// when enabled (heuristic ones never are).
class FileScanner : public IScanner {
    Q_OBJECT
public:
    explicit FileScanner(ScanContext context, QObject* parent = nullptr);

    void cancel() override { m_cancel = true; }

    // Checks one file without touching it. Returns false when it looks clean.
    // Usable from any thread.
    // `notes`, when given, receives remarks such as "archive could not be opened".
    bool inspect(const QString& path, Detection& out, QStringList* notes = nullptr) const;

public slots:
    void start(const QStringList& paths) override;

private:
    // Rules, YARA, script/document and executable heuristics over file content.
    // `realPath` is empty for a file found inside an archive.
    bool inspectContent(const QString& name, const QByteArray& data, qint64 size,
                        const QString& realPath, Detection& out) const;
    // One unpacked file: hash, content checks, then archives nested in it.
    bool inspectEntry(const QString& chain, const QByteArray& data, int depth,
                      ArchiveBudget& budget, Detection& out) const;
    bool scanZip(const QByteArray& zip, const QString& chain, int depth, ArchiveBudget& budget,
                 Detection& out) const;
    bool scanArchiveFile(const QString& path, ArchiveBudget& budget, Detection& out) const;

    ScanContext m_ctx;
    ArchiveLimits m_limits;
    std::atomic_bool m_cancel{false};
};

} // namespace bcg
