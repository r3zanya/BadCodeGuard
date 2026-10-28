#pragma once

#include "Swap.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace bcg {

struct YaraLoadReport {
    int files = 0;   // rule files compiled
    int failed = 0;  // rule files skipped because they did not compile
    int rules = 0;   // individual rules loaded
    QStringList errors; // first few compile errors, for display
};

// YARA rules (libyara). Every rule file is compiled on its own, so one broken
// or unsupported file never takes the others down. Immutable after loading:
// scan() may be called from many threads at once.
class YaraEngine {
public:
    YaraEngine();
    ~YaraEngine();
    YaraEngine(const YaraEngine&) = delete;
    YaraEngine& operator=(const YaraEngine&) = delete;

    // Compiles every *.yar / *.yara below `folder` (recursive).
    YaraLoadReport loadFolder(const QString& folder);
    // Compiles rule text. Returns an error message, or an empty string on success.
    QString addSource(const QString& source);

    int ruleCount() const { return m_ruleCount; }
    bool empty() const { return m_sets.empty(); }

    // Name of the first rule that matches (its `threat` meta string when it has
    // one), prefixed with "YARA: "; empty when nothing matches. `fileName` feeds
    // the external variables (filename, extension, ...) community rules use.
    QString scan(const QByteArray& data, const QString& fileName = QString()) const;

private:
    std::vector<void*> m_sets; // YR_RULES*, one per source file
    int m_ruleCount = 0;
};

using YaraStore = Swap<YaraEngine>;

} // namespace bcg
