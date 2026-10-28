#include "Registry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vector>

namespace bcg::reg {

namespace {

HKEY rootKey(Root root)
{
    return root == Root::LocalMachine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

const wchar_t* wide(const QString& s)
{
    return reinterpret_cast<const wchar_t*>(s.utf16());
}

bool openForRead(Root root, const QString& subKey, HKEY* key)
{
    return RegOpenKeyExW(rootKey(root), wide(subKey), 0, KEY_READ | KEY_WOW64_64KEY, key)
        == ERROR_SUCCESS;
}

bool openForWrite(Root root, const QString& subKey, HKEY* key)
{
    return RegCreateKeyExW(rootKey(root), wide(subKey), 0, nullptr, 0,
                           KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, key, nullptr)
        == ERROR_SUCCESS;
}

} // namespace

std::optional<quint32> readDword(Root root, const QString& subKey, const QString& name)
{
    HKEY key = nullptr;
    if (!openForRead(root, subKey, &key))
        return std::nullopt;
    DWORD type = 0;
    DWORD data = 0;
    DWORD size = sizeof(data);
    const LONG rc = RegQueryValueExW(key, wide(name), nullptr, &type,
                                     reinterpret_cast<LPBYTE>(&data), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_DWORD)
        return std::nullopt;
    return static_cast<quint32>(data);
}

std::optional<QString> readString(Root root, const QString& subKey, const QString& name)
{
    HKEY key = nullptr;
    if (!openForRead(root, subKey, &key))
        return std::nullopt;
    DWORD type = 0;
    DWORD size = 0;
    LONG rc = RegQueryValueExW(key, wide(name), nullptr, &type, nullptr, &size);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    rc = RegQueryValueExW(key, wide(name), nullptr, &type,
                          reinterpret_cast<LPBYTE>(buffer.data()), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS)
        return std::nullopt;
    return QString::fromWCharArray(buffer.data());
}

bool writeDword(Root root, const QString& subKey, const QString& name, quint32 value)
{
    HKEY key = nullptr;
    if (!openForWrite(root, subKey, &key))
        return false;
    const DWORD data = value;
    const LONG rc = RegSetValueExW(key, wide(name), 0, REG_DWORD,
                                   reinterpret_cast<const BYTE*>(&data), sizeof(data));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool writeString(Root root, const QString& subKey, const QString& name, const QString& value)
{
    HKEY key = nullptr;
    if (!openForWrite(root, subKey, &key))
        return false;
    const LONG rc = RegSetValueExW(key, wide(name), 0, REG_SZ,
                                   reinterpret_cast<const BYTE*>(wide(value)),
                                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool deleteValue(Root root, const QString& subKey, const QString& name)
{
    HKEY key = nullptr;
    const LONG open = RegOpenKeyExW(rootKey(root), wide(subKey), 0,
                                    KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
    if (open == ERROR_FILE_NOT_FOUND)
        return true;
    if (open != ERROR_SUCCESS)
        return false;
    const LONG rc = RegDeleteValueW(key, wide(name));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

} // namespace bcg::reg
