#include "settingsfit.h"

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVariant>

namespace {

// The columns of a 'responsive_' container (the frames its layout holds), each form with its labels and
// fields side by side, as they are measured.
QList<QWidget *> responsiveColumns(QWidget *container)
{
    QList<QWidget *> columns;
    if (QLayout *layout = container->layout()) {
        for (int i = 0; i < layout->count(); ++i) {
            if (QWidget *column = layout->itemAt(i)->widget()) {
                if (auto *form = qobject_cast<QFormLayout *>(column->layout())) {
                    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
                }
                columns.append(column);
            }
        }
    }
    return columns;
}

// The width a container needs for its columns side by side. They share it evenly (the team's
// layout), so every column gets as much as the widest asks for.
int sideBySideWidth(QWidget *container, const QList<QWidget *> &columns)
{
    QLayout *layout = container->layout();
    int widest = 0;
    for (QWidget *column : columns) {
        widest = qMax(widest, column->sizeHint().width());
    }
    const int count = int(columns.size());
    return layout->contentsMargins().left() + layout->contentsMargins().right() + count * widest
        + qMax(0, count - 1) * qMax(0, layout->spacing());
}

QScrollArea *responsiveScrollArea(QWidget *container)
{
    QWidget *w = container->parentWidget();
    while (w && !qobject_cast<QScrollArea *>(w)) {
        w = w->parentWidget();
    }
    return qobject_cast<QScrollArea *>(w);
}

// The width of a scroll area's page with a vertical scroll bar showing
int usablePageWidth(QScrollArea *scroll)
{
    return scroll->maximumViewportSize().width() - scroll->verticalScrollBar()->sizeHint().width();
}

// The layout margins between a scroll area's page and a container inside it
int marginsAround(QWidget *container)
{
    QScrollArea *scroll = responsiveScrollArea(container);
    int margins = 0;
    for (QWidget *w = container; scroll && w && w != scroll->widget(); w = w->parentWidget()) {
        QWidget *parent = w->parentWidget();
        if (parent && parent->layout()) {
            margins += parent->layout()->contentsMargins().left() + parent->layout()->contentsMargins().right();
        }
    }
    return margins;
}

QList<QWidget *> responsiveContainers(QWidget *window)
{
    return window->findChildren<QWidget *>(QRegularExpression("^responsive_.*"));
}

} // namespace

namespace SettingsFit {

void fitControls(QWidget *window)
{
    for (QComboBox *comboBox : window->findChildren<QComboBox *>()) {
        fitComboBoxToEntries(comboBox);
    }
    // A number field as wide as its largest number, not a text field's width: the columns can then
    // stand side by side in a window of a sensible width
    for (QLineEdit *edit : window->findChildren<QLineEdit *>()) {
        if (auto *validator = qobject_cast<const QIntValidator *>(edit->validator())) {
            const int digits = int(QString::number(validator->top()).size()) + 1;
            const int frame = 2 * edit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, edit);
            edit->setMaximumWidth(edit->fontMetrics().horizontalAdvance(QString(digits, QLatin1Char('0'))) + frame + 12);
        }
    }
}

// A dropdown as wide as its longest entry, now and whenever its entries change. Qt sizes a dropdown's
// minimum once, while it is still empty, so a form short of room would squeeze it below its text.
void fitComboBoxToEntries(QComboBox *comboBox)
{
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    auto fit = [comboBox]() { comboBox->setMinimumWidth(comboBox->sizeHint().width()); };
    fit();
    QObject::connect(comboBox->model(), &QAbstractItemModel::rowsInserted, comboBox, fit);
    QObject::connect(comboBox->model(), &QAbstractItemModel::rowsRemoved, comboBox, fit);
    QObject::connect(comboBox->model(), &QAbstractItemModel::dataChanged, comboBox, fit);
}

void hideRow(QWidget *field)
{
    auto *form = qobject_cast<QFormLayout *>(field->parentWidget() ? field->parentWidget()->layout() : nullptr);
    if (form && form->indexOf(field) >= 0) {
        form->setRowVisible(field, false);
    } else {
        field->setVisible(false);
    }
}

// Every 'responsive_' container decides for itself, from what its columns ask for and the width a
// tab's scroll area has. A form still too wide for its column (a long translation at a large font)
// puts each label above its field.
void updateColumns(QWidget *window, const SwitchLayout &switchLayout)
{
    const QList<QWidget *> containers = responsiveContainers(window);

    // Every tab's page is as wide as the open one's (a tab not open has not been laid out at the
    // window's size yet), so all tabs are decided from the open tab's scroll area.
    int pageWidth = -1;
    for (QWidget *container : containers) {
        if (QScrollArea *scroll = responsiveScrollArea(container); scroll && container->isVisibleTo(window)) {
            pageWidth = usablePageWidth(scroll);
            break;
        }
    }

    for (QWidget *container : containers) {
        QBoxLayout *layout = qobject_cast<QBoxLayout *>(container->layout());
        if (!layout || (pageWidth < 0 && !container->isVisibleTo(window))) {
            continue;
        }
        const QList<QWidget *> columns = responsiveColumns(container);
        QScrollArea *scroll = responsiveScrollArea(container);
        const int room = (pageWidth >= 0 ? pageWidth : scroll ? usablePageWidth(scroll) : container->width())
            - (scroll ? marginsAround(container) : 0);
        const bool useHorizontal = sideBySideWidth(container, columns) <= room;

        const QVariant current = container->property("responsiveHorizontal");
        if (!current.isValid() || current.toBool() != useHorizontal) {
            switchLayout(container, useHorizontal);
            container->setProperty("responsiveHorizontal", useHorizontal);
        }

        QLayout *now = container->layout();
        const int margins = now->contentsMargins().left() + now->contentsMargins().right();
        const int count = int(columns.size());
        const int gaps = qMax(0, count - 1) * qMax(0, now->spacing());
        const int columnWidth = useHorizontal ? (room - margins - gaps) / qMax(1, count) : room - margins;
        for (QWidget *column : columns) {
            if (auto *form = qobject_cast<QFormLayout *>(column->layout())) {
                form->setRowWrapPolicy(column->sizeHint().width() > columnWidth ? QFormLayout::WrapAllRows : QFormLayout::DontWrapRows);
            }
            // Never narrower than its content allows: the team's columns ignore their size hints, so
            // a tight layout would otherwise cut them
            column->setMinimumWidth(column->minimumSizeHint().width());
        }
    }
}

