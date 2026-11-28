# Script Mod Examples

These examples are the next step after `templates/script-mod-template`. Each
directory is a complete Mod with exactly one `*.mod.as` entry.

| Example | What it demonstrates | Result |
| --- | --- | --- |
| `command-config` | Register a command and persist a setting | `/examplefeature toggle` changes and saves a switch |
| `input-ui` | Read keyboard input and draw an ImGui window | F9 hides or shows a small window |
| `game-state` | Read typed game state and borrow a CK object safely | Entering a level logs the current active ball |

Copy one example directory to `<Ballance>/ModLoader/Mods`, start Player, and
verify the result above before changing the source. Do not install the whole
`script-mod` directory at once: every example has its own Mod identity.

These are focused examples, not application skeletons. Start a real Mod from
the template, then copy only the part you need. The full guides are under
`share/BML/docs/en/script-mod` and `share/BML/docs/zh-CN/script-mod-tutorial`.

These examples use the aligned authoring API. Service operations return an int
status plus out values; success is BML::ERROR_OK. Keep the explicit callback
Context and use Config/Input/Timers/Commands views. A script Ref is not RAII: end
requests explicitly or let its runtime retire them. See the migration guide in
share/BML/docs/en/script-mod/migration.md; no old aliases are registered.
