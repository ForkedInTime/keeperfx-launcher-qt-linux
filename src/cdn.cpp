#include "cdn.h"
#include "settings.h"

#include <QCoreApplication>
#include <QString>

QString CDN::endPoint;

// clang-format off
const QList<std::pair<QString, CDN::EndpointInfo>> CDN::endPointList = {
    {"keeperfx.net",    {tr("KeeperFX.net (Official, Germany)",     "Download Server"),         "https://keeperfx.net"}},
    {"cloudflare",      {tr("Cloudflare CDN (Worldwide)",           "Download Server"),         "https://cdn-cf1.keeperfx.net"}},
};
// clang-format on

QString CDN::getEndpoint()
{
    if (CDN::endPoint.isEmpty()) {

        CDN::endPoint = "https://keeperfx.net";
        QString savedKey = Settings::getLauncherSetting("CDN_ENDPOINT").toString();

        for (const auto& [key, info] : CDN::endPointList) {
            if (key == savedKey) {
                CDN::endPoint = info.url;
                break;
            }
        }

        if(CDN::endPoint.endsWith("/")){
            CDN::endPoint.chop(1);
        }
    }

    return CDN::endPoint;
}