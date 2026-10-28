#pragma once

#include "PeAnalyzer.h"

namespace bcg {

// True for files the script/document analyser understands: PowerShell, batch,
// JS/VBS/HTA, shortcuts (.lnk), PDF and legacy Office files.
bool isScriptOrDocument(const QString& name, const QByteArray& data);

// Static heuristics over script text, shortcuts, PDFs and Office files. Scores
// add up from independent signals (encoded commands, download-and-run, shadow
// copy deletion, auto-run macros, ...); kSuspiciousScore or more is worth
// reporting. Pure function, thread-safe.
HeuristicReport analyzeScript(const QString& name, const QByteArray& data);

} // namespace bcg
