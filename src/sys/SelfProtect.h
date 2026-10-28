#pragma once

namespace bcg::selfprotect {

// Takes the right to end, suspend or inject into this process away from other
// programs (a deny entry in the process's access list). That stops ordinary
// malware from killing the guard; it does not stop kernel-level attackers or an
// administrator who holds the debug privilege. While it is on, "End task" in
// Task Manager fails for a normal user; use the tray's "Выход" instead.
// Returns true when the access list was changed.
bool enable();
// Restores the normal access list.
bool disable();

} // namespace bcg::selfprotect
