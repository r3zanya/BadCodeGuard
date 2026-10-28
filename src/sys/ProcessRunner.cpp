#include "ProcessRunner.h"

#include <QProcess>
#include <QRegularExpression>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace bcg::sys {

namespace {

void hideConsole(QProcess* process)
{
    process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
}

QString decodeOem(const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return {};
    const int count = MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(), bytes.size(), nullptr, 0);
    QString out(count, QChar());
    MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(), bytes.size(),
                        reinterpret_cast<wchar_t*>(out.data()), count);
    return out;
}

// DISM draws its progress as "[=====  42.0%  ]": the percentage is reported
// separately and the bar itself is not worth logging.
bool isProgressBar(const QString& text)
{
    static const QRegularExpression bar(QStringLiteral(R"(^\[[= ]*[\d.,]+%[= ]*\]$)"));
    return bar.match(text).hasMatch();
}

} // namespace

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    hideConsole(m_process);
    connect(m_process, &QProcess::readyRead, this, &ProcessRunner::onReadyRead);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        onReadyRead();
        if (!m_partialLine.trimmed().isEmpty())
            emit line(m_partialLine.trimmed());
        m_partialLine.clear();
        emit finished(code, status == QProcess::NormalExit && !m_cancelled);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit line(tr("Не удалось запустить программу."));
            emit finished(-1, false);
        }
    });
}

ProcessRunner::~ProcessRunner()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void ProcessRunner::start(const QString& program, const QStringList& arguments)
{
    m_detected = Detected::Unknown;
    m_utf8.resetState();
    m_oddByte.clear();
    m_partialLine.clear();
    m_cancelled = false;
    m_process->start(program, arguments);
}

bool ProcessRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void ProcessRunner::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    m_process->kill();
}

void ProcessRunner::onReadyRead()
{
    const QByteArray data = m_process->readAll();
    if (!data.isEmpty())
        consume(data);
}

void ProcessRunner::consume(const QByteArray& chunk)
{
    QString text;
    if (m_encoding == Encoding::Utf8) {
        text = m_utf8(chunk);
    } else {
        if (m_detected == Detected::Unknown)
            m_detected = chunk.contains('\0') ? Detected::Utf16 : Detected::Oem;

        if (m_detected == Detected::Utf16) {
            QByteArray bytes = m_oddByte + chunk;
            m_oddByte.clear();
            if (bytes.size() % 2 != 0) {
                m_oddByte = bytes.right(1);
                bytes.chop(1);
            }
            text = QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData()),
                                      bytes.size() / 2);
        } else {
            text = decodeOem(chunk);
        }
    }
    text.remove(QChar(0xFEFF)); // BOM

    static const QRegularExpression percentRe(QStringLiteral(R"((\d{1,3}(?:[.,]\d+)?)\s*%)"));
    auto it = percentRe.globalMatch(text);
    while (it.hasNext()) {
        QString number = it.next().captured(1);
        number.replace(QLatin1Char(','), QLatin1Char('.'));
        const double value = number.toDouble();
        if (value >= 0.0 && value <= 100.0)
            emit percent(value);
    }

    m_partialLine += text;
    static const QRegularExpression separators(QStringLiteral("[\r\n]+"));
    QStringList parts = m_partialLine.split(separators);
    m_partialLine = parts.takeLast();
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty() && !isProgressBar(trimmed))
            emit line(trimmed);
    }
}

int runHidden(const QString& program, const QStringList& arguments, int timeoutMs, QString* output)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    hideConsole(&process);
    process.start(program, arguments);
    if (!process.waitForStarted(5000))
        return -1;
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        return -1;
    }
    if (output)
        *output = decodeOem(process.readAll());
    return process.exitCode();
}

} // namespace bcg::sys
