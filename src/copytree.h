#pragma once
/******************************************************************************/
// Recursive copy helpers for add-on installation.
//
// Header-only and QtCore-only, so tests/test_copytree.cpp can exercise them
// without linking the launcher (which would drag in bit7z and LIEF).
/******************************************************************************/
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// Archive junk that should never be installed into the game tree -- version-control
// metadata and OS cruft. Some workshop archives ship a whole .git/ folder.
inline bool copytree_is_junk_entry(const QString & name)
{
	static const QStringList junk = {".git", ".svn", ".hg", "__MACOSX", ".DS_Store", "Thumbs.db"};
	return junk.contains(name, Qt::CaseInsensitive);
}

// Outcome of a no-clobber merge.
//
// `skipped` counts files left alone because they already existed -- that is a
// normal, successful outcome, not a problem.
//
// `ok` is false when a copy actually failed. It exists because the previous
// version returned only the skipped count and dropped QFile::copy()'s result on
// the floor: a merge into a read-only directory copied nothing, reported zero
// skipped, and the caller announced a successful install. On a packaged install
// campgns/ and levels/ resolved into root-owned /usr trees, so every add-on
// install "succeeded" while writing nothing at all.
struct CopyTreeOutcome {
	int  skipped = 0;
	bool ok      = true;
};

// Merge src into dst without ever overwriting an existing destination file, so a
// careless or crafted archive cannot clobber stock campaigns, maps, or another
// add-on's files.
inline CopyTreeOutcome copy_tree_no_clobber(const QString & src, const QString & dst)
{
	CopyTreeOutcome out;
	const QFileInfo info(src);

	if (info.isDir()) {
		if (!QDir().mkpath(dst)) {
			out.ok = false;
			return out;
		}
		const QStringList entries = QDir(src).entryList(
			QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
		for (const QString & entry : entries) {
			if (copytree_is_junk_entry(entry)) {
				continue;
			}
			const CopyTreeOutcome child = copy_tree_no_clobber(src + "/" + entry, dst + "/" + entry);
			out.skipped += child.skipped;
			// Keep going so the caller learns the full picture, but remember the failure.
			out.ok = out.ok && child.ok;
		}
		return out;
	}

	if (QFileInfo::exists(dst)) {
		out.skipped = 1; // already present -- keep it, don't overwrite
		return out;
	}
	out.ok = QFile::copy(src, dst);
	return out;
}

// Plain recursive copy, replacing whatever is at the destination.
inline bool copy_tree(const QString & src, const QString & dst)
{
	const QFileInfo info(src);
	if (info.isDir()) {
		if (!QDir().mkpath(dst)) {
			return false;
		}
		const QStringList entries = QDir(src).entryList(
			QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
		for (const QString & entry : entries) {
			if (copytree_is_junk_entry(entry)) {
				continue;
			}
			if (!copy_tree(src + "/" + entry, dst + "/" + entry)) {
				return false;
			}
		}
		return true;
	}
	QFile::remove(dst);
	return QFile::copy(src, dst);
}
