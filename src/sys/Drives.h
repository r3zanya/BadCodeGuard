#pragma once

#include <QStringList>

namespace bcg::drives {

// Roots of removable drives that are ready right now, e.g. "E:/" (USB sticks,
// memory cards). Fixed disks, network drives and optical drives are not listed.
QStringList removableRoots();

} // namespace bcg::drives
