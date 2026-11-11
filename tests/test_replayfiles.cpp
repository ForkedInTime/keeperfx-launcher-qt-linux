// Standalone tests for listing the game's replays: every .fxpkt the engine saves under
// replays/ (campaign/, multiplayer/, freeplay/, a mod's own folder, or replays/ itself),
// newest first, with paths relative to the game folder for -packetload.
//
// Build and run:
//   tests/run.sh
#include "../src/replayfiles.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++g_failures;
	}
}

// Creates <root>/<relative> with a few bytes and the given modification time.
static void touch(const QDir &root, const QString &relative, const QDateTime &modified)
{
	const QString path = root.filePath(relative);
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		std::printf("  cannot create %s\n", qPrintable(path));
		return;
	}
	file.write("KFXS");
	file.flush(); // a write still buffered at close() would set the time to now again
	file.setFileTime(modified, QFileDevice::FileModificationTime);
	file.close();
}

static QStringList paths(const QList<ReplayFiles::Entry> &entries)
{
	QStringList out;
	for (const ReplayFiles::Entry &entry : entries) {
		out << entry.relativePath;
	}
	return out;
}

static QStringList folders(const QList<ReplayFiles::Entry> &entries)
{
	QStringList out;
	for (const ReplayFiles::Entry &entry : entries) {
		out << entry.folder;
	}
	return out;
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	const QDateTime base = QDateTime::fromString("2026-11-08T18:00:00", Qt::ISODate);

	{
		// lists_every_mode
		QTemporaryDir tmp;
		const QDir game(tmp.path());
		touch(game, "replays/campaign/a.fxpkt", base);
		touch(game, "replays/multiplayer/b.fxpkt", base.addSecs(60));
		touch(game, "replays/freeplay/c.fxpkt", base.addSecs(120));
		touch(game, "replays/mymod/d.fxpkt", base.addSecs(180));
		touch(game, "replays/e.fxpkt", base.addSecs(240));
		const QList<ReplayFiles::Entry> entries = ReplayFiles::list(game);
		expect(paths(entries) == QStringList({"replays/e.fxpkt", "replays/mymod/d.fxpkt", "replays/freeplay/c.fxpkt",
			"replays/multiplayer/b.fxpkt", "replays/campaign/a.fxpkt"}), "every mode's replays, newest first");
		expect(folders(entries) == QStringList({"", "mymod", "freeplay", "multiplayer", "campaign"}),
			"each with the folder it is in (none for replays/ itself)");
		expect(!entries.isEmpty() && entries.first().size == 4, "with its size");
	}

	{
		// ignores_non_replays
		QTemporaryDir tmp;
		const QDir game(tmp.path());
		touch(game, "replays/campaign/notes.txt", base);
		QDir(game.filePath("replays/campaign/x.fxpkt")).mkpath(".");
		touch(game, "old.pck", base);
		expect(ReplayFiles::list(game).isEmpty(), "other files, a folder named .fxpkt and a root .pck are not listed");
	}

	{
		// missing_folder_is_empty
		QTemporaryDir tmp;
		expect(ReplayFiles::list(QDir(tmp.path())).isEmpty(), "no replays folder: nothing listed");
	}

	{
		// paths_are_relative_with_slashes
		QTemporaryDir tmp;
		const QDir game(tmp.path());
		touch(game, QString::fromUtf8("replays/campaign/a b \xc3\xb1.fxpkt"), base);
		const QList<ReplayFiles::Entry> entries = ReplayFiles::list(game);
		expect(entries.size() == 1 && entries.first().relativePath == QString::fromUtf8("replays/campaign/a b \xc3\xb1.fxpkt"),
			"a name with spaces and non-ASCII stays one relative path");
	}

	{
		// root_fxpkt_listed: a replay put straight into the game folder (one a friend sent, say)
		QTemporaryDir tmp;
		const QDir game(tmp.path());
		touch(game, "from-a-friend.fxpkt", base.addSecs(60));
		touch(game, "replays/campaign/a.fxpkt", base);
		const QList<ReplayFiles::Entry> entries = ReplayFiles::list(game);
		expect(paths(entries) == QStringList({"from-a-friend.fxpkt", "replays/campaign/a.fxpkt"}),
			"a replay in the game folder itself is listed too");
		expect(folders(entries) == QStringList({"", "campaign"}), "with no folder");
	}

	{
		// equal times: by path
		QTemporaryDir tmp;
		const QDir game(tmp.path());
		touch(game, "replays/campaign/b.fxpkt", base);
		touch(game, "replays/campaign/a.fxpkt", base);
		expect(paths(ReplayFiles::list(game)) == QStringList({"replays/campaign/a.fxpkt", "replays/campaign/b.fxpkt"}),
			"replays saved at the same time are listed by path");
	}

	if (g_failures) {
		std::printf("%d replay listing test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all replay listing tests passed\n");
	return 0;
}
