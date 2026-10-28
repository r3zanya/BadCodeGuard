#pragma once

#include <QColor>
#include <QString>

namespace bcg::theme {

inline const QColor kBackground{0x00, 0x00, 0x00};
inline const QColor kSidebar{0x07, 0x07, 0x09};
inline const QColor kCard{0x0F, 0x0F, 0x12};
inline const QColor kCardHover{0x18, 0x18, 0x1D};
inline const QColor kBorder{0x1F, 0x1F, 0x25};
inline const QColor kText{0xF2, 0xF2, 0xF5};
inline const QColor kMuted{0x8A, 0x8A, 0x95};
inline const QColor kAccent{0x2E, 0xE6, 0xB0};
inline const QColor kAccentLight{0x6B, 0xF2, 0xCC};
inline const QColor kAccentDark{0x04, 0x23, 0x1A};
inline const QColor kDanger{0xFF, 0x5C, 0x6C};
inline const QColor kTrackOff{0x2B, 0x2B, 0x32};

// Linear blend of two colours, t in [0, 1].
inline QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

inline QString styleSheet()
{
    return QStringLiteral(R"(
* { font-family: "Segoe UI"; font-size: 14px; color: #F2F2F5; }
QMainWindow, QWidget#page { background: #000000; }
QFrame#sidebar { background: #070709; border-right: 1px solid #141418; }
QFrame#card { background: #0F0F12; border: 1px solid #1F1F25; border-radius: 14px; }
QFrame#tile { background: #0A0A0C; border: 1px solid #1F1F25; border-radius: 10px; }
QFrame#divider { background: #1F1F25; border: none; }
QLabel { background: transparent; }
QLabel#title { font-size: 26px; font-weight: 600; }
QLabel#brand { font-size: 17px; font-weight: 700; }
QLabel#muted, QLabel#version { color: #8A8A95; }
QLabel#status { font-size: 22px; font-weight: 600; color: #2EE6B0; }
QLabel#stat { font-size: 22px; font-weight: 600; }
QLabel#rowTitle { font-size: 15px; font-weight: 600; }
QListWidget, QPlainTextEdit { background: #0A0A0C; border: 1px solid #1F1F25; border-radius: 10px; padding: 6px; }
QListWidget { outline: none; }
QListWidget::item { padding: 8px 6px; border-radius: 6px; }
QListWidget::item:hover { background: #18181D; }
QListWidget::item:selected { background: rgba(46, 230, 176, 0.14); }
QPlainTextEdit { selection-background-color: #2EE6B0; selection-color: #000000; }
QLabel#sectionTitle { margin-top: 4px; }
QPlainTextEdit { font-family: Consolas; font-size: 12px; color: #8A8A95; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #2B2B32; border-radius: 3px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #3C3C45; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
QMenu { background: #0F0F12; border: 1px solid #1F1F25; padding: 4px; }
QMenu::item { padding: 6px 20px; border-radius: 4px; }
QMenu::item:selected { background: #18181D; }
QDialog { background: #0A0A0C; }
QLineEdit { background: #0F0F12; border: 1px solid #1F1F25; border-radius: 8px; padding: 8px 10px; selection-background-color: #2EE6B0; selection-color: #000000; }
QLineEdit:focus { border: 1px solid #2EE6B0; }
QScrollArea { background: transparent; border: none; }
QWidget#scrollBody { background: transparent; }
QLabel#sectionTitle { font-size: 16px; font-weight: 600; }
QLabel#tileMain { font-size: 15px; font-weight: 600; }
QLabel#bigScore { font-size: 34px; font-weight: 700; }
QLabel#scoreValue { font-size: 15px; font-weight: 600; }
QLabel#warnText { color: #FFB84C; }
QLabel#badge { background: rgba(46, 230, 176, 0.14); color: #2EE6B0; border-radius: 9px; padding: 2px 10px; font-size: 12px; font-weight: 600; }
QLabel#tag { background: rgba(255, 184, 76, 0.14); color: #FFB84C; border-radius: 9px; padding: 2px 10px; font-size: 12px; font-weight: 600; }
QLabel#tag[muted="true"] { background: rgba(138, 138, 149, 0.14); color: #8A8A95; }
QComboBox, QTimeEdit { background: #0F0F12; border: 1px solid #1F1F25; border-radius: 8px; padding: 6px 10px; min-height: 20px; }
QComboBox:hover, QTimeEdit:hover { border: 1px solid #2B2B32; }
QComboBox:focus, QTimeEdit:focus, QComboBox:on { border: 1px solid #2EE6B0; }
QComboBox:disabled, QTimeEdit:disabled { color: #55555E; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(:/icons/chevron.svg); width: 14px; height: 14px; }
QComboBox QAbstractItemView { background: #0F0F12; border: 1px solid #1F1F25; padding: 4px; outline: none; selection-background-color: rgba(46, 230, 176, 0.14); selection-color: #F2F2F5; }
QTimeEdit::up-button, QTimeEdit::down-button { width: 0; border: none; }
QToolTip { background: #18181D; color: #F2F2F5; border: 1px solid #1F1F25; }
)");
}

} // namespace bcg::theme
