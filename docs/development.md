# Building and developing the launcher

<sub>[← Back to the README](../README.md)</sub>

## Build from source

You need a C++17 toolchain, CMake 3.24+, Ninja, and **Qt 6.7+** (the launcher uses
`QNetworkRequestFactory`). Ubuntu 24.04 ships Qt 6.4, so install a newer Qt (e.g. via
[aqtinstall](https://github.com/miurahr/aqtinstall)) — the CI workflow shows the exact steps, and pins
**6.8.3**. CPM fetches zlib/bit7z/LIEF automatically.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This produces `build/keeperfx-launcher-qt`. The launcher expects to live in the game's install directory
(it reads/writes config and finds the engine there); the AppImage's `AppRun` handles that by running it from a
writable install dir.

The 7-Zip library committed here, `ext/7z.so`, can list a RAR archive but not extract one. Released builds
replace it with a RAR-capable build made by the game repo (`packaging/ci/build-7zip-lib.sh`), so a launcher
built from this repo alone will refuse RAR Workshop items — with a message saying so.

> ⚠️ **Do not drop a locally built binary into an AppImage install.** The AppImage bundles Qt 6.8.3, so a
> binary built against a newer distro Qt dies with `libQt6Core.so.6: version 'Qt_6.x' not found`. Run your
> local build directly against your system Qt instead, and let CI produce the binary that ships.

## Tests

`tests/run.sh` builds and runs the standalone logic tests (version parsing and channel classification,
archive-shape detection, tree copying, workshop URL handling, crash-report context) straight from source — no
launcher build required. It exits non-zero on failure.

## CI

The release AppImage and Flatpak are built by **`build-appimage.yml` and `build-flatpak.yml` in the
[game repo](https://github.com/ForkedInTime/keeperfx-tux-edition)**, which clone this repo's **`alpha`**
branch. They live there because a workflow can only be triggered by, and attach assets to, a release in its
own repository. This repo keeps a manual-dispatch copy of
[`build-appimage.yml`](../.github/workflows/build-appimage.yml) for launcher-side debugging: it builds a working
AppImage from the current branch without touching any release.

Push launcher work to **both `alpha` and `master`** — `alpha` is what the release build clones.