// Open wide enough for the tab bar, so no tab hides behind scroll arrows (a long translation's tabs
// need more than the default width), and for every tab's two columns side by side, and tall enough
// for the tallest tab, where the screen allows; never larger than the screen, centred over the
// launcher. On a smaller screen a tab shows one column and scrolls.
void fitWindow(QWidget *window, QTabWidget *tabs, const SwitchLayout &switchLayout)
{
    // Tabs not opened yet are not styled yet, and an unstyled control reports a smaller size; their
    // layouts may still hold sizes from before their controls were filled
    for (QWidget *w : window->findChildren<QWidget *>()) {
        w->ensurePolished();
    }
    for (QLayout *layout : window->findChildren<QLayout *>()) {
        layout->invalidate();
    }
    QSize wanted = window->size();
    wanted.rwidth() += qMax(0, tabs->tabBar()->sizeHint().width() - tabs->width());

    // What the window takes around a tab's page (margins, frames, a scroll bar), from the open tab
    const QList<QWidget *> containers = responsiveContainers(window);
    int chrome = -1;
    for (QWidget *container : containers) {
        if (QScrollArea *scroll = responsiveScrollArea(container); scroll && container->isVisibleTo(window)) {
            chrome = window->width() - usablePageWidth(scroll);
            break;
        }
    }
    if (chrome >= 0) {
        for (QWidget *container : containers) {
            if (container->layout()) {
                const int need = sideBySideWidth(container, responsiveColumns(container)) + marginsAround(container);
                wanted.setWidth(qMax(wanted.width(), need + chrome));
            }
        }
    }

    // The window frame is not known before the window is on screen: leave room for a title bar
    const QSize frame(qMax(window->frameGeometry().width() - window->width(), 16),
                      qMax(window->frameGeometry().height() - window->height(), 48));
    if (QScreen *screen = window->screen()) {
        wanted = wanted.boundedTo(screen->availableGeometry().size() - frame);
    }
    // Tabs that still do not fit (a long translation on a small screen or at a large size) get less
    // padding around their titles before they would need scroll arrows
    const int tabRoom = wanted.width() - (window->width() - tabs->width());
    if (tabs->tabBar()->sizeHint().width() > tabRoom) {
        tabs->setStyleSheet(tabs->styleSheet() + QStringLiteral("\nQTabBar::tab { padding-left: 6px; padding-right: 6px; }"));
        tabs->tabBar()->ensurePolished();
    }
    if (wanted != window->size()) {
        window->resize(wanted);
    }
    // The open tab is laid out at the window's size only now
    updateColumns(window, switchLayout);

    // Tall enough for the tallest tab. A page is as tall as it is at the width it gets (a wrapping
    // label takes more lines when narrow).
    QScrollArea *openScroll = nullptr;
    for (int i = 0; i < tabs->count(); ++i) {
        for (QScrollArea *scroll : tabs->widget(i)->findChildren<QScrollArea *>(Qt::FindDirectChildrenOnly)) {
            if (scroll->widget() && scroll->isVisibleTo(window)) {
                openScroll = scroll;
            }
        }
    }
    if (openScroll) {
        const int pageWidth = openScroll->maximumViewportSize().width();
        int tallest = 0;
        for (int i = 0; i < tabs->count(); ++i) {
            for (QScrollArea *scroll : tabs->widget(i)->findChildren<QScrollArea *>(Qt::FindDirectChildrenOnly)) {
                if (QWidget *page = scroll->widget()) {
                    const int pageHeight = page->hasHeightForWidth() ? page->heightForWidth(pageWidth) : page->sizeHint().height();
                    tallest = qMax(tallest, qMax(pageHeight, page->minimumSizeHint().height()));
                }
            }
        }
        int wantedHeight = qMax(window->height(), tallest + window->height() - openScroll->maximumViewportSize().height());
        if (QScreen *screen = window->screen()) {
            wantedHeight = qMin(wantedHeight, screen->availableGeometry().height() - frame.height());
        }
        if (wantedHeight != window->height()) {
            window->resize(window->width(), wantedHeight);
        }
    }

    // Grown after it was placed: centre it again over the launcher, inside the screen
    if (QScreen *screen = window->screen()) {
        const QRect available = screen->availableGeometry();
        QRect placed(window->pos(), window->size() + frame);
        placed.moveCenter(window->parentWidget() ? window->parentWidget()->window()->frameGeometry().center() : available.center());
        placed.moveLeft(qBound(available.left(), placed.left(), qMax(available.left(), available.right() + 1 - placed.width())));
        placed.moveTop(qBound(available.top(), placed.top(), qMax(available.top(), available.bottom() + 1 - placed.height())));
        window->move(placed.topLeft());
    }
}

} // namespace SettingsFit
