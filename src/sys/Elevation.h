#pragma once

namespace bcg::sys {

bool isElevated();

// Starts a new elevated instance (UAC prompt). Returns true when the user
// accepted; the caller should then quit the current instance.
bool relaunchElevated();

} // namespace bcg::sys
