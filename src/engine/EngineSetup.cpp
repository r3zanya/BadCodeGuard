#include "EngineSetup.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace bcg {

QString dataFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString signaturesFolder()
{
    return dataFolder() + QStringLiteral("/signatures");
}

QString yaraFolder()
{
    return signaturesFolder() + QStringLiteral("/yara");
}

namespace {

// Built-in hashes plus every list in the user's signatures folder.
std::shared_ptr<SignatureDb> loadSignatures()
{
    auto db = std::make_shared<SignatureDb>();
    db->loadFile(QStringLiteral(":/signatures/hashes.txt"));
    const QString folder = signaturesFolder();
    QDir().mkpath(folder);
    db->loadFolder(folder);
    return db;
}

} // namespace

int reloadSignatures(const ScanContext& ctx)
{
    std::shared_ptr<SignatureDb> db = loadSignatures();
    const int count = int(db->size());
    if (ctx.signatures)
        ctx.signatures->set(std::move(db));
    return count;
}

YaraLoadReport reloadYara(const ScanContext& ctx)
{
    auto engine = std::make_shared<YaraEngine>();
    QDir().mkpath(yaraFolder());
    const YaraLoadReport report = engine->loadFolder(yaraFolder());
    if (ctx.yara)
        ctx.yara->set(std::move(engine));
    return report;
}

ScanContext buildContext(std::shared_ptr<ExclusionList> exclusions, EngineLoadReport* report)
{
    EngineLoadReport local;
    EngineLoadReport& rep = report ? *report : local;

    auto signatures = std::make_shared<SignatureStore>();
    auto rules = std::make_shared<RuleEngine>();

    auto loadRules = [&](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        QString error;
        rep.rules += rules->loadText(QString::fromUtf8(f.readAll()), &error);
        if (!error.isEmpty())
            rep.errors << QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), error);
    };

    std::shared_ptr<SignatureDb> db = loadSignatures();
    rep.hashes += int(db->size());
    signatures->set(std::move(db));
    loadRules(QStringLiteral(":/signatures/rules.bcg"));

    const QDir dir(signaturesFolder());
    for (const QFileInfo& fi : dir.entryInfoList({QStringLiteral("*.bcg")}, QDir::Files))
        loadRules(fi.absoluteFilePath());

    ScanContext ctx;
    ctx.exclusions = std::move(exclusions);
    ctx.signatures = signatures;
    ctx.rules = rules;
    ctx.yara = std::make_shared<YaraStore>();
    const YaraLoadReport yaraReport = reloadYara(ctx);
    rep.yaraRules = yaraReport.rules;
    rep.yaraFailed = yaraReport.failed;
    ctx.quarantine = std::make_shared<Quarantine>(dataFolder() + QStringLiteral("/quarantine"));
    ctx.options = std::make_shared<ScanOptions>();
    return ctx;
}

} // namespace bcg
