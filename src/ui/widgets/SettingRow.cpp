#include "SettingRow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

SettingRow::SettingRow(const QString& title, const QString& description, QWidget* control,
                       QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(16);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(3);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("rowTitle"));
    m_badge = new QLabel;
    m_badge->setObjectName(QStringLiteral("badge"));
    m_badge->hide();
    m_tag = new QLabel;
    m_tag->setObjectName(QStringLiteral("tag"));
    m_tag->hide();
    titleRow->addWidget(titleLabel, 0, Qt::AlignVCenter);
    titleRow->addWidget(m_badge, 0, Qt::AlignVCenter);
    titleRow->addWidget(m_tag, 0, Qt::AlignVCenter);
    titleRow->addStretch();
    texts->addLayout(titleRow);

    m_description = new QLabel(description);
    m_description->setObjectName(QStringLiteral("muted"));
    m_description->setWordWrap(true);
    texts->addWidget(m_description);

    layout->addLayout(texts, 1);
    if (control)
        layout->addWidget(control, 0, Qt::AlignVCenter);
}

void SettingRow::setBadge(const QString& text)
{
    m_badge->setText(text);
    m_badge->setVisible(!text.isEmpty());
}

void SettingRow::setTag(const QString& text, bool muted)
{
    m_tag->setProperty("muted", muted);
    m_tag->style()->unpolish(m_tag);
    m_tag->style()->polish(m_tag);
    m_tag->setText(text);
    m_tag->setVisible(!text.isEmpty());
}

void SettingRow::setDescription(const QString& text)
{
    m_description->setText(text);
}
