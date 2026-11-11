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

The fork has grown well past the porting patches: 145 commits on top of the team's `master`, 126 of them this
fork's and 19 the team's own work taken early by cherry-pick, authors kept (count them with
`git log --oneline --no-merges upstream/master..master`, `upstream` being dkfans/keeperfx-launcher-qt).
Everything below is ours, not upstream's:

- **Workshop browser** (`workshopbrowserdialog.cpp`) — browse the full [keeperfx.net](https://keeperfx.net)
  catalogue in-launcher with thumbnails, categories, search and ratings; install with one click. Thumbnails
  and the catalogue are cached on disk (stale-while-revalidate), and downloads fall back to the website when
  the API publishes no file.
- **Installed manager** — a second tab listing what you actually have across `campgns/`, `levels/`,
  `personal/` and `mods/`, distinguishing stock content from your own, with a reversible **Uninstall →
  Restore** backed by a recycle bin. The launcher records what each install created (`installrecord.h`), so
  a Workshop map that adds itself to a stock pack (`levels/standard/mapNNNNN.*`, the usual way single maps
  ship) is listed and uninstalled on its own, and Browse marks an item installed by its id, not by guessing
  from its name; before, such a map never appeared here and Browse kept offering *Install*. One installed by an
  earlier launcher is registered by a *Reinstall*: files already there that are identical to the archive's
  count as the item's, and a package install's stock files, which are links, never do.
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
  rather than read as "up to date". Switching to the alpha channel never offers an alpha older than the
  stable you have (`kfxversion.cpp`): every stable is promoted from the alpha line, so right after a promotion
  the newest alpha is behind it, and the team's rule (offer the other channel's release whenever the channel
  differs) offered 1.4.0.6013-alpha to 1.4.0.6062. Switching back to stable is still offered either way.
- **Crash reports come to us, and nothing is sent anywhere** (`crashdialog.cpp`, `crashcontext.cpp`) — the
  team's dialog uploaded the report (description, contact details, `keeperfx.cfg`, `keeperfx.log`, a save) to
  keeperfx.net. This one saves it as a zip on the player's computer and opens a new issue on this edition's
  tracker with the versions, the description and the end of the log filled in, the home folder shown as `~`;
  the player attaches the zip and submits the issue. `ApiClient` only does GET now, so nothing in the launcher
  can upload to keeperfx.net. The log goes in only if the failing run wrote it, never a stale one from an
  earlier session.
- **Where to ask** (`launchermainwindow.cpp`, `settingsdialog.cpp`) — before the Discord button opens the
  invite, it says that Keeper Klan is the official KeeperFX's community (a Windows game): a good place to talk
  about the game, while problems with this edition go to our GitHub page. "Don't show this again" turns the
  note off. The About tab says the same, and the messages that sent players to that Discord for help now
  point to our tracker.
- **About tab** (`settingsdialog.ui`, `aboutcredits.h`) — the tab is laid out, so its lines wrap instead of
  being cut off at the edge (at the window's fixed size the Discord and GitHub links were cut short from a
  10 pt font up), and a line under the translators credits this edition's contributors.
- **Single-instance lock** (`main.cpp`) — one launcher at a time; `--allow-multiple` overrides it.
- **UI scale** — scale the whole launcher from 100% up to 200% (110 / 125 / 150 / 200) for readability.
- **Camera settings that survive Save** (`settingsdialog.cpp`) — *Zoom to cursor* and *Rotate around cursor*
  read the config the way the engine does (one word, any case; `ON`, `TRUE`, `YES`, `ENABLED` are on), so an
  unlisted spelling no longer shows empty and is saved as *Never*; the two *keys only* rotate choices write
  `MOVEMENT_KEYS` / `ROTATION_KEYS`, where upstream writes `ON MOVEMENT_KEYS` / `ON ROTATION_KEYS`, which the
  engine reads as *Always*.
