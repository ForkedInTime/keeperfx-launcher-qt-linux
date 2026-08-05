// Tests the REAL KfxVersion release-type parsing and version comparison, by
// compiling src/kfxversion.cpp into the test. Only ApiClient's three network
// entry points are stubbed; nothing under test reaches them.
#include "kfxversion.h"
#include "apiclient.h"

#include <QCoreApplication>
#include <cstdio>

QJsonObject ApiClient::getLatestStable() { return QJsonObject(); }
QJsonObject ApiClient::getLatestAlpha() { return QJsonObject(); }
std::optional<QMap<QString, QString>> ApiClient::getGameFileList(KfxVersion::ReleaseType, QString) { return std::nullopt; }

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf(ok ? "  ok   %s\n" : "  FAIL %s\n", what);
	if (!ok) ++g_failures;
}

static const char *typeName(KfxVersion::ReleaseType t)
{
	switch (t) {
		case KfxVersion::ReleaseType::STABLE:    return "STABLE";
		case KfxVersion::ReleaseType::ALPHA:     return "ALPHA";
		case KfxVersion::ReleaseType::PROTOTYPE: return "PROTOTYPE";
		default:                                 return "UNKNOWN";
	}
}

static void expectParse(const char *input, KfxVersion::ReleaseType wantType,
	const char *wantVersion, const char *what)
{
	const KfxVersion::VersionInfo got = KfxVersion::getVersionFromString(QString(input));
	const bool ok = (got.type == wantType) && (got.version == QString(wantVersion));
	if (ok) {
		std::printf("  ok   %s\n", what);
	} else {
		std::printf("  FAIL %s: '%s' -> type=%s version='%s', wanted type=%s version='%s'\n",
			what, input, typeName(got.type), qUtf8Printable(got.version),
			typeName(wantType), wantVersion);
		++g_failures;
	}
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	// --- Release-type detection -------------------------------------------------
	expectParse("1.4.0.5397 alpha", KfxVersion::ReleaseType::ALPHA, "1.4.0.5397",
		"an alpha keeps all four version parts");
	expectParse("v1.4.0.5397-alpha", KfxVersion::ReleaseType::ALPHA, "1.4.0.5397",
		"an alpha tag is recognised too");

	// The fork identifies builds by the fourth part. A stable that discards it
	// cannot be told apart from any other stable on the same 1.4.0 base.
	expectParse("1.4.0.5408", KfxVersion::ReleaseType::STABLE, "1.4.0.5408",
		"a stable keeps the build number");
	expectParse("v1.4.0.5408", KfxVersion::ReleaseType::STABLE, "1.4.0.5408",
		"a stable tag keeps the build number");

	// --- Version comparison -----------------------------------------------------
	expect(KfxVersion::isNewerVersion("1.4.0.5408", "1.4.0.5397"),
		"a higher build is newer");
	expect(!KfxVersion::isNewerVersion("1.4.0.5397", "1.4.0.5408"),
		"a lower build is not newer");
	expect(!KfxVersion::isNewerVersion("1.4.0.5397", "1.4.0.5397"),
		"the same version is not newer than itself");

	// A stable install must not be told to update to the build it is already on.
	expect(!KfxVersion::isNewerVersion("1.4.0.5408", "1.4.0.5408"),
		"a stable is not offered the build it already runs");

	// Ordering must respect significance: a big build number on an older base is
	// NOT newer. Comparing part-by-part without stopping at the first lower part
	// gets this backwards.
	expect(!KfxVersion::isNewerVersion("1.3.0.9999", "1.4.0.0"),
		"an older minor with a huge build is not newer");
	expect(KfxVersion::isNewerVersion("1.4.0.0", "1.3.0.9999"),
		"a newer minor beats a huge build on an older one");
	expect(!KfxVersion::isNewerVersion("0.9.9.9999", "1.0.0.0"),
		"an older major with a huge build is not newer");

	// Mixed-length inputs (a 3-part stable vs a 4-part alpha) must still order.
	expect(KfxVersion::isNewerVersion("1.4.1", "1.4.0.5408"),
		"a higher patch beats a longer lower version");
	expect(!KfxVersion::isNewerVersion("1.4.0", "1.4.0.5408"),
		"a 3-part version is not newer than the same base with a build");

	// --- The actual update decision, end to end ---------------------------------
	// This is the pairing that matters: the installed version comes from
	// version.txt through getVersionFromString(), the latest comes from the
	// release tag. If the installed side loses its build number, the two can
	// never compare equal and the launcher offers an update to the build it is
	// already running -- every check, forever.
	{
		const KfxVersion::VersionInfo installed = KfxVersion::getVersionFromString("1.4.0.5408");
		expect(!KfxVersion::isNewerVersion("1.4.0.5408", installed.version),
			"a stable install is NOT told to update to itself");
		const KfxVersion::VersionInfo installedAlpha = KfxVersion::getVersionFromString("1.4.0.5397 alpha");
		expect(KfxVersion::isNewerVersion("1.4.0.5408", installedAlpha.version),
			"an alpha install IS offered a newer build");
	}

	if (g_failures > 0) {
		std::printf("\n%d version test(s) FAILED.\n", g_failures);
		return 1;
	}
	std::printf("\nAll version tests passed.\n");
	return 0;
}
