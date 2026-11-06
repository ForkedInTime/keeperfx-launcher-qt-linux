#pragma once

#include <QDateTime>
#include <QString>
#include <QUrl>

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

    /**
     * Where this edition's crash reports go: its own issue tracker, and nowhere else.
     *
     * The upstream launcher POSTed every report -- the player's description and
     * contact details, keeperfx.cfg, keeperfx.log and optionally a save -- to
     * keeperfx.net. This edition sends nothing anywhere. The dialog saves the
     * report as a zip and opens a new issue here with the details filled in;
     * the player attaches the zip and submits it themselves.
     */
    inline constexpr const char *ISSUE_TRACKER_URL = "https://github.com/ForkedInTime/keeperfx-tux-edition/issues";

    /** What a crash report says, gathered by the dialog. */
    struct ReportFacts {
        QString gameVersion;
        QString launcherVersion;
        QString system;       // distribution, kernel, session, how it was installed
        QString description;  // what the player wrote, may be empty
        QString gameOutput;   // what the game wrote to stderr, may be empty
        QString logTail;      // keeperfx.log (or its end), empty when it was not attached
        QString archiveName;  // file name of the saved zip, empty when it could not be written
    };

    /**
     * Replace the home folder in text with "~", so lines pasted into a public
     * issue do not carry the player's user name. Only whole path components
     * match: "/home/al" hides "/home/al/x" but not "/home/alice".
     */
    QString hideHomeDir(QString text, const QString &homeDir);

    /**
     * The last lines of text: at most maxLines, whole lines only, and no more
     * than maxChars in all. A single last line longer than maxChars keeps its end.
     */
    QString tailLines(const QString &text, int maxLines, int maxChars);

    /**
     * The Markdown body of a new issue. The game output and the log are cut
     * from the top until the whole body fits maxChars, because the body travels
     * in the URL.
     */
    QString issueBody(const ReportFacts &facts, int maxChars);

    /** A link that opens a new issue on ISSUE_TRACKER_URL with this title and body. */
    QUrl newIssueUrl(const QString &title, const QString &body);

}
