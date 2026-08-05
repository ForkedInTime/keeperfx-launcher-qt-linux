#include "apiclient.h"

#include "kfxversion.h"
#include "launcheroptions.h"

#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequestFactory>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#define API_ENDPOINT "https://keeperfx.net/api"

#ifndef Q_OS_WINDOWS
// keeperfx-linux-alpha: the launcher's "is there a newer version?" check should look at
// OUR GitHub releases, not keeperfx.net. This returns the newest release OF THE REQUESTED
// CHANNEL, shaped like the keeperfx.net response the updater expects:
// { "version": "1.3.2.5200", "download_url": ... }.
//
// The channel matters. This used to request /releases/latest, which is simply the newest
// release of any kind -- so publishing a stable offered it to every alpha user, and the
// next alpha after it offered an alpha to every stable user. The stable channel would
// have lasted exactly until the following alpha release.
//
// Marking alphas as GitHub prereleases would separate them, but prereleases are excluded
// from /releases/latest, which the README's download links, the Flatpak attach step and
// the updater itself all rely on. So the full list is fetched and filtered by tag instead.
static QJsonObject getLatestLinuxAlphaRelease(KfxVersion::ReleaseType wantedType)
{
    QNetworkAccessManager manager;
    // The list endpoint, newest first. per_page is small because only the first
    // match of each channel is ever needed.
    QNetworkRequest req(QUrl("https://api.github.com/repos/ForkedInTime/keeperfx-linux-alpha/releases?per_page=30"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "keeperfx-launcher-qt-linux");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = manager.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Linux update check failed:" << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (doc.isArray() == false) {
        return QJsonObject();
    }

    // GitHub returns releases newest first, so the first tag matching the wanted
    // channel is the one to offer. Drafts and prereleases are skipped: a draft is
    // not published yet, and a prerelease is not part of either channel here.
    const QJsonArray releases = doc.array();
    for (const QJsonValue &r : releases) {
        const QJsonObject root = r.toObject();
        if (root["draft"].toBool() || root["prerelease"].toBool()) {
            continue;
        }

        const QString tag = root["tag_name"].toString();

        // Classified by the same code that classifies the installed version, so
        // the two sides of the comparison can never disagree about what a tag
        // means: "v1.4.0.5409-alpha" is ALPHA, "v1.4.0.5409" is STABLE.
        if (KfxVersion::getVersionFromString(tag).type != wantedType) {
            continue;
        }

        // tag like "v1.3.2.5200-alpha" -> numeric version "1.3.2.5200"
        QRegularExpression re(QStringLiteral("([0-9]+\\.[0-9]+\\.[0-9]+(?:\\.[0-9]+)?)"));
        QRegularExpressionMatch m = re.match(tag);
        if (m.hasMatch() == false) {
            continue;
        }

        // find the complete game package asset for the in-launcher updater
        QString downloadUrl;
        const QJsonArray assets = root["assets"].toArray();
        for (const QJsonValue &a : assets) {
            const QString name = a.toObject()["name"].toString();
            if (name.endsWith("-full.7z")) {
                downloadUrl = a.toObject()["browser_download_url"].toString();
                break;
            }
        }

        // A release with no payload attached cannot be updated to. Keep looking
        // rather than offering an update that would fail to download -- that is
        // exactly what stranded users when a release was published before its
        // assets finished uploading.
        if (downloadUrl.isEmpty()) {
            qWarning() << "Skipping release with no -full.7z asset:" << tag;
            continue;
        }

        QJsonObject out;
        out["version"] = m.captured(1);
        out["download_url"] = downloadUrl;
        return out;
    }

    return QJsonObject();
}
#endif

QString ApiClient::getApiEndpoint()
{
    // Check if custom endpoint is set
    if (LauncherOptions::isSet("api-endpoint")) {
        return LauncherOptions::getValue("api-endpoint");
    }

    // Return default endpoint
    return QString(API_ENDPOINT);
}

