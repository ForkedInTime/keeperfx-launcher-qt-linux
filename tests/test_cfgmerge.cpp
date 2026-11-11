// Standalone tests for how a saved .cfg file is written: the player's own file
// keeps its comments, order and layout; the values change in place; a setting it
// lacks arrives with the release's comment about it.
//
// Build and run:
//   tests/run.sh
#include "../src/cfgmerge.h"

#include <QCoreApplication>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++g_failures;
	}
}

static void expectText(const QString &got, const QString &want, const char *what)
{
	expect(got == want, what);
	if (got != want) {
		std::printf("--- got:\n%s\n--- want:\n%s\n---\n", qPrintable(got), qPrintable(want));
	}
}

// What SettingsCfgFormat::readFile would load from `text`.
static QSettings::SettingsMap load(const QString &text)
{
	QSettings::SettingsMap map;
	for (const QString &line : text.split('\n')) {
		QString key;
		QString value;
		if (CfgMerge::parseSetting(line, &key, &value)) {
			map.insert(key, value);
		}
	}
	return map;
}

static QSettings::SettingsMap with(QSettings::SettingsMap map, const QString &key, const QVariant &value)
{
	map.insert(key, value);
	return map;
}

static QSettings::SettingsMap without(QSettings::SettingsMap map, const QString &key)
{
	map.remove(key);
	return map;
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	// The release's copy, as the AppImage and the in-launcher update leave it next
	// to the player's file.
	const QString defaults = QStringLiteral(
		"; Path to the DK files.\n"
		"INSTALL_PATH=./\n"
		"\n"
		"; Language definition.\n"
		"LANGUAGE=ENG\n"
		"\n"
		"; Video resolutions are in the format WxHx32.\n"
		"\n"
		"; Three frontend resolutions.\n"
		"; Set movies and menu same as ingame resolution.\n"
		"FRONTEND_RES=640x480w32 DESKTOP DESKTOP\n"
		"\n"
		"; Vertical sync.\n"
		"VSYNC=ON\n");

	// A player's file: their own notes, their own order, a setting they turned
	// off by commenting it out, and no VSYNC yet (an older release).
	const QString player = QStringLiteral(
		"; my settings -- do not lose these\n"
		"LANGUAGE=POL\n"
		"\n"
		"# windowed on the laptop\n"
		"FRONTEND_RES=1280x800w32 1280x800w32 1280x800w32\n"
		";FRONTEND_RES=DESKTOP DESKTOP DESKTOP\n"
		"INSTALL_PATH=./\n");

	{
		// The launcher's start: the player's file plus what _keeperfx.cfg adds.
		QSettings::SettingsMap map = load(player);
		map.insert("VSYNC", "ON");
		const QString written = CfgMerge::merge(player, defaults, map, true);
		expectText(written, QStringLiteral(
			"; my settings -- do not lose these\n"
			"LANGUAGE=POL\n"
			"\n"
			"# windowed on the laptop\n"
			"FRONTEND_RES=1280x800w32 1280x800w32 1280x800w32\n"
			";FRONTEND_RES=DESKTOP DESKTOP DESKTOP\n"
			"INSTALL_PATH=./\n"
			"\n"
			"; Vertical sync.\n"
			"VSYNC=ON\n"),
			"the player's comments, order and values stay; a new setting comes with its comment");
		expect(load(written) == map, "the written file loads back as every setting");
		expectText(CfgMerge::merge(written, defaults, map, true), written, "saving again changes nothing");
	}

	{
		// A change in the settings window.
		const QSettings::SettingsMap map = with(load(player), "LANGUAGE", "GER");
		const QString written = CfgMerge::merge(player, defaults, map, true);
		expect(written.contains("; my settings -- do not lose these\nLANGUAGE=GER\n"), "a value changes where it is");
		expect(written.contains("# windowed on the laptop\n"), "a '#' comment stays");
		expect(written.contains(";FRONTEND_RES=DESKTOP DESKTOP DESKTOP\n"), "a commented-out setting stays commented out");
		expect(!written.contains("Language definition"), "the release's comments are not mixed in for settings the player has");
	}

	{
		// The settings window drops DISABLE_SPLASH_SCREENS for engines that use STARTUP.
		const QString old = QStringLiteral("LANGUAGE=ENG\nDISABLE_SPLASH_SCREENS=true\nVSYNC=ON\n");
		const QSettings::SettingsMap map = with(without(load(old), "DISABLE_SPLASH_SCREENS"), "STARTUP", "LEGAL FX");
		const QString written = CfgMerge::merge(old, defaults, map, true);
		expect(!written.contains("DISABLE_SPLASH_SCREENS"), "a setting the launcher removed is gone from the file");
		expect(load(written) == map, "and the file loads back as exactly the settings");
	}

	{
		// Lines the launcher cannot read are not its to change.
		const QString odd = QStringLiteral("LANGUAGE=ENG\nCOMMAND_LINE=a=b\nnot a setting\nVSYNC=ON\n");
		const QString written = CfgMerge::merge(odd, defaults, with(load(odd), "VSYNC", "OFF"), true);
		expect(written.contains("COMMAND_LINE=a=b\n"), "a line with two '=' stays as it was");
		expect(written.contains("not a setting\n"), "a line with no '=' stays as it was");
		expect(written.contains("VSYNC=OFF"), "the setting next to them still changes");
	}

	{
		// No file yet: the release's copy is the start, comments and all.
		const QSettings::SettingsMap map = with(load(defaults), "LANGUAGE", "FRE");
		const QString written = CfgMerge::merge(QString(), defaults, map, true);
		expect(written.startsWith("; Path to the DK files.\nINSTALL_PATH=./\n"), "a new file starts as the release's copy");
		expect(written.contains("; Language definition.\nLANGUAGE=FRE\n"), "with the value put in");
		expect(written.contains("; Video resolutions are in the format WxHx32.\n"), "keeping the release's free-standing comments");
		expect(load(written) == map, "and loads back as every setting");
	}

	{
		// Several new settings: in the release's order, each with its own comment.
		const QString bare = QStringLiteral("INSTALL_PATH=./\n");
		QSettings::SettingsMap map = load(bare);
		map.insert("VSYNC", "ON");
		map.insert("LANGUAGE", "ENG");
		map.insert("FRONTEND_RES", "640x480w32 DESKTOP DESKTOP");
		map.insert("API_ENABLED", "false");
		const QString written = CfgMerge::merge(bare, defaults, map, true);
		expectText(written, QStringLiteral(
			"INSTALL_PATH=./\n"
			"\n"
			"; Language definition.\n"
			"LANGUAGE=ENG\n"
			"\n"
			"; Three frontend resolutions.\n"
			"; Set movies and menu same as ingame resolution.\n"
			"FRONTEND_RES=640x480w32 DESKTOP DESKTOP\n"
			"\n"
			"; Vertical sync.\n"
			"VSYNC=ON\n"
			"\n"
			"API_ENABLED=false\n"),
			"new settings follow the release's order with their comments; unknown ones last");
		expectText(CfgMerge::merge(written, defaults, map, true), written, "saving again changes nothing");
	}

	{
		// Two settings under one comment in the release's copy stay together.
		const QString pair = QStringLiteral(
			"; Trash limits, whichever is hit first.\n"
			"TRASH_MAX_COUNT=10\n"
			"TRASH_MAX_DAYS=30\n");
		const QString bare = QStringLiteral("LANGUAGE=ENG\n");
		QSettings::SettingsMap map = load(bare);
		map.insert("TRASH_MAX_COUNT", "10");
		map.insert("TRASH_MAX_DAYS", "30");
		expectText(CfgMerge::merge(bare, pair, map, true), QStringLiteral(
			"LANGUAGE=ENG\n"
			"\n"
			"; Trash limits, whichever is hit first.\n"
			"TRASH_MAX_COUNT=10\n"
			"TRASH_MAX_DAYS=30\n"),
			"settings on neighbouring lines of the release's copy are added together");
	}

	{
		// The launcher's own file: no release copy, no blank lines.
		const QString launcher = QStringLiteral("; launcher\nCHECK_FOR_UPDATES_RELEASE=ALPHA\n");
		const QSettings::SettingsMap map = with(load(launcher), "WEBSITE_INTEGRATION", "true");
		expectText(CfgMerge::merge(launcher, QString(), map, false),
			QStringLiteral("; launcher\nCHECK_FOR_UPDATES_RELEASE=ALPHA\nWEBSITE_INTEGRATION=true\n"),
			"a launcher setting is added straight after the others");
	}

	{
		// A file without a final newline, and one with blank lines at the top.
		const QString noFinal = QStringLiteral("LANGUAGE=ENG");
		expectText(CfgMerge::merge(noFinal, QString(), with(load(noFinal), "LANGUAGE", "ITA"), true),
			QStringLiteral("LANGUAGE=ITA"), "no final newline stays that way");
		const QString leading = QStringLiteral("\n\nLANGUAGE=ENG\n");
		expectText(CfgMerge::merge(leading, QString(), load(leading), true),
			QStringLiteral("LANGUAGE=ENG\n"), "blank lines at the top are dropped");
	}

	{
		// A line with nothing before its '=' is not a setting: it stays once.
		const QString empty = QStringLiteral("LANGUAGE=ENG\n=5\n");
		expect(!CfgMerge::parseSetting("=5"), "'=5' is not read as a setting");
		const QString once = CfgMerge::merge(empty, QString(), load(empty), true);
		expectText(once, empty, "it stays as it was");
		expectText(CfgMerge::merge(once, QString(), load(once), true), empty, "and is not added again by the next save");
	}

	{
		// Every setting cleared (what Settings::resetKfxSettings does before it fills the
		// release's defaults back in): the comments stay, no setting line does.
		const QString written = CfgMerge::merge(player, defaults, QSettings::SettingsMap(), true);
		expect(load(written).isEmpty(), "a cleared map leaves no setting");
		expect(written.contains("; my settings -- do not lose these\n"), "but keeps the comments");
		const QString refilled = CfgMerge::merge(player, defaults, load(defaults), true);
		expect(refilled.contains("; my settings -- do not lose these\nLANGUAGE=ENG\n"), "reset to the defaults: values back to the release's, in place");
		expect(load(refilled) == load(defaults), "and exactly the release's settings");
	}

	if (g_failures) {
		std::printf("%d cfg merge test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all cfg merge tests passed\n");
	return 0;
}
