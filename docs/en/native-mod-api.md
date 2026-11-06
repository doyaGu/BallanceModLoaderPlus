# Native Mod API

SDL Native Mods use the versioned `bml.mod` C protocol. The C++ SDK is a
header-only authoring layer: its classes, STL values, virtual calls and callback
captures stay inside the Mod module. The loader receives C records, handles and
function pointers. The original Windows backend is maintained on its own branch.

## Entry and events

```cpp
#include <BML/Mod.hpp>

class MyMod final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("My Mod loaded");
    }
    void OnStartLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Level started");
    }
};

BML_DEFINE_MOD(MyMod, {
    .Id = "example.my-mod",
    .Name = "My Mod",
    .Version = "1.0.0",
})
```

`BML_DEFINE_MOD` exports `BML_GetModInterface`. Querying it repeatedly returns
immutable metadata and never constructs a Mod. The loader copies that metadata,
validates the protocol and dependencies, then calls Create and OnLoad. Keep
registration in OnLoad; an Instance must be default constructible. Its destructor
must not throw or start new host work. The module that allocates the Instance
also destroys it. `examples/native-c-mod` shows the same protocol in C11.

Use named fields directly in `BML_DEFINE_MOD`; optional fields may be omitted.
Dependencies fit in the same declaration:

```cpp
.Dependencies = {{"example.provider", "1.2.0"}, {"example.optional", "", true}},
```

No separate Define function, identity getters, entry function, constructor or
empty lifecycle callbacks are needed. The macro supplies the existing C entry,
local Create/Destroy and only the callbacks you override. Owned registrations
stay in Instance members and are cleaned by the host before Instance destruction;
OnUnload remains available for work such as joining your own threads.
`BML_DEFINE_MOD(MyMod, DefineMyMod)` remains supported for computed metadata.
Both forms evaluate metadata once at the first successful query, inside the
exception-safe entry. They do not add initialization at DLL load or a new C ABI.

Id, Name and Version are required. Author and Description may be empty.
Versions use `MAJOR.MINOR.PATCH` with no leading zeroes or suffixes. Declare
required and optional dependencies in `Definition::Dependencies`; an empty
minimum version imposes no version floor. Do not register dependencies from
OnProcess. The SDK does not silently raise the minimum loader version.

Override only the callbacks needed. Their existing names remain available:

- OnLoad, OnUnload, OnProcess and OnRender;
- OnLoadObject, OnLoadScript, OnPhysicalize and OnUnphysicalize;
- OnModifyConfig, OnCheatEnabled, OnPreCommandExecute and OnPostCommandExecute;
- the existing menu, level, checkpoint, ball, navigation, counter and life
  callbacks, including OnStartLevel, OnDead, OnPauseLevel and OnUnpauseLevel.

Each receives `const Mod::Context &` first. Engine notifications carry the named
records in `Mod.hpp`, using checked ObjectRefs and borrowed spans/string_views.
The C protocol combines game notifications in OnGameEvent and command phases
in OnCommandEvent. The C++ default dispatch preserves the individual names; an
override of the combined callback should call the base implementation when it
also wants those convenience overrides.

OnProcess runs inside the active ImGui frame, after the engine managers'
PostProcess and the Behavior safe point. Timer/IMC/script pumps retain their
existing order. BML then finishes deferred input cleanup and updates snapshots.
This occurs inside CKContext::Process, before it returns. OnRender does not
create an ImGui frame or advance timers. Render and Process may run separately;
`UI::SkipNextRender(ctx)` consumes its request at the next Render, not at
the next Process.

Context binds the common services as `ctx.Config()`, `ctx.Input()`,
`ctx.Timers()` and `ctx.Commands()`. These are cheap non-owning views over the
existing wrappers. `<BML/Mod.hpp>` supplies this common surface without engine
or ImGui headers. See the complete `examples/native-mod/Mod.cpp`: declarations
include defaults/comments together, commands return an owned Registration, and
timers accept either a no-argument lambda or the full Context/Event callback.
Void interval callbacks continue until cancellation; bool callbacks may return
false to finish. Result access failures retain `std::bad_optional_access` as
the exception family and report the status and any supplied diagnostic.

## Pick a service

