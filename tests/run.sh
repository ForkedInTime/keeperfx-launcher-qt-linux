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
