#include "ScriptAnalyzer.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace bcg {

namespace {

constexpr qsizetype kMaxText = 2 * 1024 * 1024;

enum class Kind { None, Script, Shortcut, Pdf, Office };

Kind kindOf(const QString& name, const QByteArray& data)
{
    const QString ext = QFileInfo(name).suffix().toLower();
    static const QSet<QString> scripts = {
        QStringLiteral("ps1"), QStringLiteral("psm1"), QStringLiteral("bat"), QStringLiteral("cmd"),
        QStringLiteral("js"),  QStringLiteral("jse"),  QStringLiteral("vbs"), QStringLiteral("vbe"),
        QStringLiteral("wsf"), QStringLiteral("wsh"),  QStringLiteral("hta"), QStringLiteral("sct")};
    static const QSet<QString> office = {
        QStringLiteral("doc"), QStringLiteral("dot"), QStringLiteral("xls"),
        QStringLiteral("xlt"), QStringLiteral("ppt"), QStringLiteral("pot"), QStringLiteral("bin")};

    if (data.startsWith("%PDF-") || ext == QLatin1String("pdf"))
        return Kind::Pdf;
    if (ext == QLatin1String("lnk"))
        return Kind::Shortcut;
    if (scripts.contains(ext))
        return Kind::Script;
    // OLE compound file: legacy Office documents and vbaProject.bin.
    if (data.startsWith("\xD0\xCF\x11\xE0") && (office.contains(ext) || ext.isEmpty()))
        return Kind::Office;
    return Kind::None;
}

// Decodes script text; PowerShell files are often UTF-16.
QString decode(const QByteArray& raw)
{
    const QByteArray d = raw.left(kMaxText);
    if (d.startsWith("\xFF\xFE"))
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(d.constData() + 2),
                                  (d.size() - 2) / 2);
    qsizetype nuls = 0;
    const qsizetype probe = std::min<qsizetype>(d.size(), 4096);
    for (qsizetype i = 0; i < probe; ++i)
        nuls += d[i] == '\0';
    if (probe > 0 && nuls * 4 > probe)
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(d.constData()), d.size() / 2);
    return QString::fromUtf8(d);
}

struct Token {
    const char* needle;
    int points;
    const char* reason;
};

// Lower-case needles; each counts once.
const Token kScriptTokens[] = {
    {"frombase64string", 20, "декодирование base64 во время выполнения"},
    {"invoke-expression", 20, "выполнение строки как кода (Invoke-Expression)"},
    {"downloadstring", 25, "загрузка кода из сети"},
    {"downloadfile", 25, "загрузка файла из сети"},
    {"invoke-webrequest", 15, "обращение к сети из скрипта"},
    {"net.webclient", 15, "обращение к сети из скрипта"},
    {"start-bitstransfer", 20, "загрузка через BITS"},
    {"-windowstyle hidden", 10, "скрытое окно"},
    {"-w hidden", 10, "скрытое окно"},
    {"-executionpolicy bypass", 15, "обход политики запуска скриптов"},
    {"-ep bypass", 15, "обход политики запуска скриптов"},
    {"-noprofile", 5, "запуск без профиля"},
    {"amsiutils", 40, "попытка обхода AMSI"},
    {"amsiinitfailed", 40, "попытка обхода AMSI"},
    {"reflection.assembly]::load", 25, "загрузка сборки из памяти"},
    {"virtualalloc", 25, "выделение исполняемой памяти"},
    {"wscript.shell", 15, "запуск команд через WScript.Shell"},
    {"shell.application", 10, "запуск команд через Shell.Application"},
    {"adodb.stream", 20, "запись файла через ADODB.Stream"},
    {"msxml2.xmlhttp", 20, "обращение к сети из скрипта"},
    {"winhttp.winhttprequest", 20, "обращение к сети из скрипта"},
    {"unescape(", 15, "обфускация через unescape"},
    {"string.fromcharcode", 15, "сборка строк из кодов символов"},
    {"regsvr32", 20, "запуск через regsvr32"},
    {"scrobj.dll", 40, "запуск скриптлета через regsvr32 (Squiblydoo)"},
    {"mshta", 25, "запуск через mshta"},
    {"rundll32 javascript:", 40, "запуск JavaScript через rundll32"},
    {"certutil -urlcache", 40, "загрузка файла через certutil"},
    {"certutil -decode", 30, "декодирование файла через certutil"},
    {"bitsadmin /transfer", 30, "загрузка через bitsadmin"},
    {"schtasks /create", 15, "создание задачи планировщика"},
    {"vssadmin delete shadows", 60, "удаление теневых копий (типично для шифровальщиков)"},
    {"wmic shadowcopy delete", 60, "удаление теневых копий (типично для шифровальщиков)"},
    {"recoveryenabled no", 40, "отключение восстановления Windows"},
    {"-enc ", 0, nullptr}, // handled by the regular expression below
};