| Headers | Authoring surface |
| --- | --- |
| `Mod.h/.hpp` | Protocol, Context, metadata, Mod queries, events, logging and resources |
| `Config.h/.hpp` | Typed declarations and non-owning Property views |
| `Timer.h/.hpp` | Owned registrations, timeouts, intervals and finite repeats |
| `Input.h/.hpp` | Raw/filtered snapshots, device blocks, cursor and repetition |
| `Command.h/.hpp` | Console registration, arguments, completion, shell status and pipelines |
| `Behavior.h/.hpp`, `Behavior/Blocks/` | Prototypes, execution, Frames, scripts and reversible graph editing |
| `Scene.h`, `Gameplay.h`, `Time.h/.hpp`, `Speedrun.h`, `UI.h` | Scene/game state, clocks, game registration, render options, HUD and loader menus |
| `ModMenu.h/.hpp` | Owner-scoped visible/hidden pages with Push/Replace/Back/Close navigation |
| `Bui.h/.hpp` | Ballance-styled immediate controls and local Window/Page/Menu helpers |
| `Gui.h/.hpp`, `Gui/*.h` | Local BGui controllers backed by owned engine UI elements |
| `DataShare.h/.hpp` | Named bytes with the existing request/cleanup contract |
| `Imc.h/.hpp`, `ImcWire.hpp` | Generated inter-Mod RPC, futures and Topics |
| `Interface.h/.hpp` | Required-provider process-local C tables |
| `BML.h` | Existing allocation, encoding, path, file and ZIP utilities |
| `Engine.h/.hpp`, `ImGui.h/.hpp` | Explicit optional CK2/VxMath and host ImGui interoperability |

`.h` service tables compile as C11. `.hpp` files provide module-local C++20
wrappers. `BMLAll.h` collects the core surface and does not include engine or
ImGui headers. `IMod`, `IBML`, `IConfig`, `ICommand`, `InputHook` and ExecuteBB
remain private implementation adapters; they are not an installed SDL authoring
protocol. See the [migration guide](native-mod-migration.md).

## Configuration

```cpp
#include <BML/Config.hpp>

auto config = ctx.Config();
auto enabled = config.DeclareProperty({"General", "Enabled", true}).Take();
const bool value = enabled.Get<bool>();
const int status = enabled.Set(false);
```

A declaration copies its metadata. Repeating the same category/key/type returns
the existing property without overwriting it; another type reports TYPE_MISMATCH.
`SetDefault` declares a default rather than resetting a user value. The stepwise
style is supported: declare a property without a default, then call SetDefault.
Read returns owning Info; Get reads a typed value and supports an explicit
fallback. Property views do not own the configuration and become invalid with
the owner or declaration generation.

Only declared keys are persisted. The existing store supplies loaded values,
notifications, migration, choice/colour editors and atomic UTF-8 saves. A failed
save retains dirty values for explicit retry or reattachment on the next load;
it does not require keeping a Mod DLL loaded solely for file I/O.

## Timers and callbacks

```cpp
#include <BML/Timer.hpp>

// Keep this as an Instance member; discarding the returned registration cancels.
auto result = ctx.Timers().SetTimeoutTicks(1, [ctx] {
    (void)ctx.LogInfo("Next timer pump");
});
BML::Timer::Registration timer = result.Take();
```

Timeout/interval methods use milliseconds or explicitly named ticks. TIME and
REALTIME definitions also use milliseconds. Finite REPEAT accepts 1..INT32_MAX;
infinite RemainingIterations is -1. Counts saturate at INT32_MAX. A zero delay
runs in a later pump. DEBOUNCE/THROTTLE enum values remain reserved and creation
reports NOT_IMPLEMENTED. Priority is clamped to -128..127.

Registration is move-only RAII. Cancel inside its callback prevents later calls;
returning true cannot resurrect it. Cancellation, natural completion and owner
cleanup share one release path with the script adapter. Script TimerRef retains
its VM semantics: dropping the reference does not cancel the registration.

At the new C seam, successful Register transfers exactly one userdata Release
responsibility. Failure leaves it with the caller. Release waits for active
callbacks and borrows. Explicit cancellation failure preserves the wrapper for
retry; a destructor that cannot cancel abandons further business calls and leaves
release to host cleanup. Existing DataShare/Behavior retain their own documented
cleanup/Retain rules; do not apply Timer ownership to those protocols.

## Input

```cpp
#include <BML/Input.hpp>

auto input = ctx.Input();
auto raw = input.ReadKeyboard(BML_INPUT_RAW).Take();
auto filtered = input.ReadKeyboard().Take();
auto block = input.AcquireBlock(BML_INPUT_BLOCK_KEYBOARD).Take();
const bool down = raw.IsKeyDown(30); // CK keyboard code; no engine header needed.
```

The engine acquires devices. BML's existing InputHook supplies filtered queries
and saved original queries for RAW. RAW bypasses BML blocks, not the input
manager's Pause. Reading never pumps events or advances input. Counted blocks
are owner-scoped, independent and move-only; Release/owner cleanup removes only
that share. Original mouse/controller, cursor, repetition and keyboard edge
behaviour are retained.

