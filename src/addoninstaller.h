#pragma once

#include <QString>
#include <QStringList>

// Shared add-on install engine. Used by both the Mod Manager's "Install…" file
// picker and the in-launcher Workshop browser, so a downloaded workshop archive
// (mod / campaign / map pack / standalone map) lands in the right place through
// exactly one code path.
class AddonInstaller
{
public:
    struct Result {
        bool ok = false;           // extraction ran without a hard error
        bool foundContent = false; // something recognisable was installed
        bool installedMod = false; // a mod (vs only campaign/map/pack) was added
        QStringList lines;         // human-readable summary lines
        QString error;             // set when ok == false
    };

    // Extract the archive at archivePath and install its contents into gameRoot.
    static Result installArchive(const QString &archivePath, const QString &gameRoot);

    // Install an already-extracted directory tree into gameRoot. archiveName is
    // used only for summary text and stub mod.cfg provenance.
    static Result installExtractedDir(const QString &tmpPath, const QString &gameRoot,
                                      const QString &archiveName);
};
