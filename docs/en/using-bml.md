# Install and use BML+

## Match the runtime first

This guide is for the **Ballanced SDL** branch and a dynamic Player. A matching
package has `runtime: "sdl"` in its ABI descriptor. Match platform, architecture
and dynamic host linkage; direct engine/ImGui Mods also match the dependencies'
compiler/runtime/configuration ABI. An old original-Player archive is not an SDL
package. Static Player integration is not implemented in this branch.

Use a complete matching runtime package. If none is available, follow
[Contributing](contributing.md) to build one against the exact Ballanced baseline.
See [Support and validation](validation.md) for tested and untested targets.

## Install into the game root

Close Player and keep a backup. The root contains `base.cmo`, game resources and
either `Bin/Player` or a flat `Player` executable (`Player.exe` on Windows).
Extract the runtime at that root, preserving its directories:

| Location | Purpose |
| --- | --- |
| `Managers/BMLPlus.dll` | Windows Loader |
| `Managers/libBMLPlus.so` / `Managers/libBMLPlus.dylib` | Linux / macOS Loader |
| `BuildingBlocks/AngelScript.dll`, `libAngelScript.so` or `libAngelScript.dylib` | Matching CKAngelScript plugin, when enabled |
| `ModLoader/BML.abi.json` | Installed runtime target and extension ABI identity |
| `ModLoader/Fonts` and `ModLoader/Licenses` | Runtime fonts and dependency notices |
| `ModLoader/Mods` | Your installed Mods |
| `ModLoader/Configs` | User configuration, created and preserved by the Loader |

Install the files from the package rather than renaming a library to imitate
another platform. The Loader belongs in **Managers**, not BuildingBlocks. Remove
an obsolete duplicate Loader from BuildingBlocks. Keep Player's CK2, VxMath and
SDL; never copy another SDL or SDK engine libraries into the game.

An SDK goes in your development workspace. Its descriptor is
`share/BML/BML.abi.json`; it is not a second runtime to extract over the game.
On POSIX keep executable permissions on Player and the supplied tools. macOS
SDK/runtime CMake performs the specified load-command and ad hoc signature
fixups; distribution signing and system security approval are separate checks.

Start Player and inspect `ModLoader/ModLoader.log`. Verify BML initialization,
the intended version/target and the loaded Mod list. A native-only build reports
script support unavailable; adding a script file cannot enable it.

## Install Mods

- Native: put `Name.bmodp` and **its** `Name.bmodp.abi.json` together in
  `ModLoader/Mods`, or install a ZIP containing the pair and resources. The same
  extension names native modules on all three OS families.
- Script: install one `Name.mod.as`, an entry directory or a ZIP. A directory or ZIP
  must contain exactly one `*.mod.as` entry; helper files use plain `.as`.
  Script support and the matching CKAngelScript plugin must be present.
- Preserve relative resource directories and exact filename case. Do not install
  editor completion files, compiler caches, a whole SDK or duplicate copies of
  the same Mod ID.

Old SDL `BMLEntry`/`IMod` binaries require [migration](native-mod-migration.md)
and rebuilding. Do not bypass a missing/mismatched sidecar by copying another
Mod's JSON. The Loader checks target/protocol and opted-in extension identities;
the SDK content hash is diagnostic, not a license to mix engine ABIs.

## Settings, fonts and input

Use the Mod Menu for declared configuration. Preserve `ModLoader/Configs` during
updates. Text and paths are UTF-8. The built-in ImGui atlas, game menu fonts and
engine-backed GUI text have separate responsibilities; fonts in `ModLoader/Fonts`
do not replace every game text path.

The SDL platform backend handles text composition and events. The Windows IME
implementation retains IMM/TSF support; other platforms may use native candidate
windows. Focus/composition/fullscreen/controller behavior still needs each
platform's real acceptance. Use `bml system` and the log for diagnostics.

## Command line

Press `/` to open the command bar. It accepts shell-style syntax:

- Words are separated by unquoted spaces. `'...'` is literal, `"..."` keeps
  spaces and allows `$NAME` inside, and `$'...'` resolves escapes such as `\n`
  and `\e`. A backslash escapes only shell characters, so
  `echo C:\Maps\x.nmo` preserves the backslashes. Prefer `/` for portable
  resource paths and quote paths containing spaces.
