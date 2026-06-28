#include "apiclient.h"

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

#define API_ENDPOINT "https://keeperfx.net/api"

#ifndef Q_OS_WINDOWS
// keeperfx-linux-alpha: the launcher's "is there a newer version?" check should look at
// OUR GitHub releases, not keeperfx.net. This returns the latest release shaped like the
// keeperfx.net response the updater expects: { "version": "1.3.2.5200", "download_url": ... }.
static QJsonObject getLatestLinuxAlphaRelease()
{
    QNetworkAccessManager manager;
    QNetworkRequest req(QUrl("https://api.github.com/repos/ForkedInTime/keeperfx-linux-alpha/releases/latest"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "keeperfx-launcher-qt-linux");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = manager.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Linux alpha update check failed:" << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (doc.isObject() == false) {
        return QJsonObject();
    }
    QJsonObject root = doc.object();

    // tag like "v1.3.2.5200-alpha" -> numeric version "1.3.2.5200"
    QRegularExpression re(QStringLiteral("([0-9]+\\.[0-9]+\\.[0-9]+(?:\\.[0-9]+)?)"));
    QRegularExpressionMatch m = re.match(root["tag_name"].toString());
    if (m.hasMatch() == false) {
        return QJsonObject();
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

    QJsonObject out;
    out["version"] = m.captured(1);
    out["download_url"] = downloadUrl;
    return out;
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
    return getLatestLinuxAlphaRelease();
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
    return getLatestLinuxAlphaRelease();
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
