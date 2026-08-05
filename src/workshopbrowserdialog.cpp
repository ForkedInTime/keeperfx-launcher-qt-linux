#include "workshopbrowserdialog.h"
#include "addoninstaller.h"
#include "apiclient.h"
#include "downloader.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

// keeperfx.net numeric category codes -> display label. Order mirrors the
// website's Workshop sidebar. code -1 is our synthetic "all categories".
const QVector<QPair<QString, int>> kCategories = {
    {QObject::tr("All categories"), -1},
    {QObject::tr("Map"), 10},
    {QObject::tr("Map Pack"), 15},
    {QObject::tr("Campaign"), 20},
    {QObject::tr("Multiplayer Map"), 30},
    {QObject::tr("Multiplayer Map Pack"), 35},
    {QObject::tr("Mod"), 40},
    {QObject::tr("Creature"), 45},
    {QObject::tr("Application"), 50},
    {QObject::tr("Other"), 100},
};

// Campaigns and map packs that ship WITH KeeperFX 1.4.x (stock). Removing these
// is allowed but clearly labelled + reversible; they must never be treated as
// user content that can be silently deleted. Basenames of the .cfg files.
const QStringList kStockCampaigns = {
    "ami2019", "ancntkpr", "anthrdunj", "burdnimp", "dzjr06lv", "jdkmaps8", "keeporig",
    "lqizgood", "origplus", "postanck", "pstunded", "revlord", "tempkpr", "twinkprs", "undedkpr"};
const QStringList kStockLevelCfgs = {
    "classic", "deepdngn", "legacy", "lostlvls", "standard", "personal"};

QString categoryLabel(int code)
{
    for (const auto &c : kCategories) {
        if (c.second == code) {
            return c.first;
        }
    }
    return QObject::tr("Other");
}

QString itemAuthor(const QJsonObject &item)
{
    const QString original = item["originalAuthor"].toString();
    if (!original.isEmpty()) {
        return original;
    }
    const QJsonObject submitter = item["submitter"].toObject();
    const QString username = submitter["username"].toString();
    return username.isEmpty() ? QObject::tr("Unknown") : username;
}

QString itemCreatedDate(const QJsonObject &item)
{
    return item["createdTimestamp"].toObject()["date"].toString();
}

QString thumbnailUrl(const QJsonObject &item)
{
    const QJsonArray images = item["images"].toArray();
    if (images.isEmpty()) {
        return QString();
    }
    const QString filename = images.first().toObject()["filename"].toString();
    if (filename.isEmpty()) {
        return QString();
    }
    return QString("https://keeperfx.net/workshop/image/%1/%2")
        .arg(item["id"].toInt())
        .arg(filename);
}

// Read "KEY = value" from a KeeperFX .cfg/.lof (case-insensitive key, ';'/'#' comments).
QString cfgValue(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
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
        if (line.left(eq).trimmed().compare(key, Qt::CaseInsensitive) == 0) {
            return line.mid(eq + 1).trimmed();
        }
    }
    return QString();
}