Keyboard spans are borrowed for the enclosing Mod/service callback. Repeated or
nested reads and same-frame block changes do not invalidate an earlier view;
new reads reflect the current block state. Copying the span does not extend its
lifetime. Store an owning copy only when work must outlive the callback.

## UI and engine access

Draw Bui and ImGui in OnProcess. `Bui::Window`, Page and Menu remain module-local;
construct them with Context. Bui::At restores its cursor and propagates local
C++ exceptions after returning from the C seam. Begin/End must pair even for a
collapsed window. Another owner or nested drawing scope cannot end your window.

`BGui::Gui`, Element, Panel, Text, Label, Button, Input and KeyInput also take
Context. Their callbacks stay in the Mod. The host owns their main and auxiliary
CK objects and invalidates them on deletion/reset. A failed composite Add rolls
back its temporary elements. Keep the three font domains separate: retail game
fonts for Label, CKSpriteText fonts for Text, ImGui fonts for the overlay.

`UI.h` controls the loader's HUD/messages/menus; it is separate from Bui and BGui.
`ModMenu::Page` adds an entry under the owning Mod. Keep the Page alive until
unregistered, pass an owning Context to your Page subclass for Bui drawing, and
register/unregister it in OnLoad/OnUnload. Visible and hidden routes, generation
checks and existing C Page callbacks remain unchanged.

For direct engine access, include `Engine.hpp` and link `BML::Engine`.
`Engine::BorrowContext`, BorrowRenderContext and BorrowObject return checked,
non-owning CK pointers. ReferenceObject produces the existing ObjectRef. Retain
ObjectRefs or CKObjectHandles, not raw pointers across world changes. Managers
come from CKContext's existing methods. `Behavior/Blocks`, ScriptHelper, Guids
and TypeConvert are engine-dependent conveniences; ScriptHelper executes locally.

Direct ImGui additionally requires `BML::ImGui` and `ImGui.hpp`. Use the host
context; never compile a second ImGui instance. `Bui/ImGui.hpp` retains advanced
ImGui flags/callback overloads, and `Gui/Engine.hpp` supplies Vx/CK conveniences.
Textures reuse CKTexture/CKSprite and existing object references. No rasterizer
ABI or second texture-handle family is exposed. Own draw callbacks, userdata and
textures must remain alive until that frame's backend submission or discard.

## ABI, errors and teardown

All new C function pointers use explicit cdecl and fixed-width scalar values.
Records use pack(8), restore caller packing and expose frozen 1.0 prefix sizes.
Unknown tails are ignored; short required prefixes are rejected without writing
beyond the caller's capacity. Use BML_IFACE_HAS before optional appended slots.
C strings are UTF-8; capacity/required includes NUL for text, not binary data.
A too-small buffer reports the required size and does not publish partial text.

Context is a copyable, non-owning owner capability. Store its value, never the
reference passed to a callback. A ModHandle is an observation identity, not a
capability or instance pointer. Default service calls require the game thread;
copying Context does not authorize worker-thread calls. DataShare/IMC keep their
existing thread contracts.

`Result<T>` owns explicitly requested values in the caller module. Failed
results have no value; check before Value/Take. Ordinary service errors remain
caller-handled. A failed main notification or Timer callback stops that owner's
ordinary dispatch and cleans up at a safe point; remaining Mods still receive
the event. C++ trampolines contain exceptions inside the module.

Unload completes consumers before their providers. It stops admission, drains
active callbacks, calls OnUnload, cleans owned resources and calls Destroy.
Successful stages are not replayed on retry. Failed OnUnload/restoration retains
the necessary instance and module. An owner must stop and join its own threads
and unregister direct system/engine callbacks in OnUnload; report failure if it
cannot. Destroy/Release must not throw or request new host work.

## Build and package

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyMod LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
# Only when used:
# target_link_libraries(MyMod PRIVATE BML::Engine BML::ImGui)
bml_install_mod(MyMod)
```

Core C ABI modules do not require the host's C++ compiler/standard library.
Optional engine/ImGui modules require matching compiler, runtime, headers and
public compile settings. The SDK resolves those dependencies only for targets
that use them, including intermediate interface libraries.

`bml_add_mod` creates a `.bmodp` plus `.bmodp.abi.json`. Schema 2 checks runtime,
platform, architecture, host linkage, `bml.mod` major and required extension ABI
ids. The SDK content hash is diagnostic. Older SDL C++-entry modules require
recompilation and receive a migration error. Distribute both files or their ZIP;
never distribute user configuration. Static-host integration is still pending.
macOS/ELF modules resolve the Player-supplied dependencies using SDK link rules;
the test runner does not relocate consumer binaries.
