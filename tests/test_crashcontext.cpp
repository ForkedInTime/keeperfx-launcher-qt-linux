// Tests the REAL crash-context decisions, by compiling src/crashcontext.cpp
// into the test. Pure logic: no files are read and no process is started.
//
// The dates below are the ffmpeg 9 incident of 2026-08-12, which is what these
// rules exist to stop: the engine never loaded, wrote nothing, and the reporter
// attached the previous day's log to a report about a failure it never saw.
#include "crashcontext.h"

#include <QCoreApplication>
#include <QStringList>
#include <QUrlQuery>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf(ok ? "  ok   %s\n" : "  FAIL %s\n", what);
	if (!ok) ++g_failures;
}

static QDateTime at(const char *iso)
{
	return QDateTime::fromString(QString(iso), Qt::ISODate);
}

static void testLogBelongsToRun()
{
	std::printf("CrashContext::logBelongsToRun\n");

	const QDateTime launch = at("2026-08-12T22:40:16");

	expect(CrashContext::logBelongsToRun(at("2026-08-12T22:40:19"), launch) == true,
		"a log written after launch belongs to the run");

	expect(CrashContext::logBelongsToRun(at("2026-08-11T09:57:18"), launch) == false,
		"yesterday's leftover log does not belong to the run");

	expect(CrashContext::logBelongsToRun(launch, launch) == true,
		"a log written in the same second as launch belongs to the run");

	expect(CrashContext::logBelongsToRun(at("2026-08-12T22:40:15"), launch) == false,
		"a log written one second before launch does not belong to the run");

	expect(CrashContext::logBelongsToRun(QDateTime(), launch) == false,
		"a missing log does not belong to the run");

	expect(CrashContext::logBelongsToRun(at("2026-08-12T22:40:19"), QDateTime()) == false,
		"without a launch time nothing can be attributed to the run");
}

static void testShouldAttachLog()
{
	std::printf("CrashContext::shouldAttachLog\n");

	const QDateTime launch = at("2026-08-12T22:40:16");

	expect(CrashContext::shouldAttachLog(at("2026-08-12T22:40:19"), launch) == true,
		"a run that wrote its own log attaches it");

	expect(CrashContext::shouldAttachLog(at("2026-08-11T09:57:18"), launch) == false,
		"a run that wrote nothing does not attach yesterday's log");

	// --crash-report opens the dialog with no game run behind it, so there is
	// no launch to contradict the log and the old behaviour is kept.
	expect(CrashContext::shouldAttachLog(at("2026-08-11T09:57:18"), QDateTime()) == true,
		"a forced report with no run behind it still attaches the existing log");
}

static void testIsStartupFailure()
{
	std::printf("CrashContext::isStartupFailure\n");

	expect(CrashContext::isStartupFailure(127,
			"/home/user/.local/share/keeperfx-alpha/keeperfx: error while loading "
			"shared libraries: libavformat.so.62: cannot open shared object file: "
			"No such file or directory\n") == true,
		"the ffmpeg soname bump is a startup failure");

	expect(CrashContext::isStartupFailure(127, "") == true,
		"exit 127 alone is a startup failure");

	expect(CrashContext::isStartupFailure(1,
			"error while loading shared libraries: libfoo.so.1: cannot open shared "
			"object file\n") == true,
		"a loader message is a startup failure whatever the exit code");

	expect(CrashContext::isStartupFailure(139, "") == false,
		"a segfault is a real crash, not a startup failure");

	expect(CrashContext::isStartupFailure(1, "Configuration load error\n") == false,
		"an engine error message is a real crash, not a startup failure");

	expect(CrashContext::isStartupFailure(0, "") == false,
		"a clean exit is not a startup failure");
}

static void testHideHomeDir()
{
	std::printf("CrashContext::hideHomeDir\n");

	expect(CrashContext::hideHomeDir("/home/al/.local/share/keeperfx-alpha/keeperfx", "/home/al")
			== "~/.local/share/keeperfx-alpha/keeperfx",
		"a path under the home folder starts with ~");

	expect(CrashContext::hideHomeDir("cwd /home/al and /home/al/x", "/home/al") == "cwd ~ and ~/x",
		"the home folder itself, and every path under it, is hidden");

	expect(CrashContext::hideHomeDir("/home/alice/game", "/home/al") == "/home/alice/game",
		"another user's folder that merely starts with the same letters is left alone");

	expect(CrashContext::hideHomeDir("/usr/share/x", "/") == "/usr/share/x",
		"a root home folder hides nothing");

	expect(CrashContext::hideHomeDir("/home/al/x", "") == "/home/al/x",
		"no home folder hides nothing");
}

