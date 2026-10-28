#include "ConfirmDialog.h"

#include "ui/Theme.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;

bool ConfirmDialog::ask(QWidget* parent, const QString& title, const QString& text,
                        const QString& confirmText, bool danger)
{
    ConfirmDialog dialog(parent, title, text, confirmText, danger);
    return dialog.exec() == QDialog::Accepted;
}

ConfirmDialog::ConfirmDialog(QWidget* parent, const QString& title, const QString& text,
                             const QString& confirmText, bool danger)
    : QDialog(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
                  | Qt::WindowCloseButtonHint)
{
    setWindowTitle(QStringLiteral("BadCode Guard"));
    setFixedWidth(460);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 26, 28, 22);
    root->setSpacing(14);

    auto* head = new QHBoxLayout;
    head->setSpacing(12);
    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(danger ? QStringLiteral("warning") : QStringLiteral("info"),
                                       danger ? th::kDanger : th::kAccent, 28, devicePixelRatioF()));
    head->addWidget(icon);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("rowTitle"));
    titleLabel->setWordWrap(true);
    head->addWidget(titleLabel, 1);
    root->addLayout(head);

    auto* body = new QLabel(text);
    body->setObjectName(QStringLiteral("muted"));
    body->setWordWrap(true);
    root->addWidget(body);
    root->addSpacing(6);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch();
    auto* cancel = new FancyButton(tr("Отмена"), QString(), FancyButton::Secondary);
    auto* confirm = new FancyButton(confirmText, QString(),
                                    danger ? FancyButton::Danger : FancyButton::Primary);
    buttons->addWidget(cancel);
    buttons->addWidget(confirm);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
    cancel->setFocus();
}

void ConfirmDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(200);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}
