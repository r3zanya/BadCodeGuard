#pragma once

#include <QDialog>

class FancyButton;
class GlowProgress;
class QLabel;
class QPlainTextEdit;

namespace bcg::sys {
class ProcessRunner;
}

// Window that runs a long console tool (SFC, DISM) and shows its progress and
// output. It is not modal: the user may keep using the app while it works.
class RunDialog : public QDialog {
    Q_OBJECT
public:
    RunDialog(QWidget* parent, const QString& title, const QString& program,
              const QStringList& arguments);

signals:
    void completed(bool success);

protected:
    void showEvent(QShowEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void reject() override;

private:
    void onFinished(int exitCode, bool ok);

    bcg::sys::ProcessRunner* m_runner;
    QString m_program;
    QStringList m_arguments;
    QLabel* m_status;
    GlowProgress* m_progress;
    QPlainTextEdit* m_log;
    FancyButton* m_cancel;
    FancyButton* m_close;
    bool m_started = false;
};
