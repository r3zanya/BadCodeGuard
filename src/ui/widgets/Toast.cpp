#include "Toast.h"

#include "ui/Theme.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>

namespace th = bcg::theme;

namespace {
constexpr int kBottomMargin = 28;
constexpr int kSlide = 18;
constexpr int kVisibleMs = 3400;
} // namespace

void Toast::show(QWidget* context, const QString& text, Kind kind)
{
    QWidget* window = context ? context->window() : nullptr;
    if (!window)
        return;
    // One toast at a time.
    for (Toast* old : window->findChildren<Toast*>(QString(), Qt::FindDirectChildrenOnly))
        old->deleteLater();
    auto* toast = new Toast(window, text, kind);
    toast->QWidget::show();
}

Toast::Toast(QWidget* window, const QString& text, Kind kind)
    : QFrame(window)
    , m_kind(kind)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);

    const QString icon = kind == Kind::Success ? QStringLiteral("check")
        : kind == Kind::Error                  ? QStringLiteral("warning")
                                               : QStringLiteral("info");
    const QColor tint = kind == Kind::Error ? th::kDanger : th::kAccent;

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 20, 12);
    layout->setSpacing(10);
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(bcg::icons::pixmap(icon, tint, 20, devicePixelRatioF()));
    layout->addWidget(iconLabel);
    auto* label = new QLabel(text);
    // Single line when it fits, otherwise wrap at a sensible width.
    const int maxWidth = qMax(260, window->width() - 240);
    if (label->sizeHint().width() > maxWidth) {
        label->setWordWrap(true);
        label->setFixedWidth(maxWidth);
    }
    layout->addWidget(label);
    adjustSize();

    const int x = (window->width() - width()) / 2;
    const int y = window->height() - height() - kBottomMargin;
    move(x, y + kSlide);

    auto* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);

    auto* group = new QParallelAnimationGroup(this);
    auto* fade = new QPropertyAnimation(effect, "opacity", group);
    fade->setDuration(260);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    auto* slide = new QPropertyAnimation(this, "pos", group);
    slide->setDuration(320);
    slide->setEndValue(QPoint(x, y));
    slide->setEasingCurve(QEasingCurve::OutCubic);
    group->start(QAbstractAnimation::DeleteWhenStopped);

    raise();
    QTimer::singleShot(kVisibleMs, this, &Toast::dismiss);
}

void Toast::dismiss()
{
    if (m_closing)
        return;
    m_closing = true;
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(graphicsEffect());
    if (!effect) {
        deleteLater();
        return;
    }
    auto* fade = new QPropertyAnimation(effect, "opacity", this);
    fade->setDuration(300);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InCubic);
    connect(fade, &QPropertyAnimation::finished, this, &QObject::deleteLater);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor border = m_kind == Kind::Error ? th::kDanger : th::mix(th::kBorder, th::kAccent, 0.5);
    p.setPen(QPen(border, 1.0));
    p.setBrush(QColor(0x14, 0x14, 0x18));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
}
