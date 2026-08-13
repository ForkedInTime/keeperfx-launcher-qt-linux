// Tests the REAL crash-context decisions, by compiling src/crashcontext.cpp
// into the test. Pure logic: no files are read and no process is started.
//
// The dates below are the ffmpeg 9 incident of 2026-08-12, which is what these
// rules exist to stop: the engine never loaded, wrote nothing, and the reporter
// attached the previous day's log to a report about a failure it never saw.
#include "crashcontext.h"

#include <QCoreApplication>
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

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);

	testLogBelongsToRun();
	testShouldAttachLog();
	testIsStartupFailure();

	if (g_failures > 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}

	std::printf("all crash-context tests passed\n");
	return 0;
}
