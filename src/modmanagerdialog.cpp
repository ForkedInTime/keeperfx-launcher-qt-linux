#include "modmanagerdialog.h"
#include "modmanager.h"
#include "modwidget.h"
#include "ui_modmanagerdialog.h"

#include <QLabel>

ModManagerDialog::ModManagerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ModManagerDialog)
{
    ui->setupUi(this);

    // Disable resizing and remove maximize button
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    // Get mods
    // Keep the manager as a member so the Mod objects persist for the dialog's lifetime
    manager = new ModManager();
    QList<Mod *> mods = manager->allMods();

    // Add the mods
    if (mods.isEmpty() == false) {
        for (auto mod : std::as_const(mods)) {
            // Create the widget
            ModWidget *modWidget = new ModWidget(mod, this);

            // Persist the load order whenever a mod is toggled
            connect(modWidget, &ModWidget::enabledChanged, this, &ModManagerDialog::saveLoadOrder);

            ui->scrollAreaWidgetContents->layout()->addWidget(modWidget);
        }
    } else {
        // No mods present
        QLabel *emptyLabel = new QLabel(tr("No mods installed. Add mods to the 'mods' folder."), this);
        ui->scrollAreaWidgetContents->layout()->addWidget(emptyLabel);
    }

    // Keep the mod widgets aligned to the top of the scroll area
    ui->verticalLayout_2->addStretch(1);
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
