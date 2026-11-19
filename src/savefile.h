#pragma once

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>
#include <QByteArray>
#include <QException>

class SaveFile
{
public:
    SaveFile(const QString& filePath);

    QFile file;
    QString fileName;
    QString saveName;
    QString campaignName;
    bool kfxs = false; // the KFXS format (KeeperFX 1.4 and later)

    bool isValid();
    QString toString();

    /** The saves in the game's save folder whose title can be read; with
     *  includeUnreadable, every fx1g*.sav there (what a backup must hold). */
    static QList<SaveFile *> getAll(bool includeUnreadable = false);
    static bool checkFileHeader(QFile& file);

    static bool backupAll();
    static bool backupAll(QList<SaveFile *> saveFiles);
};
