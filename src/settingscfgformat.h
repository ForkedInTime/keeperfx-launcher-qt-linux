#pragma once

#include <QSettings>
#include <QIODevice>
#include <QTextStream>

class SettingsCfgFormat
{
public:
    // Registers the format the first time and returns its ID
    static QSettings::Format registerFormat();

private:
    static bool readFile(QIODevice &device, QSettings::SettingsMap &map);
    static bool writeFile(QIODevice &device, const QSettings::SettingsMap &map);
};
