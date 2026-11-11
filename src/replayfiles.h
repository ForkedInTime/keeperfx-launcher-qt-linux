#pragma once

// The game's replays as the replays dialog lists them.
//
// The engine saves each replay as replays/<folder>/<name>.fxpkt in the game folder: campaign/,
// multiplayer/, freeplay/, a mod's own folder, or replays/ itself. This lists every one of them,
// and any replay put straight into the game folder (one a friend sent, say), newest first (equal
// times by path), with the path relative to the game folder -- what the launcher passes to
// -packetload, since the engine runs in the game folder. Also listed: .pck files, which is what a
// player with "Enable Packetsave" gets (-packetsave <name>.pck; the engine then saves no automatic
// replay) and what older builds wrote; the engine's own replay housekeeping treats both alike. The
// replays folder is found in any case (Replays/ from an archive). Header-only and QtCore-only, so
// tests/test_replayfiles.cpp can check it without the launcher.

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QList>
#include <QString>

#include <algorithm>

namespace ReplayFiles {

struct Entry
{
	QString relativePath; // relative to the game folder, '/' separators
	QString folder;       // first folder under replays/, empty for a file directly in it
	qint64 size = 0;
	QDateTime modified;
};

inline QList<Entry> list(const QDir &gameDir)
{
	QList<Entry> entries;
	const QStringList filters = QStringList() << "*.fxpkt" << "*.pck";

	// replays/, or the same name in another case when that is all there is
	QString replaysName = "replays";
	if (!QFileInfo(gameDir.filePath(replaysName)).isDir()) {
		for (const QString &name : gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			if (name.compare("replays", Qt::CaseInsensitive) == 0) {
				replaysName = name;
				break;
			}
		}
	}
	const QString replaysPath = gameDir.filePath(replaysName);
	const QDir replaysDir(replaysPath);

	auto add = [&](const QFileInfo &info, const QString &folder) {
		Entry entry;
		entry.relativePath = QDir::fromNativeSeparators(gameDir.relativeFilePath(info.absoluteFilePath()));
		entry.folder = folder;
		entry.size = info.size();
		entry.modified = info.lastModified();
		entries << entry;
	};

	for (const QFileInfo &info : gameDir.entryInfoList(filters, QDir::Files)) {
		add(info, QString());
	}
	if (replaysDir.exists()) {
		QDirIterator it(replaysPath, filters, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QFileInfo info(it.next());
			const QStringList parts = QDir::fromNativeSeparators(replaysDir.relativeFilePath(info.absoluteFilePath())).split('/');
			add(info, parts.size() > 1 ? parts.first() : QString());
		}
	}

	std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
		if (a.modified != b.modified) {
			return a.modified > b.modified;
		}
		return a.relativePath < b.relativePath;
	});
	return entries;
}

} // namespace ReplayFiles