QImage ApiClient::downloadImage(QUrl url)
{
    // Disk cache (shared dir with ImageHelper): keep the raw bytes keyed by a URL hash,
    // so re-opening the Workshop loads thumbnails from disk instead of the network. This
    // is what made the browser re-download ~150 images on every open. Thread-safe (QImage /
    // QFile / QCryptographicHash), so it's fine to call from the thumbnail worker threads.
    const bool useCache = !LauncherOptions::isSet("no-image-cache");
    const QString cacheDir = QDir::temp().filePath("kfx-launcher-img-cache");
    const QString cachePath = cacheDir + "/"
        + QString::fromLatin1(
              QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Sha256).toHex().left(16))
        + "_orig";

    if (useCache) {
        QFile cf(cachePath);
        if (cf.open(QIODevice::ReadOnly)) {
            QImage cached;
            if (cached.loadFromData(cf.readAll())) {
                return cached;
            }
        }
    }

    QNetworkAccessManager manager;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "keeperfx-launcher-qt");

    QNetworkReply *reply = manager.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage image;
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        if (image.loadFromData(data) && useCache) {
            QDir().mkpath(cacheDir);
            QFile cf(cachePath);
            if (cf.open(QIODevice::WriteOnly)) {
                cf.write(data);
                cf.close();
            }
        }
    } else {
        qWarning() << "downloadImage failed:" << url.toString() << "->" << reply->errorString();
    }
    reply->deleteLater();
    return image;
}

QJsonDocument ApiClient::getJsonResponse(QUrl endpointPath, HttpMethod method, QJsonObject jsonPostObject)
{
    // Strip '/api' and slashes from the endpoint path
    QString endpointPathString = endpointPath.toString();
    if (endpointPathString.startsWith("/api")) {
        endpointPathString.remove(0, 4);
    }
    if (endpointPathString.startsWith("/")) {
        endpointPathString.remove(0, 1);
    }

    // Create full URL for logging
    QString endpointUrlString = ApiClient::getApiEndpoint() + "/" + endpointPathString;
    qDebug() << "ApiClient:" << (method == HttpMethod::GET ? "GET" : "POST") << endpointUrlString;

    // Setup network manager and API
    QNetworkAccessManager manager;
    QNetworkRequest apiRequest(QUrl(ApiClient::getApiEndpoint() + "/" + endpointPathString));
    apiRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Create the network reply object
    QNetworkReply *reply = nullptr;
    if (method == HttpMethod::GET) {
        reply = manager.get(apiRequest);
    } else if (method == HttpMethod::POST) {
        QJsonDocument jsonPostDoc(jsonPostObject);
        reply = manager.post(apiRequest, jsonPostDoc.toJson());
    }

    // Create an event loop to wait for the request to finish
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();  // Block until the request is finished

    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "ApiClient [ERROR]" << endpointUrlString << "->" << reply->errorString();
        reply->deleteLater();
        return QJsonDocument();  // Return an empty QJsonDocument on error
    }

    // We retrieved something!
    qDebug() << "ApiClient:" << endpointUrlString << "-> Success";

    // Read the response and parse it as JSON
    QByteArray response = reply->readAll();
    reply->deleteLater();

    return QJsonDocument::fromJson(response);
}

QJsonObject ApiClient::getLatestStable(){
#ifndef Q_OS_WINDOWS
    // Native Linux build: check OUR GitHub releases, not keeperfx.net.
    return getLatestLinuxAlphaRelease(KfxVersion::ReleaseType::STABLE);
#else
    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/release/stable/latest");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return QJsonObject();
    }

    // Convert response and return
    QJsonObject jsonObj = jsonDoc.object();
    return jsonObj["release"].toObject();
#endif
}

