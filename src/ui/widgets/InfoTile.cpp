#include "InfoTile.h"

#include "ui/widgets/CardPaint.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;

namespace {

// Rounded square with the icon drawn in accent colour.
class IconBadge : public QWidget {
public:
    explicit IconBadge(const QString& icon, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(38, 38);
        m_pixmap = bcg::icons::pixmap(icon, th::kAccent, 20, devicePixelRatioF());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fill = th::kAccent;
        fill.setAlphaF(0.12f);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(rect()), 11, 11);
        p.drawPixmap(QPointF((width() - 20) / 2.0, (height() - 20) / 2.0), m_pixmap);
    }

private:
    QPixmap m_pixmap;
};

} // namespace

InfoTile::InfoTile(const QString& icon, const QString& caption, QWidget* parent)
    : QFrame(parent)
{
    setMinimumHeight(136);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(10);

    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    head->addWidget(new IconBadge(icon));
    auto* captionLabel = new QLabel(caption);
    captionLabel->setObjectName(QStringLiteral("muted"));
    head->addWidget(captionLabel, 1);
    root->addLayout(head);

    m_content = new QWidget;
    auto* content = new QVBoxLayout(m_content);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(3);
    m_main = new QLabel;
    m_main->setObjectName(QStringLiteral("tileMain"));
    m_main->setWordWrap(true);
    m_sub = new QLabel;
    m_sub->setObjectName(QStringLiteral("muted"));
    m_sub->setWordWrap(true);
    content->addWidget(m_main);
    content->addWidget(m_sub);
    content->addStretch();
    root->addWidget(m_content, 1);

    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(200);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    setLoading();
}

void InfoTile::setLoading()
{
    m_main->setText(tr("Загрузка…"));
    m_sub->clear();
}

void InfoTile::setContent(const QString& main, const QString& sub)
{
    if (main == m_main->text() && sub == m_sub->text())
        return;
    m_main->setText(main);
    m_sub->setText(sub);
    m_sub->setVisible(!sub.isEmpty());

    auto* effect = new QGraphicsOpacityEffect(m_content);
    effect->setOpacity(0.0);
    m_content->setGraphicsEffect(effect);
    auto* fade = new QPropertyAnimation(effect, "opacity", effect);
    fade->setDuration(420);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    // Drop the effect after the animation has fully stopped.
    connect(fade, &QPropertyAnimation::finished, m_content, [this] {
        QMetaObject::invokeMethod(m_content, [this] { m_content->setGraphicsEffect(nullptr); },
                                  Qt::QueuedConnection);
    });
    fade->start();
}

void InfoTile::setMainColor(const QColor& color)
{
    m_main->setStyleSheet(color.isValid() ? QStringLiteral("color: %1;").arg(color.name()) : QString());
}

void InfoTile::setHover(qreal v)
{
    m_hover = v;
    update();
}

void InfoTile::enterEvent(QEnterEvent* e)
{
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hover);
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
    QFrame::enterEvent(e);
}

void InfoTile::leaveEvent(QEvent* e)
{
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hover);
    m_hoverAnim->setEndValue(0.0);
    m_hoverAnim->start();
    QFrame::leaveEvent(e);
}

void InfoTile::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    bcg::paintCard(p, QRectF(rect()), m_hover);
}
