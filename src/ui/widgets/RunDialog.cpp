#include "RunDialog.h"

#include "sys/ProcessRunner.h"
#include "ui/Theme.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/GlowProgress.h"

#include <QCloseEvent>
#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;

RunDialog::RunDialog(QWidget* parent, const QString& title, const QString& program,
                     const QStringList& arguments)
    : QDialog(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
                  | Qt::WindowMinimizeButtonHint)
    , m_runner(new bcg::sys::ProcessRunner(this))
    , m_program(program)
    , m_arguments(arguments)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);
    resize(640, 440);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(12);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("rowTitle"));
    root->addWidget(titleLabel);
    m_status = new QLabel(tr("Запуск…"));
    m_status->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_status);

    m_progress = new GlowProgress;
    root->addWidget(m_progress);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    root->addWidget(m_log, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = new FancyButton(tr("Остановить"), QStringLiteral("cancel"), FancyButton::Secondary);
    m_close = new FancyButton(tr("Закрыть"), QString(), FancyButton::Primary);
    m_close->setEnabled(false);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_close);
    root->addLayout(buttons);

    connect(m_cancel, &QPushButton::clicked, m_runner, &bcg::sys::ProcessRunner::cancel);
    connect(m_close, &QPushButton::clicked, this, &QDialog::close);

    connect(m_runner, &bcg::sys::ProcessRunner::line, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_runner, &bcg::sys::ProcessRunner::percent, this, [this](double value) {
        m_progress->setBusy(false);
        m_progress->setValue(value / 100.0);
        m_status->setText(tr("Выполнено: %1%").arg(QString::number(value, 'f', 0)));
    });
    connect(m_runner, &bcg::sys::ProcessRunner::finished, this, &RunDialog::onFinished);
}

void RunDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_started)
        return;
    m_started = true;

    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(220);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    m_progress->setBusy(true);
    m_status->setText(tr("Выполняется… Это может занять несколько минут."));
    m_runner->start(m_program, m_arguments);
}

void RunDialog::onFinished(int exitCode, bool ok)
{
    m_progress->setBusy(false);
    m_cancel->setEnabled(false);
    m_close->setEnabled(true);

    const bool success = ok && exitCode == 0;
    if (m_runner->wasCancelled()) {
        m_status->setText(tr("Остановлено пользователем."));
        m_progress->setValue(0.0);
    } else if (success) {
        m_status->setText(tr("Завершено успешно."));
        m_progress->setValue(1.0);
    } else {
        m_status->setText(tr("Завершено с кодом %1. Подробности в журнале ниже.").arg(exitCode));
    }
    emit completed(success);
}

// The window cannot be closed while the tool runs; use "Остановить".
void RunDialog::closeEvent(QCloseEvent* event)
{
    if (m_runner->isRunning()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void RunDialog::reject()
{
    if (!m_runner->isRunning())
        QDialog::reject();
}
