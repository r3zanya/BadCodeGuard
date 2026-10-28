#pragma once

#include <QRectF>
#include <QStringList>
#include <QWidget>

class QPropertyAnimation;

// Row of text tabs with a highlight pill that glides to the selected tab.
class SegmentedTabs : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QRectF pill READ pill WRITE setPill)
public:
    explicit SegmentedTabs(QWidget* parent = nullptr);

    void addTab(const QString& text);
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index, bool animate = true);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    QRectF pill() const { return m_pill; }
    void setPill(const QRectF& r);

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF tabRect(int index) const;
    int tabAt(const QPoint& pos) const;

    QStringList m_tabs;
    int m_current = 0;
    int m_hover = -1;
    QRectF m_pill;
    QPropertyAnimation* m_pillAnim;
};
