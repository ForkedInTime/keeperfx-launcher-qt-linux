#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>

#include <optional>

/**
 * The title of a KFXS saved game: the name the player gave it and its
 * campaign.
 *
 * Since KeeperFX 1.4 (the team's #5424, in this edition since 2026-10-19) a
 * save is a KFXS file: a header, then chunks. The META chunk holds the title
 * record and the SCHM chunk describes its layout (src/kfx/save/FORMAT.txt in
 * the engine), so the fields are found by name rather than at fixed offsets.
 * Only the header and those two chunks are read; the game state is skipped
 * over, so listing saves does not read megabytes per file.
 *
 * Every read is checked against what the file holds: a damaged or cut-off
 * file gives no title instead of reading past its end.
 *
 * QtCore and zlib only, so it can be tested directly.
 */
namespace KfxsSave {

    struct Title {
        QString saveName;
        QString campaignName;
    };

    /** Whether the first bytes of a file are the KFXS magic. */
    bool isKfxs(const QByteArray &start);

    /** The title of the KFXS file the device holds, read from its start. */
    std::optional<Title> readTitle(QIODevice &device);

}
