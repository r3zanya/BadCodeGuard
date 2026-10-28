#include "Scoring.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QLocale>
#include <QRegularExpression>
#include <QThread>
#include <QtMath>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace bcg::sys {

namespace {

// Calibration: a Ryzen 5 7500F (the development machine) is rated about 7.8
// for a single thread and 7.0 for all threads.
constexpr double kReferenceSingle = 710.0;
constexpr double kReferenceMulti = 7400.0;

double clampScore(double v)
{
    return std::clamp(v, 0.0, 10.0);
}

double round1(double v)
{
    return std::round(v * 10.0) / 10.0;
}

// Piecewise-linear interpolation through (x, y) points sorted by x.
double interpolate(const std::vector<std::pair<double, double>>& points, double x)
{
    if (x <= points.front().first)
        return points.front().second;
    for (size_t i = 1; i < points.size(); ++i) {
        if (x <= points[i].first) {
            const auto [x0, y0] = points[i - 1];
            const auto [x1, y1] = points[i];
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        }
    }
    return points.back().second;
}

double arrNum(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toDouble();
}

// ---- CPU benchmark ----

// A mix of integer and floating-point work that the compiler cannot remove.
quint64 kernel(quint64 seed, int chunks)
{
    quint64 x = seed | 1;
    double f = 1.0;
    for (int c = 0; c < chunks; ++c) {
        for (int i = 0; i < 1000; ++i) {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            f = f * 1.0000001 + static_cast<double>(x & 0xFF) * 1e-9;
        }
    }
    return x ^ static_cast<quint64>(f);
}

// Runs the kernel for about `ms` milliseconds and returns kilo-iterations done.
double timedRun(int ms, quint64 seed, std::atomic<quint64>& sink)
{
    QElapsedTimer timer;
    timer.start();
    double chunks = 0;
    quint64 acc = 0;
    while (timer.elapsed() < ms) {
        acc ^= kernel(seed + static_cast<quint64>(chunks), 100);
        chunks += 100;
    }
    sink ^= acc;
    return chunks; // each chunk is 1000 iterations -> chunks == kilo-iterations
}

// ---- GPU ----

double nvidiaScore(int model, bool plus)
{
    const int generation = model / 100;
    const int tier = model % 100;
    const double base = interpolate({{30, 2.5}, {50, 3.8}, {60, 5.4}, {70, 7.0}, {80, 8.4}, {90, 9.4}},
                                    tier);
    double bonus = 0.8;
    if (generation < 16)
        bonus = -1.5;
    else if (generation == 16)
        bonus = -1.0;
    else if (generation < 30)
        bonus = -0.6;
    else if (generation < 40)
        bonus = -0.1;
    else if (generation < 50)
        bonus = 0.4;
    return base + bonus + (plus ? 0.4 : 0.0);
}

double amdScore(int model, bool plus)
{
    const int generation = model / 1000;
    const int tier = (model % 1000) / 10;
    const double base = interpolate({{30, 2.5}, {50, 3.8}, {60, 5.4}, {70, 7.0}, {80, 8.4}, {90, 9.4}},
                                    tier);
    double bonus = 0.8;
    if (generation <= 5)
        bonus = -0.8;
    else if (generation == 6)
        bonus = -0.3;
    else if (generation == 7)
        bonus = 0.3;
    return base + bonus + (plus ? 0.4 : 0.0);
}

// Falls back to dedicated memory when the model is not recognised.
double vramFallbackScore(double vramGb)
{
    return interpolate({{0.0, 1.0}, {1.0, 1.5}, {2.0, 2.5}, {4.0, 4.0}, {6.0, 5.2}, {8.0, 6.3},
                        {12.0, 7.8}, {16.0, 9.0}, {24.0, 10.0}},
                       vramGb);
}

double scoreGpu(const QString& name, double vramBytes)
{
    static const QRegularExpression nvidia(QStringLiteral(R"((?:RTX|GTX)\s*(\d{4})\s*(Ti|SUPER)?)"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression amd(QStringLiteral(R"(RX\s*(\d{4})\s*(XTX|XT|GRE)?)"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression amdOld(QStringLiteral(R"(RX\s*(4|5)\d{2}\b)"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression arc(QStringLiteral(R"(Arc.*\b([AB])(\d{3})\b)"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression amdIgpu(QStringLiteral(R"(Radeon.*\b(\d{3})M\b)"),
                                            QRegularExpression::CaseInsensitiveOption);
    const double vramGb = vramBytes / (1024.0 * 1024.0 * 1024.0);

    if (auto m = nvidia.match(name); m.hasMatch())
        return nvidiaScore(m.captured(1).toInt(), !m.captured(2).isEmpty());
    if (auto m = amd.match(name); m.hasMatch())
        return amdScore(m.captured(1).toInt(), !m.captured(2).isEmpty());
    if (amdOld.match(name).hasMatch())
        return 3.4;
    if (auto m = arc.match(name); m.hasMatch()) {
        const int n = m.captured(2).toInt();
        if (m.captured(1).toUpper() == QLatin1String("B"))
            return 6.0 + (n - 570) / 20.0;
        return interpolate({{380, 2.8}, {580, 5.0}, {750, 5.8}, {770, 6.3}}, n);
    }
    if (auto m = amdIgpu.match(name); m.hasMatch())
        return 2.0 + (m.captured(1).toInt() - 600) / 150.0;
    if (name.contains(QStringLiteral("Iris"), Qt::CaseInsensitive))
        return 2.4;
    if (name.contains(QStringLiteral("UHD"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("HD Graphics"), Qt::CaseInsensitive))
        return 1.5;
    if (name.contains(QStringLiteral("Radeon"), Qt::CaseInsensitive)
        && name.contains(QStringLiteral("Graphics"), Qt::CaseInsensitive))
        return 2.2;
    return vramFallbackScore(vramGb);
}

// ---- RAM ----

double scoreRamCapacity(double gib)
{
    return interpolate({{0, 0}, {2, 1.0}, {4, 2.2}, {8, 4.5}, {16, 7.2}, {32, 9.2}, {64, 10.0}}, gib);
}

double scoreRamSpeed(int smbiosType, double mhz, int modules)
{
    double score;
    const bool ddr5 = smbiosType == 34 || smbiosType == 35 || (smbiosType == 0 && mhz >= 4400);
    const bool ddr4 = smbiosType == 26 || smbiosType == 30 || (smbiosType == 0 && mhz >= 2133 && !ddr5);
    if (ddr5)
        score = 7.5 + (mhz - 4800.0) / 1200.0;
    else if (ddr4)
        score = 4.0 + (mhz - 2133.0) / (3600.0 - 2133.0) * 3.0;
    else
        score = 2.0 + (mhz - 1066.0) / 800.0 * 1.5; // DDR3 and older
    if (modules >= 2)
        score += 0.3; // dual channel
    return score;
}

} // namespace

BenchmarkResult runCpuBenchmark()
{
    std::atomic<quint64> sink{0};

    timedRun(40, 1, sink); // warm-up: let the clock ramp up
    const double single = timedRun(300, 7, sink) / 300.0; // kilo-iterations per ms == Miter/s

    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<double> results(threads, 0.0);
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < threads; ++i) {
        pool.emplace_back([&, i] { results[i] = timedRun(350, 100 + i, sink); });
    }
    for (auto& t : pool)
        t.join();
    double multi = 0;
    for (double r : results)
        multi += r;
    return {single, multi / 350.0};
}

Scores computeScores(const QJsonObject& hw)
{
    Scores s;

    const BenchmarkResult bench = runCpuBenchmark();
    const double single = 7.8 + 2.2 * std::log2(std::max(bench.single, 1e-3) / kReferenceSingle);
    const double multi = 7.0 + 1.7 * std::log2(std::max(bench.multi, 1e-3) / kReferenceMulti);
    s.cpu = clampScore(0.45 * single + 0.55 * multi);

    double bestGpu = 0;
    for (const QJsonValue& v : hw.value(QStringLiteral("gpu")).toArray()) {
        const QJsonObject gpu = v.toObject();
        const QString name = gpu.value(QStringLiteral("name")).toString();
        if (name.contains(QStringLiteral("Virtual"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("Basic"), Qt::CaseInsensitive))
            continue;
        bestGpu = std::max(bestGpu, scoreGpu(name, arrNum(gpu, "vramBytes")));
    }
    s.gpu = clampScore(bestGpu);

    const QJsonArray modules = hw.value(QStringLiteral("ram")).toArray();
    double totalBytes = 0;
    double speed = 0;
    int type = 0;
    for (const QJsonValue& v : modules) {
        const QJsonObject m = v.toObject();
        totalBytes += arrNum(m, "bytes");
        const double mhz = arrNum(m, "configuredSpeed") > 0 ? arrNum(m, "configuredSpeed")
                                                            : arrNum(m, "speed");
        speed = speed == 0 ? mhz : std::min(speed, mhz);
        type = static_cast<int>(arrNum(m, "smbiosType"));
    }
    if (totalBytes <= 0)
        totalBytes = arrNum(hw, "totalRamBytes");
    s.ramCapacity = clampScore(scoreRamCapacity(totalBytes / (1024.0 * 1024.0 * 1024.0)));
    s.ramSpeed = clampScore(scoreRamSpeed(type, speed, modules.size()));

    s.total = clampScore(0.30 * s.cpu + 0.35 * s.gpu + 0.20 * s.ramCapacity + 0.15 * s.ramSpeed);

    s.cpu = round1(s.cpu);
    s.gpu = round1(s.gpu);
    s.ramCapacity = round1(s.ramCapacity);
    s.ramSpeed = round1(s.ramSpeed);
    s.total = round1(s.total);
    return s;
}

QString formatScore(double score)
{
    const double rounded = std::round(score * 10.0) / 10.0;
    const bool whole = std::abs(rounded - std::round(rounded)) < 1e-9;
    const QString text = whole ? QString::number(static_cast<int>(std::round(rounded)))
                               : QLocale(QLocale::Russian).toString(rounded, 'f', 1);
    return QStringLiteral("%1/10").arg(text);
}

} // namespace bcg::sys
