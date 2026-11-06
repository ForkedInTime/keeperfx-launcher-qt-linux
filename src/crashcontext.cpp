#include "crashcontext.h"

#include <QRegularExpression>
#include <QStringList>

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

    QString hideHomeDir(QString text, const QString &homeDir)
    {
        // Nothing to hide, or hiding it would turn every absolute path into "~".
        if (homeDir.isEmpty() || homeDir == QStringLiteral("/")) {
            return text;
        }

        // The folder itself, followed by a separator, a quote, white space or
        // the end -- never by more letters of a longer name.
        static const QString boundary = QStringLiteral("(?=[/\\s\"']|$)");
        return text.replace(QRegularExpression(QRegularExpression::escape(homeDir) + boundary), QStringLiteral("~"));
    }

    QString tailLines(const QString &text, int maxLines, int maxChars)
    {
        QStringList lines = text.split(QLatin1Char('\n'));
        while (lines.isEmpty() == false && lines.last().trimmed().isEmpty()) {
            lines.removeLast();
        }
        if (lines.isEmpty() || maxLines <= 0 || maxChars <= 0) {
            return QString();
        }

        // A last line too long on its own: its end is where an error is.
        if (lines.last().size() > maxChars) {
            return lines.last().right(maxChars);
        }

        QStringList kept;
        int size = 0;
        for (int i = lines.size() - 1; i >= 0 && kept.size() < maxLines; i--) {
            const int added = lines.at(i).size() + (kept.isEmpty() ? 0 : 1);
            if (size + added > maxChars) {
                break;
            }
            kept.prepend(lines.at(i));
            size += added;
        }
        return kept.join(QLatin1Char('\n'));
    }

    static QString codeSection(const QString &heading, const QString &content)
    {
        return QStringLiteral("### %1\n```\n%2\n```\n\n").arg(heading, content);
    }

    QString issueBody(const ReportFacts &facts, int maxChars)
    {
        QString body;

        body += QStringLiteral("### What happened\n%1\n\n")
                    .arg(facts.description.trimmed().isEmpty()
                             ? QStringLiteral("_Please describe what you were doing when the game crashed._")
                             : facts.description.trimmed());

        body += QStringLiteral("### Versions\n- Game: %1\n- Launcher: %2\n- System: %3\n\n")
                    .arg(facts.gameVersion, facts.launcherVersion, facts.system);

        if (facts.archiveName.isEmpty() == false) {
            body += QStringLiteral("### Crash report file\n"
                                   "Please attach `%1` to this issue: drag it into this box. "
                                   "The launcher saved it on your computer and opened its folder for you.\n\n")
                        .arg(facts.archiveName);
        } else {
            body += QStringLiteral("### Crash report file\n"
                                   "The launcher could not save a crash report file. "
                                   "Please attach `keeperfx.log` from your game folder to this issue.\n\n");
        }

        // What is left for the two excerpts, less their headings and fences.
        const int sectionOverhead = 40;
        int room = maxChars - body.size();

        if (facts.gameOutput.trimmed().isEmpty() == false && room > sectionOverhead + 20) {
            const int budget = qMin(800, (room - sectionOverhead) / 3);
            const QString output = tailLines(facts.gameOutput, 20, budget);
            if (output.isEmpty() == false) {
                body += codeSection(QStringLiteral("Game output"), output);
                room = maxChars - body.size();
            }
        }

        if (facts.logTail.trimmed().isEmpty() == false && room > sectionOverhead + 20) {
            const QString log = tailLines(facts.logTail, 60, room - sectionOverhead);
            if (log.isEmpty() == false) {
                body += codeSection(QStringLiteral("End of keeperfx.log"), log);
            }
        }

        return body.trimmed().left(maxChars);
    }

    QUrl newIssueUrl(const QString &title, const QString &body)
    {
        // Every value fully percent-encoded by hand: QUrlQuery leaves '+' as it
        // is, and GitHub reads a literal '+' in a query as a space.
        const QByteArray encoded = QByteArray(ISSUE_TRACKER_URL) + "/new?title=" + QUrl::toPercentEncoding(title)
                                   + "&body=" + QUrl::toPercentEncoding(body);
        return QUrl::fromEncoded(encoded, QUrl::StrictMode);
    }

}
