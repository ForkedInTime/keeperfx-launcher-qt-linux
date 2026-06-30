#include "modmanagerdialog.h"
#include "modmanager.h"
#include "modwidget.h"
#include "mod.h"
#include "archiver.h"
#include "ui_modmanagerdialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QTextStream>

#include <bit7z/bitarchivereader.hpp>

namespace {

// Subdirectories that mark a folder as an actual mod (engine config trees).
const QStringList kModContentDirs = {
    "creatrs", "fxdata", "ldata", "cmpgfx", "sound", "data", "lang", "campgns", "levels"
};

// A folder "is a mod" if it has a mod.cfg or any engine config subdirectory.
bool looksLikeMod(const QString &dir)
{
    if (QFileInfo::exists(dir + "/mod.cfg")) {
        return true;
    }
    for (const QString &content : kModContentDirs) {
        if (QFileInfo(dir + "/" + content).isDir()) {
            return true;
        }
    }
    return false;
}

bool copyRecursively(const QString &src, const QString &dst)
{
    QFileInfo info(src);
    if (info.isDir()) {
        QDir().mkpath(dst);
        const QStringList entries = QDir(src).entryList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QString &entry : entries) {
            if (!copyRecursively(src + "/" + entry, dst + "/" + entry)) {
                return false;
            }
        }
        return true;
    }
    QFile::remove(dst);
    return QFile::copy(src, dst);
}

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

} // namespace

ModManagerDialog::ModManagerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ModManagerDialog)
{
    ui->setupUi(this);

    // Disable resizing and remove maximize button
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    reloadMods();
}

void ModManagerDialog::reloadMods()
{
    // Clear any existing widgets / stretch from the scroll area
    QLayout *layout = ui->scrollAreaWidgetContents->layout();
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget(); // null for the stretch spacer item; safe
        delete item;
    }

    // (Re)scan the mods folder and the load order. Deleting the old manager frees
    // its Mod objects (see ModManager::~ModManager); the widgets above are already
    // gone, so nothing still points at them.
    delete manager;
    manager = new ModManager();
    const QList<Mod *> mods = manager->allMods();

    if (mods.isEmpty() == false) {
        for (Mod *mod : mods) {
            ModWidget *modWidget = new ModWidget(mod, this);

            // Persist the load order whenever a mod is toggled
            connect(modWidget, &ModWidget::enabledChanged, this, &ModManagerDialog::saveLoadOrder);

            layout->addWidget(modWidget);
        }
    } else {
        QLabel *emptyLabel = new QLabel(
            tr("No mods installed. Use “Install…”, or drop a mod folder into the 'mods' folder."),
            this);
        emptyLabel->setWordWrap(true);
        layout->addWidget(emptyLabel);
    }

    // Keep the mod widgets aligned to the top of the scroll area
    ui->verticalLayout_2->addStretch(1);
}

