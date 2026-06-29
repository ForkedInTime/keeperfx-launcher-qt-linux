#include "downloadmapeditordialog.h"
#include "apiclient.h"
#include "archiver.h"
#include "downloader.h"
#include "ui_downloadmapeditordialog.h"
#include "extractor.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QMainWindow>
#include <QMessageBox>
#include <QScrollBar>
#include <QThreadPool>

DownloadMapEditorDialog::DownloadMapEditorDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DownloadMapEditorDialog)
{
    ui->setupUi(this);

    // Turn this dialog into a normal window
    setWindowFlags(Qt::Window
                   | Qt::WindowTitleHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint
                   | Qt::WindowStaysOnTopHint // Always on top
        );

    // Fixed size, portable across Wayland/X11/Windows
    setFixedSize(size());

    // Move to center of primary screen
    QRect geometry = QGuiApplication::primaryScreen()->geometry();
    move(geometry.left() + (geometry.width() - width()) / 2, geometry.top() + (geometry.height() - height()) / 2 - 75); // move 75 pixels up

    // Bring to foreground
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();

    // Setup signals and slots
    connect(this, &DownloadMapEditorDialog::appendLog, this, &DownloadMapEditorDialog::onAppendLog);
    connect(this, &DownloadMapEditorDialog::clearProgressBar, this, &DownloadMapEditorDialog::onClearProgressBar);
    connect(this, &DownloadMapEditorDialog::setDownloadFailed, this, &DownloadMapEditorDialog::onDownloadFailed);

    connect(this, &DownloadMapEditorDialog::updateProgressBar, ui->progressBar, &QProgressBar::setValue);
    connect(this, &DownloadMapEditorDialog::setProgressMaximum, ui->progressBar, &QProgressBar::setMaximum);
    connect(this, &DownloadMapEditorDialog::setProgressBarFormat, ui->progressBar, &QProgressBar::setFormat);
}

DownloadMapEditorDialog::~DownloadMapEditorDialog()
{
    delete ui;
}

void DownloadMapEditorDialog::on_cancelButton_clicked()
{
    this->close();
}

void DownloadMapEditorDialog::on_downloadButton_clicked()
{
    // Change GUI
    ui->downloadButton->setDisabled(true);
    ui->progressBar->setTextVisible(true);

    // Start download
    startDownload();
}

void DownloadMapEditorDialog::startDownload()
{
    // Get download URL
    emit appendLog("Getting download URL for the map editor");
    this->downloadUrl = ApiClient::getDownloadUrlMapEditor();
    if (downloadUrl.isEmpty()) {
        emit appendLog("Failed to get download URL for the map editor");
        emit setDownloadFailed(tr("Failed to get download URL for the map editor", "Failure Message"));
        return;
    }

    // Show download URL to end user
    emit appendLog(QString("Map editor URL: %1").arg(downloadUrl.toString()));

    // Make sure file is a zip archive
    if (downloadUrl.toString().endsWith(".zip", Qt::CaseInsensitive) == false) {
        emit appendLog("Invalid map editor file extension.");
        emit setDownloadFailed(tr("Invalid map editor file extension. It must be a zip archive.", "Failure Message"));
        return;
    }

    QString outputFilePath = QCoreApplication::applicationDirPath() + "/" + downloadUrl.fileName() + ".tmp";
    QFile *outputFile = new QFile(outputFilePath);

    Downloader *downloader = new Downloader(this);
    connect(downloader, &Downloader::downloadProgress, this, &DownloadMapEditorDialog::updateProgressBarDownload);
    connect(downloader, &Downloader::downloadCompleted, this, &DownloadMapEditorDialog::onDownloadFinished);

    downloader->download(downloadUrl, outputFile);
}

void DownloadMapEditorDialog::onDownloadFinished(bool success)
{
    if (!success) {
        emit appendLog("Failed to download the map editor archive");
        emit setDownloadFailed(tr("Failed to download the map editor archive", "Failure Message"));
        return;
    }

    emit appendLog("Map editor archive successfully downloaded");
    emit clearProgressBar();

    QFile *outputFile = new QFile(QCoreApplication::applicationDirPath() + "/" + this->downloadUrl.fileName() + ".tmp");

    // Test archive
    emit appendLog("Testing map editor archive...");
    QThreadPool::globalInstance()->start([this, outputFile]() {
        uint64_t archiveSize = Archiver::testArchiveAndGetSize(outputFile);
        QMetaObject::invokeMethod(this, "onArchiveTestComplete", Qt::QueuedConnection, Q_ARG(uint64_t, archiveSize));
    });
}

