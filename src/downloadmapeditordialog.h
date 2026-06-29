#pragma once

#include <QDialog>
#include <QUrl>

namespace Ui { class DownloadMapEditorDialog; }

class DownloadMapEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadMapEditorDialog(QWidget *parent = nullptr);
    ~DownloadMapEditorDialog();

private slots:
    void on_cancelButton_clicked();
    void on_downloadButton_clicked();

    void onAppendLog(const QString &string);
    void onDownloadFailed(const QString &reason);
    void onClearProgressBar();
    void updateProgressBarDownload(qint64 bytesReceived, qint64 bytesTotal);

    void onDownloadFinished(bool success);
    void onArchiveTestComplete(uint64_t archiveSize);
    void onExtractComplete();

signals:
    void appendLog(const QString &string);
    void setDownloadFailed(const QString &reason);
    void clearProgressBar();
    void updateProgressBar(int value);
    void setProgressMaximum(int value);
    void setProgressBarFormat(QString format);

private:
    Ui::DownloadMapEditorDialog *ui;
    void closeEvent(QCloseEvent *event) override;

    QUrl downloadUrl;

    void startDownload();
    void testAndExtractArchive();
};
