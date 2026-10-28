#pragma once

class QWidget;

namespace bcg {

// Fades a widget in; the temporary graphics effect is removed afterwards.
void fadeIn(QWidget* widget, int durationMs = 260);

} // namespace bcg
