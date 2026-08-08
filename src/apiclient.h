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
    // Newest Unearth release tag; empty if it cannot be determined.
    static QString getLatestMapEditorVersion();

    // Workshop browser: the full catalogue (client filters/sorts it) and the
    // download URL for a given workshop item's primary file.
    static QJsonArray getWorkshopCatalog();
    // Our own releases, shaped like keeperfx.net news articles so the news panel
    // can show them beside upstream's. Empty on failure -- never fatal.
    static QJsonArray getTuxEditionNews(int maxItems = 2);
    static QUrl getWorkshopItemDownloadUrl(int itemId);
    // Fallback for items the API publishes no files for; reads the link off the
    // item's own web page. Host-checked before use.
    static QUrl getWorkshopItemDownloadUrlFromWebsite(int itemId);

    static std::optional<QMap<QString, QString>> getGameFileList(KfxVersion::ReleaseType type, QString version);
};
