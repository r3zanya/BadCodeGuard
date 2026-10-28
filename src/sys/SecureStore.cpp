#include "SecureStore.h"

#include <QByteArray>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace bcg::secure {

namespace {
const char kPrefix[] = "dpapi:";

DATA_BLOB blobOf(const QByteArray& bytes)
{
    DATA_BLOB b;
    b.cbData = DWORD(bytes.size());
    b.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(bytes.constData()));
    return b;
}

// A fixed extra key mixed into the encryption; it ties values to this program.
const QByteArray& entropy()
{
    static const QByteArray e("BadCodeGuard-settings-v1");
    return e;
}
} // namespace

QString protect(const QString& plain)
{
    if (plain.isEmpty())
        return {};
    const QByteArray bytes = plain.toUtf8();
    DATA_BLOB in = blobOf(bytes);
    DATA_BLOB extra = blobOf(entropy());
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"BadCodeGuard", &extra, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    const QByteArray cipher(reinterpret_cast<const char*>(out.pbData), qsizetype(out.cbData));
    LocalFree(out.pbData);
    return QString::fromLatin1(kPrefix) + QString::fromLatin1(cipher.toBase64());
}

QString unprotect(const QString& stored)
{
    if (!stored.startsWith(QLatin1String(kPrefix)))
        return stored;
    const QByteArray cipher = QByteArray::fromBase64(stored.mid(int(sizeof(kPrefix)) - 1).toLatin1());
    if (cipher.isEmpty())
        return {};
    DATA_BLOB in = blobOf(cipher);
    DATA_BLOB extra = blobOf(entropy());
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &extra, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    const QString plain = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData), qsizetype(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return plain;
}

QString readSecret(QSettings& settings, const QString& key)
{
    const QString stored = settings.value(key).toString();
    if (stored.isEmpty())
        return {};
    const QString plain = unprotect(stored);
    // Migrate a value that was saved as plain text by an older version.
    if (!stored.startsWith(QLatin1String(kPrefix)) && !plain.isEmpty()) {
        const QString wrapped = protect(plain);
        if (!wrapped.isEmpty())
            settings.setValue(key, wrapped);
    }
    return plain;
}

void writeSecret(QSettings& settings, const QString& key, const QString& value)
{
    if (value.isEmpty()) {
        settings.remove(key);
        return;
    }
    const QString wrapped = protect(value);
    // If encryption is unavailable keep working rather than lose the setting.
    settings.setValue(key, wrapped.isEmpty() ? value : wrapped);
}

} // namespace bcg::secure
