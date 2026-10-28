#include "PeAnalyzer.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>
#endif

namespace bcg {

namespace {

constexpr quint32 kSectionExec = 0x20000000;
constexpr quint32 kSectionWrite = 0x80000000;
constexpr double kHighEntropy = 7.2;
constexpr int kEntropySample = 256 * 1024;

quint16 u16(const QByteArray& d, qsizetype off)
{
    return qFromLittleEndian<quint16>(d.constData() + off);
}
quint32 u32(const QByteArray& d, qsizetype off)
{
    return qFromLittleEndian<quint32>(d.constData() + off);
}

const QSet<QString>& executableExtensions()
{
    static const QSet<QString> set{
        QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("sys"), QStringLiteral("scr"),
        QStringLiteral("ocx"), QStringLiteral("cpl"), QStringLiteral("drv"), QStringLiteral("efi"),
        QStringLiteral("mui"), QStringLiteral("com"), QStringLiteral("tmp"), QStringLiteral("bin"),
        QStringLiteral("dat"), QStringLiteral("node"), QStringLiteral("pyd"), QStringLiteral("ax"),
        QStringLiteral("winmd"), QStringLiteral("tlb"), QStringLiteral("rll"), QStringLiteral("lib"),
        QStringLiteral("mun"), QStringLiteral("vdm"), QStringLiteral("xll"), QStringLiteral("wll"),
    };
    return set;
}

const QSet<QString>& harmlessExtensions()
{
    static const QSet<QString> set{
        QStringLiteral("txt"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("pdf"), QStringLiteral("doc"),
        QStringLiteral("docx"), QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("ppt"),
        QStringLiteral("pptx"), QStringLiteral("mp3"), QStringLiteral("mp4"), QStringLiteral("avi"),
        QStringLiteral("mkv"), QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("csv"),
        QStringLiteral("rtf"), QStringLiteral("ini"), QStringLiteral("log"), QStringLiteral("json"),
    };
    return set;
}

// Imports that together make up the classic "inject into another process" recipe.
const QList<QByteArray>& injectionApis()
{
    static const QList<QByteArray> list{
        "VirtualAllocEx", "WriteProcessMemory", "CreateRemoteThread", "NtUnmapViewOfSection",
        "QueueUserAPC", "SetThreadContext", "NtWriteVirtualMemory", "RtlCreateUserThread",
    };
    return list;
}

} // namespace

double entropy(const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return 0.0;
    quint32 counts[256] = {};
    for (unsigned char c : bytes)
        ++counts[c];
    double e = 0.0;
    const double n = double(bytes.size());
    for (quint32 c : counts) {
        if (c) {
            const double p = c / n;
            e -= p * std::log2(p);
        }
    }
    return e;
}

HeuristicReport analyzeFile(const QString& path, const QByteArray& d, qint64 fileSize)
{
    HeuristicReport r;
    const QFileInfo info(path);
    const QString name = info.fileName().toLower();
    const QString ext = info.suffix().toLower();

    static const QRegularExpression doubleExt(
        QStringLiteral("\\.(pdf|docx?|xlsx?|pptx?|jpe?g|png|gif|txt|mp3|mp4|zip|rar)\\s*\\.(exe|scr|com|bat|cmd|vbs|js|jse|wsf|pif)$"));
    if (doubleExt.match(name).hasMatch()) {
        r.score += 50;
        r.reasons << QStringLiteral("двойное расширение (маскировка под документ)");
    }

    // Is this a PE file at all?
    if (d.size() < 0x100 || d[0] != 'M' || d[1] != 'Z')
        return r;
    const quint32 pe = u32(d, 0x3C);
    if (pe < 0x40 || qsizetype(pe) + 0x108 > d.size() || d.mid(pe, 4) != QByteArray("PE\0\0", 4))
        return r;

    const quint16 sectionCount = u16(d, pe + 6);
    const quint16 optSize = u16(d, pe + 20);
    const qsizetype opt = pe + 24;
    const quint16 magic = u16(d, opt);
    if ((magic != 0x10B && magic != 0x20B) || sectionCount == 0 || sectionCount > 96)
        return r;

    if (!executableExtensions().contains(ext) && harmlessExtensions().contains(ext)) {
        r.score += 50;
        r.reasons << QStringLiteral("исполняемый файл с расширением .%1").arg(ext);
    }

    const quint32 entryRva = u32(d, opt + 16);
    const qsizetype dirStart = opt + (magic == 0x20B ? 112 : 96);
    quint32 importSize = 0;
    if (dirStart + 16 <= d.size())
        importSize = u32(d, dirStart + 8 + 4);

    bool entryFound = false;
    bool entryInLast = false;
    bool entryNotExec = false;
    int writeExec = 0;
    int highEntropy = 0;
    QStringList packerSections;
    const qsizetype table = opt + optSize;
    for (int i = 0; i < sectionCount; ++i) {
        const qsizetype s = table + qsizetype(i) * 40;
        if (s + 40 > d.size())
            break;
        const QByteArray secName = QByteArray(d.constData() + s, 8).split('\0').value(0);
        const quint32 vsize = u32(d, s + 8);
        const quint32 vaddr = u32(d, s + 12);
        const quint32 rawSize = u32(d, s + 16);
        const quint32 rawPtr = u32(d, s + 20);
        const quint32 flags = u32(d, s + 36);

        if ((flags & kSectionExec) && (flags & kSectionWrite))
            ++writeExec;

        const QByteArray lower = secName.toLower();
        if (lower.startsWith("upx") || lower == ".aspack" || lower == ".adata" || lower == ".themida"
            || lower == ".vmp0" || lower == ".vmp1" || lower == ".petite" || lower == ".mpress1"
            || lower == ".enigma1")
            packerSections << QString::fromLatin1(secName);

        if (rawSize >= 8192 && qint64(rawPtr) + 4096 < d.size()) {
            const qsizetype len = std::min<qsizetype>(
                std::min<qsizetype>(qsizetype(rawSize), kEntropySample), d.size() - qsizetype(rawPtr));
            if (entropy(d.mid(rawPtr, len)) >= kHighEntropy)
                ++highEntropy;
        }

        const quint32 span = qMax(vsize, rawSize);
        if (entryRva >= vaddr && entryRva < vaddr + span) {
            entryFound = true;
            entryInLast = (i == sectionCount - 1) && sectionCount > 1;
            entryNotExec = !(flags & kSectionExec);
        }
    }

    if (writeExec > 0) {
        r.score += 15;
        r.reasons << QStringLiteral("секция одновременно для записи и исполнения");
    }
    if (highEntropy > 0) {
        r.score += 20;
        r.reasons << QStringLiteral("высокая энтропия данных (упакован или зашифрован)");
    }
    if (!packerSections.isEmpty()) {
        r.score += 15;
        r.reasons << QStringLiteral("секции упаковщика: %1").arg(packerSections.join(QLatin1String(", ")));
    }
    if (entryInLast) {
        r.score += 15;
        r.reasons << QStringLiteral("точка входа в последней секции");
    }
    if (!entryFound || entryNotExec) {
        r.score += 15;
        r.reasons << QStringLiteral("точка входа вне исполняемой секции");
    }
    if (importSize == 0 && highEntropy > 0) {
        r.score += 15;
        r.reasons << QStringLiteral("нет таблицы импорта");
    }

    int injection = 0;
    QStringList found;
    for (const QByteArray& api : injectionApis()) {
        if (d.contains(api)) {
            ++injection;
            found << QString::fromLatin1(api);
        }
    }
    if (injection >= 3) {
        r.score += 30;
        r.reasons << QStringLiteral("импорт функций внедрения кода: %1").arg(found.join(QLatin1String(", ")));
    }
    Q_UNUSED(fileSize);
    return r;
}

bool hasTrustedSignature(const QString& path)
{
#ifdef Q_OS_WIN
    const std::wstring wide = QDir::toNativeSeparators(path).toStdWString();
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = wide.c_str();

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    return status == ERROR_SUCCESS;
#else
    Q_UNUSED(path);
    return false;
#endif
}

} // namespace bcg
