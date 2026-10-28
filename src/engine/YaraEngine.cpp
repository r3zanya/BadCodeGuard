#include "YaraEngine.h"

#include <yara.h>

#include <QDirIterator>
#include <QFileInfo>

#include <cstdio>
#include <cstring>

namespace bcg {

namespace {

bool ensureInit()
{
    static const bool ok = (yr_initialize() == 0);
    return ok;
}

// Variables that public rule sets expect the host to provide.
constexpr const char* kExternals[] = {"filename", "filepath", "extension", "filetype", "owner"};

void onCompileMessage(int level, const char* file, int line, const YR_RULE*, const char* message,
                      void* user)
{
    if (level != YARA_ERROR_LEVEL_ERROR)
        return;
    auto* errors = static_cast<QStringList*>(user);
    if (errors->size() < 3)
        errors->append(QStringLiteral("%1:%2: %3")
                           .arg(QString::fromUtf8(file ? file : "rules"))
                           .arg(line)
                           .arg(QString::fromUtf8(message)));
}

YR_COMPILER* newCompiler(QStringList* errors)
{
    YR_COMPILER* compiler = nullptr;
    if (yr_compiler_create(&compiler) != 0)
        return nullptr;
    yr_compiler_set_callback(compiler, onCompileMessage, errors);
    for (const char* name : kExternals)
        yr_compiler_define_string_variable(compiler, name, "");
    return compiler;
}

int countRules(YR_RULES* rules)
{
    int n = 0;
    YR_RULE* rule = nullptr;
    yr_rules_foreach(rules, rule) { ++n; }
    return n;
}

struct Hit {
    QString name;
};

int onScanMessage(YR_SCAN_CONTEXT*, int message, void* data, void* user)
{
    if (message != CALLBACK_MSG_RULE_MATCHING)
        return CALLBACK_CONTINUE;
    auto* rule = static_cast<YR_RULE*>(data);
    auto* hit = static_cast<Hit*>(user);
    hit->name = QString::fromUtf8(rule->identifier);
    YR_META* meta = nullptr;
    yr_rule_metas_foreach(rule, meta)
    {
        if (meta->type == META_TYPE_STRING && std::strcmp(meta->identifier, "threat") == 0
            && meta->string && *meta->string)
            hit->name = QString::fromUtf8(meta->string);
    }
    return CALLBACK_ABORT;
}

} // namespace

YaraEngine::YaraEngine()
{
    ensureInit();
}

YaraEngine::~YaraEngine()
{
    for (void* set : m_sets)
        yr_rules_destroy(static_cast<YR_RULES*>(set));
}

QString YaraEngine::addSource(const QString& source)
{
    if (!ensureInit())
        return QStringLiteral("libyara не запустилась");
    QStringList errors;
    YR_COMPILER* compiler = newCompiler(&errors);
    if (!compiler)
        return QStringLiteral("не хватает памяти");

    QString error;
    YR_RULES* rules = nullptr;
    if (yr_compiler_add_string(compiler, source.toUtf8().constData(), nullptr) != 0
        || yr_compiler_get_rules(compiler, &rules) != 0) {
        error = errors.isEmpty() ? QStringLiteral("правило не скомпилировалось")
                                 : errors.join(QLatin1Char('\n'));
    }
    yr_compiler_destroy(compiler);
    if (rules) {
        m_ruleCount += countRules(rules);
        m_sets.push_back(rules);
    }
    return error;
}

YaraLoadReport YaraEngine::loadFolder(const QString& folder)
{
    YaraLoadReport report;
    if (!ensureInit())
        return report;
    // Pass 1: compile every file alone, so a broken one cannot spoil the rest.
    struct Compiled {
        QString path;
        YR_RULES* rules;
    };
    std::vector<Compiled> good;
    QDirIterator it(folder, {QStringLiteral("*.yar"), QStringLiteral("*.yara")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QStringList errors;
        YR_COMPILER* compiler = newCompiler(&errors);
        if (!compiler)
            break;
        FILE* file = _wfopen(reinterpret_cast<const wchar_t*>(path.utf16()), L"rb");
        YR_RULES* rules = nullptr;
        bool ok = false;
        if (file) {
            const QByteArray name = QFileInfo(path).fileName().toUtf8();
            ok = yr_compiler_add_file(compiler, file, nullptr, name.constData()) == 0
                && yr_compiler_get_rules(compiler, &rules) == 0;
            fclose(file);
        }
        yr_compiler_destroy(compiler);
        if (ok && rules) {
            ++report.files;
            report.rules += countRules(rules);
            good.push_back({path, rules});
        } else {
            ++report.failed;
            if (report.errors.size() < 3)
                report.errors << (errors.isEmpty() ? QFileInfo(path).fileName() : errors.first());
        }
    }

    // Pass 2: put the good files into one rule set (each in its own namespace, as
    // rule names repeat between files). One set is scanned in a single pass over
    // the data, which is many times faster than one pass per file.
    QStringList ignored;
    YR_COMPILER* all = good.empty() ? nullptr : newCompiler(&ignored);
    bool merged = all != nullptr;
    int index = 0;
    for (const Compiled& c : good) {
        if (!merged)
            break;
        FILE* file = _wfopen(reinterpret_cast<const wchar_t*>(c.path.utf16()), L"rb");
        if (!file) {
            merged = false;
            break;
        }
        const QByteArray ns = QByteArray("n") + QByteArray::number(index++);
        const QByteArray name = QFileInfo(c.path).fileName().toUtf8();
        merged = yr_compiler_add_file(all, file, ns.constData(), name.constData()) == 0;
        fclose(file);
    }
    YR_RULES* combined = nullptr;
    if (merged && yr_compiler_get_rules(all, &combined) == 0 && combined) {
        for (const Compiled& c : good)
            yr_rules_destroy(c.rules);
        m_ruleCount += countRules(combined);
        m_sets.push_back(combined);
    } else {
        // Could not merge: keep one set per file (works, only slower).
        for (const Compiled& c : good) {
            m_ruleCount += countRules(c.rules);
            m_sets.push_back(c.rules);
        }
    }
    if (all)
        yr_compiler_destroy(all);
    return report;
}

QString YaraEngine::scan(const QByteArray& data, const QString& fileName) const
{
    if (m_sets.empty() || data.isEmpty())
        return {};
    const QFileInfo info(fileName);
    const QByteArray name = info.fileName().toUtf8();
    const QByteArray path = fileName.toUtf8();
    const QByteArray ext = info.suffix().toLower().toUtf8();

    for (void* set : m_sets) {
        YR_SCANNER* scanner = nullptr;
        if (yr_scanner_create(static_cast<YR_RULES*>(set), &scanner) != 0)
            continue;
        Hit hit;
        yr_scanner_set_callback(scanner, onScanMessage, &hit);
        yr_scanner_set_timeout(scanner, 5);
        yr_scanner_set_flags(scanner, SCAN_FLAGS_FAST_MODE);
        yr_scanner_define_string_variable(scanner, "filename", name.constData());
        yr_scanner_define_string_variable(scanner, "filepath", path.constData());
        yr_scanner_define_string_variable(scanner, "extension", ext.constData());
        yr_scanner_scan_mem(scanner, reinterpret_cast<const uint8_t*>(data.constData()),
                            size_t(data.size()));
        yr_scanner_destroy(scanner);
        if (!hit.name.isEmpty())
            return QStringLiteral("YARA: %1").arg(hit.name);
    }
    return {};
}

} // namespace bcg
