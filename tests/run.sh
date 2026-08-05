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
