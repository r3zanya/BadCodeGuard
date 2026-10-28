#pragma once

#include <QJsonObject>
#include <QString>

namespace bcg::sys {

// Estimated ratings on a 0..10 scale. The CPU score comes from a short
// built-in benchmark; the others are derived from the hardware description.
struct Scores {
    double cpu = 0;
    double gpu = 0;
    double ramCapacity = 0;
    double ramSpeed = 0;
    double total = 0;
};

// Blocking (about one second of CPU load): call it off the GUI thread.
Scores computeScores(const QJsonObject& hardware);

// "9,8/10", "10/10", "0/10".
QString formatScore(double score);

// Raw single-thread / all-thread throughput of the benchmark kernel
// (million iterations per second). Exposed for calibration.
struct BenchmarkResult {
    double single = 0;
    double multi = 0;
};
BenchmarkResult runCpuBenchmark();

} // namespace bcg::sys
