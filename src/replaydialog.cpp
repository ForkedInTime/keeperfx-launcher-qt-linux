// The replays dialog, from the team's launcher (69cf8fe, f438ddd, 19ace8c, 9617a34, 5ba3488).
// This edition's change: the list comes from ReplayFiles::list (src/replayfiles.h, tested on its own).
#include "helper.h"
#include "replaydialog.h"
#include "replayfiles.h"
#include "ui_replaydialog.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QLocale>
#include <QHeaderView>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QTimer>
#include <QMenu>
#include <QShortcut>
#include <QKeySequence>

#define REPLAY_FILE_EXTENSION "fxpkt"

// Helper class to allow correct numeric/date sorting in QTableWidget
class SortableTableWidgetItem : public QTableWidgetItem {
        public:
    SortableTableWidgetItem(const QString &text, const QVariant &sortValue)
        : QTableWidgetItem(text), m_sortValue(sortValue) {}

    bool operator<(const QTableWidgetItem &other) const override {
        return m_sortValue.toDouble() < static_cast<const SortableTableWidgetItem*>(&other)->m_sortValue.toDouble();
    }
        private:
    QVariant m_sortValue;
};

// Helper class to draw the .fxpkt extension with a muted color
class MutedExtensionDelegate : public QStyledItemDelegate {
        public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QString fileExtension = REPLAY_FILE_EXTENSION;
        QString fileExtensionWithDot = "." + fileExtension;

        QString text = opt.text;
        if (text.endsWith(fileExtensionWithDot, Qt::CaseInsensitive)) {
            // Temporarily clear text so the default painter only draws the background/selection color
            opt.text = QString();
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

            // Get the rectangle where the text should normally be drawn
            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

            QString baseName = text.chopped(fileExtension.length() + 1);

            painter->save();

            // Determine base color based on whether the row is currently selected
            QColor textColor = (opt.state & QStyle::State_Selected) ?
                                   opt.palette.color(QPalette::HighlightedText) :
                                   opt.palette.color(QPalette::Text);

            // Draw base name normally
            painter->setPen(textColor);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, baseName);

            // Move drawing position to the right by the width of the base name
            int baseWidth = opt.fontMetrics.horizontalAdvance(baseName);
            QRect extRect = textRect.adjusted(baseWidth, 0, 0, 0);

            // Mute the color (50% transparency) and draw the extension
            QColor mutedColor = textColor;
            mutedColor.setAlphaF(0.3);
            painter->setPen(mutedColor);
            painter->drawText(extRect, Qt::AlignLeft | Qt::AlignVCenter, fileExtensionWithDot);

            painter->restore();
        } else {

            // Fallback for normal text (just in case)
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

ReplayDialog::ReplayDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReplayDialog)
{
    ui->setupUi(this);

    // Setup table columns
    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setHorizontalHeaderLabels({
        tr("Type", "Replay Table Column Header"),
        tr("Name", "Replay Table Column Header"),
        tr("Size", "Replay Table Column Header"),
        tr("Date", "Replay Table Column Header")
    });

    // Set selection and edit behavior
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Hide the vertical header (row numbers)
    ui->tableWidget->verticalHeader()->setVisible(false);

    // Slightly taller rows
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(32);

    // Set resize modes for the columns
    // 'Name' expands fully
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    // Apply custom painter for the 'Name' column that mutes the file extension
    ui->tableWidget->setItemDelegateForColumn(1, new MutedExtensionDelegate(this));

    // Temporary disable sorting while populating
    ui->tableWidget->setSortingEnabled(false);

    // The game folder: the engine runs there (Game sets it as the working directory), so a path
    // relative to it is what -packetload takes. ReplayFiles (src/replayfiles.h) finds the replays.
    const QDir gameDir(QCoreApplication::applicationDirPath());
    const QList<ReplayFiles::Entry> replays = ReplayFiles::list(gameDir);

    // Populate the table
    ui->tableWidget->setRowCount(replays.size());
    for (int i = 0; i < replays.size(); ++i) {
        const ReplayFiles::Entry &replay = replays.at(i);

        // The folder the engine saved it in says what kind of game it was; a mod's own folder keeps
        // its name; a replay directly in replays/ or the game folder is 'Unknown'
        QString typeName = tr("Unknown", "Replay Type");
        const QString folder = replay.folder.toLower();
        if (folder == "freeplay") {
            typeName = tr("Freeplay", "Replay Type");
        } else if (folder == "campaign") {
            typeName = tr("Campaign", "Replay Type");
        } else if (folder == "multiplayer") {
            typeName = tr("Multiplayer", "Replay Type");
        } else if (!replay.folder.isEmpty()) {
            typeName = replay.folder;
        }

        // Column 0: Type
        QTableWidgetItem *typeItem = new QTableWidgetItem(typeName);
        ui->tableWidget->setItem(i, 0, typeItem);

        // Column 1: Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(QFileInfo(replay.relativePath).fileName());
        ui->tableWidget->setItem(i, 1, nameItem);

        // Column 2: Size
        qint64 sizeInKiB = replay.size / 1024;
        QString sizeString = tr("%1 KiB", "Replay Filesize").arg(sizeInKiB);
        SortableTableWidgetItem *sizeItem = new SortableTableWidgetItem(sizeString, sizeInKiB);
        ui->tableWidget->setItem(i, 2, sizeItem);

        // Column 3: Date
        QString dateString = QLocale().toString(replay.modified, QLocale::ShortFormat);
        SortableTableWidgetItem *dateItem = new SortableTableWidgetItem(dateString, replay.modified.toMSecsSinceEpoch());
        ui->tableWidget->setItem(i, 3, dateItem);

        // Store hidden data in the cells
        typeItem->setData(Qt::UserRole, replay.relativePath);                   // Column 0 -> relative path (starts the replay)
        nameItem->setData(Qt::UserRole, gameDir.filePath(replay.relativePath)); // Column 1 -> absolute path (copies the file)
    }

    // Re-enable sorting and sort by Date (Column 3) Descending (newest on top)
    ui->tableWidget->setSortingEnabled(true);
    ui->tableWidget->sortItems(3, Qt::DescendingOrder);

    // Nothing to list: say so, instead of an empty table under "These are your stored replays"
    if (replays.isEmpty()) {
        ui->infoLabel->setText(tr("No replays yet. Replays of your games appear here once you have played.", "Information Label"));
    }

    // Ensure nothing is selected upon loading
    ui->tableWidget->clearSelection();
    ui->tableWidget->setCurrentItem(nullptr);

    // Disable start and copy button at start
    ui->startButton->setDisabled(true);
    ui->copyButton->setDisabled(true);

    // Make selecting a replay enable the buttons at the bottom
    connect(ui->tableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ReplayDialog::updateButtons);

    // Add a right click context menu to the table
    ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, this, &ReplayDialog::showContextMenu);

    // Setup Ctrl+C shortcut to trigger the copy file logic
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, [this]() {
        if (ui->copyButton->isEnabled()) {
            on_copyButton_clicked();
        }
    });
}

