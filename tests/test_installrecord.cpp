// Tests for the install record (src/installrecord.h).
//
// The bug these exist for: a Workshop map ships as levels/standard/mapNNNNN.*, an
// addition to the stock Standard pack, and nothing on disk tells it from the stock
// maps beside it. The Installed tab never listed it and Browse kept saying
// "Install" after it was installed. The record names what each install created.
//
// Build and run:
//   tests/run.sh
#include "../src/installrecord.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf(ok ? "  ok   %s\n" : "  FAIL %s\n", what);
	if (!ok) ++g_failures;
}

static void touch(const QString &path)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (f.open(QIODevice::WriteOnly)) f.write("x");
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QTemporaryDir tmp;
	const QString game = tmp.path();

	expect(load_install_records(game).isEmpty(), "no record file: no installs, no error");

	// A Workshop map added to the Standard pack.
	touch(game + "/levels/standard/map27194.dat");
	touch(game + "/levels/standard/map27194.lof");
	InstallRecord map;
	map.id = "870";
	map.name = "6th Haven Siege";
	map.files = {"levels/standard/map27194.dat", "levels/standard/map27194.lof"};
	expect(save_install_record(game, map), "the record is written");
	QVector<InstallRecord> all = load_install_records(game);
	expect(all.size() == 1 && all[0].id == "870" && all[0].name == "6th Haven Siege"
	           && all[0].files == map.files,
	       "and read back as written");
	expect(install_record_present(game, all[0]).size() == 2, "both files are present");

	// A reinstall over the existing copy creates nothing; the item stays installed.
	InstallRecord again = map;
	again.files.clear();
	save_install_record(game, again);
	all = load_install_records(game);
	expect(all.size() == 1 && all[0].files == map.files,
	       "a reinstall that created nothing keeps what the first install created");

	// A second item is kept beside the first.
	InstallRecord other;
	other.id = "file:pack.zip";
	other.name = "pack";
	other.files = {"multiplayer/original/map09999.txt"};
	save_install_record(game, other);
	expect(load_install_records(game).size() == 2, "a second install is added, not swapped in");

	// Uninstall moves the files away: the item counts as not installed.
	QFile::remove(game + "/levels/standard/map27194.dat");
	QFile::remove(game + "/levels/standard/map27194.lof");
	all = load_install_records(game);
	expect(install_record_present(game, all[0]).isEmpty(),
	       "with its files gone, an item has nothing present");

	// Paths that would reach outside the game directory are never used.
	expect(install_record_path_is_safe("levels/standard/map27194.dat"), "a plain relative path is used");
	expect(!install_record_path_is_safe("/home/user/.bashrc"), "an absolute path is refused");
	expect(!install_record_path_is_safe("../../.bashrc"), "a path climbing out is refused");
	expect(!install_record_path_is_safe("levels/../../x"), "a path climbing out midway is refused");
	expect(!install_record_path_is_safe(""), "an empty path is refused");
	expect(!install_record_path_is_safe("levels\\standard"), "a backslash path is refused");
	{
		QFile f(install_record_path(game));
		expect(f.open(QIODevice::WriteOnly), "a hand-edited record can be written");
		f.write(R"({"installs":[{"id":"1","name":"evil","files":["../outside","/etc/passwd","levels/ok.dat"]}]})");
	}
	all = load_install_records(game);
	expect(all.size() == 1 && all[0].files == QStringList{"levels/ok.dat"},
	       "an edited record loses every unsafe path on reading");

	// A damaged record reads as empty rather than failing.
	{
		QFile f(install_record_path(game));
		expect(f.open(QIODevice::WriteOnly), "a damaged record can be written");
		f.write("{not json");
	}
	expect(load_install_records(game).isEmpty(), "a damaged record reads as no installs");

	// Coverage: an item another scan already lists is not listed twice.
	const QStringList paths = {"levels/KeeperTD2", "levels/KeeperTD2.cfg", "mods/mymod"};
	expect(install_record_covered("levels/KeeperTD2/map00001.dat", paths), "a file inside a listed pack is covered");
	expect(install_record_covered("mods/mymod", paths), "a listed mod's own folder is covered");
	expect(!install_record_covered("levels/KeeperTD22/map00001.dat", paths), "a sibling with a longer name is not");
	expect(!install_record_covered("levels/standard/map27194.dat", paths), "a map added to the Standard pack is not");

	if (g_failures > 0) {
		std::printf("\n%d install record test(s) FAILED.\n", g_failures);
		return 1;
	}
	std::printf("\nAll install record tests passed.\n");
	return 0;
}