- **Save keeps the game's defaults** (`settings.cpp`) — a setting the config file has no line for (a config
  older than the setting) shows what the engine does without one, and Save writes that. It used to show off or
  empty and write it back: *VSync* and *Relative mouse mode* switched off (on Wayland the cursor jumped), edge
  panning, delta time and the possession cursor lock off, the startup screens and the zoom-out limit gone, and
  the creature flower, select box and hand sizes and the API port saved empty, which the engine reads as 0. An
  empty value left by such a Save reads the same way.
- **Your own notes in `keeperfx.cfg` stay** (`cfgmerge.h`, `settingscfgformat.cpp`, `settings.cpp`) — the
  team's launcher writes the file again from the release's copy of the defaults (`_keeperfx.cfg`, which the
  AppImage and an update from inside the launcher leave next to yours), so the first start after an update
  replaced your own comment lines, order and layout with the release's. Now your file is the start: values
  change on their own lines, a setting the launcher removes loses its line, and a setting your file lacks
  is added at the end under the release's comment for it. Resetting the settings (the installer does) puts
  back the release's defaults, and keeps your settings when there is no release copy to go back to. The
  settings format is also registered once: every load registered it again, and Qt takes sixteen, so a few
  *Restore defaults* in one session left the launcher unable to read or write its settings.
- **OpenGL as the renderer, once, for configs that never chose** (`settings.cpp`) — the engine's default renderer
  became OpenGL (with the software renderer taking over by itself where OpenGL cannot start). A config written
  before then says `RENDERER=SOFTWARE` because that was the shipped default, and copying missing keys never
  changes a key that is there, so the first time the launcher sees such an engine it moves `SOFTWARE` to
  `OPENGL` once and notes that it did; whatever the player picks afterwards is kept. A config with no
  `RENDERER` line shows *OpenGL* in the Graphics tab, as the engine runs it.
- **Variable refresh on the Graphics tab** (`settingsdialog.cpp`) — *Variable refresh: Auto / On / Off* beside
  VSync writes the engine's `VARIABLE_REFRESH` (engines from 1.4.0.6086; hidden on older ones). Auto lets the
  engine ask the desktop, which GNOME answers; on another desktop a FreeSync / G-Sync owner picks On. The
  refresh rate the launcher writes as the frame cap's fallback now goes up to 500 Hz instead of stopping at 200.
- **No dead *Smoothen edges* checkbox** (`settingsdialog.cpp`, `settings.cpp`) — engines from 1.4.0.6151 ignore
  `-vidsmooth` and only log that it no longer works, so the checkbox is hidden there and the launcher stops
  passing it; older engines still get both, and the saved choice is kept.
- **Our own online lobby** (`lobbyserver.h`, `settingsdialog.cpp`) — the default matchmaking server is
  KeeperFX: Tux Edition's own (`lobby.keeperfx-tux.workers.dev`; its code is `lobby-server/` in the engine
  repository), not the KeeperFX team's. A `keeperfx.cfg` that still names theirs, or anything under
  keeperfx.workers.dev or keeperfx.net, shows ours on the Multiplayer tab, so Save writes ours. The engine
  makes the same swap when it reads the file. *Set to default server* gives ours.

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
From their October work: the **Replays** window (Play ▾ → *Replays…*) in place of *Run packetfile*,
which looked for `*.pck` files in the game folder and so found none of the `.fxpkt` replays the engine
saves under `replays/` — here the list comes from our own tested `replayfiles.h`, also holds the `.pck`
that *Enable Packetsave* records instead, and says so when it is empty, and Copy to Clipboard pastes in
the GTK file managers too; the hover highlight drawn above an item's thumbnail; the contributor list; and their translations,
merged string by string into a translation template that now also holds this edition's own strings. Left
out on purpose: their release-type mismatch pop-up (wrong where a package manager picks the version), a
settings-dialog layout fix (our settings dialog has its own layout), and the restyled main-window scroll
area.
