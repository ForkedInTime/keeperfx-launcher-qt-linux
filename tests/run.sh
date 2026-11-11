#!/bin/sh
# Builds and runs the standalone version-logic tests. No launcher build needed:
# kfxversion.cpp is compiled straight into the test, with ApiClient's three
# network entry points stubbed in the test itself.
#
#   tests/run.sh
#
# Exits non-zero if the build or the tests fail.
set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT_DIR/bin"

g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core Qt6Gui Qt6Network) \
	-o "$ROOT_DIR/bin/test_kfxversion" \
	"$ROOT_DIR/tests/test_kfxversion.cpp" "$ROOT_DIR/src/kfxversion.cpp" \
	$(pkg-config --libs Qt6Core Qt6Gui Qt6Network) -lLIEF
"$ROOT_DIR/bin/test_kfxversion"

# Workshop download-URL repair (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_workshopurl" "$ROOT_DIR/tests/test_workshopurl.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_workshopurl"

# Add-on copy helpers (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_copytree" "$ROOT_DIR/tests/test_copytree.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_copytree"

# Archive-shape detection (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_addonshape" "$ROOT_DIR/tests/test_addonshape.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_addonshape"

# Crash-context decisions -- whether the log is from this run, and whether the
# engine ever started (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_crashcontext" "$ROOT_DIR/tests/test_crashcontext.cpp" \
	"$ROOT_DIR/src/crashcontext.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_crashcontext"

# The install record: what each add-on install created (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_installrecord" "$ROOT_DIR/tests/test_installrecord.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_installrecord"
# The lobby server choice: the team's lobby is swapped for ours (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_lobbyserver" "$ROOT_DIR/tests/test_lobbyserver.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_lobbyserver"

# How a saved .cfg file is written: the player's comments stay (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_cfgmerge" "$ROOT_DIR/tests/test_cfgmerge.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_cfgmerge"

# Which replays the replays dialog lists, and the path it passes to -packetload (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_replayfiles" "$ROOT_DIR/tests/test_replayfiles.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_replayfiles"

# What Copy to Clipboard offers for a file (pure, QtCore only).
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" \
	$(pkg-config --cflags Qt6Core) \
	-o "$ROOT_DIR/bin/test_clipboardfile" "$ROOT_DIR/tests/test_clipboardfile.cpp" \
	$(pkg-config --libs Qt6Core)
"$ROOT_DIR/bin/test_clipboardfile"

# The About tab shows every line whole at the window's fixed size (Qt Widgets, offscreen; the
# window comes from the real ui/settingsdialog.ui through uic).
UIC="$(pkg-config --variable=libexecdir Qt6Core)/uic"
"$UIC" "$ROOT_DIR/ui/settingsdialog.ui" -o "$ROOT_DIR/bin/ui_settingsdialog.h"
g++ -std=c++17 -fPIC -Wall -Wextra -I"$ROOT_DIR/src" -I"$ROOT_DIR/bin" \
	-DSOURCE_ROOT="\"$ROOT_DIR\"" \
	$(pkg-config --cflags Qt6Widgets) \
	-o "$ROOT_DIR/bin/test_aboutlayout" "$ROOT_DIR/tests/test_aboutlayout.cpp" \
	$(pkg-config --libs Qt6Widgets)
QT_QPA_PLATFORM=offscreen "$ROOT_DIR/bin/test_aboutlayout"
