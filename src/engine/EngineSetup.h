#pragma once

#include "FileScanner.h"

namespace bcg {

// Folder for user data: quarantine vault and extra signature files.
QString dataFolder();
QString signaturesFolder();
// *.yar / *.yara files below this folder are loaded as YARA rules.
QString yaraFolder();

struct EngineLoadReport {
    int hashes = 0;
    int rules = 0;
    int yaraRules = 0;
    int yaraFailed = 0; // rule files that did not compile
    QStringList errors; // rule files that failed to parse
};

// Builds the scanner context: built-in resources (":/signatures/…") plus the
// user's files from signaturesFolder(). `exclusions` is loaded by the caller.
ScanContext buildContext(std::shared_ptr<ExclusionList> exclusions, EngineLoadReport* report = nullptr);

// Re-reads the hash lists and swaps them into the running context. Returns the
// number of hashes now loaded. Safe while scans are running.
int reloadSignatures(const ScanContext& ctx);

// Same for the YARA rule files. Returns what was loaded.
YaraLoadReport reloadYara(const ScanContext& ctx);

} // namespace bcg
