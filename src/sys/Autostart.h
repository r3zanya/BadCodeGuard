#pragma once

#include <QString>

// Start-with-Windows via the per-user Run key (no administrator rights needed).
namespace bcg::autostart {

// Command-line flag the registered entry passes, so the app starts in the tray.
inline const char* kFlag = "--autostart";

// True when the Run entry exists and points at this executable.
bool isEnabled();
bool setEnabled(bool enabled);

} // namespace bcg::autostart