- `a; b` runs both, `a && b` runs `b` only when `a` succeeded, `a || b` only
  when it failed, and `a | b` feeds what `a` printed into `b`. `$?` holds the
  status of the last command. `# ...` is a comment.
- `set NAME VALUE` defines a variable for this session, `set -U NAME VALUE`
  keeps it across sessions, `set -e NAME` removes it, and `$NAME` or `${NAME}`
  inserts it. `$(cmd)` inserts what `cmd` printed.
- `alias name='cmd args'` defines a shortcut for the first word of a line;
  `alias` lists them and `unalias name` removes one. Aliases persist.
- `history` numbers past lines; `!!`, `!n`, `!-n`, and `!text` rerun them.
- Filters for pipelines: `grep`, `head`, `tail`, `wc`, `sort`, `uniq`, and
  `xargs`. `true` and `false` set a status without doing anything.

Editing keys: Tab completes commands, aliases, `$` variables, and a command's
own arguments, also after `;` or `|`. Up and Down walk the history, keeping the
text being typed and filtering by it. A grey suggestion from history follows the
caret; Right or End accepts it. Ctrl+R searches the history, Ctrl+A and Ctrl+E
jump to the line ends, Ctrl+U, Ctrl+K, and Ctrl+W cut to the start, to the end,
or one word, Ctrl+Y pastes what was cut, and Ctrl+L clears the board. An
unfinished line, such as an open quote or a trailing `&&`, asks for another row
instead of running. Continuation rows grow upward so the completion/search/IME
rail stays below the row being edited and the message board stays above the
whole command. The physical rows are saved and recalled as one history entry.

Variables, aliases, and history are stored in `ModLoader/CommandBar.shell.json`
and `ModLoader/CommandBar.history`. Set `CommandBar.KeepOpen` in `BML.cfg` to
keep the bar open after a command runs. Run `help` to see the commands available
in the current installation.

The `CommandBar` configuration category also provides independent switches for
syntax highlighting, Tab completion and its candidate rail, inline history
suggestions, Ctrl+R reverse search, and Up/Down history navigation. Timed
notifications and the scrollback shown while the command bar is open can be
controlled separately with `ShowNotifications` and `ShowScrollback`. These
settings can be changed from the Mod Menu and take effect immediately.

While the command bar is open, its input supports the usual mouse selection,
copy, cut, and paste operations. The message board is read-only: drag across
its scrollback to select text, then press Ctrl+C to copy it without ANSI color
codes. Ctrl+A selects all scrollback after the message board has been clicked.
Dragging past the top or bottom edge continues the selection while scrolling.
Clicking or selecting scrollback keeps the command session open.

Syntax highlighting uses the Atom One Dark palette by default. Every role can
be customised independently in the `CommandBarTheme` category with a
`#RRGGBB` or `#RRGGBBAA` value; malformed values fall back to that role's One
Dark default. Valid colour values use a palette button in the Mod Menu, and
theme changes take effect immediately.

## Updates and recovery

Use [Updater and doctor](updater-sdl.md). Close Player before update or rollback.
Channels are target-specific, signed, and preserve user Mods/configuration.
Doctor is a diagnostic tool, not a substitute for launching the game.

A manual update also uses the matching complete runtime package. Preserve user
configuration, keep Player's engine/SDL and do not overlay an unrelated SDK.

## Uninstall

Close Player. Remove the Loader from Managers and its owned runtime files.
Keep `ModLoader` if you want to retain Mods and settings. Remove the script
plugin only if it is not needed by other game content. Do not remove Player's
CK2, VxMath or SDL.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No BML startup log | Correct game root and Managers library; wrong OS/architecture; duplicate old Loader |
| Module rejected | Matching `.bmodp.abi.json`, `bml.mod` protocol, required provider/version and extensions |
| Missing library / wrong image | Run doctor; inspect the matching runtime, executable permissions and host dependencies |
| Script unavailable | Runtime descriptor `scripts`, matching BuildingBlocks plugin, `script status` / `script diag <id>` |
| Resource or include not found | Exact case, relative path, packaged resource and ZIP entry layout |
| Two SDL instances or broken input | Restore matching Player/SDK deployment; do not bundle a second SDL with the Mod |
| Interrupted update | Keep the transaction state and use doctor/rollback as described in the updater guide |

Report the target descriptor, BML/Player versions, log, reproduction steps and
which package was installed. State what ran on real hardware; a successful
cross-build alone is not gameplay acceptance.
