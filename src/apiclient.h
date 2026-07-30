#pragma once

#include "kfxversion.h"

#include <QUrl>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class ApiClient
{

public:
    enum class HttpMethod { GET, POST };

    static QString getApiEndpoint();

    static QImage downloadImage(QUrl url);

    static QJsonDocument getJsonResponse(QUrl endpointPath, HttpMethod method = HttpMethod::GET, QJsonObject jsonPostObject = QJsonObject());

    static QJsonObject getLatestStable();
    static QJsonObject getLatestAlpha();

    static QUrl getDownloadUrlStable();
    static QUrl getDownloadUrlAlpha();
    static QUrl getDownloadUrlMusic();
    static QUrl getDownloadUrlMapEditor();

    // Workshop browser: the full catalogue (client filters/sorts it) and the
    // download URL for a given workshop item's primary file.
    static QJsonArray getWorkshopCatalog();
    static QUrl getWorkshopItemDownloadUrl(int itemId);

    static std::optional<QMap<QString, QString>> getGameFileList(KfxVersion::ReleaseType type, QString version);
};
