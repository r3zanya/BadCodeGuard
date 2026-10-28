#include "StatusBadge.h"

#include "ui/Theme.h"

#include <QFontMetrics>
#include <QPainter>

namespace th = bcg::theme;
using bcg::sys::Status;

namespace {
constexpr int kHeight = 28;
constexpr int kPadLeft = 12;
constexpr int kPadRight = 14;
constexpr int kDot = 8;
constexpr int kGap = 8;
} // namespace

StatusBadge::StatusBadge(const QString& text, Status status, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
    , m_status(status)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QColor StatusBadge::colorFor(Status status)
{
    switch (status) {
    case Status::Good: return th::kAccent;
    case Status::Warn: return QColor(0xFF, 0xB8, 0x4C);
    case Status::Bad: return th::kDanger;
    case Status::Neutral: return th::kMuted;
    case Status::None: break;
    }
    return th::kText;
}

QSize StatusBadge::sizeHint() const
{
    return {kPadLeft + kDot + kGap + fontMetrics().horizontalAdvance(m_text) + kPadRight, kHeight};
}

void StatusBadge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor color = colorFor(m_status);

    QColor fill = color;
    fill.setAlphaF(0.12f);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()), height() / 2.0, height() / 2.0);

    p.setBrush(color);
    p.drawEllipse(QRectF(kPadLeft, (height() - kDot) / 2.0, kDot, kDot));

    p.setPen(color);
    p.drawText(QRectF(kPadLeft + kDot + kGap, 0, width(), height()),
               Qt::AlignVCenter | Qt::AlignLeft, m_text);
}