void DownloadMapEditorDialog::onArchiveTestComplete(uint64_t archiveSize)
{
    // Make sure test is successful and archive size is valid
    if (archiveSize < 0) {
        emit appendLog("Map editor archive test failed");
        emit setDownloadFailed(tr("Map editor archive test failed. It may be corrupted.", "Failure message"));
        return;
    }

    // Get size
    double archiveSizeInMiB = static_cast<double>(archiveSize) / (1024 * 1024);
    QString archiveSizeString = QString::number(archiveSizeInMiB, 'f',
                                                2); // Format to 2 decimal places
    emit appendLog(QString("Total size: %1MiB").arg(archiveSizeString));

    // Start extraction process
    emit setProgressMaximum(static_cast<int>(archiveSize));
    emit setProgressBarFormat(tr("Extracting: %p%", "Progress bar"));
    emit appendLog("Extracting...");

    // TODO: use temp file
    QFile *outputFile = new QFile(QCoreApplication::applicationDirPath() + "/" + downloadUrl.fileName() + ".tmp");

    Extractor *extractor = new Extractor(this);
    connect(extractor, &Extractor::progress, this, &DownloadMapEditorDialog::updateProgressBar);
    connect(extractor, &Extractor::extractComplete, this, &DownloadMapEditorDialog::onExtractComplete);
    connect(extractor, &Extractor::extractFailed, this, &DownloadMapEditorDialog::setDownloadFailed);

    extractor->extract(outputFile, QApplication::applicationDirPath());
}

void DownloadMapEditorDialog::onExtractComplete()
{
    emit appendLog("Extraction completed");
    emit clearProgressBar();

    // Remove temp archive
    emit appendLog("Removing temporary archive");
    QFile *archiveFile = new QFile(QCoreApplication::applicationDirPath() + "/" + downloadUrl.fileName() + ".tmp");
    if (archiveFile->exists()) {
        archiveFile->remove();
    }

    // NOTE: we deliberately do NOT create a keeperfx.exe symlink here. Unearth's
    // "Save & Play" would only run it through Wine (it assumes a Windows binary),
    // which fails against the native Linux build — and a keeperfx.exe in the install
    // dir confuses the launcher's own version detection. Map editing/saving works
    // fine without it; maps are tested via KeeperFX's own Play. (Deferred: native
    // Save & Play integration.)

    // Done!
    emit appendLog("Done!");
    QMessageBox::information(this, "KeeperFX", tr("The Unearth map editor has been installed.", "MessageBox Text"));
    accept();
}

void DownloadMapEditorDialog::updateProgressBarDownload(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        ui->progressBar->setMaximum(static_cast<int>(bytesTotal / 1024 / 1024));
        ui->progressBar->setValue(static_cast<int>(bytesReceived / 1024 / 1024));
        ui->progressBar->setFormat(tr("Downloading: %p% (%vMiB)", "Progress bar"));
    }
}

void DownloadMapEditorDialog::onAppendLog(const QString &string)
{
    // Log to debug output
    qDebug() << "Download map editor log:" << string;

    // Set the cursor to the end
    ui->logTextArea->moveCursor(QTextCursor::End);

    // Add string to log with timestamp
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestampString = currentDateTime.toString("HH:mm:ss");
    ui->logTextArea->insertPlainText("[" + timestampString + "] " + string + "\n");

    // Scroll to the left on new text
    QScrollBar *hScrollBar = ui->logTextArea->horizontalScrollBar();
    if (hScrollBar) {
        hScrollBar->setValue(hScrollBar->minimum());
    }

    // Scroll to the bottom on new text
    QScrollBar *vScrollBar = ui->logTextArea->verticalScrollBar();
    if (vScrollBar) {
        vScrollBar->setValue(vScrollBar->maximum());
    }

    // Force redraw
    QApplication::processEvents();
}

void DownloadMapEditorDialog::onClearProgressBar()
{
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(1);
    ui->progressBar->setFormat("");
}

void DownloadMapEditorDialog::onDownloadFailed(const QString &reason)
{
    ui->downloadButton->setDisabled(false);
    onClearProgressBar();
    QMessageBox::warning(this, tr("Download failed", "MessageBox Title"), reason);
}

void DownloadMapEditorDialog::closeEvent(QCloseEvent *event)
{
    // Ask if user is sure
    int result = QMessageBox::question(this,
                                       tr("Confirmation", "MessageBox Title"),
                                       tr("Are you sure you want to cancel?", "MessageBox Text"));

    // Handle answer
    if (result == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}