void ModManagerDialog::on_installButton_clicked()
{
    // Pick the archive (a keeperfx.net workshop download: mod, campaign or map pack)
    const QString archivePath = QFileDialog::getOpenFileName(
        this,
        tr("Install add-on (mod, campaign, or map pack)"),
        QDir::homePath(),
        tr("KeeperFX add-ons (*.7z *.zip)"));
    if (archivePath.isEmpty()) {
        return;
    }

    const QString archiveName = QFileInfo(archivePath).fileName();
    const QString gameRoot = QCoreApplication::applicationDirPath();

    // Extract into a temp dir on the SAME filesystem as the install, so files can be
    // merged into place without a cross-device copy.
    const QString tmpPath = gameRoot + "/.kfx-install-tmp";
    QDir tmpDir(tmpPath);
    if (tmpDir.exists()) {
        tmpDir.removeRecursively();
    }
    if (!QDir().mkpath(tmpPath)) {
        QMessageBox::warning(this, tr("Install failed"),
                             tr("Could not create a temporary folder to install into."));
        return;
    }

    try {
        bit7z::BitArchiveReader reader = Archiver::getReader(archivePath.toStdString());
        reader.extractTo(tmpPath.toStdString());
    } catch (const bit7z::BitException &ex) {
        qWarning() << "Add-on install: extract failed:" << ex.what();
        QMessageBox::warning(this, tr("Install failed"),
                             tr("Could not extract the archive:\n%1").arg(ex.what()));
        tmpDir.removeRecursively();
        return;
    }

    QStringList installed;     // human-readable summary lines
    bool installedMod = false; // whether a mod (vs only campaign/map pack) was added

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
        const QString src = tmpPath + "/" + dirName;
        const QString dst = gameRoot + "/" + kind; // canonical lowercase install folder

        // Summarise what's being added (before the merge)
        if (kind == "campgns") {
            const QStringList cfgs = QDir(src).entryList(QStringList{"*.cfg"}, QDir::Files);
            for (const QString &cfg : cfgs) {
                installed << tr("Campaign: %1").arg(campaignDisplayName(src + "/" + cfg));
            }
        } else if (kind == "mods") {
            const QStringList modDirs = QDir(src).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &m : modDirs) {
                installed << tr("Mod: %1").arg(m);
            }
            installedMod = installedMod || !modDirs.isEmpty();
        } else if (kind == "levels") {
            installed << tr("Map pack (added to 'levels')");
        } else if (kind == "multiplayer") {
            installed << tr("Multiplayer maps");
        }

        if (!copyRecursively(src, dst)) {
            QMessageBox::warning(this, tr("Install failed"),
                                 tr("Could not copy the '%1' folder into the game directory.").arg(kind));
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

    // Case 2 — a bare mod: no container folder, just a mod folder (or its contents) at
    // the archive root. Install it into mods/.
    if (!handledAsContainer) {
        const QString modsRoot = gameRoot + "/mods";
        QDir modsDir(modsRoot);
        if (!modsDir.exists()) {
            QDir().mkpath(modsRoot);
        }

        QList<QPair<QString, QString>> found; // (name, sourcePath)
        if (looksLikeMod(tmpPath)) {
            found.append({QFileInfo(archivePath).completeBaseName(), tmpPath});
        } else {
            const QStringList subDirs = QDir(tmpPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &subDir : subDirs) {
                found.append({subDir, tmpPath + "/" + subDir});
            }
        }

        for (const auto &entry : std::as_const(found)) {
            QString name = entry.first;
            name.replace('/', '_').replace('\\', '_'); // never let a name escape mods/
            const QString dest = modsDir.absoluteFilePath(name);

            if (QFileInfo::exists(dest)) {
                const auto answer = QMessageBox::question(
                    this, tr("Replace mod?"),
                    tr("“%1” is already installed. Replace it?").arg(name));
                if (answer != QMessageBox::Yes) {
                    continue;
                }
                QDir(dest).removeRecursively();
            }
            if (!copyRecursively(entry.second, dest)) {
                QMessageBox::warning(this, tr("Install failed"),
                                     tr("Could not copy “%1” into the mods folder.").arg(name));
                continue;
            }
            if (!QFileInfo::exists(dest + "/mod.cfg")) {
                writeStubModCfg(dest, name, archiveName);
            }
            installed << tr("Mod: %1").arg(name);
            installedMod = true;
        }
    }

    tmpDir.removeRecursively();

    // Refresh the mod list (campaigns/map packs show up in-game, not in this list)
    reloadMods();

    if (installed.isEmpty()) {
        QMessageBox::warning(this, tr("Nothing to install"),
                             tr("No mod, campaign or map pack was found inside %1.").arg(archiveName));
        return;
    }

    QString body = tr("Installed from %1:").arg(archiveName) + "\n\n• " + installed.join("\n• ") + "\n\n";
    if (installedMod) {
        body += tr("Mods appear in this list — tick “Enabled” to turn them on. ");
    }
    body += tr("Campaigns and map packs appear in the game itself (Land selection / free play) "
               "the next time you launch it.");
    QMessageBox::information(this, tr("Add-on installed"), body);
}

void ModManagerDialog::saveLoadOrder()
{
    ModManager::writeLoadOrder(manager->allMods());
}

ModManagerDialog::~ModManagerDialog()
{
    delete ui;
}

void ModManagerDialog::on_closeButton_clicked()
{
    this->close();
}
