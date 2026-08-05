#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class Downloader : public QObject {
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);
    ~Downloader();

    void download(const QUrl &url, QFile *localFileOutput);

    // Why the last download failed, empty if it succeeded. The reason was only
    // ever written to the log, so callers could report nothing beyond "it did not
    // work" -- a 404 from a bad URL and a full disk looked identical to the user.
    QString lastError() const { return lastErrorString; }

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(bool success);

public slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onReadyRead();
    void onFinished();

private:
    QNetworkAccessManager *manager;
    QNetworkReply *reply;
    QFile *localFileOutput;

    QString lastErrorString;

    qint64 bytesWritten = 0;
    qint64 bytesTotal   = -1;
};
