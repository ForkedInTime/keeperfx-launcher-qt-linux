#pragma once
/******************************************************************************/
// Recognising what shape an add-on archive is.
//
// Header-only and QtCore-only so tests/test_addonshape.cpp can exercise it
// without linking the launcher.
/******************************************************************************/
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>

// A KeeperFX campaign is "<name>.cfg" beside a "<name>/" directory, optionally with
// "<name>_*" siblings holding language and land-view data -- keeporig.cfg,
// keeporig/, keeporig_eng/, keeporig_lnd/ and so on. Workshop campaigns ship
// exactly that at the root of the archive, with no campgns/ wrapper around it.
//
// Returns the campaign's base name, or an empty string when the tree is not one.
inline QString find_loose_campaign(const QString & root)
{
	const QStringList cfgs = QDir(root).entryList(QStringList{"*.cfg"}, QDir::Files);
	for (const QString & cfg : cfgs) {
		const QString base = cfg.left(cfg.size() - 4); // strip ".cfg"
		if (!base.isEmpty() && QFileInfo(root + "/" + base).isDir()) {
			return base;
		}
	}
	return QString();
}

// Everything belonging to that campaign: the cfg, its directory, and any "<name>_*"
// siblings. Nothing else in the archive is claimed, so a stray readme or screenshot
// is left where it is rather than being installed into the game.
inline QStringList loose_campaign_entries(const QString & root, const QString & base)
{
	QStringList out;
	if (base.isEmpty()) {
		return out;
	}
	out << base + ".cfg" << base;
	out += QDir(root).entryList(QStringList{base + "_*"}, QDir::Dirs | QDir::NoDotAndDotDot);
	return out;
}

// Engine config folders: a folder containing one of these is a mod even without
// a mod.cfg.
static const QStringList kAddonModContentDirs = {
    "creatrs", "fxdata", "ldata", "cmpgfx", "sound", "data", "lang", "campgns", "levels"
};

// A folder "is a mod" if it has a mod.cfg or any engine config subdirectory.
inline bool looks_like_mod(const QString &dir)
{
    if (QFileInfo::exists(dir + "/mod.cfg")) {
        return true;
    }
    for (const QString &content : kAddonModContentDirs) {
        if (QFileInfo(dir + "/" + content).isDir()) {
            return true;
        }
    }
    return false;
}

inline QString find_standalone_map_dir(const QString &root)
{
    if (!QDir(root).entryList(QStringList{"map*.slb"}, QDir::Files).isEmpty()) {
        return root;
    }
    const QStringList subs = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &s : subs) {
        if (!QDir(root + "/" + s).entryList(QStringList{"map*.slb"}, QDir::Files).isEmpty()) {
            return root + "/" + s;
        }
    }
    return QString();
}

// Where a "<name>.cfg + <name>/" add-on wants to live.
//
// The cfg says so itself: campaign files declare LEVELS_LOCATION = campgns/<name>
// and map packs declare levels/<name>. Reading it beats guessing from the archive
// shape, which cannot tell the two apart -- and beats trusting the website's
// category, which is sometimes wrong (Modern Keeper is listed as a map pack and is
// declared a campaign by its own cfg).
//
// Returns "campgns" or "levels"; campgns when the file says nothing useful, which
// is the commoner shape and matches the previous behaviour.
inline QString loose_addon_install_dir(const QString & cfg_path)
{
	QFile f(cfg_path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QStringLiteral("campgns");
	}
	QTextStream in(&f);
	while (!in.atEnd()) {
		const QString line = in.readLine().trimmed();
		if (!line.startsWith(QStringLiteral("LEVELS_LOCATION"), Qt::CaseInsensitive)) {
			continue;
		}
		const int eq = line.indexOf('=');
		if (eq < 0) {
			continue;
		}
		const QString value = line.mid(eq + 1).trimmed();
		if (value.startsWith(QStringLiteral("levels/"), Qt::CaseInsensitive)) {
			return QStringLiteral("levels");
		}
		if (value.startsWith(QStringLiteral("campgns/"), Qt::CaseInsensitive)) {
			return QStringLiteral("campgns");
		}
	}
	return QStringLiteral("campgns");
}
