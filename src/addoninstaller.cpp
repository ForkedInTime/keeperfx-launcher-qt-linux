#include "addoninstaller.h"

#include "addonshape.h"
#include "copytree.h"
#include "archiver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QTextStream>

#include <bit7z/bitarchivereader.hpp>

namespace {

// Subdirectories that mark a folder as an actual mod (engine config trees).


// The recursive copy helpers live in copytree.h so tests can exercise them
// without linking bit7z and LIEF. copy_tree_no_clobber() reports whether the
// copy actually happened, which the previous local version could not.

// Workshop mods don't always ship a mod.cfg; without one the manager can't list
// them. Write a minimal one so the mod shows up and is toggleable.
void writeStubModCfg(const QString &dir, const QString &modName, const QString &archiveName)
{
    QFile file(dir + "/mod.cfg");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QString pretty = modName;
    pretty.replace('_', ' ');
    QTextStream out(&file);
    out << "[mod]\n";
    out << "Name=" << pretty << "\n";
    out << "Description=" << QObject::tr("Installed from %1.").arg(archiveName) << "\n";
    file.close();
}

// Read a campaign's display name from its .cfg (the "NAME = ..." line), so the
// install summary shows "Another Dungeon" rather than "anthrdunj".
QString campaignDisplayName(const QString &cfgPath)
{
    QFile file(cfgPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
                continue;
            }
            const int eq = line.indexOf('=');
            if (eq <= 0) {
                continue;
            }
            if (line.left(eq).trimmed().compare("NAME", Qt::CaseInsensitive) == 0) {
                const QString value = line.mid(eq + 1).trimmed();
                if (!value.isEmpty()) {
                    return value;
                }
            }
        }
    }
    return QFileInfo(cfgPath).completeBaseName();
}

// A standalone KeeperFX map is a set of loose files sharing a mapNNNNN stem
// (map00378.slb, .dat, .clm, .own, .lof …). Every map has a .slb slab file, so
// use that as the signature. Returns the directory holding them (the archive
// root, or a single nested folder), or empty if this isn't a standalone map.

} // namespace

AddonInstaller::Result AddonInstaller::installArchive(const QString &archivePath,
                                                      const QString &gameRoot)
{
    Result r;

    // Extract into a temp dir on the SAME filesystem as the install, so files can be
    // merged into place without a cross-device copy.
    const QString tmpPath = gameRoot + "/.kfx-install-tmp";
    QDir tmpDir(tmpPath);
    if (tmpDir.exists()) {
        tmpDir.removeRecursively();
    }
    if (!QDir().mkpath(tmpPath)) {
        r.error = QObject::tr("Could not create a temporary folder to install into.");
        return r;
    }

    try {
        bit7z::BitArchiveReader reader = Archiver::getReader(archivePath.toStdString());
        reader.extractTo(tmpPath.toStdString());
    } catch (const bit7z::BitException &ex) {
        qWarning() << "Add-on install: extract failed:" << ex.what();
        r.error = QObject::tr("Could not extract the archive:\n%1").arg(ex.what());
        tmpDir.removeRecursively();
        return r;
    }

    r = installExtractedDir(tmpPath, gameRoot, QFileInfo(archivePath).fileName());
    tmpDir.removeRecursively();
    return r;
}

