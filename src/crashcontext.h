#pragma once

#include <QDateTime>
#include <QString>

/**
 * Decisions about what a finished game process actually tells us.
 *
 * A non-zero exit is not the same thing as a crash, and the keeperfx.log
 * sitting next to the binary is not necessarily the log of the run that just
 * ended. Both assumptions were baked into the crash reporter, and together they
 * sent a report containing a log from a different day.
 *
 * Kept free of Qt GUI and file access so it can be tested directly.
 */
namespace CrashContext {

    /**
     * Whether keeperfx.log was written by the run that just ended.
     *
     * The engine truncates its log at startup, so a log belonging to this run
     * is modified at or after the moment the process was launched. Anything
     * older is a leftover from a previous session and must not be presented as
     * evidence of this one.
     *
     * @param logModified Last-modified time of keeperfx.log
     * @param runStarted  Moment the game process was launched
     * @return true if the log can be attributed to this run
     */
    bool logBelongsToRun(const QDateTime &logModified, const QDateTime &runStarted);

    /**
     * Whether the process died before the engine ever ran.
     *
     * A dynamic-loader failure (a system library moved its soname out from
     * under an installed build) never reaches main(), so nothing is logged and
     * there is no crash to report -- the installation is broken, which is a
     * different problem with a different answer.
     *
     * @param exitCode      Process exit code
     * @param standardError Whatever the process wrote to stderr
     * @return true if the failure happened before the engine started
     */
    bool isStartupFailure(int exitCode, const QString &standardError);

    /**
     * Whether keeperfx.log should be attached to a crash report at all.
     *
     * @param logModified Last-modified time of keeperfx.log
     * @param runStarted  Moment the game was launched, or invalid if the report
     *                    was raised without a game run behind it
     * @return true if the log may be attached
     */
    bool shouldAttachLog(const QDateTime &logModified, const QDateTime &runStarted);

}
