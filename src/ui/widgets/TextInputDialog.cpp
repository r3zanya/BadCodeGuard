#include "TextInputDialog.h"

#include "ui/Theme.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;

QString TextInputDialog::getText(QWidget* parent, const QString& title, const QString& hint,
                                 const QString& placeholder)
{
    TextInputDialog dialog(parent, title, hint, placeholder);
    return dialog.exec() == QDialog::Accepted ? dialog.m_edit->text().trimmed() : QString();
}

TextInputDialog::TextInputDialog(QWidget* parent, const QString& title, const QString& hint,
                                 const QString& placeholder)
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
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("exclude"), th::kAccent, 28,
                                       devicePixelRatioF()));
    head->addWidget(icon);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("rowTitle"));
    titleLabel->setWordWrap(true);
    head->addWidget(titleLabel, 1);
    root->addLayout(head);

    auto* hintLabel = new QLabel(hint);
    hintLabel->setObjectName(QStringLiteral("muted"));
    hintLabel->setWordWrap(true);
    root->addWidget(hintLabel);

    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(placeholder);
    root->addWidget(m_edit);
    root->addSpacing(6);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch();
    auto* cancel = new FancyButton(tr("Отмена"), QString(), FancyButton::Secondary);
    auto* add = new FancyButton(tr("Добавить"), QString(), FancyButton::Primary);
    buttons->addWidget(cancel);
    buttons->addWidget(add);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);
    m_edit->setFocus();
}

void TextInputDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(200);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}
