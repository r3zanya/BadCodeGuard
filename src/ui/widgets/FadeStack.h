#pragma once

#include <QPointer>
#include <QStackedWidget>

class QLabel;
class QParallelAnimationGroup;

// QStackedWidget that cross-fades between pages: the page being left fades
// out (from a snapshot) while the new one fades in and rises into place.
class FadeStack : public QStackedWidget {
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;

    void setCurrentIndexAnimated(int index);

private:
    void finishAnimation();

    QPointer<QParallelAnimationGroup> m_group;
    QPointer<QWidget> m_page;
    QPointer<QLabel> m_overlay;
};
