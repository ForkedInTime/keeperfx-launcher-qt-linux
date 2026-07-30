#include "modmanagerdialog.h"
#include "modmanager.h"
#include "modwidget.h"
#include "mod.h"
#include "addoninstaller.h"
#include "ui_modmanagerdialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QMessageBox>

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
    // Pick the archive (a keeperfx.net workshop download: mod, campaign, map or map pack)
    const QString archivePath = QFileDialog::getOpenFileName(
        this,
        tr("Install add-on (mod, campaign, map or map pack)"),
        QDir::homePath(),
        tr("KeeperFX add-ons (*.7z *.zip)"));
    if (archivePath.isEmpty()) {
        return;
    }

    const QString archiveName = QFileInfo(archivePath).fileName();

    // Do the extraction + install through the shared engine (same path the Workshop
    // browser uses). It handles mods, campaigns, map packs and standalone maps.
    const AddonInstaller::Result result =
        AddonInstaller::installArchive(archivePath, QCoreApplication::applicationDirPath());

    // Refresh the mod list (campaigns/maps/map packs show up in-game, not in this list)
    reloadMods();

    if (!result.ok) {
        QMessageBox::warning(this, tr("Install failed"), result.error);
        return;
    }

    if (!result.foundContent) {
        QMessageBox::warning(this, tr("Nothing to install"),
                             tr("No mod, campaign, map or map pack was found inside %1.").arg(archiveName));
        return;
    }

    QString body = tr("Installed from %1:").arg(archiveName) + "\n\n• " + result.lines.join("\n• ") + "\n\n";
    if (result.installedMod) {
        body += tr("Mods appear in this list — tick “Enabled” to turn them on. ");
    }
    body += tr("Campaigns, maps and map packs appear in the game itself (Land selection / free play) "
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
