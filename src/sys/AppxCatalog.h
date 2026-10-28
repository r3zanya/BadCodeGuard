#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace bcg::sys {

struct AppEntry {
    QString title;
    QString description;
    QStringList packages; // AppX package names; one entry may span several
};

enum class AppGroup { Extra, Popular, Essential };

const QList<AppEntry>& appsIn(AppGroup group);

// Blocking: names of the AppX packages installed for the current user.
QSet<QString> installedPackages();

bool isInstalled(const AppEntry& entry, const QSet<QString>& installed);

// PowerShell script that removes the entry's packages for all users and
// from the provisioning list (so they do not return for new accounts).
QString removalScript(const AppEntry& entry);

} // namespace bcg::sys
