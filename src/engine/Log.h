#pragma once

#include <QString>

namespace bcg::log {

// Plain-text log of what the program itself did and what went wrong (failed
// updates, quarantine errors, watchers that stopped, ...). One line per event,
// rotated at about 1 MB. Thread-safe; never throws. Secrets must not be passed in.
void info(const QString& text);
void warn(const QString& text);
void error(const QString& text);

// Where the log lives (logs/badcodeguard.log in the data folder).
QString file();
// Redirects the log, for tests.
void setFile(const QString& path);

} // namespace bcg::log