ReplayDialog::~ReplayDialog()
{
    delete ui;
}

QString ReplayDialog::getReplayFileName()
{
    return this->replayFileName;
}

void ReplayDialog::updateButtons()
{
    bool hasSelection = ui->tableWidget->selectionModel()->hasSelection();
    ui->startButton->setEnabled(hasSelection);
    ui->copyButton->setEnabled(hasSelection);
}

void ReplayDialog::on_cancelButton_clicked()
{
    qDebug() << "Closing dialog";
    this->close();
}

void ReplayDialog::on_startButton_clicked()
{
    qDebug() << "Start button clicked";

    // Get selection
    QModelIndexList selected = ui->tableWidget->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        qDebug() << "Nothing was selected";
        return;
    }

    // Look at Column 0 (Type) where the path is stored
    int row = selected.first().row();
    QTableWidgetItem *typeItem = ui->tableWidget->item(row, 0);

    // Retrieve the exact app-relative path format requested
    QString replayFileNameString = typeItem->data(Qt::UserRole).toString();
    qDebug() << "Selected replay to start:" << replayFileNameString;

    // Remember replay filename so we can use it from the calling window
    this->replayFileName = replayFileNameString;

    // Accept dialog
    this->accept();
}

void ReplayDialog::on_copyButton_clicked()
{
    qDebug() << "Copy button clicked";

    // Get selection
    QModelIndexList selected = ui->tableWidget->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        qDebug() << "Nothing was selected";
        return;
    }

    // Look at Column 1 (Name) where the path is stored
    int row = selected.first().row();
    QTableWidgetItem *nameItem = ui->tableWidget->item(row, 1);

    // Retrieve the absolute path format requested
    QString replayFilePathString = nameItem->data(Qt::UserRole).toString();
    qDebug() << "Selected replay to copy:" << replayFilePathString;

    bool copySuccess = Helper::copyFileToClipboard(replayFilePathString);

    if(copySuccess){

        // Remember the original text and size
        QString originalText = ui->copyButton->text();
        ui->copyButton->setMinimumWidth(ui->copyButton->width());

        // Change button state
        ui->copyButton->setText(tr("Copied!", "Button State Text"));
        ui->copyButton->setEnabled(false);

        // 3. Create a one-shot timer to restore the button after 2000 ms (2 seconds)
        // Passing 'this' as the second argument ensures that if the dialog is closed
        // before the 2 seconds are up, the timer is safely canceled.
        QTimer::singleShot(2000, this, [this, originalText]() {
            ui->copyButton->setText(originalText);
            ui->copyButton->setEnabled(true);
        });
    } else {
        qDebug() << "Failed to copy to clipboard (file missing or clipboard error).";
    }
}

void ReplayDialog::showContextMenu(const QPoint &pos)
{
    // Find the item under the mouse cursor
    QTableWidgetItem *item = ui->tableWidget->itemAt(pos);

    // If the user right-clicked on empty space, don't show the menu
    if (!item) {
        return;
    }

    // Crucial: Select the right-clicked row.
    // This ensures your existing button slots process the correct file.
    ui->tableWidget->selectRow(item->row());

    // Create the context menu
    QMenu contextMenu(this);

    // Create actions, fetching the translated text directly from the buttons
    QAction *startAction = new QAction(ui->startButton->text(), &contextMenu);
    QAction *copyAction = new QAction(ui->copyButton->text(), &contextMenu);

    // Sync the enabled state (so if copy is disabled for 2s due to "Copied!", the menu matches)
    startAction->setEnabled(ui->startButton->isEnabled());
    copyAction->setEnabled(ui->copyButton->isEnabled());

    // Connect the menu actions directly to your existing button slots
    connect(startAction, &QAction::triggered, this, &ReplayDialog::on_startButton_clicked);
    connect(copyAction, &QAction::triggered, this, &ReplayDialog::on_copyButton_clicked);

    // Add actions to the menu
    contextMenu.addAction(startAction);
    contextMenu.addAction(copyAction);

    // Show the menu at the global position of the cursor
    contextMenu.exec(ui->tableWidget->viewport()->mapToGlobal(pos));
}