AddonInstaller::Result AddonInstaller::installExtractedDir(const QString &tmpPath,
                                                           const QString &gameRoot,
                                                           const QString &archiveName)
{
    Result r;
    r.ok = true; // extraction already succeeded by the time we get here

    // Case 1 — "extract into the game directory": the archive has one or more known
    // container folders at its root (campgns/, mods/, levels/, multiplayer/). This is
    // how workshop campaigns, mods and map packs ship. Merge each into the install,
    // leaving other add-ons' files untouched. We only ever touch these known folders,
    // so a stray root file can't overwrite core game config.
    static const QStringList kContainers = {"campgns", "mods", "levels", "multiplayer"};
    const QStringList topDirs = QDir(tmpPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool handledAsContainer = false;

    for (const QString &dirName : topDirs) {
        const QString kind = dirName.toLower();
        if (!kContainers.contains(kind)) {
            continue;
        }
        handledAsContainer = true;
        r.foundContent = true;
        const QString src = tmpPath + "/" + dirName;
        const QString dst = gameRoot + "/" + kind; // canonical lowercase install folder

        // Summarise what's being added (before the merge)
        if (kind == "campgns") {
            const QStringList cfgs = QDir(src).entryList(QStringList{"*.cfg"}, QDir::Files);
            for (const QString &cfg : cfgs) {
                r.lines << QObject::tr("Campaign: %1").arg(campaignDisplayName(src + "/" + cfg));
            }
        } else if (kind == "mods") {
            const QStringList modDirs = QDir(src).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &m : modDirs) {
                r.lines << QObject::tr("Mod: %1").arg(m);
            }
            r.installedMod = r.installedMod || !modDirs.isEmpty();
        } else if (kind == "levels") {
            r.lines << QObject::tr("Map pack (added to 'levels')");
        } else if (kind == "multiplayer") {
            r.lines << QObject::tr("Multiplayer maps");
        }

        // No-clobber merge: keep existing files (stock campaigns/maps, other add-ons)
        const CopyTreeOutcome merge = copy_tree_no_clobber(src, dst);
        if (!merge.ok) {
            // Nothing was written, or only part of it was. Saying so beats the old
            // behaviour, which counted skipped files, ignored failed ones, and
            // reported a successful install for a directory it could not write to.
            r.error = QObject::tr("Could not write into “%1”.\n\n"
                                  "The folder may be read-only — on a package-managed "
                                  "install the game's data folders are owned by the "
                                  "package manager.").arg(kind);
            return r;
        }
        const int kept = merge.skipped;
        if (kept > 0) {
            r.lines << QObject::tr("(kept %1 existing file(s) in '%2', not overwritten)")
                           .arg(kept).arg(kind);
        }

        // Mods shipped without a mod.cfg still need one to list in the manager
        if (kind == "mods") {
            const QStringList modDirs = QDir(src).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &m : modDirs) {
                const QString modDest = dst + "/" + m;
                if (!QFileInfo::exists(modDest + "/mod.cfg")) {
                    writeStubModCfg(modDest, m, archiveName);
                }
            }
        }
    }

    // Case 1b — a loose campaign: "<name>.cfg" beside "<name>/" at the archive root,
    // which is how workshop campaigns ship (no campgns/ wrapper).
    //
    // This MUST be tested before the standalone-map case below. A campaign's own
    // levels live inside "<name>/" as map*.slb, so the map detector matches a
    // campaign archive: Pandaemonium scattered 282 map files into levels/personal,
    // dropped the campaign cfg and its _configs/_land/_media directories entirely,
    // and reported a successful map-pack install.
    bool handledAsCampaign = false;
    if (!handledAsContainer) {
        const QString base = find_loose_campaign(tmpPath);
        if (!base.isEmpty()) {
            handledAsCampaign = true;
            // campgns/ or levels/ as the add-on's own cfg declares -- see
            // loose_addon_install_dir(). A map pack shipped in this shape used to be
            // forced into campgns/ where the game would never list it.
            const QString kindDir = loose_addon_install_dir(tmpPath + "/" + base + ".cfg");
            const QString dst = gameRoot + "/" + kindDir;
            QDir().mkpath(dst);
            int kept = 0;
            for (const QString &rel : loose_campaign_entries(tmpPath, base)) {
                const CopyTreeOutcome m = copy_tree_no_clobber(tmpPath + "/" + rel, dst + "/" + rel);
                if (!m.ok) {
                    r.error = QObject::tr("Could not write into “%1”.\n\n"
                                          "The folder may be read-only — on a package-managed "
                                          "install the game's data folders are owned by the "
                                          "package manager.").arg(kindDir);
                    return r;
                }
                kept += m.skipped;
            }
            r.foundContent = true;
            r.lines << (kindDir == "levels"
                            ? QObject::tr("Map pack: %1").arg(campaignDisplayName(tmpPath + "/" + base + ".cfg"))
                            : QObject::tr("Campaign: %1").arg(campaignDisplayName(tmpPath + "/" + base + ".cfg")));
            if (kept > 0) {
                r.lines << QObject::tr("(kept %1 existing file(s) in '%2', not overwritten)")
                               .arg(kept).arg(kindDir);
            }
        }
    }

    // Case 2 — a standalone map: loose mapNNNNN.* files (no container folder). These
    // are single workshop maps; the "Install…" file picker and the older installer
    // both used to reject them. Drop them into levels/personal so they show up under
    // the "Personal levels" pack in free play.
    bool handledAsMap = false;
    if (!handledAsContainer && !handledAsCampaign) {
        const QString mapDir = find_standalone_map_dir(tmpPath);
        if (!mapDir.isEmpty()) {
            handledAsMap = true;
            const QString dst = gameRoot + "/levels/personal";
            QDir().mkpath(dst);
            const QStringList mapFiles = QDir(mapDir).entryList(QStringList{"map*"}, QDir::Files);
            QSet<QString> stems;
            int copied = 0, kept = 0;
            for (const QString &f : mapFiles) {
                stems.insert(f.left(f.indexOf('.')));
                const QString d = dst + "/" + f;
                if (QFileInfo::exists(d)) {
                    kept++;
                    continue;
                }
                QFile::copy(mapDir + "/" + f, d);
                copied++;
            }
            if (copied > 0) {
                r.lines << QObject::tr("%n map(s) (added to Personal levels)", "", stems.size());
                r.foundContent = true;
            } else if (kept > 0) {
                r.lines << QObject::tr("Map already installed (Personal levels)");
                r.foundContent = true;
            }
        }
    }

    // Case 3 — a bare mod: no container folder and not a map, just a mod folder (or its
    // contents) at the archive root. Install it into mods/.
    if (!handledAsContainer && !handledAsCampaign && !handledAsMap) {
        const QString modsRoot = gameRoot + "/mods";
        QDir modsDir(modsRoot);
        if (!modsDir.exists()) {
            QDir().mkpath(modsRoot);
        }

        QList<QPair<QString, QString>> found; // (name, sourcePath)
        const QString archiveBase = QFileInfo(archiveName).completeBaseName();
        if (looks_like_mod(tmpPath)) {
            found.append({archiveBase, tmpPath});
        } else {
            // Subdirectories are separate mods only when they actually look like mods
            // -- an archive bundling several. Otherwise they are the *parts* of one
            // mod: a creature replacement ships stand_fp/, attack_td/, icons/ and so
            // on, and treating each as its own mod turned one sprite pack into 27
            // broken ones. An archive with no subdirectories at all (loose sprite zips
            // beside magic.cfg, say) previously installed nothing whatsoever.
            const QStringList subDirs = QDir(tmpPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &subDir : subDirs) {
                if (looks_like_mod(tmpPath + "/" + subDir)) {
                    found.append({subDir, tmpPath + "/" + subDir});
                }
            }
            if (found.isEmpty()) {
                found.append({archiveBase, tmpPath});
            }
        }

        for (const auto &entry : std::as_const(found)) {
            QString name = entry.first;
            name.replace('/', '_').replace('\\', '_'); // never let a name escape mods/
            const QString dest = modsDir.absoluteFilePath(name);

            // Unlike the file picker, callers here can't answer a "replace?" prompt per
            // item, so replace an existing mod of the same name (a re-install/update).
            if (QFileInfo::exists(dest)) {
                QDir(dest).removeRecursively();
            }
            if (!copy_tree(entry.second, dest)) {
                r.lines << QObject::tr("Could not copy “%1” into the mods folder.").arg(name);
                continue;
            }
            if (!QFileInfo::exists(dest + "/mod.cfg")) {
                writeStubModCfg(dest, name, archiveName);
            }
            r.lines << QObject::tr("Mod: %1").arg(name);
            r.installedMod = true;
            r.foundContent = true;
        }
    }

    return r;
}
