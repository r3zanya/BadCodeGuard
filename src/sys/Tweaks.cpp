#include "Tweaks.h"

#include "sys/ProcessRunner.h"
#include "sys/Registry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace bcg::sys {

namespace {

using reg::Root;

const QString kLm = QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion");

struct DwordValue {
    Root root;
    QString key;
    QString name;
    quint32 value;
};

bool writeAll(const QList<DwordValue>& values)
{
    bool ok = true;
    for (const auto& v : values)
        ok = reg::writeDword(v.root, v.key, v.name, v.value) && ok;
    return ok;
}

bool deleteAll(const QList<DwordValue>& values)
{
    bool ok = true;
    for (const auto& v : values)
        ok = reg::deleteValue(v.root, v.key, v.name) && ok;
    return ok;
}

bool isDword(Root root, const QString& key, const QString& name, quint32 expected)
{
    const auto value = reg::readDword(root, key, name);
    return value && *value == expected;
}

// ---- Sticky keys ----
// SKF_HOTKEYACTIVE (0x4) is the "press Shift five times" shortcut.
constexpr DWORD kStickyKeysOff = 506;
constexpr DWORD kStickyKeysDefault = 510;

bool stickyKeysDisabled()
{
    STICKYKEYS sk{sizeof(STICKYKEYS), 0};
    if (!SystemParametersInfoW(SPI_GETSTICKYKEYS, sizeof(sk), &sk, 0))
        return false;
    return !(sk.dwFlags & SKF_HOTKEYACTIVE) && !(sk.dwFlags & SKF_STICKYKEYSON);
}

bool setStickyKeysDisabled(bool disabled)
{
    STICKYKEYS sk{sizeof(STICKYKEYS), disabled ? kStickyKeysOff : kStickyKeysDefault};
    return SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(sk), &sk,
                                 SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)
        != 0;
}

// ---- Core isolation / memory integrity ----
const QString kHvciKey = QStringLiteral(
    "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity");

// ---- UAC ----
const QString kPoliciesSystem = kLm + QStringLiteral("\\Policies\\System");

// ---- SmartScreen ----
const QString kExplorerKey = kLm + QStringLiteral("\\Explorer");
const QString kSmartScreenPolicyKey = QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System");
const QString kAppHostKey = kLm + QStringLiteral("\\AppHost");

bool smartScreenDisabled()
{
    if (isDword(Root::LocalMachine, kSmartScreenPolicyKey, QStringLiteral("EnableSmartScreen"), 0))
        return true;
    const auto value = reg::readString(Root::LocalMachine, kExplorerKey,
                                       QStringLiteral("SmartScreenEnabled"));
    return value && value->compare(QStringLiteral("Off"), Qt::CaseInsensitive) == 0;
}

bool setSmartScreenDisabled(bool disabled)
{
    if (disabled) {
        return reg::writeDword(Root::LocalMachine, kSmartScreenPolicyKey,
                               QStringLiteral("EnableSmartScreen"), 0)
            & reg::writeString(Root::LocalMachine, kExplorerKey,
                               QStringLiteral("SmartScreenEnabled"), QStringLiteral("Off"))
            & reg::writeDword(Root::CurrentUser, kAppHostKey,
                              QStringLiteral("EnableWebContentEvaluation"), 0);
    }
    return reg::deleteValue(Root::LocalMachine, kSmartScreenPolicyKey,
                            QStringLiteral("EnableSmartScreen"))
        & reg::writeString(Root::LocalMachine, kExplorerKey, QStringLiteral("SmartScreenEnabled"),
                           QStringLiteral("Warn"))
        & reg::writeDword(Root::CurrentUser, kAppHostKey,
                          QStringLiteral("EnableWebContentEvaluation"), 1);
}

// ---- Bing in the Start menu search ----
const QString kSearchKey = kLm + QStringLiteral("\\Search");
const QString kExplorerPolicyKey = QStringLiteral("Software\\Policies\\Microsoft\\Windows\\Explorer");

bool bingDisabled()
{
    return isDword(Root::CurrentUser, kExplorerPolicyKey, QStringLiteral("DisableSearchBoxSuggestions"), 1)
        || isDword(Root::CurrentUser, kSearchKey, QStringLiteral("BingSearchEnabled"), 0);
}

