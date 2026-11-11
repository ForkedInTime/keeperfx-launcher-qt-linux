#pragma once

#include <QDialog>
#include <QScrollArea>

#include "popupsignalcombobox.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onOpenConfigButtonClicked();
    void onResolutionChanged(int index);
    void on_pushButtonShowLauncherParams_clicked();

private:
    Ui::SettingsDialog *ui;

    bool settingHasChanged = false;

    void loadSettings();
    void saveSettings();
    void restoreSettings();
    void cancel();

    void addSettingsChangedHandler();

    PopupSignalComboBox *popupComboBoxMonitorDisplay;
    void setupDisplayMonitorDropdown();

    QList<QWidget *> monitorNumberOverlays;
    void showMonitorNumberOverlays();
    void hideMonitorNumberOverlays();

    QStringList hiddenStartupScreens;

    void sortLanguageComboBox(QComboBox *comboBox);

    void showCustomResolutionDialog(QComboBox *sourceCombo);
    void addCustomResolution(int width, int height, QComboBox *sourceCombo);

    void switchContainerLayout(QWidget *container, bool useHorizontal);
    void updateResponsiveLayouts();
    static QList<QWidget *> responsiveColumns(QWidget *container);
    static int sideBySideWidth(QWidget *container, const QList<QWidget *> &columns);
    static QScrollArea *responsiveScrollArea(QWidget *container);
    static int usablePageWidth(QScrollArea *scroll);
    static int marginsAround(QWidget *container);
    bool initialSizeFitted = false;
    void hideSettingRow(QWidget *field);
    void fitComboBoxToEntries(QComboBox *comboBox);
};