QVector<InstalledEntry> scanInstalled(const QString &gameRoot)
{
    QVector<InstalledEntry> out;

    // Campaigns: campgns/*.cfg (+ a same-named level folder if present)
    const QStringList campCfgs = QDir(gameRoot + "/campgns").entryList(QStringList{"*.cfg"}, QDir::Files);
    for (const QString &cfg : campCfgs) {
        const QString base = QFileInfo(cfg).completeBaseName();
        InstalledEntry e;
        e.kind = "Campaign";
        e.stock = kStockCampaigns.contains(base, Qt::CaseInsensitive);
        e.name = cfgValue(gameRoot + "/campgns/" + cfg, "NAME");
        if (e.name.isEmpty()) {
            e.name = base;
        }
        e.relPaths << "campgns/" + cfg;
        if (QFileInfo(gameRoot + "/campgns/" + base).isDir()) {
            e.relPaths << "campgns/" + base;
        }
        out << e;
    }

    // Map packs: levels/*.cfg (+ the folders it references)
    const QStringList packCfgs = QDir(gameRoot + "/levels").entryList(QStringList{"*.cfg"}, QDir::Files);
    for (const QString &cfg : packCfgs) {
        const QString base = QFileInfo(cfg).completeBaseName();
        InstalledEntry e;
        e.kind = "Map pack";
        e.stock = kStockLevelCfgs.contains(base, Qt::CaseInsensitive);
        const QString cfgPath = gameRoot + "/levels/" + cfg;
        e.name = cfgValue(cfgPath, "NAME");
        if (e.name.isEmpty()) {
            e.name = base;
        }
        e.relPaths << "levels/" + cfg;
        for (const QString &key : {"LEVELS_LOCATION", "CREATURES_LOCATION",
                                   "CONFIGS_LOCATION", "MEDIA_LOCATION"}) {
            const QString loc = cfgValue(cfgPath, key);
            if (!loc.isEmpty() && QFileInfo(gameRoot + "/" + loc).isDir()) {
                e.relPaths << loc;
            }
        }
        out << e;
    }

    // Standalone maps: levels/personal/map*.lof (always user content)
    const QString personalDir = gameRoot + "/levels/personal";
    const QStringList lofs = QDir(personalDir).entryList(QStringList{"map*.lof"}, QDir::Files);
    for (const QString &lof : lofs) {
        const QString stem = QFileInfo(lof).completeBaseName(); // map00225
        InstalledEntry e;
        e.kind = "Map";
        e.stock = false;
        e.name = cfgValue(personalDir + "/" + lof, "NAME_TEXT");
        if (e.name.isEmpty()) {
            e.name = QObject::tr("Personal map %1").arg(stem);
        }
        const QStringList mapFiles = QDir(personalDir).entryList(QStringList{stem + ".*"}, QDir::Files);
        for (const QString &f : mapFiles) {
            e.relPaths << "levels/personal/" + f;
        }
        out << e;
    }

    // Mods: mods/*/ (always user content; the Mod Manager owns these too)
    const QStringList modDirs = QDir(gameRoot + "/mods").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &m : modDirs) {
        InstalledEntry e;
        e.kind = "Mod";
        e.stock = false;
        e.name = cfgValue(gameRoot + "/mods/" + m + "/mod.cfg", "Name");
        if (e.name.isEmpty()) {
            e.name = m;
        }
        e.relPaths << "mods/" + m;
        out << e;
    }

    return out;
}

QString backupRoot(const QString &gameRoot)
{
    return gameRoot + "/.kfx-uninstalled";
}

QVector<BackupEntry> scanBackups(const QString &gameRoot)
{
    QVector<BackupEntry> out;
    const QStringList manifests =
        QDir(backupRoot(gameRoot)).entryList(QStringList{"*.json"}, QDir::Files);
    for (const QString &mf : manifests) {
        QFile f(backupRoot(gameRoot) + "/" + mf);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        BackupEntry b;
        b.token = QFileInfo(mf).completeBaseName();
        b.name = o["name"].toString();
        b.kind = o["kind"].toString();
        b.stock = o["stock"].toBool();
        for (const QJsonValue &v : o["paths"].toArray()) {
            b.relPaths << v.toString();
        }
        out << b;
    }
    return out;
}

QString sanitizeToken(const QString &s)
{
    QString t;
    for (const QChar &c : s) {
        t += (c.isLetterOrNumber() ? c : QChar('_'));
    }
    return t.left(40);
}

// Normalise a name for fuzzy install-state matching (lowercase, alnum only), so
// a catalogue "Easter Egg Level" can match an installed "Easter Egg".
QString normName(const QString &s)
{
    QString t;
    for (const QChar &c : s) {
        if (c.isLetterOrNumber()) {
            t += c.toLower();
        }
    }
    return t;
}

// Total size (bytes) of a directory tree.
qint64 folderSize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString originLabel(bool stock)
{
    return stock ? QObject::tr("🏰 KeeperFX stock") : QObject::tr("🌐 Workshop / user");
}

// --- Catalogue disk cache (stale-while-revalidate) ---
QString catalogCachePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return dir + "/workshop-catalog.json";
}

QVector<QJsonObject> loadCachedCatalog()
{
    QVector<QJsonObject> out;
    QFile f(catalogCachePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        out.append(v.toObject());
    }
    return out;
}

