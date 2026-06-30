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
            tr("No mods installed. Use “Install mod…”, or drop a mod folder into the 'mods' folder."),
            this);
        emptyLabel->setWordWrap(true);
        layout->addWidget(emptyLabel);
    }

    // Keep the mod widgets aligned to the top of the scroll area
    ui->verticalLayout_2->addStretch(1);
}

void ModManagerDialog::on_installButton_clicked()
{
    // Pick the archive
    const QString archivePath = QFileDialog::getOpenFileName(
        this,
        tr("Select a mod archive"),
        QDir::homePath(),
        tr("Mod archives (*.7z *.zip)"));
    if (archivePath.isEmpty()) {
        return;
    }

    const QString archiveName = QFileInfo(archivePath).fileName();
    const QString modsRoot = QCoreApplication::applicationDirPath() + "/mods";
    QDir modsDir(modsRoot);
    if (!modsDir.exists()) {
        QDir().mkpath(modsRoot);
    }

    // Extract into a temp dir on the SAME filesystem as the mods folder, so the
    // installed files can be moved/copied into place without a cross-device issue.
    const QString tmpPath = modsDir.absoluteFilePath(".kfx-install-tmp");
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
        qWarning() << "Mod install: extract failed:" << ex.what();
        QMessageBox::warning(this, tr("Install failed"),
                             tr("Could not extract the archive:\n%1").arg(ex.what()));
        tmpDir.removeRecursively();
        return;
    }

    // Find the mods inside the extracted tree. Workshop archives wrap everything in
    // a top-level "mods/" folder; others ship the mod folder (or its bare contents)
    // at the root.
    QString base = tmpPath;
    if (QFileInfo(tmpPath + "/mods").isDir()) {
        base = tmpPath + "/mods";
    }

    QList<QPair<QString, QString>> found; // (name, sourcePath)
    if (looksLikeMod(base)) {
        // A single bare mod (content at the root) -> name it after the archive
        found.append({QFileInfo(archivePath).completeBaseName(), base});
    } else {
        const QStringList subDirs = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &subDir : subDirs) {
            found.append({subDir, base + "/" + subDir});
        }
    }

    if (found.isEmpty()) {
        QMessageBox::warning(this, tr("Nothing to install"),
                             tr("No mod was found inside %1.").arg(archiveName));
        tmpDir.removeRecursively();
        return;
    }

    // Install each mod folder
    QStringList installed;
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

        // Give metadata-less mods a mod.cfg so they list nicely in the manager
        if (!QFileInfo::exists(dest + "/mod.cfg")) {
            writeStubModCfg(dest, name, archiveName);
        }

        installed << name;
    }

    tmpDir.removeRecursively();

    // Refresh the list so the newly installed mod(s) appear
    reloadMods();

    if (installed.isEmpty()) {
        return;
    }
    QMessageBox::information(
        this, tr("Mod installed"),
        tr("Installed: %1\n\nTick “Enabled” to turn it on.").arg(installed.join(", ")));
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
