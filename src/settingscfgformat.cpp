#include <QSaveFile>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "settingscfgformat.h"
#include "cfgmerge.h"

QSettings::Format SettingsCfgFormat::registerFormat()
{
    // Once: Qt takes 16 custom formats and then returns InvalidFormat, and every Settings::load()
    // asked again (the Settings window's restore defaults loads the settings each time).
    static const QSettings::Format format = QSettings::registerFormat("cfg", readFile, writeFile);
    return format;
}

bool SettingsCfgFormat::readFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QTextStream stream(&device);

    while (!stream.atEnd()) {
        QString key;
        QString value;
        if (CfgMerge::parseSetting(stream.readLine(), &key, &value)) {
            map.insert(key, value);
        }
    }

    return true;
}

bool SettingsCfgFormat::writeFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    // Try to get the filename
    QString filePath;
    if (QSaveFile *sf = qobject_cast<QSaveFile*>(&device)) {
        filePath = sf->fileName();
    } else if (QFile *f = qobject_cast<QFile*>(&device)) {
        filePath = f->fileName();
    }

    // The file as it is now, and for keeperfx.cfg the release's own copy
    QString current;
    QString defaults;
    if (!filePath.isEmpty()) {
        QFile readFile(filePath);
        if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            current = QTextStream(&readFile).readAll();
        }
        if (QFileInfo(filePath).fileName() == "keeperfx.cfg") {
            QFile kfxDefaultConfigFile(QCoreApplication::applicationDirPath() + QDir::separator() + "_keeperfx.cfg");
            if (kfxDefaultConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                defaults = QTextStream(&kfxDefaultConfigFile).readAll();
            }
        }
    }

    // Launcher configs get no blank lines before added settings
    bool launcherConfig = filePath.endsWith("keeperfx-launcher-qt.cfg") || filePath.endsWith("launcher.cfg");

    QTextStream out(&device);
    out << CfgMerge::merge(current, defaults, map, !launcherConfig);

    return true;
}