void saveCachedCatalog(const QVector<QJsonObject> &items)
{
    const QString path = catalogCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray arr;
    for (const QJsonObject &o : items) {
        arr.append(o);
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        f.close();
    }
}

} // namespace

WorkshopBrowserDialog::WorkshopBrowserDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Workshop — browse & install"));
    resize(760, 580);
    setMinimumSize(560, 420);

    auto *root = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildBrowseTab(), tr("Browse && install"));
    tabs->addTab(buildInstalledTab(), tr("Installed"));
    root->addWidget(tabs, 1);

    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) {
            rescanInstalled();
        }
    });

    // --- Bottom bar: status + website link + close (shared) ---
    auto *bottom = new QHBoxLayout();
    statusLabel = new QLabel(tr("Loading workshop…"), this);
    bottom->addWidget(statusLabel, 1);
    auto *websiteButton = new QPushButton(tr("Open website"), this);
    connect(websiteButton, &QPushButton::clicked, this,
            []() { QDesktopServices::openUrl(QUrl("https://keeperfx.net/workshop")); });
    bottom->addWidget(websiteButton);
    auto *closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(closeButton);
    root->addLayout(bottom);

    fetchCatalog();
    rescanInstalled();
}

QWidget *WorkshopBrowserDialog::buildBrowseTab()
{
    auto *tab = new QWidget(this);
    auto *v = new QVBoxLayout(tab);

    auto *controls = new QHBoxLayout();
    categoryBox = new QComboBox(tab);
    for (const auto &c : kCategories) {
        categoryBox->addItem(c.first, c.second);
    }
    categoryBox->setCurrentIndex(1); // default to "Map"

    sortBox = new QComboBox(tab);
    sortBox->addItem(tr("Highest rated"), "rating");
    sortBox->addItem(tr("Most downloaded"), "downloads");
    sortBox->addItem(tr("Newest"), "newest");

    searchEdit = new QLineEdit(tab);
    searchEdit->setPlaceholderText(tr("Search by name…"));
    searchEdit->setClearButtonEnabled(true);

    controls->addWidget(new QLabel(tr("Category:"), tab));
    controls->addWidget(categoryBox);
    controls->addWidget(new QLabel(tr("Sort:"), tab));
    controls->addWidget(sortBox);
    controls->addWidget(searchEdit, 1);
    v->addLayout(controls);

    auto *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto *listContainer = new QWidget(scroll);
    listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);
    scroll->setWidget(listContainer);
    v->addWidget(scroll, 1);

    connect(categoryBox, &QComboBox::currentIndexChanged, this, &WorkshopBrowserDialog::applyFilter);
    connect(sortBox, &QComboBox::currentIndexChanged, this, &WorkshopBrowserDialog::applyFilter);
    connect(searchEdit, &QLineEdit::textChanged, this, &WorkshopBrowserDialog::applyFilter);

    return tab;
}

QWidget *WorkshopBrowserDialog::buildInstalledTab()
{
    auto *tab = new QWidget(this);
    auto *v = new QVBoxLayout(tab);

    // Category + search, mirroring the Browse tab.
    auto *controls = new QHBoxLayout();
    installedCategoryBox = new QComboBox(tab);
    installedCategoryBox->addItem(tr("All categories"), "");
    installedCategoryBox->addItem(tr("Campaigns"), "Campaign");
    installedCategoryBox->addItem(tr("Map packs"), "Map pack");
    installedCategoryBox->addItem(tr("Maps"), "Map");
    installedCategoryBox->addItem(tr("Mods"), "Mod");

    installedSearchEdit = new QLineEdit(tab);
    installedSearchEdit->setPlaceholderText(tr("Search by name…"));
    installedSearchEdit->setClearButtonEnabled(true);

    controls->addWidget(new QLabel(tr("Category:"), tab));
    controls->addWidget(installedCategoryBox);
    controls->addWidget(installedSearchEdit, 1);
    v->addLayout(controls);

    auto *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto *container = new QWidget(scroll);
    installedListLayout = new QVBoxLayout(container);
    installedListLayout->setContentsMargins(0, 0, 0, 0);
    installedListLayout->setSpacing(4);
    scroll->setWidget(container);
    v->addWidget(scroll, 1);

    connect(installedCategoryBox, &QComboBox::currentIndexChanged, this,
            &WorkshopBrowserDialog::renderInstalled);
    connect(installedSearchEdit, &QLineEdit::textChanged, this,
            &WorkshopBrowserDialog::renderInstalled);

    return tab;
}