QJsonObject ApiClient::getLatestAlpha(){
#ifndef Q_OS_WINDOWS
    // Native Linux build: check OUR GitHub releases, not keeperfx.net.
    return getLatestLinuxAlphaRelease(KfxVersion::ReleaseType::ALPHA);
#else
    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/release/alpha/latest");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return QJsonObject();
    }

    // Convert response and return
    QJsonObject jsonObj = jsonDoc.object();
    return jsonObj["alpha_build"].toObject();
#endif
}

// keeperfx-linux-alpha: the complete native-Linux package lives on our GitHub
// releases, not keeperfx.net. GitHub's "latest release" download URL always
// resolves to the newest release's asset, so no API call/parsing is needed.
#ifndef Q_OS_WINDOWS
static const char *KFX_LINUX_ALPHA_PACKAGE_URL =
    "https://github.com/ForkedInTime/keeperfx-linux-alpha/releases/latest/download/"
    "keeperfx-linux-alpha-x86_64-full.7z";
#endif

QUrl ApiClient::getDownloadUrlStable()
{
#ifndef Q_OS_WINDOWS
    // Native Linux build always installs our complete alpha package.
    qDebug() << "Linux alpha package URL:" << KFX_LINUX_ALPHA_PACKAGE_URL;
    return QUrl(KFX_LINUX_ALPHA_PACKAGE_URL);
#else
    // Get JSON object from API
    QJsonObject releaseObj = getLatestStable();
    if(releaseObj.isEmpty()){
        return QUrl();
    }

    // Get download URL
    QString downloadUrlString = releaseObj["download_url"].toString();
    qDebug() << "Stable Download URL:" << downloadUrlString;

    // Return
    return QUrl(downloadUrlString);
#endif
}

QUrl ApiClient::getDownloadUrlAlpha()
{
#ifndef Q_OS_WINDOWS
    // Native Linux build: one package for both channels.
    qDebug() << "Linux alpha package URL:" << KFX_LINUX_ALPHA_PACKAGE_URL;
    return QUrl(KFX_LINUX_ALPHA_PACKAGE_URL);
#else
    // Get JSON object from API
    QJsonObject releaseObj = getLatestAlpha();
    if(releaseObj.isEmpty()){
        return QUrl();
    }

    // Get download URL
    QString downloadUrlString = releaseObj["download_url"].toString();
    qDebug() << "Alpha Download URL:" << downloadUrlString;

    // Return
    return QUrl(downloadUrlString);
#endif
}

QUrl ApiClient::getDownloadUrlMusic()
{
    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/workshop/item/393");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        qWarning() << "Download music URL: Invalid response";
        return QUrl();
    }

    // Convert response
    QJsonObject jsonObj = jsonDoc.object();

    // Get workshop item obj
    QJsonObject workshopItemObj = jsonObj["workshop_item"].toObject();
    if (workshopItemObj.isEmpty()) {
        qWarning() << "Download music URL: Workshop item object not found";
        return QUrl();
    }

    // Get files obj
    QJsonArray filesArray = workshopItemObj["files"].toArray();
    if (filesArray.isEmpty()) {
        qWarning() << "Download music URL: Files array not found";
        return QUrl();
    }

    // Get first file
    QJsonObject fileObj = filesArray[0].toObject();
    if (fileObj.isEmpty()) {
        qWarning() << "Download music URL: First file object not found";
        return QUrl();
    }

    // Get URL
    QString fileDownloadString = fileObj["url"].toString();
    if (fileDownloadString.isEmpty() || fileDownloadString.isNull()) {
        qWarning() << "Download music URL: File download string not found";
        return QUrl();
    }

    qDebug() << "Download music URL:" << fileDownloadString;

    // Return
    return QUrl(fileDownloadString);
}

