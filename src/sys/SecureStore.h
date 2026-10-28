#pragma once

#include <QSettings>
#include <QString>

namespace bcg::secure {

// Encrypts text with the Windows Data Protection API, bound to the current
// Windows user: a copy of the registry or of a settings file is useless on
// another account or computer. It does not hide a secret from other programs
// that run as the same user. Returns "dpapi:<base64>"; empty input gives empty.
QString protect(const QString& plain);
// Reverses protect(). A value without the "dpapi:" prefix is returned as it is
// (settings written by older versions). Empty on failure.
QString unprotect(const QString& stored);

// Reads a secret from QSettings. Old plain-text values are re-saved encrypted.
QString readSecret(QSettings& settings, const QString& key);
void writeSecret(QSettings& settings, const QString& key, const QString& value);

} // namespace bcg::secure
