#include "SubPage.h"

#include "ui/widgets/IconButton.h"
#include "ui/widgets/SmoothScrollArea.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

SubPage::SubPage(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* headerWidget = new QWidget;
    m_header = new QHBoxLayout(headerWidget);
    m_header->setContentsMargins(36, 28, 36, 14);
    m_header->setSpacing(12);
    auto* back = new IconButton(QStringLiteral("back"), tr("Назад"));
    m_header->addWidget(back);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("title"));
    m_header->addWidget(titleLabel);
    m_header->addStretch();
    root->addWidget(headerWidget);
    connect(back, &QAbstractButton::clicked, this, &SubPage::backRequested);

    auto* scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("scrollBody"));
    m_body = new QVBoxLayout(content);
    m_body->setContentsMargins(36, 6, 30, 30);
    m_body->setSpacing(16);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void SubPage::addHeaderAction(QWidget* widget)
{
    m_header->addWidget(widget);
}
