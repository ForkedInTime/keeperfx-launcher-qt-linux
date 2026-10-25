# KeeperFX Launcher — Tux Edition

![KeeperFX Tux Edition](docs/img/tux-edition-banner.png)

This is a **Linux-native fork** of the KeeperFX team's settings launcher
([dkfans/keeperfx-launcher-qt](https://github.com/dkfans/keeperfx-launcher-qt)), patched to drive the
**native Linux engine** (no Wine) and to install and update
[KeeperFX Tux Edition](https://github.com/ForkedInTime/keeperfx-tux-edition).

![The KeeperFX Tux Edition launcher — the latest workshop items, Tux Edition release news, and buttons for
Settings, Mods, Workshop, Browse Workshop and Map Editor](./docs/img/launcher-1.4.0.6021.png)

![The Workshop browser inside the launcher — maps sorted by rating, with Details, Install and Reinstall
buttons](./docs/img/launcher-1.4.0.6021-workshop.png)

<sub>The launcher as of 1.4.0.6021 (October 2026). These pictures are replaced whenever the launcher changes.
**[Every screen of the launcher »](docs/screenshots.md)** — all eight Settings tabs, the Mod Manager, installed add-ons, the Play menu and its dialogs, the update check, the map editor.</sub>

> ⚠️ **Unofficial — not affiliated with the KeeperFX team.** The launcher is *their* work; this fork only adds
> the Linux-native changes below. The official launcher (and the official game) live at
> [dkfans/keeperfx-launcher-qt](https://github.com/dkfans/keeperfx-launcher-qt) and
> [dkfans/keeperfx](https://github.com/dkfans/keeperfx).

## 🎮 Players: you don't need this repo

**Just want to play?** Don't build anything here. The launcher comes with the game — download it from
**[KeeperFX Tux Edition](https://github.com/ForkedInTime/keeperfx-tux-edition)**: one AppImage with
everything in it, or the Arch package or Flatpak
([install guide](https://github.com/ForkedInTime/keeperfx-tux-edition/blob/alpha/docs/install.md)).

## What's different from the team's launcher

- 🐧 **Drives the native Linux engine** — no Wine, and it reads the engine's real version.
- 🛒 **Workshop browser** — the whole [keeperfx.net](https://keeperfx.net) workshop inside the launcher:
  search, thumbnails, one-click install, and an **Installed** tab with a reversible uninstall.
- 🧩 **Mod Manager and add-on installer** — mods, campaigns, map packs and single maps from a
  `.7z`/`.zip`/`.rar`, each put where it belongs; no archive can write outside its folder.
- 🔄 **Updates on two channels** — stable or alpha, a small patch when one fits, and the engine it
  replaces kept so older saves stay playable.
- 🧰 **And** a log viewer, the Unearth map editor, music recovery, crash reports with the right log,
  a single-instance lock and UI scaling.

112 commits on top of the team's `master`, 93 of them this fork's.
**[Everything this fork changes »](docs/changes.md)**

## 🛠️ Build from source

You need a C++17 toolchain, CMake 3.24+, Ninja and **Qt 6.7+**:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Getting a new enough Qt, running the tests, how releases are built, and what not to do with a local
build: **[Building and developing »](docs/development.md)** ·
Translating the launcher: [docs/translations.md](docs/translations.md)

## Credits & License

The launcher is the work of the **KeeperFX team and the Keeper Klan community**
([dkfans/keeperfx-launcher-qt](https://github.com/dkfans/keeperfx-launcher-qt)), based on
[ImpLauncher](https://keeperfx.net/workshop/item/410/implauncher-beta). This fork adds the Linux-native
patches and [the features listed here](docs/changes.md).

GNU General Public License v2.0 — same as upstream.
