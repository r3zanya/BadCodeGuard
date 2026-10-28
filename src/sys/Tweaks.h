#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace bcg::sys {

// A system setting the user can switch on or off. "Applied" means the
// tweak is active, e.g. for "Disable UAC" it means UAC is disabled.
struct Tweak {
    QString id;
    QString title;
    QString description;
    bool needsAdmin = true;
    bool needsReboot = false;
    bool risky = false; // lowers protection: ask before applying
    std::function<bool()> isApplied;
    std::function<bool(bool apply)> apply; // blocking: call it off the GUI thread
};

const QList<Tweak>& tweaks();

} // namespace bcg::sys
