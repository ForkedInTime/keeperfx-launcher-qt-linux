#pragma once

#include <QFile>
#include <QString>
#include <QMetaEnum>

class KfxVersion : public QObject {
    Q_OBJECT

public:

    enum ReleaseType
    {
        UNKNOWN,
        STABLE,
        ALPHA,
        PROTOTYPE,
    };
    Q_ENUM(ReleaseType)

    inline static ReleaseType getReleaseTypefromString(const QString &str) {
        QMetaEnum metaEnum = QMetaEnum::fromType<ReleaseType>();
        int value = metaEnum.keyToValue(str.toUtf8().toUpper().constData());
        return (value == -1) ? UNKNOWN : static_cast<ReleaseType>(value);
    }

    struct VersionInfo
    {
        ReleaseType type = ReleaseType::UNKNOWN;
        QString version = "0.0.0";
        QString fullString = QString();
        QString downloadUrl = QString();
        // An update archive holding only what changed since the INSTALLED version,
        // present when the release carries one that starts from exactly it. Empty
        // otherwise, and downloadUrl is then the only way forward. Roughly 2% the
        // size of the full payload, because between releases almost nothing but the
        // engine and launcher binaries moves.
        QString patchUrl = QString();
    };

    static const QMap<QString, QString> versionFunctionaltyMap;
    static bool hasFunctionality(QString functionalityString);

    static VersionInfo currentVersion;

    static QString getVersionString(const QFile& binary);
    static QString getVersionStringFromAppDir();
    static VersionInfo getVersionFromString(QString versionString);

    static bool loadCurrentVersion();

    static bool isVersionLowerOrEqual(const QString &fileVersion, const QString &currentVersion);
    static bool isVersionHigherOrEqual(const QString &fileVersion, const QString &currentVersion);
    static bool isNewerVersion(const QString &fileVersion, const QString &currentVersion);
    static bool checkIfAlphaUpdateNeedsNewStable(const QString &version1, const QString &version2);

    static std::optional<VersionInfo> getLatestVersion(ReleaseType type);
    static std::optional<QMap<QString, QString>> getGameFileMap(ReleaseType type, QString version);
};
