#include "Drives.h"

#include <QDir>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bcg::drives {

QStringList removableRoots()
{
    QStringList roots;
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i)))
            continue;
        const QString root = QStringLiteral("%1:/").arg(QChar(u'A' + i));
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(root).utf16())) != DRIVE_REMOVABLE)
            continue;
        if (QDir(root).exists()) // a card reader without a card does not exist as a folder
            roots << root;
    }
    return roots;
}

} // namespace bcg::drives