bool setBingDisabled(bool disabled)
{
    if (disabled) {
        return reg::writeDword(Root::CurrentUser, kSearchKey, QStringLiteral("BingSearchEnabled"), 0)
            & reg::writeDword(Root::CurrentUser, kExplorerPolicyKey,
                              QStringLiteral("DisableSearchBoxSuggestions"), 1);
    }
    return reg::writeDword(Root::CurrentUser, kSearchKey, QStringLiteral("BingSearchEnabled"), 1)
        & reg::deleteValue(Root::CurrentUser, kExplorerPolicyKey,
                           QStringLiteral("DisableSearchBoxSuggestions"));
}

// ---- Chkdsk countdown ----
const QString kSessionManagerKey = QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Session Manager");
constexpr quint32 kChkdskTimeoutSeconds = 60;

// ---- BitLocker automatic device encryption ----
const QString kBitLockerKey = QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\BitLocker");

// ---- Telemetry ----
const QList<DwordValue>& telemetryValues()
{
    static const QList<DwordValue> values = {
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection"),
         QStringLiteral("AllowTelemetry"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection"),
         QStringLiteral("DoNotShowFeedbackNotifications"), 1},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\AdvertisingInfo"),
         QStringLiteral("DisabledByGroupPolicy"), 1},
        {Root::CurrentUser, kLm + QStringLiteral("\\AdvertisingInfo"), QStringLiteral("Enabled"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System"),
         QStringLiteral("EnableActivityFeed"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System"),
         QStringLiteral("PublishUserActivities"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System"),
         QStringLiteral("UploadUserActivities"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\SQMClient\\Windows"),
         QStringLiteral("CEIPEnable"), 0},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting"),
         QStringLiteral("Disabled"), 1},
        {Root::CurrentUser, QStringLiteral("Software\\Policies\\Microsoft\\Windows\\CloudContent"),
         QStringLiteral("DisableTailoredExperiencesWithDiagnosticData"), 1},
        {Root::LocalMachine, QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent"),
         QStringLiteral("DisableWindowsConsumerFeatures"), 1},
    };
    return values;
}

const QString kDiagTrackKey = QStringLiteral("SYSTEM\\CurrentControlSet\\Services\\DiagTrack");

bool telemetryDisabled()
{
    return isDword(Root::LocalMachine, telemetryValues().first().key, telemetryValues().first().name, 0)
        && isDword(Root::LocalMachine, kDiagTrackKey, QStringLiteral("Start"), 4);
}

bool setTelemetryDisabled(bool disabled)
{
    // Restoring removes our policy values; the "restore" path of the
    // Windows Error Reporting and advertising values writes 0 instead, which
    // is why those two are reset explicitly.
    bool ok = true;
    if (disabled) {
        ok = writeAll(telemetryValues());
        ok = runHidden(QStringLiteral("sc"), {QStringLiteral("config"), QStringLiteral("DiagTrack"),
                                              QStringLiteral("start="), QStringLiteral("disabled")}) == 0
            && ok;
        runHidden(QStringLiteral("sc"), {QStringLiteral("config"), QStringLiteral("dmwappushservice"),
                                         QStringLiteral("start="), QStringLiteral("disabled")});
        runHidden(QStringLiteral("sc"), {QStringLiteral("stop"), QStringLiteral("DiagTrack")});
        runHidden(QStringLiteral("sc"), {QStringLiteral("stop"), QStringLiteral("dmwappushservice")});
        return ok;
    }
    ok = deleteAll(telemetryValues());
    reg::writeDword(Root::LocalMachine, QStringLiteral("SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting"),
                    QStringLiteral("Disabled"), 0);
    reg::writeDword(Root::CurrentUser, kLm + QStringLiteral("\\AdvertisingInfo"),
                    QStringLiteral("Enabled"), 1);
    ok = runHidden(QStringLiteral("sc"), {QStringLiteral("config"), QStringLiteral("DiagTrack"),
                                          QStringLiteral("start="), QStringLiteral("auto")}) == 0
        && ok;
    runHidden(QStringLiteral("sc"), {QStringLiteral("config"), QStringLiteral("dmwappushservice"),
                                     QStringLiteral("start="), QStringLiteral("demand")});
    runHidden(QStringLiteral("sc"), {QStringLiteral("start"), QStringLiteral("DiagTrack")});
    return ok;
}

// ---- Hibernation ----
const QString kPowerKey = QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Power");

QList<Tweak> buildTweaks()
{
    QList<Tweak> list;

    list.append({QStringLiteral("stickykeys"), QObject::tr("Отключить залипание клавиш"),
                 QObject::tr("Убирает окно, которое появляется при пятикратном нажатии Shift."),
                 /*needsAdmin*/ false, /*needsReboot*/ false, /*risky*/ false,
                 stickyKeysDisabled, setStickyKeysDisabled});

    list.append({QStringLiteral("hvci"), QObject::tr("Отключить изоляцию ядра / целостность памяти"),
                 QObject::tr("Выключает защиту памяти на основе виртуализации. Может ускорить игры, "
                             "но снижает защиту от атак на ядро. Нужна перезагрузка."),
                 true, true, true,
                 [] { return isDword(Root::LocalMachine, kHvciKey, QStringLiteral("Enabled"), 0); },
                 [](bool on) {
                     return reg::writeDword(Root::LocalMachine, kHvciKey, QStringLiteral("Enabled"),
                                            on ? 0 : 1);
                 }});

    list.append({QStringLiteral("uac"), QObject::tr("Отключить контроль учётных записей (UAC)"),
                 QObject::tr("Программы перестанут спрашивать подтверждение при запуске от "
                             "администратора. Небезопасно. Нужна перезагрузка."),
                 true, true, true,
                 [] { return isDword(Root::LocalMachine, kPoliciesSystem, QStringLiteral("EnableLUA"), 0); },
                 [](bool on) {
                     return reg::writeDword(Root::LocalMachine, kPoliciesSystem,
                                            QStringLiteral("EnableLUA"), on ? 0 : 1);
                 }});

    list.append({QStringLiteral("smartscreen"), QObject::tr("Отключить SmartScreen"),
                 QObject::tr("Windows не будет проверять скачанные файлы и сайты по базе "
                             "Microsoft."),
                 true, false, true, smartScreenDisabled, setSmartScreenDisabled});

    list.append({QStringLiteral("bing"), QObject::tr("Отключить онлайн-поиск через Bing"),
                 QObject::tr("Поиск в меню «Пуск» будет искать только на этом компьютере."),
                 false, false, false, bingDisabled, setBingDisabled});

    list.append({QStringLiteral("chkdsk"), QObject::tr("Установить тайм-аут для Chkdsk на 1 минуту"),
                 QObject::tr("Время, которое Windows ждёт перед автоматической проверкой диска при "
                             "загрузке (по умолчанию несколько секунд)."),
                 true, false, false,
                 [] {
                     return isDword(Root::LocalMachine, kSessionManagerKey,
                                    QStringLiteral("AutoChkTimeout"), kChkdskTimeoutSeconds);
                 },
                 [](bool on) {
                     return on ? reg::writeDword(Root::LocalMachine, kSessionManagerKey,
                                                 QStringLiteral("AutoChkTimeout"), kChkdskTimeoutSeconds)
                               : reg::deleteValue(Root::LocalMachine, kSessionManagerKey,
                                                  QStringLiteral("AutoChkTimeout"));
                 }});

    list.append({QStringLiteral("bitlocker"), QObject::tr("Выключить авто-шифрование BitLocker"),
                 QObject::tr("Запрещает Windows автоматически шифровать диск. Уже зашифрованные "
                             "диски не расшифровываются."),
                 true, false, true,
                 [] { return isDword(Root::LocalMachine, kBitLockerKey, QStringLiteral("PreventDeviceEncryption"), 1); },
                 [](bool on) {
                     return on ? reg::writeDword(Root::LocalMachine, kBitLockerKey,
                                                 QStringLiteral("PreventDeviceEncryption"), 1)
                               : reg::deleteValue(Root::LocalMachine, kBitLockerKey,
                                                  QStringLiteral("PreventDeviceEncryption"));
                 }});

    list.append({QStringLiteral("telemetry"), QObject::tr("Отключить всю телеметрию и слежку в системе"),
                 QObject::tr("Отключает сбор диагностических данных, рекламный идентификатор, "
                             "журнал действий, отчёты об ошибках и службу DiagTrack."),
                 true, false, true, telemetryDisabled, setTelemetryDisabled});

    list.append({QStringLiteral("hibernate"), QObject::tr("Отключить гибернацию"),
                 QObject::tr("Удаляет файл hiberfil.sys и освобождает место на диске. Быстрый "
                             "запуск Windows при этом тоже отключается."),
                 true, false, false,
                 [] { return isDword(Root::LocalMachine, kPowerKey, QStringLiteral("HibernateEnabled"), 0); },
                 [](bool on) {
                     return runHidden(QStringLiteral("powercfg"),
                                      {QStringLiteral("/hibernate"), on ? QStringLiteral("off")
                                                                        : QStringLiteral("on")})
                         == 0;
                 }});

    return list;
}

} // namespace

const QList<Tweak>& tweaks()
{
    static const QList<Tweak> list = buildTweaks();
    return list;
}

} // namespace bcg::sys