int scoreScript(const QString& text, QStringList& reasons)
{
    int score = 0;
    QSet<QString> seen;
    auto add = [&](int points, const QString& reason) {
        score += points;
        if (!seen.contains(reason)) {
            seen.insert(reason);
            reasons << reason;
        }
    };

    for (const Token& t : kScriptTokens) {
        if (t.points > 0 && text.contains(QLatin1String(t.needle)))
            add(t.points, QString::fromUtf8(t.reason));
    }
    static const QRegularExpression encoded(
        QStringLiteral("-(?:e|ec|enc|encodedcommand)\\s+[a-z0-9+/=]{40,}"));
    if (encoded.match(text).hasMatch())
        add(35, QStringLiteral("закодированная команда PowerShell"));
    static const QRegularExpression blob(QStringLiteral("[a-z0-9+/]{300,}={0,2}"));
    if (blob.match(text).hasMatch())
        add(15, QStringLiteral("длинный закодированный фрагмент"));
    if (text.contains(QLatin1String("powershell")) && text.contains(QLatin1String("http")))
        add(10, QStringLiteral("PowerShell обращается по адресу в сети"));
    return score;
}

int scorePdf(const QString& text, QStringList& reasons)
{
    int score = 0;
    auto has = [&](const char* s) { return text.contains(QLatin1String(s)); };
    if (has("/launch")) {
        score += 40;
        reasons << QStringLiteral("PDF запускает внешнюю программу (/Launch)");
    }
    if (has("/javascript") || has("/js")) {
        score += 15;
        reasons << QStringLiteral("PDF содержит JavaScript");
    }
    if (has("/openaction") || has("/aa")) {
        score += 15;
        reasons << QStringLiteral("действие при открытии PDF");
    }
    if (has("/embeddedfile")) {
        score += 15;
        reasons << QStringLiteral("во вложении PDF есть файл");
    }
    if (has("/richmedia") || has("/xfa")) {
        score += 10;
        reasons << QStringLiteral("активное содержимое PDF");
    }
    return score;
}

int scoreOffice(const QByteArray& raw, QStringList& reasons)
{
    // Streams are stored as UTF-16; dropping the zero bytes leaves searchable text.
    QByteArray flat = raw.left(kMaxText);
    flat.replace('\0', "");
    const QByteArray lower = flat.toLower();
    auto has = [&](const char* s) { return lower.contains(s); };

    int score = 0;
    if (!has("_vba_project") && !has("vbaproject"))
        return 0;
    score += 30;
    reasons << QStringLiteral("документ содержит макросы");
    if (has("autoopen") || has("auto_open") || has("document_open") || has("workbook_open")
        || has("document_new")) {
        score += 25;
        reasons << QStringLiteral("макрос запускается при открытии документа");
    }
    if (has("wscript.shell") || has("powershell") || has("cmd.exe") || has("urldownloadtofile")
        || has("createobject")) {
        score += 25;
        reasons << QStringLiteral("макрос запускает программы или обращается к сети");
    }
    return score;
}

} // namespace

bool isScriptOrDocument(const QString& name, const QByteArray& data)
{
    return kindOf(name, data) != Kind::None;
}

HeuristicReport analyzeScript(const QString& name, const QByteArray& data)
{
    HeuristicReport report;
    switch (kindOf(name, data)) {
    case Kind::Script:
        report.score = scoreScript(decode(data).toLower(), report.reasons);
        break;
    case Kind::Shortcut: {
        // Shortcuts hold their target as UTF-16 and/or ANSI text.
        QByteArray flat = data.left(kMaxText);
        flat.replace('\0', "");
        report.score = scoreScript(QString::fromLatin1(flat).toLower(), report.reasons);
        break;
    }
    case Kind::Pdf:
        report.score = scorePdf(QString::fromLatin1(data.left(kMaxText)).toLower(), report.reasons);
        break;
    case Kind::Office:
        report.score = scoreOffice(data, report.reasons);
        break;
    case Kind::None:
        break;
    }
    report.score = std::min(report.score, 100);
    return report;
}

} // namespace bcg