QUrl ApiClient::getDownloadUrlMapEditor()
{
    // The Unearth map editor is hosted on its own GitHub repo, NOT keeperfx.net.
    // Query the latest release directly via the raw GitHub API.
    QNetworkAccessManager manager;
    QNetworkRequest req(QUrl("https://api.github.com/repos/rainlizard/Unearth/releases/latest"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "keeperfx-launcher-qt");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = manager.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Map editor download URL: request failed:" << reply->errorString();
        reply->deleteLater();
        return QUrl();
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (doc.isObject() == false) {
        qWarning() << "Map editor download URL: invalid response";
        return QUrl();
    }

    // Platform-specific asset name token
#ifdef Q_OS_WINDOWS
    const QString platformToken = "windows";
#else
    const QString platformToken = "linux";
#endif

    // Find the matching asset: name contains the platform token and ends with ".zip"
    const QJsonArray assets = doc.object()["assets"].toArray();
    for (const QJsonValue &a : assets) {
        const QString name = a.toObject()["name"].toString();
        if (name.contains(platformToken, Qt::CaseInsensitive)
            && name.endsWith(".zip", Qt::CaseInsensitive)) {
            const QString downloadUrl = a.toObject()["browser_download_url"].toString();
            qDebug() << "Map editor download URL:" << downloadUrl;
            return QUrl(downloadUrl);
        }
    }

    qWarning() << "Map editor download URL: no matching asset found";
    return QUrl();
}

QJsonArray ApiClient::getWorkshopCatalog()
{
    // The keeperfx.net search endpoint requires a query and ignores category/sort
    // params, but a single space matches the whole catalogue in one call. We then
    // filter and sort client-side by the numeric "category" code and the rating /
    // download / date fields each item carries.
    QUrl url("v1/workshop/search");
    QUrlQuery query;
    query.addQueryItem("q", " ");
    url.setQuery(query);

    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        qWarning() << "Workshop catalog: invalid response";
        return QJsonArray();
    }
    return jsonDoc.object()["workshop_items"].toArray();
}

QUrl ApiClient::getWorkshopItemDownloadUrl(int itemId)
{
    QUrl url(QString("v1/workshop/item/%1").arg(itemId));

    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        qWarning() << "Workshop item download URL: invalid response for item" << itemId;
        return QUrl();
    }

    const QJsonObject workshopItemObj = jsonDoc.object()["workshop_item"].toObject();
    const QJsonArray filesArray = workshopItemObj["files"].toArray();
    if (filesArray.isEmpty()) {
        qWarning() << "Workshop item download URL: no files for item" << itemId;
        return QUrl();
    }

    // The primary (latest) file is what the website's Download button serves.
    const QString fileUrl = filesArray.first().toObject()["url"].toString();
    if (fileUrl.isEmpty()) {
        qWarning() << "Workshop item download URL: empty file URL for item" << itemId;
        return QUrl();
    }

    qDebug() << "Workshop item" << itemId << "download URL:" << fileUrl;
    return QUrl(fileUrl);
}

std::optional<QMap<QString, QString>> ApiClient::getGameFileList(KfxVersion::ReleaseType type,
                                                                 QString version)
{
    // Get type as string
    QString typeString;
    if (type == KfxVersion::ReleaseType::STABLE) {
        typeString = "stable";
    } else if (type == KfxVersion::ReleaseType::ALPHA) {
        typeString = "alpha";
    } else {
        return std::nullopt;
    }

    // Get URL
    QUrl url("v1/release/" + typeString + "/" + version + "/files");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return std::nullopt;
    }

    // Convert to JSON object
    QJsonObject jsonObj = jsonDoc.object();

    // Make sure object is valid
    if (jsonObj["success"].toBool() != true || jsonObj["release_type"].toString() != typeString
        || jsonObj["version"].toString() != version || jsonObj.contains("files") == false) {
        return std::nullopt;
    }

    // Create path -> checksum map
    QMap<QString, QString> map;

    // Loop trough all files
    QJsonObject fileObj = jsonObj["files"].toObject();
    foreach (const QString &path, fileObj.keys()) {
        // Add to map
        map[path] = fileObj.value(path).toString();
    }

    return map;
}
