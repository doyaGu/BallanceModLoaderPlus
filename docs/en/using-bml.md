# Use BML+ with Ballanced

## Requirements

Use a Ballanced SDL Player and a BML+ runtime matching its platform,
architecture, host linkage and build configuration. Native Mods also require
a matching SDK. Engine/ImGui extensions additionally require matching compiler
ABI and C++ runtime. Dynamic hosts are supported;
static-host integration remains pending.

## Install BML+

Close Player and extract the matching runtime package into the game root.
BML+ belongs in `Managers`, CKAngelScript in `BuildingBlocks`, and native Mods
in `ModLoader/Mods`. CK2, VxMath and SDL are supplied by Player; keep one engine
and one SDL instance in the process. Standard `Bin/Player` and flat `Player`
layouts are supported. On Windows the executable is `Player.exe`.

The runtime's `ModLoader/BML.abi.json` identifies its target. Start Player and
check `ModLoader/ModLoader.log`; open the in-game Mod menu to inspect versions.
Native packages include both `.bmodp` and `.bmodp.abi.json`. Resource and script
packages use ZIP. Extracted script projects may also live in `ModLoader/Mods`.

## Configuration and diagnostics

User settings live under `ModLoader/Configs`; preserve them during updates.
Use the in-game console's `bml system` command and `ModLoader/ModLoader.log` for
live diagnostics. Fonts ship under `ModLoader/Fonts`. Chinese composition uses
SDL text input. The Windows IMM/TSF implementation is retained; other platforms
use their native input services.

## Command line

Press `/` to open the command bar. It accepts shell-style syntax:

- Words are separated by unquoted spaces. `'...'` is literal, `"..."` keeps
  spaces and allows `$NAME` inside, and `$'...'` resolves escapes such as `\n`
  and `\e`. A backslash escapes only shell characters, so
  `map load C:\Maps\x.nmo` needs no quoting.
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

## Update BML+

Follow [Updater and doctor](updater-sdl.md). Signed channels distinguish runtime,
platform, architecture and linkage. The updater preserves configuration, keeps
backups and supports recovery after an interrupted transaction. Player must be
closed. A matching full runtime package can also be installed manually.

## Uninstall

Close Player. Remove the BML+ library from `Managers` and its owned runtime
files. Keep `ModLoader` if you want to retain configuration and installed Mods.
Do not remove Player's engine or SDL libraries.

## Troubleshooting

A rejected native Mod usually has a missing or mismatched ABI descriptor.
Rebuild it using the matching installed SDL SDK. For missing dependencies, run
`Updater doctor` and inspect the loader log. Case-sensitive filesystems require
exact resource and script filenames. A successful cross-build does not certify
native game functionality on that target.

Older SDL BMLEntry/IMod modules require migration and recompilation; schema 2
packages use the C protocol. See [native migration](native-mod-migration.md).
