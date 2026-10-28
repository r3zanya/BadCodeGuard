#include "ScoreCard.h"

#include "ui/Theme.h"
#include "ui/widgets/GlowProgress.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace th = bcg::theme;

namespace {

QColor scoreColor(double score)
{
    if (score < 4.0)
        return th::kDanger;
    if (score < 7.0)
        return QColor(0xFF, 0xB8, 0x4C);
    return th::kAccent;
}

} // namespace

ScoreCard::ScoreCard(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 20);
    root->setSpacing(12);

    auto* head = new QHBoxLayout;
    auto* title = new QLabel(tr("Оценка характеристик"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    head->addWidget(title);
    head->addStretch();
    m_hint = new QLabel;
    m_hint->setObjectName(QStringLiteral("muted"));
    head->addWidget(m_hint);
    root->addLayout(head);

    m_cpu = addRow(root, tr("Процессор"), QString());
    m_gpu = addRow(root, tr("Видеокарта"), QString());
    m_capacity = addRow(root, tr("Всего памяти в системе"), QString());
    m_speed = addRow(root, tr("Оперативная память (тип и частота)"), QString());

    auto* divider = new QFrame;
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFixedHeight(1);
    root->addWidget(divider);

    auto* totalRow = new QHBoxLayout;
    auto* totalTitle = new QLabel(tr("Итоговый балл"));
    totalTitle->setObjectName(QStringLiteral("rowTitle"));
    totalRow->addWidget(totalTitle);
    totalRow->addStretch();
    m_total = new QLabel(QStringLiteral("—"));
    m_total->setObjectName(QStringLiteral("bigScore"));
    totalRow->addWidget(m_total);
    root->addLayout(totalRow);

    setMaximumHeight(0);
    hide();
}

ScoreCard::Row ScoreCard::addRow(QVBoxLayout* layout, const QString& title, const QString&)
{
    Row row;
    auto* line = new QHBoxLayout;
    line->setSpacing(14);
    auto* label = new QLabel(title);
    label->setFixedWidth(250);
    line->addWidget(label);
    row.bar = new GlowProgress;
    line->addWidget(row.bar, 1);
    row.value = new QLabel(QStringLiteral("—"));
    row.value->setObjectName(QStringLiteral("scoreValue"));
    row.value->setFixedWidth(70);
    row.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    line->addWidget(row.value);
    layout->addLayout(line);
    return row;
}

void ScoreCard::reveal()
{
    if (m_revealed)
        return;
    m_revealed = true;
    show();
    const int target = sizeHint().height();
    auto* anim = new QPropertyAnimation(this, "maximumHeight", this);
    anim->setDuration(420);
    anim->setStartValue(0);
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this] { setMaximumHeight(QWIDGETSIZE_MAX); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScoreCard::showLoading()
{
    m_hint->setText(tr("Идёт тест процессора, около секунды…"));
    for (Row* row : {&m_cpu, &m_gpu, &m_capacity, &m_speed}) {
        row->bar->setValue(0.0);
        row->bar->setBusy(true);
        row->value->setText(QStringLiteral("…"));
        row->value->setStyleSheet(QString());
    }
    m_total->setText(QStringLiteral("…"));
    m_total->setStyleSheet(QString());
    reveal();
}

void ScoreCard::animateTo(QLabel* label, double target, int duration)
{
    auto* anim = new QVariantAnimation(label);
    anim->setDuration(duration);
    anim->setStartValue(0.0);
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    label->setStyleSheet(QStringLiteral("color: %1;").arg(scoreColor(target).name()));
    connect(anim, &QVariantAnimation::valueChanged, label,
            [label](const QVariant& v) { label->setText(bcg::sys::formatScore(v.toDouble())); });
    connect(anim, &QVariantAnimation::finished, label,
            [label, target] { label->setText(bcg::sys::formatScore(target)); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScoreCard::showScores(const bcg::sys::Scores& scores)
{
    m_hint->setText(tr("Ориентировочная оценка по характеристикам, это не бенчмарк"));
    const struct {
        Row* row;
        double score;
    } items[] = {{&m_cpu, scores.cpu},
                 {&m_gpu, scores.gpu},
                 {&m_capacity, scores.ramCapacity},
                 {&m_speed, scores.ramSpeed}};

    int delay = 0;
    for (const auto& item : items) {
        // Stagger the rows so the card fills in one after another.
        QTimer::singleShot(delay, this, [this, item] {
            item.row->bar->setBusy(false);
            item.row->bar->setValue(item.score / 10.0);
            animateTo(item.row->value, item.score, 900);
        });
        delay += 140;
    }
    QTimer::singleShot(delay + 120, this, [this, scores] { animateTo(m_total, scores.total, 1200); });
    reveal();
}
