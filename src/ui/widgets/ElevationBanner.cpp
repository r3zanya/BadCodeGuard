#include "ElevationBanner.h"

#include "sys/Elevation.h"
#include "ui/Theme.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/Icons.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;

namespace {
const QColor kAmber(0xFF, 0xB8, 0x4C);
}

ElevationBanner::ElevationBanner(QWidget* parent)
    : QFrame(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(18, 14, 16, 14);
    layout->setSpacing(14);

    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("lock"), kAmber, 26, devicePixelRatioF()));
    layout->addWidget(icon);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto* title = new QLabel(tr("Нужны права администратора"));
    title->setObjectName(QStringLiteral("rowTitle"));
    auto* body = new QLabel(tr("Чтобы менять эти параметры, перезапустите BadCode Guard от имени "
                               "администратора."));
    body->setObjectName(QStringLiteral("muted"));
    body->setWordWrap(true);
    texts->addWidget(title);
    texts->addWidget(body);
    layout->addLayout(texts, 1);

    auto* restart = new FancyButton(tr("Перезапустить"), QStringLiteral("shield"), FancyButton::Secondary);
    layout->addWidget(restart);
    connect(restart, &QPushButton::clicked, this, [] {
        if (bcg::sys::relaunchElevated())
            QApplication::quit();
    });

    m_flashAnim = new QPropertyAnimation(this, "flash", this);
    m_flashAnim->setDuration(900);
    m_flashAnim->setKeyValueAt(0.0, 0.0);
    m_flashAnim->setKeyValueAt(0.25, 1.0);
    m_flashAnim->setKeyValueAt(0.5, 0.2);
    m_flashAnim->setKeyValueAt(0.75, 1.0);
    m_flashAnim->setKeyValueAt(1.0, 0.0);

    setVisible(!bcg::sys::isElevated());
}

void ElevationBanner::pulse()
{
    if (!isVisible())
        return;
    m_flashAnim->stop();
    m_flashAnim->start();
}

void ElevationBanner::setFlash(qreal v)
{
    m_flash = v;
    update();
}

void ElevationBanner::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = kAmber;
    fill.setAlphaF(static_cast<float>(0.07 + 0.10 * m_flash));
    QColor border = kAmber;
    border.setAlphaF(static_cast<float>(0.30 + 0.60 * m_flash));
    p.setPen(QPen(border, 1.2));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.6, 0.6, -0.6, -0.6), 14, 14);
}
