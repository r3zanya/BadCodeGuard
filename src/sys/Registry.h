#pragma once

#include <QString>

#include <optional>

// Thin wrappers over the Win32 registry API (64-bit view).
namespace bcg::reg {

enum class Root { LocalMachine, CurrentUser };

std::optional<quint32> readDword(Root root, const QString& subKey, const QString& name);
std::optional<QString> readString(Root root, const QString& subKey, const QString& name);

// Write helpers create the key when needed. Return false on failure
// (typically "access denied" without administrator rights).
bool writeDword(Root root, const QString& subKey, const QString& name, quint32 value);
bool writeString(Root root, const QString& subKey, const QString& name, const QString& value);

// Succeeds when the value is gone afterwards, including when it never existed.
bool deleteValue(Root root, const QString& subKey, const QString& name);

} // namespace bcg::reg
