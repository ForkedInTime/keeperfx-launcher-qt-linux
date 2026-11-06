// Standalone tests for the lobby server choice: a keeperfx.cfg naming the
// KeeperFX team's lobby is moved to ours, anything else is left alone.
//
// Build and run:
//   tests/run.sh
#include "../src/lobbyserver.h"

#include <QCoreApplication>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++g_failures;
	}
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	const QString ours = QString::fromLatin1(LobbyServer::DEFAULT_HOST);

	// What every keeperfx.cfg written before our lobby says.
	expect(LobbyServer::isRetired("matchmaking.keeperfx.workers.dev"), "the team's lobby, as the old config names it");
	expect(LobbyServer::isRetired("https://matchmaking.keeperfx.workers.dev/"), "with a scheme and a trailing slash");
	expect(LobbyServer::isRetired("  MatchMaking.KeeperFX.Workers.Dev  "), "in any letter case, with spaces around it");
	expect(LobbyServer::isRetired("lobby.keeperfx.net"), "a name under keeperfx.net");
	expect(LobbyServer::isRetired("keeperfx.net"), "keeperfx.net itself");

	// Ours, and lookalikes that are not the team's.
	expect(!LobbyServer::isRetired(ours), "our own lobby");
	expect(!LobbyServer::isRetired("notkeeperfx.net"), "a different domain ending in the same letters");
	expect(!LobbyServer::isRetired("keeperfx.net.example.org"), "the team's domain as part of someone else's");
	expect(!LobbyServer::isRetired("192.168.1.20:8787"), "a player's own server");
	expect(!LobbyServer::isRetired("OFF"), "OFF");

	// What the settings show.
	expect(LobbyServer::fromConfig("matchmaking.keeperfx.workers.dev") == ours, "the team's lobby shows as ours");
	expect(LobbyServer::fromConfig("") == ours, "no server at all shows ours");
	expect(LobbyServer::fromConfig("OFF") == "OFF", "OFF stays OFF");
	expect(LobbyServer::fromConfig("lobby.example.org") == "lobby.example.org", "a player's own choice is kept");

	if (g_failures) {
		std::printf("%d lobby server test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all lobby server tests passed\n");
	return 0;
}
