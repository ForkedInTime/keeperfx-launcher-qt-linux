#pragma once

// How a saved .cfg file is written: the file's text with the new values put in.
//
// Header-only so tests/test_cfgmerge.cpp can exercise it without linking the
// launcher. SettingsCfgFormat::writeFile reads the files and calls merge().

#include <QList>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <utility>

namespace CfgMerge {

    // Whether `line` is a KEY=VALUE line as SettingsCfgFormat::readFile loads it:
    // not blank, not a comment, exactly one '=', a key before it. Sets `key` and
    // `value` if so.
    inline bool parseSetting(const QString &line, QString *key = nullptr, QString *value = nullptr)
    {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#'))) {
            return false;
        }
        const QStringList keyValue = trimmed.split(QLatin1Char('='), Qt::KeepEmptyParts);
        if (keyValue.size() != 2 || keyValue[0].trimmed().isEmpty()) {
            return false;
        }
        if (key != nullptr) {
            *key = keyValue[0].trimmed();
        }
        if (value != nullptr) {
            *value = keyValue[1].trimmed();
        }
        return true;
    }

    inline bool isBlankOrComment(const QString &line)
    {
        const QString trimmed = line.trimmed();
        return trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#'));
    }

    // The text to write. `current` is the file as it is now, `defaults` the
    // release's own copy (_keeperfx.cfg, empty if there is none), `map` every
    // setting with its value. `spaceBeforeAdded` puts a blank line before the
    // settings the file did not have yet.
    //
    // The file is the player's: its comments, order and layout stay, each value
    // changes on its own line, and a setting the launcher removed loses its line.
    // A setting the file lacks goes at the end, in the release's order, under the
    // comment the release's copy has right above it (settings on neighbouring
    // lines there stay together). Only a file with nothing in
    // it yet starts from the release's copy. Lines that are not KEY=VALUE as
    // SettingsCfgFormat::readFile reads them are never changed.
    //
    // (Up to 1.4.0.6103 the release's copy was always the start, so every save
    // replaced the player's own comments with the release's.)
    inline QString merge(const QString &current, const QString &defaults, const QSettings::SettingsMap &map, bool spaceBeforeAdded)
    {
        const bool fromCurrent = !current.trimmed().isEmpty();
        const QString base = fromCurrent ? current : defaults;

        QStringList lines;
        if (!base.isEmpty()) {
            lines = base.split(QLatin1Char('\n'));
        }
        const bool finalNewline = base.endsWith(QLatin1Char('\n'));
        if (finalNewline) {
            lines.removeLast();
        }

        // Every line as it is, with the new values put in
        QStringList output;
        QSet<QString> written;
        for (const QString &line : std::as_const(lines)) {
            if (isBlankOrComment(line)) {
                output.append(line);
                continue;
            }
            const QString trimmed = line.trimmed();
            const qsizetype separator = trimmed.indexOf(QLatin1Char('='));
            if (separator > 0) {
                const QString key = trimmed.left(separator).trimmed();
                if (map.contains(key)) {
                    output.append(key + QLatin1Char('=') + map.value(key).toString());
                    written.insert(key);
                    continue;
                }
                if (fromCurrent && parseSetting(line)) {
                    // The launcher removed this setting
                    continue;
                }
            }
            output.append(line);
        }

        // The settings still missing: the release's order first, then the rest
        struct Added {
            QString key;
            QStringList comment;
            bool followsPrevious; // on the line right after the previous one added
        };
        QList<Added> added;
        QSet<QString> queued;
        const QStringList defaultLines = defaults.split(QLatin1Char('\n'));
        qsizetype previousLine = -2;
        for (qsizetype i = 0; i < defaultLines.size(); ++i) {
            QString key;
            if (!parseSetting(defaultLines[i], &key) || !map.contains(key) || written.contains(key) || queued.contains(key)) {
                continue;
            }
            QStringList comment;
            for (qsizetype j = i - 1; j >= 0 && isBlankOrComment(defaultLines[j]) && !defaultLines[j].trimmed().isEmpty(); --j) {
                comment.prepend(defaultLines[j]);
            }
            added.append({key, comment, i == previousLine + 1});
            queued.insert(key);
            previousLine = i;
        }
        bool firstUnknown = true;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!written.contains(it.key()) && !queued.contains(it.key())) {
                added.append({it.key(), {}, !firstUnknown});
                firstUnknown = false;
            }
        }

        for (const Added &setting : std::as_const(added)) {
            const bool together = setting.comment.isEmpty() && setting.followsPrevious;
            if (spaceBeforeAdded && !together && !output.isEmpty() && !output.last().trimmed().isEmpty()) {
                output.append(QString());
            }
            output.append(setting.comment);
            output.append(setting.key + QLatin1Char('=') + map.value(setting.key).toString());
        }

        // Blank lines at the top go (some files without comments have them)
        while (!output.isEmpty() && output.first().trimmed().isEmpty()) {
            output.removeFirst();
        }

        QString text = output.join(QLatin1Char('\n'));
        if (finalNewline && !output.isEmpty()) {
            text += QLatin1Char('\n');
        }
        return text;
    }

}
