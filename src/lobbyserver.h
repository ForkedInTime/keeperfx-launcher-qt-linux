#pragma once

// The online lobby the game lists and joins internet games through.
//
// KeeperFX: Tux Edition runs its own lobby server (lobby-server/ in the engine
// repository). The KeeperFX team's lobby is never used: our builds cannot join
// their games anyway (a join needs exactly the same build), and their lobby
// announces every hosted game on their Discord. A keeperfx.cfg that still names
// it -- every one written before this edition had its own lobby does -- is shown
// and saved with ours instead. The engine makes the same swap when it reads the
// file, so a player who never opens the settings moves over too.
//
// Header-only so tests/test_lobbyserver.cpp can exercise it without linking
// the launcher.

#include <QString>
#include <QStringList>

namespace LobbyServer {

    // Our lobby server's host name.
    inline constexpr const char *DEFAULT_HOST = "lobby.keeperfx-tux.workers.dev";

    // Whether `server` (a MATCHMAKING_SERVER value: a host, maybe with a scheme
    // and a trailing slash) is one of the KeeperFX team's servers.
    inline bool isRetired(QString server)
    {
        server = server.trimmed();
        const qsizetype schemeEnd = server.indexOf(QStringLiteral("://"));
        if (schemeEnd >= 0) {
            server = server.mid(schemeEnd + 3);
        }
        if (server.endsWith(QLatin1Char('/'))) {
            server.chop(1);
        }
        server = server.toLower();
        const QStringList retiredDomains = {QStringLiteral("keeperfx.workers.dev"), QStringLiteral("keeperfx.net")};
        for (const QString &domain : retiredDomains) {
            if (server == domain || server.endsWith(QLatin1Char('.') + domain)) {
                return true;
            }
        }
        return false;
    }

    // The server to show for a MATCHMAKING_SERVER value read from keeperfx.cfg:
    // ours when the file names none or one of the team's, otherwise the value
    // as it is ("OFF" included).
    inline QString fromConfig(const QString &server)
    {
        if (server.trimmed().isEmpty() || isRetired(server)) {
            return QString::fromLatin1(DEFAULT_HOST);
        }
        return server;
    }

}
