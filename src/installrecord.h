#pragma once
/******************************************************************************/
// What each add-on install created, so the launcher can tell an installed item
// from the game's own content.
//
// A Workshop map ships as levels/standard/mapNNNNN.*: an addition to a stock map
// pack, so it is neither a map pack, a campaign, a personal map nor a mod, and
// nothing on disk sets it apart from the stock maps beside it -- the Standard pack
// itself ships map20202, and outside a package install every stock map is an
// ordinary file too. The Installed tab never listed such a map and Browse kept
// offering "Install" after it was installed. The record is the exact answer: the
// files an install created, under the Workshop item's id.
//
// Header-only and QtCore-only, so tests/test_installrecord.cpp can exercise it
// without linking the launcher (which would drag in bit7z and LIEF).
/******************************************************************************/
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QVector>

struct InstallRecord {
	QString     id;    // Workshop item id, or "file:<archive name>" for a local archive
	QString     name;  // the item's name as the user saw it
	QStringList files; // gameRoot-relative files (or a mod's folder) the install created
};

inline QString install_record_path(const QString & gameRoot)
{
	return gameRoot + "/.kfx-installed.json";
}

// A recorded path is only ever used under the game directory: never absolute, never
// climbing out of it. The record sits in the player's own folder, but Uninstall
// moves whatever it names, so a hand-edited or damaged record must not reach
// anything else.
inline bool install_record_path_is_safe(const QString & rel)
{
	if (rel.isEmpty() || QDir::isAbsolutePath(rel) || rel.startsWith('/') || rel.contains('\\')) {
		return false;
	}
	for (const QString & part : rel.split('/')) {
		if (part.isEmpty() || part == "." || part == "..") {
			return false;
		}
	}
	return true;
}

inline QVector<InstallRecord> load_install_records(const QString & gameRoot)
{
	QVector<InstallRecord> out;
	QFile f(install_record_path(gameRoot));
	if (!f.open(QIODevice::ReadOnly)) {
		return out;
	}
	const QJsonArray items = QJsonDocument::fromJson(f.readAll()).object()["installs"].toArray();
	for (const QJsonValue & v : items) {
		const QJsonObject o = v.toObject();
		InstallRecord r;
		r.id = o["id"].toString();
		r.name = o["name"].toString();
		for (const QJsonValue & p : o["files"].toArray()) {
			const QString rel = p.toString();
			if (install_record_path_is_safe(rel)) {
				r.files << rel;
			}
		}
		if (!r.id.isEmpty()) {
			out << r;
		}
	}
	return out;
}

// Add an install to the record. Installing the same item again keeps what it
// created before and adds what it created this time: a reinstall over an existing
// copy creates nothing, and must not make the item look uninstalled.
inline bool save_install_record(const QString & gameRoot, const InstallRecord & rec)
{
	QVector<InstallRecord> all = load_install_records(gameRoot);
	bool merged = false;
	for (InstallRecord & r : all) {
		if (r.id == rec.id) {
			r.name = rec.name;
			for (const QString & f : rec.files) {
				if (!r.files.contains(f)) {
					r.files << f;
				}
			}
			merged = true;
			break;
		}
	}
	if (!merged) {
		all << rec;
	}

	QJsonArray items;
	for (const InstallRecord & r : std::as_const(all)) {
		QJsonArray files;
		for (const QString & f : r.files) {
			if (install_record_path_is_safe(f)) {
				files.append(f);
			}
		}
		QJsonObject o;
		o["id"] = r.id;
		o["name"] = r.name;
		o["files"] = files;
		items.append(o);
	}
	QJsonObject root;
	root["installs"] = items;

	QSaveFile f(install_record_path(gameRoot));
	if (!f.open(QIODevice::WriteOnly)) {
		return false;
	}
	f.write(QJsonDocument(root).toJson());
	return f.commit();
}

// The recorded files still in place. An item that was uninstalled (its files moved
// to the backup) or deleted by hand has none, and counts as not installed.
inline QStringList install_record_present(const QString & gameRoot, const InstallRecord & rec)
{
	QStringList out;
	for (const QString & rel : rec.files) {
		if (QFileInfo::exists(gameRoot + "/" + rel)) {
			out << rel;
		}
	}
	return out;
}

// Whether rel is one of paths or lies inside one of them (a map pack's folder, a
// mod's folder), so an item another scan already lists is not listed twice.
inline bool install_record_covered(const QString & rel, const QStringList & paths)
{
	for (const QString & p : paths) {
		if (rel == p || rel.startsWith(p + "/")) {
			return true;
		}
	}
	return false;
}
