#include "Autostart.h"

#include "sys/Registry.h"

#include <QCoreApplication>
#include <QDir>

namespace bcg::autostart {

namespace {
const QString kRunKey = QStringLiteral("Software\Microsoft\Windows\CurrentVersion\Run");
const QString kValueName = QStringLiteral("BadCodeGuard");

QString command()
{
    return QStringLiteral("\"%1\" %2")
        .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
             QString::fromLatin1(kFlag));
}
} // namespace

bool isEnabled()
{
    const auto value = reg::readString(reg::Root::CurrentUser, kRunKey, kValueName);
    return value && value->compare(command(), Qt::CaseInsensitive) == 0;
}

bool setEnabled(bool enabled)
{
    return enabled ? reg::writeString(reg::Root::CurrentUser, kRunKey, kValueName, command())
                   : reg::deleteValue(reg::Root::CurrentUser, kRunKey, kValueName);
}

} // namespace bcg::autostart