void WorkshopBrowserDialog::fetchCatalog()
{
    // Stale-while-revalidate: render the disk-cached catalogue instantly (no blank
    // "Loading…" wait), then refresh from the network in the background and quietly
    // update if the workshop changed.
    const QVector<QJsonObject> cached = loadCachedCatalog();
    const bool hadCache = !cached.isEmpty();
    if (hadCache) {
        catalog = cached;
        loaded = true;
        applyFilter();
        statusLabel->setText(tr("Refreshing workshop…"));
    }

    auto *watcher = new QFutureWatcher<QVector<QJsonObject>>(this);
    connect(watcher, &QFutureWatcher<QVector<QJsonObject>>::finished, this,
            [this, watcher, hadCache]() {
                const QVector<QJsonObject> fresh = watcher->result();
                watcher->deleteLater();

                if (fresh.isEmpty()) {
                    // Network failed — keep showing the cache if we have one.
                    statusLabel->setText(hadCache ? tr("Offline — showing cached workshop.")
                                                  : tr("Could not load the workshop (offline?)."));
                    return;
                }

                // Diff by item id for a subtle "updated" note.
                QSet<int> oldIds, newIds;
                for (const QJsonObject &o : std::as_const(catalog)) {
                    oldIds.insert(o["id"].toInt());
                }
                for (const QJsonObject &o : std::as_const(fresh)) {
                    newIds.insert(o["id"].toInt());
                }
                int added = 0, removed = 0;
                for (int id : std::as_const(newIds)) {
                    if (!oldIds.contains(id)) added++;
                }
                for (int id : std::as_const(oldIds)) {
                    if (!newIds.contains(id)) removed++;
                }

                saveCachedCatalog(fresh);
                catalog = fresh;
                loaded = true;
                applyFilter(); // sets the normal item count in the status bar
                if (hadCache && (added > 0 || removed > 0)) {
                    statusLabel->setText(statusLabel->text()
                                         + tr("  ·  updated (+%1 / −%2)").arg(added).arg(removed));
                }
            });
    watcher->setFuture(QtConcurrent::run([]() {
        QVector<QJsonObject> items;
        const QJsonArray arr = ApiClient::getWorkshopCatalog();
        items.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            items.append(v.toObject());
        }
        return items;
    }));
}

