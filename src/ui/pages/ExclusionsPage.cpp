#include "ExclusionsPage.h"

#include "ui/Theme.h"
#include "ui/widgets/Animations.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/IconButton.h"
#include "ui/widgets/Icons.h"
#include "ui/widgets/SmoothScrollArea.h"
#include "ui/widgets/TextInputDialog.h"
#include "ui/widgets/Toast.h"

#include <QDir>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSettings>
#include <QVBoxLayout>

namespace th = bcg::theme;
using bcg::Exclusion;

namespace {

constexpr const char* kKeyExclusions = "exclusions";

struct TypeInfo {
    const char* icon;
    const char* label;
};

TypeInfo typeInfo(Exclusion::Type type)
{
    switch (type) {
    case Exclusion::File: return {"file", "Файл"};
    case Exclusion::Folder: return {"folder", "Папка"};
    case Exclusion::Extension: return {"hash", "Расширение"};
    case Exclusion::Mask: return {"scan", "Маска"};
    }
    return {"file", ""};
}

QString display(const Exclusion& e)
{
    switch (e.type) {
    case Exclusion::File:
    case Exclusion::Folder: return QDir::toNativeSeparators(e.value);
    case Exclusion::Extension: return QStringLiteral("*.") + e.value;
    case Exclusion::Mask: return e.value;
    }
    return e.value;
}

} // namespace

void ExclusionsPage::load(bcg::ExclusionList& list)
{
    list.setItems(bcg::ExclusionList::fromStrings(
        QSettings().value(kKeyExclusions).toStringList()));
}

void ExclusionsPage::save() const
{
    QSettings().setValue(kKeyExclusions, m_list->toStrings());
}

