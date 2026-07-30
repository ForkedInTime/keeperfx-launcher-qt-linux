#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QLabel;
class QWidget;

// An add-on found in the install. relPaths are gameRoot-relative files/dirs that
// together make up the item (moved as a unit on uninstall). kind is a stable
// English key ("Campaign"/"Map pack"/"Map"/"Mod") used for filtering.
struct InstalledEntry {
    QString name;
    QString kind;
    bool stock = false;
    QStringList relPaths;
};

// A previously-uninstalled item sitting in the backup, restorable.
struct BackupEntry {
    QString token;
    QString name;
    QString kind;
    bool stock = false;
    QStringList relPaths;
};

// In-launcher Workshop browser. Two tabs:
//  - "Browse & install": the full keeperfx.net catalogue (filter/search/sort),
//    one-click install via AddonInstaller.
//  - "Installed": scans the install for add-ons, labels each as KeeperFX-stock or
//    Workshop/user, filters by category + search, and offers a reversible
//    Uninstall (moves to a .kfx-uninstalled backup) + Restore.
class WorkshopBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkshopBrowserDialog(QWidget *parent = nullptr);

private slots:
    void applyFilter();

private:
    void fetchCatalog();
    void installItem(const QJsonObject &item);
    long ourGameBuild() const;

    QWidget *buildBrowseTab();
    QWidget *buildInstalledTab();
    void rescanInstalled();  // filesystem scan → fill caches → renderInstalled()
    void renderInstalled();  // apply category + search filter → rebuild rows

    // Browse tab
    QComboBox *categoryBox = nullptr;
    QComboBox *sortBox = nullptr;
    QLineEdit *searchEdit = nullptr;
    QVBoxLayout *listLayout = nullptr;
    QLabel *statusLabel = nullptr;

    // Installed tab
    QComboBox *installedCategoryBox = nullptr;
    QLineEdit *installedSearchEdit = nullptr;
    QVBoxLayout *installedListLayout = nullptr;
    QVector<InstalledEntry> installedCache;
    QVector<BackupEntry> backupCache;

    QVector<QJsonObject> catalog;
    bool loaded = false;
};