void WorkshopBrowserDialog::applyFilter()
{
    QLayoutItem *child;
    while ((child = listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    if (!loaded) {
        return;
    }

    const int wantedCat = categoryBox->currentData().toInt();
    const QString needle = searchEdit->text().trimmed();

    QVector<QJsonObject> filtered;
    for (const QJsonObject &item : std::as_const(catalog)) {
        if (wantedCat != -1 && item["category"].toInt() != wantedCat) {
            continue;
        }
        if (!needle.isEmpty()
            && !item["name"].toString().contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(item);
    }

    const QString sortKey = sortBox->currentData().toString();
    std::sort(filtered.begin(), filtered.end(),
              [&sortKey](const QJsonObject &a, const QJsonObject &b) {
                  if (sortKey == "downloads") {
                      return a["downloadCount"].toInt() > b["downloadCount"].toInt();
                  }
                  if (sortKey == "newest") {
                      return itemCreatedDate(a) > itemCreatedDate(b);
                  }
                  const double ra = a["ratingScore"].toDouble();
                  const double rb = b["ratingScore"].toDouble();
                  if (ra != rb) {
                      return ra > rb;
                  }
                  return a["downloadCount"].toInt() > b["downloadCount"].toInt();
              });

    const int kMaxRows = 150;
    const int shown = qMin(filtered.size(), kMaxRows);

    // Best-effort "already installed" detection by normalised name.
    QStringList installedNorms;
    for (const InstalledEntry &e : std::as_const(installedCache)) {
        installedNorms << normName(e.name);
    }

    for (int i = 0; i < shown; ++i) {
        const QJsonObject item = filtered.at(i);
        const QString itemNorm = normName(item["name"].toString());
        bool isInstalled = false;
        for (const QString &n : std::as_const(installedNorms)) {
            if (!n.isEmpty() && !itemNorm.isEmpty()
                && (n == itemNorm || (n.size() >= 4 && itemNorm.contains(n))
                    || (itemNorm.size() >= 4 && n.contains(itemNorm)))) {
                isInstalled = true;
                break;
            }
        }

        auto *row = new QFrame();
        row->setFrameShape(QFrame::StyledPanel);
        auto *rowLayout = new QHBoxLayout(row);

        auto *thumb = new QLabel(row);
        thumb->setFixedSize(96, 54);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setText("…");
        rowLayout->addWidget(thumb);

        auto *info = new QVBoxLayout();
        auto *name = new QLabel(item["name"].toString(), row);
        QFont nameFont = name->font();
        nameFont.setBold(true);
        name->setFont(nameFont);
        info->addWidget(name);
        info->addWidget(new QLabel(
            tr("%1 · by %2").arg(categoryLabel(item["category"].toInt()), itemAuthor(item)), row));
        const double rating = item["ratingScore"].toDouble();
        info->addWidget(new QLabel(
            tr("★ %1   ⬇ %2 downloads")
                .arg(QString::number(rating, 'f', 1))
                .arg(item["downloadCount"].toInt()),
            row));
        if (isInstalled) {
            auto *badge = new QLabel(tr("✓ Installed"), row);
            badge->setStyleSheet("color: #4caf50;");
            info->addWidget(badge);
        }
        rowLayout->addLayout(info, 1);

        // "Details" opens the item's full page on keeperfx.net (description,
        // screenshots, comments) so users can see exactly what an item is before
        // installing — the name alone often isn't enough.
        auto *detailsButton = new QPushButton(tr("Details"), row);
        detailsButton->setToolTip(tr("Open this item's page on keeperfx.net"));
        const int itemId = item["id"].toInt();
        connect(detailsButton, &QPushButton::clicked, this, [itemId]() {
            QDesktopServices::openUrl(
                QUrl(QString("https://keeperfx.net/workshop/item/%1").arg(itemId)));
        });
        rowLayout->addWidget(detailsButton);

        auto *installButton = new QPushButton(isInstalled ? tr("Reinstall") : tr("Install"), row);
        connect(installButton, &QPushButton::clicked, this,
                [this, item]() { installItem(item); });
        rowLayout->addWidget(installButton);

        listLayout->addWidget(row);

        const QString turl = thumbnailUrl(item);
        if (!turl.isEmpty()) {
            auto *thumbWatcher = new QFutureWatcher<QImage>(thumb);
            connect(thumbWatcher, &QFutureWatcher<QImage>::finished, thumb,
                    [thumbWatcher, thumb]() {
                        const QImage img = thumbWatcher->result();
                        if (!img.isNull()) {
                            thumb->setPixmap(QPixmap::fromImage(img).scaled(
                                thumb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        } else {
                            thumb->setText(QObject::tr("no image"));
                        }
                        thumbWatcher->deleteLater();
                    });
            thumbWatcher->setFuture(QtConcurrent::run(
                [turl]() { return ApiClient::downloadImage(QUrl(turl)); }));
        } else {
            thumb->setText(tr("no image"));
        }
    }

    listLayout->addStretch(1);

    if (filtered.size() > shown) {
        statusLabel->setText(
            tr("Showing %1 of %2 — refine with search.").arg(shown).arg(filtered.size()));
    } else {
        statusLabel->setText(tr("%n item(s).", "", filtered.size()));
    }
}

void WorkshopBrowserDialog::rescanInstalled()
{
    const QString gameRoot = QCoreApplication::applicationDirPath();
    installedCache = scanInstalled(gameRoot);
    backupCache = scanBackups(gameRoot);
    renderInstalled();
}

void WorkshopBrowserDialog::renderInstalled()
{
    if (!installedListLayout) {
        return;
    }
    QLayoutItem *child;
    while ((child = installedListLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    const QString gameRoot = QCoreApplication::applicationDirPath();
    const QString wantKind = installedCategoryBox ? installedCategoryBox->currentData().toString()
                                                  : QString();
    const QString needle = installedSearchEdit ? installedSearchEdit->text().trimmed() : QString();

    auto matches = [&](const QString &kind, const QString &name) {
        if (!wantKind.isEmpty() && kind != wantKind) {
            return false;
        }
        if (!needle.isEmpty() && !name.contains(needle, Qt::CaseInsensitive)) {
            return false;
        }
        return true;
    };

    auto addSectionLabel = [this](const QString &text) {
        auto *l = new QLabel(text);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        installedListLayout->addWidget(l);
    };

    // --- Installed add-ons (filtered) ---
    QVector<InstalledEntry> shown;
    for (const InstalledEntry &e : std::as_const(installedCache)) {
        if (matches(e.kind, e.name)) {
            shown << e;
        }
    }
    addSectionLabel(tr("Installed add-ons (%1)").arg(shown.size()));

    for (const InstalledEntry &entry : std::as_const(shown)) {
        auto *row = new QFrame();
        row->setFrameShape(QFrame::StyledPanel);
        auto *h = new QHBoxLayout(row);

        auto *info = new QVBoxLayout();
        auto *name = new QLabel(entry.name, row);
        QFont nf = name->font();
        nf.setBold(true);
        name->setFont(nf);
        info->addWidget(name);
        info->addWidget(new QLabel(QString("%1 · %2").arg(entry.kind, originLabel(entry.stock)), row));
        h->addLayout(info, 1);

        auto *uninstall = new QPushButton(tr("Uninstall"), row);
        connect(uninstall, &QPushButton::clicked, this, [this, entry, gameRoot]() {
            const QString warn =
                entry.stock
                    ? tr("“%1” ships with KeeperFX. You can Restore it later from this tab.\n\n"
                         "Uninstall it anyway?").arg(entry.name)
                    : tr("Uninstall “%1”? You can Restore it later from this tab.").arg(entry.name);
            if (QMessageBox::question(this, tr("Uninstall"), warn,
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                return;
            }
            const QString token = entry.kind.left(4).toLower() + "_" + sanitizeToken(entry.name)
                                  + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
            const QString bRoot = backupRoot(gameRoot) + "/" + token;
            QStringList movedPaths;
            QStringList failedPaths;
            for (const QString &rel : entry.relPaths) {
                const QString src = gameRoot + "/" + rel;
                if (!QFileInfo::exists(src)) {
                    continue;
                }
                const QString dst = bRoot + "/" + rel;
                QDir().mkpath(QFileInfo(dst).absolutePath());
                if (QDir().rename(src, dst)) {
                    movedPaths << rel;
                } else {
                    failedPaths << rel;
                }
            }

            // A rename that fails used to be dropped silently: the item stayed
            // installed, no message appeared, and the list simply refreshed -- so
            // Uninstall looked like it had done nothing at all. Partial success is
            // worse still, leaving half an add-on behind and a manifest describing
            // only the half that moved.
            if (!failedPaths.isEmpty()) {
                QMessageBox::warning(this, tr("Uninstall failed"),
                    tr("Could not remove %1 of “%2”.\n\n"
                       "The folder may be read-only — on a package-managed install the "
                       "game's data folders are owned by the package manager.\n\n%3")
                        .arg(movedPaths.isEmpty() ? tr("any part") : tr("every part"))
                        .arg(entry.name)
                        .arg(failedPaths.join("\n")));
                if (movedPaths.isEmpty()) {
                    rescanInstalled();
                    return;   // nothing moved: leave no manifest claiming otherwise
                }
            }

            QJsonObject o;
            o["name"] = entry.name;
            o["kind"] = entry.kind;
            o["stock"] = entry.stock;
            QJsonArray paths;
            for (const QString &p : std::as_const(movedPaths)) {
                paths.append(p);
            }
            o["paths"] = paths;
            QFile mf(backupRoot(gameRoot) + "/" + token + ".json");
            if (mf.open(QIODevice::WriteOnly)) {
                mf.write(QJsonDocument(o).toJson());
                mf.close();
            }
            rescanInstalled();
        });
        h->addWidget(uninstall);
        installedListLayout->addWidget(row);
    }

    // --- Uninstalled (restorable), also filtered ---
    QVector<BackupEntry> shownBackups;
    for (const BackupEntry &b : std::as_const(backupCache)) {
        if (matches(b.kind, b.name)) {
            shownBackups << b;
        }
    }
    if (!shownBackups.isEmpty()) {
        // Header row: count + total backup size + "Empty all backups".
        const qint64 sz = folderSize(backupRoot(gameRoot));
        auto *hdr = new QWidget();
        auto *hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(0, 0, 0, 0);
        auto *hlabel = new QLabel(tr("Uninstalled — restorable (%1) · %2 MB total")
                                      .arg(shownBackups.size())
                                      .arg(QString::number(sz / (1024.0 * 1024.0), 'f', 1)));
        QFont hf = hlabel->font();
        hf.setBold(true);
        hlabel->setFont(hf);
        hl->addWidget(hlabel, 1);
        auto *emptyAll = new QPushButton(tr("Empty all backups"), hdr);
        connect(emptyAll, &QPushButton::clicked, this, [this, gameRoot]() {
            if (QMessageBox::question(
                    this, tr("Empty backups"),
                    tr("Permanently delete ALL uninstalled backups? Restore will no longer be "
                       "possible for any of them."),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                return;
            }
            QDir(backupRoot(gameRoot)).removeRecursively();
            rescanInstalled();
        });
        hl->addWidget(emptyAll);
        installedListLayout->addWidget(hdr);

        for (const BackupEntry &entry : std::as_const(shownBackups)) {
            auto *row = new QFrame();
            row->setFrameShape(QFrame::StyledPanel);
            auto *h = new QHBoxLayout(row);

            auto *info = new QVBoxLayout();
            info->addWidget(new QLabel(entry.name, row));
            info->addWidget(new QLabel(QString("%1 · %2").arg(entry.kind, originLabel(entry.stock)), row));
            h->addLayout(info, 1);

            auto *restore = new QPushButton(tr("Restore"), row);
            connect(restore, &QPushButton::clicked, this, [this, entry, gameRoot]() {
                const QString bRoot = backupRoot(gameRoot) + "/" + entry.token;
                for (const QString &rel : entry.relPaths) {
                    const QString src = bRoot + "/" + rel;
                    const QString dst = gameRoot + "/" + rel;
                    if (!QFileInfo::exists(src)) {
                        continue;
                    }
                    QDir().mkpath(QFileInfo(dst).absolutePath());
                    QDir().rename(src, dst);
                }
                QDir(bRoot).removeRecursively();
                QFile::remove(backupRoot(gameRoot) + "/" + entry.token + ".json");
                rescanInstalled();
            });
            h->addWidget(restore);

            auto *purge = new QPushButton(tr("Delete permanently"), row);
            connect(purge, &QPushButton::clicked, this, [this, entry, gameRoot]() {
                if (QMessageBox::question(
                        this, tr("Delete permanently"),
                        tr("Permanently delete the backup of “%1”? This cannot be undone.")
                            .arg(entry.name),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                    != QMessageBox::Yes) {
                    return;
                }
                QDir(backupRoot(gameRoot) + "/" + entry.token).removeRecursively();
                QFile::remove(backupRoot(gameRoot) + "/" + entry.token + ".json");
                rescanInstalled();
            });
            h->addWidget(purge);
            installedListLayout->addWidget(row);
        }
    }

    installedListLayout->addStretch(1);
}

long WorkshopBrowserDialog::ourGameBuild() const
{
    QFile f(QCoreApplication::applicationDirPath() + "/version.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    const QString versionField = QString::fromUtf8(f.readAll()).trimmed().section(' ', 0, 0);
    const QStringList parts = versionField.split('.');
    return parts.isEmpty() ? 0 : parts.last().toLong();
}

void WorkshopBrowserDialog::installItem(const QJsonObject &item)
{
    const QString name = item["name"].toString();
    const int id = item["id"].toInt();

    if (item["minGameBuild"].isDouble()) {
        const long minBuild = static_cast<long>(item["minGameBuild"].toDouble());
        const long ours = ourGameBuild();
        if (ours > 0 && minBuild > ours) {
            if (QMessageBox::warning(
                    this, tr("Needs a newer KeeperFX"),
                    tr("“%1” was made for KeeperFX build %2, but you have %3.\n\n"
                       "It may not load correctly. Install anyway?")
                        .arg(name).arg(minBuild).arg(ours),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                return;
            }
        }
    }

    if (item["isLastFileBroken"].toBool()) {
        if (QMessageBox::warning(
                this, tr("Marked broken"),
                tr("The latest file for “%1” is flagged as broken on the workshop.\n\n"
                   "Download and install it anyway?").arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
    }

    const QUrl downloadUrl = ApiClient::getWorkshopItemDownloadUrl(id);
    if (downloadUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Install failed"),
                             tr("Could not get a download link for “%1”.").arg(name));
        return;
    }

    const QString gameRoot = QCoreApplication::applicationDirPath();
    QString suffix = QFileInfo(downloadUrl.path()).suffix();
    if (suffix.isEmpty()) {
        suffix = "7z";
    }
    const QString tmpArchive = gameRoot + "/.kfx-workshop-download." + suffix;

    // Modal progress via a plain QProgressBar (NOT QProgressDialog — its setValue
    // re-enters the event loop and corrupts the in-flight QNetworkReply).
    QDialog progressDlg(this);
    progressDlg.setWindowTitle(tr("Installing"));
    progressDlg.setModal(true);
    auto *progressLayout = new QVBoxLayout(&progressDlg);
    progressLayout->addWidget(new QLabel(tr("Downloading “%1”…").arg(name), &progressDlg));
    auto *progressBar = new QProgressBar(&progressDlg);
    progressBar->setRange(0, 0);
    progressLayout->addWidget(progressBar);
    progressDlg.resize(380, 100);
    progressDlg.show();

    QFile *outFile = new QFile(tmpArchive);
    bool downloadOk = false;
    QEventLoop loop;

    Downloader *downloader = new Downloader(this);
    connect(downloader, &Downloader::downloadProgress, &progressDlg,
            [progressBar](qint64 received, qint64 total) {
                if (total > 0) {
                    progressBar->setRange(0, static_cast<int>(total / 1024));
                    progressBar->setValue(static_cast<int>(received / 1024));
                }
            });
    connect(downloader, &Downloader::downloadCompleted, &progressDlg, [&](bool success) {
        downloadOk = success;
        loop.quit();
    });
    downloader->download(downloadUrl, outFile);
    loop.exec();
    progressDlg.hide();
    // Read the reason before scheduling deletion: deleteLater() only defers the
    // delete to the event loop, so touching the object afterwards happens to work
    // and is exactly the kind of ordering that breaks later.
    const QString downloadError = downloader->lastError();
    downloader->deleteLater();

    if (!downloadOk) {
        // Include the reason. Without it "Could not download" covers a dead link, a
        // full disk and a network outage alike, and the first diagnosis of a bad
        // download URL took an evening rather than a glance.
        const QString reason = downloadError;
        QFile::remove(tmpArchive);
        QString message = tr("Could not download “%1”.").arg(name);
        if (!reason.isEmpty()) {
            message += "\n\n" + reason;
        }
        QMessageBox::warning(this, tr("Install failed"), message);
        return;
    }

    const AddonInstaller::Result result = AddonInstaller::installArchive(tmpArchive, gameRoot);
    QFile::remove(tmpArchive);

    if (!result.ok) {
        QMessageBox::warning(this, tr("Install failed"), result.error);
        return;
    }
    if (!result.foundContent) {
        QMessageBox::warning(this, tr("Nothing to install"),
                             tr("No installable content was found inside “%1”.").arg(name));
        return;
    }

    rescanInstalled(); // the new item shows up in the Installed tab
    applyFilter();     // refresh Browse badges (item is now marked installed)

    QString body = tr("Installed “%1”:").arg(name) + "\n\n• " + result.lines.join("\n• ") + "\n\n";
    body += tr("Campaigns, maps and map packs appear in the game (Land selection / free play). "
               "Mods appear in the Mods manager — enable them there. "
               "You can remove any add-on from the “Installed” tab.");
    QMessageBox::information(this, tr("Installed"), body);
}