ExclusionsPage::ExclusionsPage(std::shared_ptr<bcg::ExclusionList> list, QWidget* parent)
    : QWidget(parent)
    , m_list(std::move(list))
{
    setObjectName(QStringLiteral("page"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Исключения"));
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);
    auto* subtitle = new QLabel(tr("Всё, что указано здесь, антивирус обходит: не проверяет, "
                                   "не показывает как угрозу и не удаляет."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    struct Def {
        const char* text;
        const char* icon;
        void (ExclusionsPage::*slot)();
    };
    const Def defs[] = {
        {"Файл", "file", &ExclusionsPage::addFile},
        {"Папка", "folder", &ExclusionsPage::addFolder},
        {"Расширение", "hash", &ExclusionsPage::addExtension},
        {"Маска", "scan", &ExclusionsPage::addMask},
    };
    for (const Def& d : defs) {
        auto* b = new FancyButton(QString::fromUtf8(d.text), QStringLiteral("plus"),
                                  FancyButton::Secondary);
        connect(b, &QPushButton::clicked, this, d.slot);
        buttons->addWidget(b);
    }
    buttons->addStretch();
    m_count = new QLabel;
    m_count->setObjectName(QStringLiteral("muted"));
    buttons->addWidget(m_count);
    root->addLayout(buttons);

    auto* scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("scrollBody"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 6, 0);
    contentLayout->setSpacing(8);
    m_rows = new QVBoxLayout;
    m_rows->setSpacing(8);
    contentLayout->addLayout(m_rows);

    m_empty = new QWidget;
    auto* emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setSpacing(6);
    emptyLayout->addSpacing(40);
    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("exclude"), th::kTrackOff, 72,
                                       devicePixelRatioF()));
    icon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(icon);
    auto* emptyTitle = new QLabel(tr("Исключений нет"));
    emptyTitle->setObjectName(QStringLiteral("rowTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto* emptyHint = new QLabel(tr("Добавьте файл, папку, расширение или маску, например *.log."));
    emptyHint->setObjectName(QStringLiteral("muted"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyHint);
    contentLayout->addWidget(m_empty);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    for (const Exclusion& e : m_list->items())
        m_rows->addWidget(makeRow(e));
    refreshEmptyState();
}

void ExclusionsPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshEmptyState();
}

void ExclusionsPage::refreshEmptyState()
{
    const int n = m_list->items().size();
    m_empty->setVisible(n == 0);
    m_count->setText(n == 0 ? QString() : tr("Записей: %1").arg(n));
}

QWidget* ExclusionsPage::makeRow(const Exclusion& item)
{
    const TypeInfo info = typeInfo(item.type);
    auto* row = new QFrame;
    row->setObjectName(QStringLiteral("tile"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 6, 8, 6);
    layout->setSpacing(12);

    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(QString::fromLatin1(info.icon), th::kAccent, 22,
                                       devicePixelRatioF()));
    layout->addWidget(icon);

    auto* text = new QLabel(display(item));
    text->setToolTip(display(item));
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setMinimumWidth(40);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(text, 1);

    auto* tag = new QLabel(QString::fromUtf8(info.label));
    tag->setObjectName(QStringLiteral("badge"));
    layout->addWidget(tag);

    auto* remove = new IconButton(QStringLiteral("trash"), tr("Убрать из исключений"));
    layout->addWidget(remove);
    connect(remove, &QAbstractButton::clicked, this,
            [this, row, item] { removeRow(row, item); });
    return row;
}

void ExclusionsPage::add(const Exclusion& raw)
{
    Exclusion item = raw;
    item.value = bcg::ExclusionList::normalizedValue(item.type, item.value);
    if (item.value.isEmpty())
        return;

    // Excluding a whole drive switches protection off for it: ask first.
    if (item.type == Exclusion::Folder && item.value.size() <= 3
        && !ConfirmDialog::ask(window(), tr("Исключить весь диск?"),
                               tr("Содержимое диска %1 не будет проверяться вообще. "
                                  "Это сильно снижает защиту.")
                                   .arg(QDir::toNativeSeparators(item.value)),
                               tr("Исключить"), true))
        return;

    if (!m_list->add(item)) {
        Toast::show(window(), tr("Это исключение уже есть в списке"), Toast::Kind::Info);
        return;
    }
    save();
    QWidget* row = makeRow(item);
    m_rows->addWidget(row);
    bcg::fadeIn(row, 320);
    refreshEmptyState();
    Toast::show(window(), tr("Добавлено в исключения"), Toast::Kind::Success);
}

void ExclusionsPage::removeRow(QWidget* row, const Exclusion& item)
{
    m_list->remove(item);
    save();
    row->setEnabled(false);

    // Fade out, then collapse the gap so the rows below glide up.
    auto* effect = new QGraphicsOpacityEffect(row);
    row->setGraphicsEffect(effect);
    auto* group = new QParallelAnimationGroup(row);
    auto* fade = new QPropertyAnimation(effect, "opacity", group);
    fade->setDuration(200);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    auto* collapse = new QPropertyAnimation(row, "maximumHeight", group);
    collapse->setDuration(280);
    collapse->setStartValue(row->height());
    collapse->setEndValue(0);
    collapse->setEasingCurve(QEasingCurve::InOutCubic);
    group->addAnimation(fade);
    group->addAnimation(collapse);
    QPointer<ExclusionsPage> self(this);
    connect(group, &QAbstractAnimation::finished, row, [self, row] {
        row->deleteLater();
        if (self)
            self->refreshEmptyState();
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);

    const int n = m_list->items().size();
    m_count->setText(n == 0 ? QString() : tr("Записей: %1").arg(n));
}

void ExclusionsPage::addFile()
{
    const QStringList files = QFileDialog::getOpenFileNames(window(), tr("Выберите файлы"));
    for (const QString& f : files)
        add({Exclusion::File, f});
}

void ExclusionsPage::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(window(), tr("Выберите папку"));
    if (!dir.isEmpty())
        add({Exclusion::Folder, dir});
}

void ExclusionsPage::addExtension()
{
    const QString ext = TextInputDialog::getText(
        window(), tr("Исключить расширение"),
        tr("Файлы с этим расширением проверяться не будут."), QStringLiteral("log"));
    if (!ext.isEmpty())
        add({Exclusion::Extension, ext});
}

void ExclusionsPage::addMask()
{
    const QString mask = TextInputDialog::getText(
        window(), tr("Исключить по маске"),
        tr("Символ * заменяет любой текст, ? один символ. Маска без слэшей сравнивается с "
           "именем файла или папки, например node_modules или backup_*.zip."),
        QStringLiteral("backup_*.zip"));
    if (!mask.isEmpty())
        add({Exclusion::Mask, mask});
}
