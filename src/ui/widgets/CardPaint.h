#pragma once

#include "ui/Theme.h"

#include <QPainter>
#include <QPen>

namespace bcg {

// Shared card look: dark fill, hairline border that turns accent on hover.
inline void paintCard(QPainter& p, const QRectF& rect, qreal hover, qreal radius = 14.0)
{
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::mix(theme::kBorder, theme::kAccent, hover * 0.8), 1.0));
    p.setBrush(theme::mix(theme::kCard, theme::kCardHover, hover));
    p.drawRoundedRect(r, radius, radius);
}

} // namespace bcg
