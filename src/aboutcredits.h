#pragma once

#include <QCoreApplication>
#include <QString>

namespace AboutCredits {

// The About tab's line for this edition's own contributors: its author, ForkedInTime. The team's
// contributors and the launcher's translators have their own lines above it. Translated in the
// Settings window's context, beside the team's contributor line.
inline QString tuxEditionContributors()
{
    return QCoreApplication::translate("SettingsDialog", "KeeperFX: Tux Edition Contributors: %1", "Label Contributor List")
        .arg(QStringLiteral("<a href=\"https://github.com/ForkedInTime\">ForkedInTime</a>"));
}

} // namespace AboutCredits
