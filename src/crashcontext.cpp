#include "crashcontext.h"

namespace CrashContext {

    bool logBelongsToRun(const QDateTime &logModified, const QDateTime &runStarted)
    {
        // No log, or no idea when we started: nothing can be attributed.
        if (logModified.isValid() == false || runStarted.isValid() == false) {
            return false;
        }

        // The engine truncates its log as it starts, so this run's log is never
        // older than the launch. Equal counts: both can land in the same second.
        return logModified >= runStarted;
    }

    bool shouldAttachLog(const QDateTime &logModified, const QDateTime &runStarted)
    {
        // No run behind this report -- the dialog was raised by hand, e.g. via
        // --crash-report. Nothing contradicts the log, so it is attached just as
        // it always was; the gate below exists to catch a log that a *failed
        // run* did not write, not to withhold a log nobody asked about.
        if (runStarted.isValid() == false) {
            return true;
        }

        return logBelongsToRun(logModified, runStarted);
    }

    bool isStartupFailure(int exitCode, const QString &standardError)
    {
        // Nothing failed.
        if (exitCode == 0) {
            return false;
        }

        // 127 is what the dynamic loader exits with when it cannot resolve a
        // library, and what a shell reports for a binary it cannot execute.
        if (exitCode == 127) {
            return true;
        }

        // The loader says so itself. Checked independently of the exit code
        // because the wrappers the game is launched through (wine,
        // flatpak-spawn) do not all preserve it.
        return standardError.contains(QStringLiteral("error while loading shared libraries"), Qt::CaseInsensitive)
            || standardError.contains(QStringLiteral("cannot open shared object file"), Qt::CaseInsensitive);
    }

}
