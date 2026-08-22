#pragma once

#include <QDialog>
#include <QDateTime>
#include <QScrollBar>
#include <QDir>
#include <QNetworkAccessManager>

#include "kfxversion.h"
#include "savefile.h"
#include "ui_updatedialog.h"

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(QWidget *parent = nullptr, KfxVersion::VersionInfo versionInfo = KfxVersion::VersionInfo(), bool autoUpdate = false);
    ~UpdateDialog();

private slots:
    void on_updateButton_clicked();
    void on_cancelButton_clicked();

    void onFileDownloadProgress();
    void onArchiveDownloadFinished(bool success);
    void onArchiveTestComplete(int64_t archiveSize);
    void onUpdateComplete();

    void onAppendLog(const QString &string);
    void onUpdateFailed(const QString &reason);
    void onClearProgressBar();
    void updateProgressBar(qint64 bytesReceived, qint64 bytesTotal);

signals:
    void fileDownloadProgress();
    void appendLog(const QString &string);
    void setUpdateFailed(const QString &reason);
    void clearProgressBar();
    void updateProgress(int value);
    void setProgressMaximum(int value);
    void setProgressBarFormat(QString format);

private:
    Ui::UpdateDialog *ui;
    QNetworkAccessManager *networkManager;

    KfxVersion::VersionInfo currentUpdateVersionInfo;
    KfxVersion::VersionInfo nextUpdateVersionInfo;
    bool updateToNewStableFirst = false;

    void closeEvent(QCloseEvent *event) override;

    QStringList updateList;
    void updateUsingFilemap(QMap<QString, QString> fileMap);
    void updateUsingArchive(QString downloadUrl);
    void update();

    QDir tempDir;
    int totalFiles;
    int downloadedFiles;
    void downloadFiles(const QString &baseUrl);

    bool autoUpdate;
    QString originalTitleText;

    void backupSaves(QList<SaveFile *> saveFiles);
    void stashCurrentEngine();
    // Set while downloading an update patch rather than the full payload, so a
    // failed patch download can retry with the full one instead of stranding the
    // player on an optimisation. The patch is a convenience; the full payload is
    // the guarantee.
    bool downloadingPatch = false;
    // Where the archive actually landed. Three places used to rebuild this from
    // currentUpdateVersionInfo.downloadUrl, which names the FULL payload -- so once
    // a patch could be downloaded instead, they tested, extracted and deleted a
    // different file than the one just fetched. With a stale full.7z.tmp lying in
    // the game directory that meant extracting a months-old payload over the
    // install. One value, set where the download starts.
    QString downloadedArchivePath;
};
