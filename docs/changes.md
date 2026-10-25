# What this fork changes

<sub>[← Back to the README](../README.md)</sub>

## Native-Linux plumbing

The team's launcher compiles cross-platform, but it was written Windows-first — it launched the game through
**Wine** and read the version from a Windows `.exe`. This fork makes it drive the **native** engine:

- **`game.cpp`** — launch the native `keeperfx` ELF directly (no Wine) when present; the team's Wine path
  is still compiled in, but only as a fallback for an install with no native engine
- **`helper.h`** — detect the native binary (not just `keeperfx.exe`) as "installed", and open external links
  in a way that survives the AppImage's sandboxed environment
- **`kfxversion.cpp`** — read the engine version from `version.txt` (a native ELF has no PE resources)
- **`apiclient.cpp`** — install and self-update from this fork's GitHub releases, and show this fork's release
  notes in the news panel (the stock panel only knows about the Windows project): stable releases with a
  golden keeper, alphas with the red one
- **`CMakeLists.txt`** — Linux only: Qt linked dynamically, no Windows or macOS build branches

## Linux only — what was removed, and what was left on purpose

The Windows build files are gone: the MinGW cross-build (`Dockerfile.win64s`, `compile-win64s.sh`), the
InnoSetup installer (`windows-installer.iss`), the Win32 resource and its icon (`app.rc`, `res/icon.ico`)
and the Windows 7-Zip library (`ext/7za.dll`). Nothing built or shipped them; every release launcher is
built for Linux by the game repo.

The Windows code inside `src/` (`#ifdef Q_OS_WINDOWS`, the Windows-executable and Wine helpers) stays.
It is compiled out on Linux or never reached there, and the team's launcher fixes are taken into this fork
by cherry-pick, often touching exactly those blocks — removing them would turn every such cherry-pick
into a conflict. A cherry-pick that touches one of the removed files conflicts once; `git rm` settles it.

## Features added on top

The fork has grown well past the porting patches: 113 commits on top of the team's `master`, 94 of them this
fork's and 19 the team's own work taken early by cherry-pick, authors kept (count them with
`git log --oneline --no-merges upstream/master..master`, `upstream` being dkfans/keeperfx-launcher-qt).
Everything below is ours, not upstream's:

- **Workshop browser** (`workshopbrowserdialog.cpp`) — browse the full [keeperfx.net](https://keeperfx.net)
  catalogue in-launcher with thumbnails, categories, search and ratings; install with one click. Thumbnails
  and the catalogue are cached on disk (stale-while-revalidate), and downloads fall back to the website when
  the API publishes no file.
- **Installed manager** — a second tab listing what you actually have across `campgns/`, `levels/`,
  `personal/` and `mods/`, distinguishing stock content from your own, with a reversible **Uninstall →
  Restore** backed by a recycle bin.
- **Mod Manager** (`modmanager*.cpp`) — list mods, enable/disable them, persist `mods/load_order.cfg`.
- **Universal installer** (`addoninstaller.cpp`) — one code path that installs mods, campaigns, map packs and
  loose maps from a `.7z`/`.zip`/`.rar`, detecting the format from the file rather than its name and
  coping with the several archive layouts the workshop actually ships. When an install fails it says why
  (a read-only folder on a package-managed install, say) instead of "nothing to install".
- **Safe extraction** (`archiver.cpp`) — no archive the launcher opens, Workshop item or game update, can
  write outside its folder: symbolic links are never restored from an archive. (The 7-Zip library checks a
  link's target only as text, so a chain of links in a hostile Workshop item could otherwise reach your home
  folder.) A corrupt or truncated download is reported as one instead of closing the launcher, and the
  pre-update save backup holds every save, not just the first.
- **Map Editor** (`downloadmapeditordialog.cpp`) — install and launch **Unearth** from the launcher, and get
  offered an update when a newer Unearth appears.
- **Log viewer** (`logviewerdialog.cpp`) — read the engine and launcher logs without hunting for the files.
- **Music recovery** — detect an installation whose `music/` folder is empty, partial or non-standard and
  offer the download, instead of silently playing nothing.
- **Updates, on two channels** (`apiclient.cpp`, `updatedialog.cpp`) — a release-channel setting picks
  **stable** or **alpha**, and the launcher only ever offers releases from the channel you chose (alphas are
  GitHub prereleases; the stable channel never sees them). It downloads the release's small **update patch**
  when one starts from the version you have and the full game package otherwise, and it **keeps the engine
  it is about to replace** under `previous/` (three deep) so saved games written by it stay playable — the
  Flatpak does the same when a bundle update overwrites the engine. A momentary network failure is retried
  rather than read as "up to date".
- **Crash reports carry the right log** (`crashcontext.cpp`) — a report is only sent with a log the failing
  run actually wrote, never a stale one from an earlier session.
- **Single-instance lock** (`main.cpp`) — one launcher at a time; `--allow-multiple` overrides it.
- **UI scale** — scale the whole launcher from 100% up to 200% (110 / 125 / 150 / 200) for readability.
- **Camera settings that survive Save** (`settingsdialog.cpp`) — *Zoom to cursor* and *Rotate around cursor*
  read the config the way the engine does (one word, any case; `ON`, `TRUE`, `YES`, `ENABLED` are on), so an
  unlisted spelling no longer shows empty and is saved as *Never*; the two *keys only* rotate choices write
  `MOVEMENT_KEYS` / `ROTATION_KEYS`, where upstream writes `ON MOVEMENT_KEYS` / `ON ROTATION_KEYS`, which the
  engine reads as *Always*.

## Kept in step with upstream

The team's own launcher work is taken in batches. On the settings dialog that has brought, most
recently, **VSync**, **Relative mouse mode**, the **Multiplayer** tab with the matchmaking server and the multiplayer port (refused when it clashes with the API port), the
cursor-capture switch written to the game config instead of a command-line flag on engines that
understand it, the **Renderer** dropdown (software or OpenGL) and the **Fade when opening the map**
checkbox on the Graphics tab, the **Viewport mode** dropdown, the **Max. zoom distance** slider, the
replay controls (**Max. replay size** and **Keep replays** per campaign, free play and multiplayer,
written under whichever key names the running engine uses), the retired *Command character* field
removed, the **Rotate around cursor** control, the zoom-to-cursor values the
engine actually accepts (`ON`/`OFF`/`WHEEL`), case-insensitive on/off parsing for every boolean game
setting, and sliders that enable Save when moved. Every control is greyed out on an engine that
predates its setting, and only settings the running engine understands are written back. The launcher binary
also carries its version in an ELF section, so a crash report or a package script can read it
without running it.
Their October work is not in yet: the replay browser that replaces *Run packetfile* (the engine still
accepts the `-packetload` that *Run packetfile* passes), the launcher/game version-mismatch notice, a settings-dialog
layout fix, the restyled main-window scroll area, and most of their translation updates since September.
