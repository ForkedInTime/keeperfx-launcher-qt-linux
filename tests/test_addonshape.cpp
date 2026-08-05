// Tests for archive-shape detection.
//
// The bug these exist for: a workshop campaign ships "<name>.cfg" beside
// "<name>/" at the archive root, and nothing recognised that shape. The campaign's
// own levels are map*.slb inside "<name>/", so the standalone-map detector matched
// instead and the installer scattered the maps into levels/personal, dropped the
// campaign, and called it a success.
//
// Build and run:
//   tests/run.sh
#include "../src/addonshape.h"

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

static void expectEq(const QString &got, const QString &wanted, const char *what)
{
	if (got == wanted) {
		std::printf("  ok   %s\n", what);
	} else {
		std::printf("  FAIL %s: got '%s', wanted '%s'\n", what,
			qUtf8Printable(got), qUtf8Printable(wanted));
		++g_failures;
	}
}

static void touch(const QString &path)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (f.open(QIODevice::WriteOnly)) { f.write("x"); f.close(); }
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QTemporaryDir tmp;
	if (!tmp.isValid()) { std::printf("  FAIL no temp dir\n"); return 1; }
	const QString root = tmp.path();

	// Pandaemonium's real shape, including the map*.slb files that made the
	// standalone-map detector claim it.
	const QString camp = root + "/campaign";
	touch(camp + "/pandaemonium.cfg");
	touch(camp + "/pandaemonium/map00001.slb");
	touch(camp + "/pandaemonium/map00001.clm");
	touch(camp + "/pandaemonium_configs/archer.cfg");
	touch(camp + "/pandaemonium_land/land01.png");
	touch(camp + "/pandaemonium_media/intro.png");
	touch(camp + "/readme.txt");

	expectEq(find_loose_campaign(camp), "pandaemonium", "a loose campaign is recognised by cfg + dir");

	const QStringList entries = loose_campaign_entries(camp, "pandaemonium");
	expect(entries.contains("pandaemonium.cfg"), "the campaign cfg is claimed");
	expect(entries.contains("pandaemonium"), "the campaign directory is claimed");
	expect(entries.contains("pandaemonium_configs"), "_configs is claimed");
	expect(entries.contains("pandaemonium_land"), "_land is claimed");
	expect(entries.contains("pandaemonium_media"), "_media is claimed");
	expect(!entries.contains("readme.txt"), "unrelated files are NOT installed");
	expect(entries.size() == 5, "exactly the campaign's own entries are claimed");

	// A cfg with no matching directory is not a campaign (a mod's config, say).
	const QString notCamp = root + "/notcamp";
	touch(notCamp + "/settings.cfg");
	touch(notCamp + "/data/thing.dat");
	expect(find_loose_campaign(notCamp).isEmpty(), "a lone cfg with no matching folder is not a campaign");

	// A standalone map pack must NOT be mistaken for a campaign.
	const QString maps = root + "/maps";
	touch(maps + "/map00123.slb");
	touch(maps + "/map00123.lif");
	expect(find_loose_campaign(maps).isEmpty(), "a standalone map pack is not a campaign");

	// An empty tree.
	const QString empty = root + "/empty";
	QDir().mkpath(empty);
	expect(find_loose_campaign(empty).isEmpty(), "an empty tree is not a campaign");
	expect(loose_campaign_entries(empty, QString()).isEmpty(), "no base name claims nothing");

	// --- Which folder a loose add-on belongs in --------------------------------
	// The cfg declares it. The website's category cannot be trusted: Modern Keeper
	// is listed as a map pack and its own cfg says campgns/.
	{
		const QString c = root + "/cfgs";
		QDir().mkpath(c);
		QFile f1(c + "/camp.cfg");
		if (f1.open(QIODevice::WriteOnly)) {
			f1.write("; KeeperFX campaign file\n[common]\nLEVELS_LOCATION = campgns/modernk\n"); f1.close();
		}
		QFile f2(c + "/pack.cfg");
		if (f2.open(QIODevice::WriteOnly)) {
			f2.write("; KeeperFX Mappack file\n[common]\nLEVELS_LOCATION = levels/classic\n"); f2.close();
		}
		QFile f3(c + "/quiet.cfg");
		if (f3.open(QIODevice::WriteOnly)) { f3.write("[common]\nNAME = nothing useful\n"); f3.close(); }

		expectEq(loose_addon_install_dir(c + "/camp.cfg"), "campgns", "campgns/ declaration installs as a campaign");
		expectEq(loose_addon_install_dir(c + "/pack.cfg"), "levels", "levels/ declaration installs as a map pack");
		expectEq(loose_addon_install_dir(c + "/quiet.cfg"), "campgns", "a cfg that says nothing defaults to campgns");
		expectEq(loose_addon_install_dir(c + "/missing.cfg"), "campgns", "an unreadable cfg defaults to campgns");
	}

	// --- Mod shapes -------------------------------------------------------------
	{
		// A creature replacement: sprite folders, no mod.cfg, no engine config dir.
		// Each folder is a PART of one mod, not a mod -- treating them separately
		// turned one sprite pack into 27 broken ones.
		const QString sprite = root + "/sprite";
		touch(sprite + "/stand_fp/r1frame01.png");
		touch(sprite + "/attack_td/r1frame01.png");
		touch(sprite + "/icons/portrait.png");
		expect(!looks_like_mod(sprite), "a sprite pack root is not itself a mod folder");
		expect(!looks_like_mod(sprite + "/stand_fp"), "a sprite subfolder is not a mod on its own");

		// A genuine bundle: each subfolder carries its own mod.cfg.
		const QString bundle = root + "/bundle";
		touch(bundle + "/modA/mod.cfg");
		touch(bundle + "/modB/mod.cfg");
		expect(looks_like_mod(bundle + "/modA"), "a folder with mod.cfg is a mod");
		expect(looks_like_mod(bundle + "/modB"), "the second bundled mod is a mod too");

		// An engine config folder also marks a mod.
		const QString cfgmod = root + "/cfgmod";
		touch(cfgmod + "/creatrs/imp.cfg");
		expect(looks_like_mod(cfgmod), "an engine config folder marks a mod");
	}

	if (g_failures > 0) {
		std::printf("\n%d addon shape test(s) FAILED.\n", g_failures);
		return 1;
	}
	std::printf("\nAll addon shape tests passed.\n");
	return 0;
}
