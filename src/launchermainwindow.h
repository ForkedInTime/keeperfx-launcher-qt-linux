#pragma once

#include "game.h"
#include "kfxversion.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QProcess>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class LauncherMainWindow;
}
QT_END_NAMESPACE

class LauncherMainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void updateFound(KfxVersion::VersionInfo versionInfo);
    void filesToRemoveFound(QStringList filesToRemove);
    void kfxNetRetrieval(QJsonDocument workshopItems, QJsonDocument latestNew);
    void kfxNetImagesLoaded(QList<QJsonObject> workshopItemList, QList<QJsonObject> newsArticleList, QMap<QString, QPixmap> pixmapMap);
    void showUpdateIcon(bool show);

public:
    LauncherMainWindow(QWidget *parent = nullptr);
    ~LauncherMainWindow();

protected:
    // Extra width should become more workshop columns, not wider cards.
    void resizeEvent(QResizeEvent *event) override;
    // Reflows the grid when the workshop panel itself changes width.
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_logFileButton_clicked();
    void on_workshopButton_clicked();
    void on_settingsButton_clicked();
    void on_playButton_clicked();
    void on_openFolderButton_clicked();
    void on_unearthButton_clicked();
    void on_modsButton_clicked();
    void on_browseWorkshopButton_clicked();

    void onUpdateFound(KfxVersion::VersionInfo versionInfo);
    void onShowUpdateIcon(bool show);

    void onFilesToRemoveFound(QStringList filesToRemove);
    void onGameEnded(int exitCode, QProcess::ExitStatus exitStatus);

    void onKfxNetRetrieval(QJsonDocument workshopItems, QJsonDocument latestNews);
    void onKfxNetImagesLoaded(QList<QJsonObject> workshopItemList, QList<QJsonObject> newsArticleList, QMap<QString, QPixmap> pixmapMap);

    void on_discordButton_clicked();
    void on_websiteButton_clicked();
    void on_checkForUpdatesButton_clicked();

private:
    Ui::LauncherMainWindow *ui;
    Game *game;

    void startGame(Game::StartType startType, QVariant data1 = QVariant(), QVariant data2 = QVariant(), QVariant data3 = QVariant());

    void setupPlayExtraMenu();

    // Re-lays the workshop cards across as many columns as the panel can fit.
    void reflowWorkshopGrid();
    // Re-entrancy guard: reflowWorkshopGrid() resizes the cards, which resizes the
    // panel, which fires the resize filter that calls it again. The version that
    // measured the panel's own width could not loop, because its answer never
    // changed; measuring the viewport removes that accidental brake, so the guard
    // supplies a deliberate one.
    bool workshopReflowInProgress = false;

    QMenu *saveFilesMenu;
    void refreshSaveFilesMenu();

    QMenu *campaignMenu;
    void refreshCampaignMenu();

    void refreshInstallationAwareButtons();
    void refreshLogfileButton();

    bool askForKeeperFxInstall();

    void showLoadingSpinner();
    void hideLoadingSpinner(bool showOnlineContent);

    bool isLoadingLatestFromKfxNet = false;
    void loadLatestFromKfxNet();
    void clearLatestFromKfxNet();

    void checkForNewLauncher();
    void checkForFileRemoval();

    void forceKfxUpdateCheck();
    void checkForKfxUpdate(bool ignoreInterval = false, bool showMessageBox = false);

    void verifyBinaryCertificates();

    void refreshKfxVersionInGui();
};
