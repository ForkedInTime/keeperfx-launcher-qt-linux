// Tests for the add-on copy helpers.
//
// The bug these exist for: a copy that fails must be distinguishable from one
// that succeeds. The previous no-clobber merge returned only a skipped count, so
// a merge into a read-only directory wrote nothing and still reported success --
// and the launcher told the user the add-on was installed.
//
// Build and run:
//   tests/run.sh
#include "../src/copytree.h"

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

static bool writeFile(const QString &path, const QByteArray &data)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly)) return false;
	f.write(data);
	f.close();
	return true;
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	QTemporaryDir tmp;
	if (!tmp.isValid()) {
		std::printf("  FAIL could not create a temporary directory\n");
		return 1;
	}
	const QString root = tmp.path();

	// A campaign-shaped source tree.
	const QString src = root + "/src";
	writeFile(src + "/mycampaign.cfg", "cfg");
	writeFile(src + "/mycampaign/map00001.lif", "lif");
	writeFile(src + "/.git/config", "vcs junk");

	// --- The success case -------------------------------------------------------
	{
		const QString dst = root + "/dst_ok";
		const CopyTreeOutcome r = copy_tree_no_clobber(src, dst);
		expect(r.ok, "a writable destination reports ok");
		expect(r.skipped == 0, "nothing was skipped on a fresh copy");
		expect(QFileInfo::exists(dst + "/mycampaign.cfg"), "the campaign cfg actually landed");
		expect(QFileInfo::exists(dst + "/mycampaign/map00001.lif"), "nested files landed");
		expect(!QFileInfo::exists(dst + "/.git"), "version-control junk is not installed");
	}

	// --- Existing files are kept, and that is still success ---------------------
	{
		const QString dst = root + "/dst_keep";
		writeFile(dst + "/mycampaign.cfg", "PRE-EXISTING");
		const CopyTreeOutcome r = copy_tree_no_clobber(src, dst);
		expect(r.ok, "keeping an existing file is not a failure");
		expect(r.skipped == 1, "the existing file is counted as skipped");
		QFile f(dst + "/mycampaign.cfg");
		expect(f.open(QIODevice::ReadOnly) && f.readAll() == QByteArray("PRE-EXISTING"),
			"the existing file was NOT overwritten");
	}

	// --- The bug: a read-only destination must NOT report success ---------------
	{
		const QString dst = root + "/dst_ro";
		QDir().mkpath(dst);
		// Strip write permission the way a root-owned /usr tree behaves for a user.
		QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::ExeOwner);

		const CopyTreeOutcome r = copy_tree_no_clobber(src, dst);
		expect(!r.ok, "a read-only destination reports FAILURE, not success");
		expect(!QFileInfo::exists(dst + "/mycampaign.cfg"), "and nothing was written");

		// Restore so QTemporaryDir can clean up.
		QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
	}

	// --- copy_tree (clobbering variant) ----------------------------------------
	{
		const QString dst = root + "/dst_replace";
		writeFile(dst + "/mycampaign.cfg", "OLD");
		expect(copy_tree(src, dst), "copy_tree succeeds on a writable destination");
		QFile f(dst + "/mycampaign.cfg");
		expect(f.open(QIODevice::ReadOnly) && f.readAll() == QByteArray("cfg"),
			"copy_tree replaces an existing file");

		const QString ro = root + "/dst_replace_ro";
		QDir().mkpath(ro);
		QFile::setPermissions(ro, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
		expect(!copy_tree(src, ro), "copy_tree reports failure on a read-only destination");
		QFile::setPermissions(ro, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
	}

	// What a merge reports as created: the files it wrote, never the ones it kept.
	// The install record keeps this list and Uninstall moves what it names, so a kept
	// stock map listed here would be taken away with the add-on.
	{
		const QString src = root + "/src_created";
		const QString dst = root + "/dst_created";
		writeFile(src + "/levels/standard/map27194.dat", "new");
		writeFile(src + "/levels/standard/map00301.dat", "archive copy of a stock map");
		writeFile(dst + "/levels/standard/map00301.dat", "STOCK");
		const CopyTreeOutcome r = copy_tree_no_clobber(src, dst);
		expect(r.ok && r.skipped == 1, "a merge over a stock map keeps it");
		expect(r.created == QStringList{dst + "/levels/standard/map27194.dat"},
		       "created lists the new file and not the kept stock one");
		expect(r.keptSame.isEmpty(), "a kept file with other content is not claimed");
	}

	// A Reinstall over an earlier install's copy: identical files are claimed, so the
	// install record can register a map installed before the record existed -- but a
	// link (a package install's stock file) never is.
	{
		const QString src = root + "/src_same";
		const QString dst = root + "/dst_same";
		writeFile(src + "/levels/standard/map27194.dat", "map");
		writeFile(src + "/levels/standard/map00301.dat", "stock");
		writeFile(dst + "/levels/standard/map27194.dat", "map");
		writeFile(root + "/pkg/map00301.dat", "stock");
		QFile::link(root + "/pkg/map00301.dat", dst + "/levels/standard/map00301.dat");
		const CopyTreeOutcome r = copy_tree_no_clobber(src, dst);
		expect(r.ok && r.created.isEmpty() && r.skipped == 2, "a reinstall over both copies writes nothing");
		expect(r.keptSame == QStringList{dst + "/levels/standard/map27194.dat"},
		       "the identical earlier copy is claimed, the identical package link is not");
	}

	if (g_failures > 0) {
		std::printf("\n%d copy tree test(s) FAILED.\n", g_failures);
		return 1;
	}
	std::printf("\nAll copy tree tests passed.\n");
	return 0;
}
