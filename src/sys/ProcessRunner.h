#pragma once

#include <QObject>
#include <QStringDecoder>

class QProcess;

namespace bcg::sys {

// Runs a program without a console window and streams its output as text.
// Handles the encodings console tools use: OEM code page, UTF-16 (sfc) and,
// when requested, UTF-8 (our PowerShell scripts).
class ProcessRunner : public QObject {
    Q_OBJECT
public:
    enum class Encoding { Auto, Utf8 };

    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    void setEncoding(Encoding encoding) { m_encoding = encoding; }
    void start(const QString& program, const QStringList& arguments);
    bool isRunning() const;
    bool wasCancelled() const { return m_cancelled; }
    void cancel();

signals:
    void line(const QString& text);       // one meaningful line of output
    void percent(double value);           // 0..100, parsed from progress output
    void finished(int exitCode, bool ok); // ok == false: failed to start, crashed or cancelled

private:
    void onReadyRead();
    void consume(const QByteArray& chunk);

    QProcess* m_process;
    Encoding m_encoding = Encoding::Auto;
    enum class Detected { Unknown, Oem, Utf16 } m_detected = Detected::Unknown;
    QStringDecoder m_utf8{QStringDecoder::Utf8};
    QByteArray m_oddByte;
    QString m_partialLine;
    bool m_cancelled = false;
};

// Blocking helper for short commands (run it off the GUI thread).
// Returns the exit code, or -1 when the program could not be started.
int runHidden(const QString& program, const QStringList& arguments, int timeoutMs = 30000,
              QString* output = nullptr);

} // namespace bcg::sys