static void testTailLines()
{
	std::printf("CrashContext::tailLines\n");

	expect(CrashContext::tailLines("a\nb\nc\nd\n", 2, 100) == "c\nd",
		"the last lines are kept, trailing newline dropped");

	expect(CrashContext::tailLines("aaaa\nbbbb\ncccc", 10, 9) == "bbbb\ncccc",
		"whole lines only, as many as fit the character limit");

	expect(CrashContext::tailLines("0123456789", 10, 4) == "6789",
		"a single line longer than the limit keeps its end");

	expect(CrashContext::tailLines("", 10, 100).isEmpty(),
		"nothing in, nothing out");
}

static CrashContext::ReportFacts sampleFacts()
{
	CrashContext::ReportFacts f;
	f.gameVersion = "1.4.0.6156 Alpha";
	f.launcherVersion = "1.11.0";
	f.system = "Arch Linux, kernel 6.18.57, wayland, GNOME, AppImage";
	f.description = "Crashed when my imps dug into the portal.";
	f.gameOutput = "Segmentation fault";
	QStringList log;
	for (int i = 1; i <= 400; i++) {
		log << QString("line %1 of the game log").arg(i);
	}
	log << "Error: the last line, where the crash is";
	f.logTail = log.join('\n');
	f.archiveName = "keeperfx-crash-20261106-101500.zip";
	return f;
}

static void testIssueBody()
{
	std::printf("CrashContext::issueBody\n");

	const CrashContext::ReportFacts f = sampleFacts();
	const QString body = CrashContext::issueBody(f, 3000);

	expect(body.size() <= 3000, "the body fits the limit");
	expect(body.contains(f.description), "the player's description is in it");
	expect(body.contains(f.gameVersion) && body.contains(f.launcherVersion) && body.contains(f.system),
		"the versions and the system are in it");
	expect(body.contains(f.archiveName), "it names the zip to attach");
	expect(body.contains(f.gameOutput), "the game's error output is in it");
	expect(body.contains("Error: the last line, where the crash is"), "the end of the log is kept");
	expect(body.contains("line 1 of the game log") == false, "the start of a long log is dropped");

	CrashContext::ReportFacts bare;
	bare.gameVersion = "1.4.0.6156 Alpha";
	bare.launcherVersion = "1.11.0";
	bare.system = "Linux";
	const QString bareBody = CrashContext::issueBody(bare, 3000);
	expect(bareBody.contains("keeperfx.log"), "without a zip it asks for keeperfx.log instead");
	expect(bareBody.contains("End of keeperfx.log") == false, "an empty log adds no log section");
}

static void testNewIssueUrl()
{
	std::printf("CrashContext::newIssueUrl\n");

	const QUrl url = CrashContext::newIssueUrl("Crash: C++ & more", "line one\nline two + three");
	const QUrlQuery query(url);

	expect(url.scheme() == "https" && url.host() == "github.com", "it is a GitHub link");
	expect(url.path() == "/ForkedInTime/keeperfx-tux-edition/issues/new", "it opens a new issue on OUR tracker");
	expect(url.toString().contains("keeperfx.net") == false, "it never points at keeperfx.net");
	expect(query.queryItemValue("title", QUrl::FullyDecoded) == "Crash: C++ & more",
		"the title survives '+' and '&'");
	expect(query.queryItemValue("body", QUrl::FullyDecoded) == "line one\nline two + three",
		"the body survives newlines and '+'");
	expect(url.toEncoded().contains('+') == false,
		"no literal '+' that GitHub would read as a space");

	expect(QString(CrashContext::ISSUE_TRACKER_URL) == "https://github.com/ForkedInTime/keeperfx-tux-edition/issues",
		"the tracker is this edition's own");
}

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);

	testLogBelongsToRun();
	testShouldAttachLog();
	testIsStartupFailure();
	testHideHomeDir();
	testTailLines();
	testIssueBody();
	testNewIssueUrl();

	if (g_failures > 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}

	std::printf("all crash-context tests passed\n");
	return 0;
}
