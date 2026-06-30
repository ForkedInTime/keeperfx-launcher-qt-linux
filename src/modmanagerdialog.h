#pragma once

#include <QDialog>

namespace Ui { class ModManagerDialog; }

class ModManager;

class ModManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModManagerDialog(QWidget *parent = nullptr);
    ~ModManagerDialog();

private slots:
    void on_closeButton_clicked();
    void saveLoadOrder();

private:
    Ui::ModManagerDialog *ui;

    ModManager *manager;
};
