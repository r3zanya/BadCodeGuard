#include "SystemInfo.h"

#include "sys/ProcessRunner.h"
#include "sys/Registry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QtMath>

namespace bcg::sys {

// ---------------------------------------------------------------------------
// Service
// ---------------------------------------------------------------------------

SystemInfoService& SystemInfoService::instance()
{
    static SystemInfoService service;
    return service;
}

SystemInfoService::SystemInfoService()
    : m_hardwareRunner(new ProcessRunner(this))
    , m_windowsRunner(new ProcessRunner(this))
{
    for (ProcessRunner* runner : {m_hardwareRunner, m_windowsRunner})
        runner->setEncoding(ProcessRunner::Encoding::Utf8);

    // The JSON arrives as a single line; collect it per runner.
    auto collect = [](ProcessRunner* runner, QString* text) {
        connect(runner, &ProcessRunner::line, runner, [text](const QString& l) { *text += l; });
    };
    collect(m_hardwareRunner, &m_hardwareText);
    collect(m_windowsRunner, &m_windowsText);

    auto parse = [](QString* text) {
        const int start = text->indexOf(QLatin1Char('{'));
        QJsonObject object;
        if (start >= 0)
            object = QJsonDocument::fromJson(text->mid(start).toUtf8()).object();
        text->clear();
        return object;
    };
    auto done = [this] {
        if (--m_pending <= 0) {
            m_pending = 0;
            m_lastRefresh = QDateTime::currentDateTime();
            emit loadFinished();
        }
    };

    connect(m_hardwareRunner, &ProcessRunner::finished, this, [this, parse, done] {
        const QJsonObject object = parse(&m_hardwareText);
        if (!object.isEmpty()) {
            m_hardware = object;
            emit hardwareChanged();
        }
        done();
    });
    connect(m_windowsRunner, &ProcessRunner::finished, this, [this, parse, done] {
        const QJsonObject object = parse(&m_windowsText).value(QStringLiteral("windows")).toObject();
        if (!object.isEmpty()) {
            m_windows = object;
            emit windowsChanged();
        }
        done();
    });
}

void SystemInfoService::load(const QString& part, ProcessRunner* runner)
{
    QFile file(QStringLiteral(":/scripts/sysinfo.ps1"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QString script = QStringLiteral("$Part = '%1'\n").arg(part) + QString::fromUtf8(file.readAll());
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    runner->start(QStringLiteral("powershell.exe"),
                  {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                   QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                   QStringLiteral("-EncodedCommand"), QString::fromLatin1(utf16.toBase64())});
}

void SystemInfoService::refresh()
{
    if (m_pending > 0)
        return;
    m_pending = 2;
    load(QStringLiteral("hw"), m_hardwareRunner);
    load(QStringLiteral("os"), m_windowsRunner);
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------

namespace info {

namespace {

constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

QLocale ru()
{
    return QLocale(QLocale::Russian);
}

QString str(const QJsonObject& o, const char* key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString().trimmed() : QString();
}

double num(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toDouble();
}

QJsonArray arr(const QJsonObject& o, const char* key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isArray())
        return v.toArray();
    // PowerShell collapses a one-element array into a bare object.
    if (v.isObject())
        return QJsonArray{v.toObject()};
    return {};
}

QString dateRu(const QString& iso)
{
    const QDate date = QDate::fromString(iso, Qt::ISODate);
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy")) : iso;
}

QString number(double value, int decimals)
{
    QString s = ru().toString(value, 'f', decimals);
    if (decimals > 0 && s.contains(QLatin1Char(','))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char(',')))
            s.chop(1);
    }
    return s;
}

QString yesNo(const QJsonValue& v, const QString& yes, const QString& no)
{
    if (v.isNull() || v.isUndefined())
        return QObject::tr("Неизвестно");
    return v.toBool() ? yes : no;
}

QString ramType(int smbios)
{
    switch (smbios) {
    case 20: return QStringLiteral("DDR");
    case 21: return QStringLiteral("DDR2");
    case 24: return QStringLiteral("DDR3");
    case 26: return QStringLiteral("DDR4");
    case 27: return QStringLiteral("LPDDR");
    case 28: return QStringLiteral("LPDDR2");
    case 29: return QStringLiteral("LPDDR3");
    case 30: return QStringLiteral("LPDDR4");
    case 34: return QStringLiteral("DDR5");
    case 35: return QStringLiteral("LPDDR5");
    default: return {};
    }
}

bool isVirtualAdapter(const QString& name)
{
    static const QStringList markers = {QStringLiteral("Virtual"), QStringLiteral("Basic Render"),
                                        QStringLiteral("Basic Display"), QStringLiteral("Remote"),
                                        QStringLiteral("Parsec"), QStringLiteral("Meta ")};
    for (const QString& m : markers) {
        if (name.contains(m, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QList<QJsonObject> realGpus(const QJsonObject& hw)
{
    QList<QJsonObject> list;
    for (const QJsonValue& v : arr(hw, "gpu")) {
        const QJsonObject gpu = v.toObject();
        if (!isVirtualAdapter(str(gpu, "name")))
            list.append(gpu);
    }
    return list;
}

double totalRamBytes(const QJsonObject& hw)
{
    double sum = 0;
    for (const QJsonValue& v : arr(hw, "ram"))
        sum += num(v.toObject(), "bytes");
    return sum > 0 ? sum : num(hw, "totalRamBytes");
}

QString ramGbText(const QJsonObject& hw)
{
    const double gib = totalRamBytes(hw) / kGiB;
    if (gib <= 0)
        return QObject::tr("Неизвестно");
    // Without module data the OS reports slightly less than installed.
    const bool fromModules = !arr(hw, "ram").isEmpty();
    return QStringLiteral("%1 GB").arg(fromModules ? number(gib, 1) : QString::number(qCeil(gib)));
}

// Mixed or unknown types are left out rather than guessed.
QString ramTypeText(const QJsonObject& hw)
{
    QString type;
    for (const QJsonValue& v : arr(hw, "ram")) {
        const QString t = ramType(static_cast<int>(num(v.toObject(), "smbiosType")));
        if (t.isEmpty() || (!type.isEmpty() && type != t))
            return {};
        type = t;
    }
    return type;
}

double ramSpeed(const QJsonObject& hw)
{
    double speed = 0;
    for (const QJsonValue& v : arr(hw, "ram")) {
        const QJsonObject m = v.toObject();
        const double s = num(m, "configuredSpeed") > 0 ? num(m, "configuredSpeed") : num(m, "speed");
        speed = speed == 0 ? s : qMin(speed, s);
    }
    return speed;
}

double totalVramBytes(const QJsonObject& hw)
{
    double sum = 0;
    for (const QJsonObject& gpu : realGpus(hw))
        sum += num(gpu, "vramBytes");
    return sum;
}

double totalDiskBytes(const QJsonObject& hw)
{
    double sum = 0;
    for (const QJsonValue& v : arr(hw, "disks"))
        sum += num(v.toObject(), "bytes");
    return sum;
}

QString architectureName(int code)
{
    switch (code) {
    case 0: return QStringLiteral("x86");
    case 5: return QStringLiteral("ARM");
    case 9: return QStringLiteral("x64");
    case 12: return QStringLiteral("ARM64");
    default: return QString();
    }
}

QString mhzToGhz(double mhz)
{
    return mhz > 0 ? QStringLiteral("%1 GHz").arg(number(mhz / 1000.0, 2)) : QObject::tr("Неизвестно");
}

QString cacheText(double kb)
{
    if (kb <= 0)
        return QObject::tr("Неизвестно");
    return kb >= 1024 ? QStringLiteral("%1 MB").arg(number(kb / 1024.0, 1))
                      : QStringLiteral("%1 KB").arg(number(kb, 0));
}

QString placeholderToEmpty(const QString& s)
{
    static const QStringList placeholders = {
        QStringLiteral("To be filled by O.E.M."), QStringLiteral("System Product Name"),
        QStringLiteral("Default string"), QStringLiteral("Not Applicable"),
        QStringLiteral("System manufacturer"), QStringLiteral("None")};
    for (const QString& p : placeholders) {
        if (s.compare(p, Qt::CaseInsensitive) == 0)
            return {};
    }
    return s;
}

QString stripVendorPrefix(QString caption)
{
    for (const QString& prefix : {QStringLiteral("Microsoft "), QStringLiteral("Майкрософт ")}) {
        if (caption.startsWith(prefix))
            return caption.mid(prefix.size());
    }
    return caption;
}

// ---- Windows protection state (some read live from the registry) ----

Status smartScreenStatus(QString* text)
{
    using reg::Root;
    const auto policy = reg::readDword(Root::LocalMachine,
                                       QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Windows\\System"),
                                       QStringLiteral("EnableSmartScreen"));
    const auto value = reg::readString(
        Root::LocalMachine, QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer"),
        QStringLiteral("SmartScreenEnabled"));
    const bool off = (policy && *policy == 0)
        || (value && value->compare(QStringLiteral("Off"), Qt::CaseInsensitive) == 0);
    *text = off ? QObject::tr("Выключен") : QObject::tr("Включён");
    return off ? Status::Warn : Status::Good;
}

Status uacStatus(QString* text)
{
    const auto value = reg::readDword(
        reg::Root::LocalMachine,
        QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"),
        QStringLiteral("EnableLUA"));
    const bool off = value && *value == 0;
    *text = off ? QObject::tr("Отключён") : QObject::tr("Включён");
    return off ? Status::Bad : Status::Good;
}

bool arrayContains(const QJsonArray& a, int value)
{
    for (const QJsonValue& v : a) {
        if (v.toInt(-1) == value)
            return true;
    }
    return false;
}

Status kernelIsolationStatus(const QJsonObject& win, QString* text)
{
    const bool running = arrayContains(win.value(QStringLiteral("securityServicesRunning")).toArray(), 2);
    const auto configured = reg::readDword(
        reg::Root::LocalMachine,
        QStringLiteral("SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity"),
        QStringLiteral("Enabled"));
    if (running) {
        *text = QObject::tr("Включена");
        return Status::Good;
    }
    if (configured && *configured == 1) {
        *text = QObject::tr("Включена в настройках, но не активна");
        return Status::Warn;
    }
    *text = QObject::tr("Выключена");
    return Status::Warn;
}

Status defenderStatus(const QJsonObject& win, QString* text)
{
    const bool enabled = win.value(QStringLiteral("defenderEnabled")).toBool();
    const bool realtime = win.value(QStringLiteral("defenderRealtime")).toBool();
    if (enabled && realtime) {
        *text = QObject::tr("Включен (Real-time)");
        return Status::Good;
    }
    if (enabled) {
        *text = QObject::tr("Включен, защита в реальном времени выключена");
        return Status::Warn;
    }
    QStringList others;
    for (const QJsonValue& v : win.value(QStringLiteral("thirdPartyAv")).toArray()) {
        const QString name = v.toString();
        if (!name.contains(QStringLiteral("Defender"), Qt::CaseInsensitive))
            others << name;
    }
    if (!others.isEmpty()) {
        *text = QObject::tr("Отключён (используется %1)").arg(others.join(QStringLiteral(", ")));
        return Status::Neutral;
    }
    *text = QObject::tr("Выключен");
    return Status::Bad;
}

} // namespace

QString formatGb(double bytes)
{
    return QStringLiteral("%1 GB").arg(number(bytes / kGiB, 2));
}

QString formatSize(double bytes)
{
    constexpr double kMiB = 1024.0 * 1024.0;
    if (bytes >= kGiB)
        return QStringLiteral("%1 GB").arg(number(bytes / kGiB, 2));
    if (bytes >= kMiB)
        return QStringLiteral("%1 MB").arg(number(bytes / kMiB, 1));
    return QStringLiteral("%1 KB").arg(number(bytes / 1024.0, 0));
}

QString formatDriveSize(double bytes)
{
    if (bytes >= 1e12)
        return QStringLiteral("%1 TB").arg(number(bytes / 1e12, 2));
    return QStringLiteral("%1 GB").arg(number(bytes / 1e9, 0));
}

QString cpuLine(const QJsonObject& hw)
{
    const QJsonArray cpus = arr(hw, "cpu");
    if (cpus.isEmpty())
        return QObject::tr("Неизвестно");
    const QJsonObject cpu = cpus.first().toObject();
    return QStringLiteral("%1 (%2 / %3)")
        .arg(str(cpu, "name"))
        .arg(static_cast<int>(num(cpu, "cores")))
        .arg(static_cast<int>(num(cpu, "threads")));
}

QString ramLine(const QJsonObject& hw)
{
    const QString type = ramTypeText(hw);
    return type.isEmpty() ? ramGbText(hw) : QStringLiteral("%1 / %2").arg(ramGbText(hw), type);
}

QString gpuLine(const QJsonObject& hw)
{
    QStringList parts;
    for (const QJsonObject& gpu : realGpus(hw)) {
        const double vram = num(gpu, "vramBytes");
        parts << (vram > 0 ? QStringLiteral("%1 (%2)").arg(str(gpu, "name"), formatGb(vram))
                           : str(gpu, "name"));
    }
    return parts.isEmpty() ? QObject::tr("Неизвестно") : parts.join(QStringLiteral("; "));
}

QString deviceLine(const QJsonObject& hw)
{
    const QJsonObject device = hw.value(QStringLiteral("device")).toObject();
    const QJsonObject board = hw.value(QStringLiteral("board")).toObject();
    QString model = placeholderToEmpty(str(device, "model"));
    QString maker = placeholderToEmpty(str(device, "manufacturer"));
    if (model.isEmpty())
        model = placeholderToEmpty(str(board, "product"));
    if (maker.isEmpty())
        maker = placeholderToEmpty(str(board, "manufacturer"));
    const QString line = (maker + QLatin1Char(' ') + model).trimmed();
    return line.isEmpty() ? QObject::tr("Неизвестно") : line;
}

QString copyText(const QJsonObject& hw)
{
    return QStringLiteral("Процессор: %1\nОперативная память: %2\nВидеокарта: %3\nМодель устройства: %4")
        .arg(cpuLine(hw), ramLine(hw), gpuLine(hw), deviceLine(hw));
}

Tile cpuTile(const QJsonObject& hw)
{
    const QJsonArray cpus = arr(hw, "cpu");
    if (cpus.isEmpty())
        return {QObject::tr("Неизвестно"), {}};
    const QJsonObject cpu = cpus.first().toObject();
    return {str(cpu, "name"),
            QObject::tr("%1 ядер / %2 потоков · %3")
                .arg(static_cast<int>(num(cpu, "cores")))
                .arg(static_cast<int>(num(cpu, "threads")))
                .arg(mhzToGhz(num(cpu, "maxMhz")))};
}

Tile gpuTile(const QJsonObject& hw)
{
    const QList<QJsonObject> gpus = realGpus(hw);
    if (gpus.isEmpty())
        return {QObject::tr("Неизвестно"), {}};
    const QJsonObject gpu = gpus.first();
    QString sub = QObject::tr("Драйвер %1").arg(str(gpu, "driverVersion"));
    if (gpus.size() > 1)
        sub += QObject::tr(" · ещё видеокарт: %1").arg(gpus.size() - 1);
    return {str(gpu, "name"), sub};
}

Tile ramTile(const QJsonObject& hw)
{
    const QJsonArray modules = arr(hw, "ram");
    QString sub = QObject::tr("Модулей: %1").arg(modules.size());
    const double speed = ramSpeed(hw);
    if (speed > 0)
        sub += QStringLiteral(" · %1 MHz").arg(static_cast<int>(speed));
    return {ramLine(hw), sub};
}

Tile vramTile(const QJsonObject& hw)
{
    const double bytes = totalVramBytes(hw);
    return {bytes > 0 ? formatGb(bytes) : QObject::tr("Неизвестно"),
            QObject::tr("Выделенная видеопамять")};
}

Tile storageTile(const QJsonObject& hw)
{
    const double bytes = totalDiskBytes(hw);
    const int count = arr(hw, "disks").size();
    return {bytes > 0 ? formatDriveSize(bytes) : QObject::tr("Неизвестно"),
            QObject::tr("Накопителей: %1").arg(count)};
}

Tile deviceTile(const QJsonObject& hw)
{
    const QJsonObject board = hw.value(QStringLiteral("board")).toObject();
    const QJsonObject device = hw.value(QStringLiteral("device")).toObject();
    QString sub = placeholderToEmpty(str(board, "product"));
    if (sub.isEmpty())
        sub = device.value(QStringLiteral("isLaptop")).toBool() ? QObject::tr("Ноутбук")
                                                                  : QObject::tr("Настольный ПК");
    return {deviceLine(hw), sub};
}

QList<InfoGroup> hardwareGroups(const QJsonObject& hw)
{
    QList<InfoGroup> groups;
    const QString unknown = QObject::tr("Неизвестно");

    // Processor
    for (const QJsonValue& v : arr(hw, "cpu")) {
        const QJsonObject cpu = v.toObject();
        InfoGroup g;
        g.title = QObject::tr("Процессор");
        g.rows = {
            {QObject::tr("Модель"), str(cpu, "name")},
            {QObject::tr("Производитель"), str(cpu, "manufacturer")},
            {QObject::tr("Ядра / потоки"),
             QStringLiteral("%1 / %2").arg(static_cast<int>(num(cpu, "cores"))).arg(static_cast<int>(num(cpu, "threads")))},
            {QObject::tr("Тактовая частота"), mhzToGhz(num(cpu, "maxMhz"))},
            {QObject::tr("Кэш L2"), cacheText(num(cpu, "l2Kb"))},
            {QObject::tr("Кэш L3"), cacheText(num(cpu, "l3Kb"))},
            {QObject::tr("Сокет"), str(cpu, "socket")},
            {QObject::tr("Архитектура"), architectureName(static_cast<int>(num(cpu, "architecture")))},
            {QObject::tr("Виртуализация в BIOS"),
             yesNo(cpu.value(QStringLiteral("virtualization")), QObject::tr("Включена"), QObject::tr("Выключена"))},
        };
        groups.append(g);
    }

    // Graphics
    const QList<QJsonObject> gpus = realGpus(hw);
    for (int i = 0; i < gpus.size(); ++i) {
        const QJsonObject& gpu = gpus[i];
        InfoGroup g;
        g.title = gpus.size() > 1 ? QObject::tr("Видеокарта %1").arg(i + 1) : QObject::tr("Видеокарта");
        const double vram = num(gpu, "vramBytes");
        g.rows = {
            {QObject::tr("Модель"), str(gpu, "name")},
            {QObject::tr("Производитель"), str(gpu, "vendor")},
            {QObject::tr("Видеопамять"), vram > 0 ? formatGb(vram) : unknown},
            {QObject::tr("Драйвер"), QStringLiteral("%1 (%2)").arg(str(gpu, "driverVersion"), dateRu(str(gpu, "driverDate")))},
        };
        if (num(gpu, "width") > 0) {
            g.rows.append({QObject::tr("Разрешение"),
                           QStringLiteral("%1 × %2 · %3 Hz")
                               .arg(static_cast<int>(num(gpu, "width")))
                               .arg(static_cast<int>(num(gpu, "height")))
                               .arg(static_cast<int>(num(gpu, "refresh")))});
        }
        groups.append(g);
    }

    // Memory
    {
        InfoGroup g;
        g.title = QObject::tr("Оперативная память");
        const QString type = ramTypeText(hw);
        g.rows = {
            {QObject::tr("Установлено"), ramGbText(hw)},
            {QObject::tr("Тип"), type.isEmpty() ? unknown : type},
            {QObject::tr("Частота"), ramSpeed(hw) > 0 ? QStringLiteral("%1 MHz").arg(static_cast<int>(ramSpeed(hw))) : unknown},
            {QObject::tr("Модулей"), QString::number(arr(hw, "ram").size())},
        };
        for (const QJsonValue& v : arr(hw, "ram")) {
            const QJsonObject m = v.toObject();
            QStringList parts{formatGb(num(m, "bytes"))};
            const QString t = ramType(static_cast<int>(num(m, "smbiosType")));
            if (!t.isEmpty())
                parts << t;
            parts << QStringLiteral("%1 MHz").arg(static_cast<int>(num(m, "configuredSpeed") > 0 ? num(m, "configuredSpeed") : num(m, "speed")));
            if (!str(m, "part").isEmpty())
                parts << str(m, "part");
            const QString maker = str(m, "manufacturer");
            if (!maker.isEmpty() && maker.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) != 0)
                parts << maker;
            g.rows.append({QObject::tr("Слот %1").arg(str(m, "slot")), parts.join(QStringLiteral(" · "))});
        }
        groups.append(g);
    }

    // Storage
    {
        InfoGroup g;
        g.title = QObject::tr("Накопители");
        for (const QJsonValue& v : arr(hw, "disks")) {
            const QJsonObject d = v.toObject();
            QStringList parts{formatDriveSize(num(d, "bytes"))};
            const QString bus = str(d, "busType");
            const QString media = str(d, "mediaType");
            if (!bus.isEmpty() && bus != QStringLiteral("Unknown"))
                parts << bus;
            if (!media.isEmpty() && media != QStringLiteral("Unspecified"))
                parts << media;
            if (!str(d, "health").isEmpty())
                parts << str(d, "health");
            g.rows.append({str(d, "name"), parts.join(QStringLiteral(" · "))});
        }
        for (const QJsonValue& v : arr(hw, "volumes")) {
            const QJsonObject vol = v.toObject();
            const QString label = str(vol, "label");
            g.rows.append({QObject::tr("Диск %1").arg(str(vol, "letter")) + (label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(label)),
                           QObject::tr("%1 · свободно %2 из %3")
                               .arg(str(vol, "fs"), formatGb(num(vol, "freeBytes")), formatGb(num(vol, "bytes")))});
        }
        if (g.rows.isEmpty())
            g.rows.append({QObject::tr("Накопители"), unknown});
        groups.append(g);
    }

    // Board / BIOS / device
    {
        const QJsonObject board = hw.value(QStringLiteral("board")).toObject();
        const QJsonObject bios = hw.value(QStringLiteral("bios")).toObject();
        const QJsonObject device = hw.value(QStringLiteral("device")).toObject();
        InfoGroup g;
        g.title = QObject::tr("Материнская плата и BIOS");
        g.rows = {
            {QObject::tr("Плата"), (str(board, "manufacturer") + QLatin1Char(' ') + str(board, "product")).trimmed()},
            {QObject::tr("Версия платы"), str(board, "version")},
            {QObject::tr("BIOS"), (str(bios, "vendor") + QLatin1Char(' ') + str(bios, "version")).trimmed()},
            {QObject::tr("Дата BIOS"), dateRu(str(bios, "date"))},
            {QObject::tr("Тип устройства"),
             device.value(QStringLiteral("isLaptop")).toBool() ? QObject::tr("Ноутбук") : QObject::tr("Настольный ПК")},
            {QObject::tr("Тип системы"), str(device, "systemType")},
        };
        groups.append(g);
    }

    // Sound and network
    {
        InfoGroup g;
        g.title = QObject::tr("Звук и сеть");
        for (const QJsonValue& v : arr(hw, "sound"))
            g.rows.append({QObject::tr("Звук"), v.toString()});
        for (const QJsonValue& v : arr(hw, "network")) {
            const QJsonObject n = v.toObject();
            g.rows.append({QObject::tr("Сеть"), QStringLiteral("%1 · %2").arg(str(n, "name"), str(n, "speed"))});
        }
        if (!g.rows.isEmpty())
            groups.append(g);
    }
    return groups;
}

QList<InfoRow> windowsOverview(const QJsonObject& win)
{
    QString activation = QObject::tr("Неизвестно");
    Status status = Status::Neutral;
    const QJsonObject lic = win.value(QStringLiteral("activation")).toObject();
    if (!lic.isEmpty()) {
        switch (static_cast<int>(num(lic, "status"))) {
        case 1: activation = QObject::tr("Активирована"); status = Status::Good; break;
        case 0: activation = QObject::tr("Не активирована"); status = Status::Bad; break;
        case 5: activation = QObject::tr("Требуется уведомление об активации"); status = Status::Warn; break;
        default: activation = QObject::tr("Льготный период"); status = Status::Warn; break;
        }
    }
    const QString build = str(win, "build");
    const int ubr = static_cast<int>(num(win, "ubr"));
    return {
        {QObject::tr("Операционная система"), stripVendorPrefix(str(win, "caption"))},
        {QObject::tr("Версия"), str(win, "displayVersion")},
        {QObject::tr("Редакция"), str(win, "edition")},
        {QObject::tr("Сборка"), ubr > 0 ? QStringLiteral("%1.%2").arg(build).arg(ubr) : build},
        {QObject::tr("Дата установки"), dateRu(str(win, "installDate"))},
        {QObject::tr("Состояние активации"), activation, status},
    };
}

QList<InfoGroup> windowsGroups(const QJsonObject& win)
{
    QList<InfoGroup> groups;

    groups.append({QObject::tr("Региональные параметры"),
                   {{QObject::tr("Язык системы"), str(win, "uiLanguage")},
                    {QObject::tr("Регион"), str(win, "region")},
                    {QObject::tr("Часовой пояс"), str(win, "timeZone")}}});

    // Virtualization and isolation
    InfoGroup virt;
    virt.title = QObject::tr("Виртуализация и изоляция");
    {
        const int cip = static_cast<int>(num(win, "codeIntegrityPolicy"));
        InfoRow appControl{QObject::tr("AppControl")};
        if (cip == 2) {
            appControl.value = QObject::tr("Включён (принудительно)");
            appControl.status = Status::Good;
        } else if (cip == 1) {
            appControl.value = QObject::tr("Режим аудита");
            appControl.status = Status::Warn;
        } else {
            appControl.value = QObject::tr("Выключен");
            appControl.status = Status::Neutral;
        }
        virt.rows.append(appControl);

        InfoRow vbs{QObject::tr("Virtualization-Based Security (VBS)")};
        switch (static_cast<int>(num(win, "vbsStatus"))) {
        case 2: vbs.value = QObject::tr("Работает"); vbs.status = Status::Good; break;
        case 1: vbs.value = QObject::tr("Включена, не запущена"); vbs.status = Status::Warn; break;
        default: vbs.value = QObject::tr("Выключена"); vbs.status = Status::Neutral; break;
        }
        virt.rows.append(vbs);

        InfoRow hyperV{QObject::tr("Hyper-V")};
        if (win.value(QStringLiteral("hypervisorPresent")).toBool()) {
            hyperV.value = QObject::tr("Включён (гипервизор активен)");
            hyperV.status = Status::Good;
        } else if (!str(win, "vmms").isEmpty()) {
            hyperV.value = QObject::tr("Установлен, гипервизор не запущен");
            hyperV.status = Status::Warn;
        } else {
            hyperV.value = QObject::tr("Выключен");
            hyperV.status = Status::Neutral;
        }
        virt.rows.append(hyperV);
    }
    groups.append(virt);

    // Protection components
    InfoGroup protection;
    protection.title = QObject::tr("Компоненты защиты");
    {
        InfoRow defender{QObject::tr("Windows Defender")};
        defender.status = defenderStatus(win, &defender.value);
        protection.rows.append(defender);

        InfoRow kernel{QObject::tr("Изоляция ядра")};
        kernel.status = kernelIsolationStatus(win, &kernel.value);
        protection.rows.append(kernel);

        InfoRow uac{QObject::tr("Контроль учётных записей (UAC)")};
        uac.status = uacStatus(&uac.value);
        protection.rows.append(uac);

        InfoRow smart{QObject::tr("SmartScreen")};
        smart.status = smartScreenStatus(&smart.value);
        protection.rows.append(smart);
    }
    groups.append(protection);
    return groups;
}

QList<InfoRow> windowsExtras(const QJsonObject& win)
{
    return {
        {QObject::tr("Разрядность"), str(win, "architecture")},
        {QObject::tr("Имя компьютера"), str(win, "computerName")},
        {QObject::tr("Последняя загрузка"), QDateTime::fromString(str(win, "lastBoot"), QStringLiteral("yyyy-MM-dd HH:mm")).toString(QStringLiteral("dd.MM.yyyy HH:mm"))},
        {QObject::tr("Сигнатуры Defender"), str(win, "defenderSignatures")},
    };
}

QString fullReport(const QJsonObject& hw, const QJsonObject& win)
{
    QString out;
    auto heading = [&out](const QString& title) {
        out += QStringLiteral("\n=== %1 ===\n").arg(title.toUpper());
    };
    auto rows = [&out](const QList<InfoRow>& list) {
        for (const InfoRow& r : list) {
            if (!r.value.isEmpty())
                out += QStringLiteral("%1: %2\n").arg(r.key, r.value);
        }
    };

    out += QStringLiteral("BadCode Guard — информация о системе\n");
    out += QStringLiteral("Создано: %1\n").arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")));

    heading(QObject::tr("Кратко"));
    out += copyText(hw) + QLatin1Char('\n');
    out += QObject::tr("Всего видеопамяти: %1\n").arg(formatGb(totalVramBytes(hw)));
    out += QObject::tr("Объём накопителей: %1\n").arg(formatDriveSize(totalDiskBytes(hw)));

    for (const InfoGroup& g : hardwareGroups(hw)) {
        heading(g.title);
        rows(g.rows);
    }

    if (!win.isEmpty()) {
        heading(QObject::tr("Windows"));
        rows(windowsOverview(win));
        rows(windowsExtras(win));
        for (const InfoGroup& g : windowsGroups(win)) {
            heading(g.title);
            rows(g.rows);
        }
    }
    return out;
}

} // namespace info
} // namespace bcg::sys
