#pragma once

#include "mod.h"

#include <QWidget>

namespace Ui { class ModWidget; }

class ModWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModWidget(Mod *mod, QWidget *parent = nullptr);
    ~ModWidget();

signals:
    void enabledChanged();

private slots:
    void on_enabledCheckBox_toggled(bool checked);

private:
    Ui::ModWidget *ui;

    Mod *mod;
};
