# Which native API to use

Use the C++ wrappers over the versioned C tables for native authoring. They are
local to your Mod, so normal C++ values and virtual overrides do not cross the
module boundary. C11 callers use the same `.h` tables directly. There is one
loader state and one set of host services shared with scripts.

| Need | Use |
| --- | --- |
| Identity, lifecycle, existing game event names | Mod.h/.hpp, Instance, immutable Definition |
| Log, directory, Mod query, packaged resource | Mod::Context |
| Typed configuration and menu editor metadata | Config::View / Property |
| One-shot/loop/finite timer | Timer::Registration; explicit ms/ticks |
| Keyboard, mouse, controller, counted blocking | Input::View / Block, RAW for original queries |
| Console command and completion | Command::Registration |
| Checked object identity and scene state | Scene.h and existing ObjectRef |
| Game, cheat, highscore, energy, level data | Gameplay.h |
| CK clock/tick and speedrun timer | Time.h/.hpp and Speedrun.h |
| Load-time ball/floor/module registration | Gameplay registration definitions with Context |
| One pending Render skip / widescreen correction | UI::SkipNextRender / SetWidescreenAdjustment |
| Initial conditions / object visibility | Scene::SetIC / RestoreIC / Show |
| Messages, built-in HUD and menus | UI.h |
| Add an entry to your Mod details | ModMenu::Page |
| Ballance-style immediate UI | Bui.hpp; local Window/Page/Menu |
| Engine-backed 2D UI | Gui.hpp and local BGui controllers |
| Prototypes, Blocks, execution and graph editing | Behavior.hpp and Behavior/Blocks |
| Direct CK objects/managers | Engine.hpp + BML::Engine, then existing CK2 APIs |
| Direct host ImGui | ImGui.hpp + BML::ImGui |
| Advanced widget flags/callbacks | Bui/ImGui.hpp (Engine + ImGui) |
| Vx/CK conveniences for BGui | Gui/Engine.hpp (Engine) |
| Small named bytes shared by Mods | DataShare |
| Typed services between native and scripts | Generated IMC RPC/Topics/futures |
| Native provider's process-local C table | Interface.hpp and a declared provider dependency |
| Existing filesystem/ZIP/encoding/allocation utility | BML.h; release with the documented BML_Free family |

`BMLAll.h` is a core aggregate. Engine-dependent Blocks, ScriptHelper, Guids,
TypeConvert and raw CK/ImGui access require explicit optional targets. IMod,
IBML and their old service vtables have been removed, including private
implementations. Internal registration uses composed ModRecord values; author
Instance virtual calls stay inside the Mod.

Use IMC for ordinary inter-Mod services. A provider C table is appropriate for
native base infrastructure whose consumers can declare and retain that provider
dependency. The loader retires consumers first; caching an undeclared provider's
pointer does not establish ownership.

New calls normally return BML_OK or a negative status. C++ Result values own only
explicitly requested snapshots; event views remain borrowed. Main notification
failure stops the owner; an ordinary service error or shell exit status remains
the caller's responsibility. The old BML.h utilities retain their documented
0/1 conventions. Default engine/service operations remain game-thread-only.

See [Native API](native-mod-api.md), [migration](native-mod-migration.md),
[Behavior authoring](behavior-authoring.md), and [IMC](imc.md).
