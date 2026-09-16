# BML+ native mod API overview

This page groups the public headers installed by the BML+ SDK by purpose. The
installed `include/BML` directory defines the supported native API.

## Minimal entry point

A native mod is a dynamic library that exports `BMLEntry` and normally uses the
`.bmodp` extension:

```cpp
#include <BML/IMod.h>

class MyMod final : public IMod {
public:
    explicit MyMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "MyMod"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "My Mod"; }
    const char *GetAuthor() override { return "Author"; }
    const char *GetDescription() override { return "Example"; }
    DECLARE_BML_VERSION;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new MyMod(bml); }
MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
```

The object returned by `BMLEntry` is allocated by the Mod DLL. Export
`BMLExit` and destroy that same object there so allocation and deallocation use
the same C++ runtime. BML calls `BMLExit` when registration fails after object
creation and when a loaded native Mod is unloaded. For compatibility, BML can
still load an older DLL without `BMLExit`, but it logs a warning and cannot
destroy that Mod instance safely.

Use the CMake helper installed with the SDK:

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

`bml_add_mod` links `BML::BML`, enables C++20, disables compiler extensions,
and produces `MyMod.bmodp`. It requires an MSVC-compatible 32-bit target and
makes the linker require the exact C symbols `BMLEntry` and `BMLExit`. A missing
or C++-mangled entry point therefore fails the build instead of producing a Mod
that the loader cannot use safely.

`bml_install_mod` adds the standard install rule. Set `CMAKE_INSTALL_PREFIX` to
the Ballance `ModLoader` directory, then use CMake's `install` target to build
and deploy the Mod under `ModLoader/Mods`.

## Public headers

| Header | Purpose |
| --- | --- |
| `Version.h`, `Defines.h` | Version macros, export macros, status codes, and base definitions |
| `Result.hpp` | Reusable C++ result carrying a BML status code, an optional value, and an optional module-specific diagnostic |
| `BML.h` | C ABI for version, loader and mod directories, command unregistration, memory, encoding, path, file, and Zip utilities |
| `BMLAll.h` | Convenience header that includes the complete native SDK surface |
| `IMod.h`, `IMessageReceiver.h` | Mod metadata, lifecycle, gameplay, and engine callbacks |
| `IBML.h` | Loader services, CK managers, lookup, commands, timers, and dependencies |
| `ICommand.h` | Command execution, completion, and basic argument parsing |
| `IConfig.h` | Typed configuration properties |
| `ILogger.h` | Info, Warn, and Error logging |
| `DataShare.h` | Low-level, named in-process byte sharing |
| `Types.h`, `TypeConvert.h` | Object references, vectors, and matrices, plus conversions to and from the Virtools types |
| `Interface.h` | The versioned interface structs the loader hands out, and how to ask for one |
| `Behavior.h`, `Behavior.hpp` | Virtools Building Block discovery, authoring, execution, graph inspection, and editing |
| `Runtime.h`, `Scene.h`, `Gameplay.h`, `Speedrun.h`, `UI.h` | Loader capabilities reached through an interface struct, with inline C++ wrappers |
| `ModMenu.h`, `ModMenu.hpp` | Pure C Mods-menu page interface and its type-safe C++ authoring layer |
| `Imc.h`, `ImcWire.hpp`, `ImcCpp.hpp` | IMC C/C++ runtime and wire format |
| `Bui.h` | Ballance-style ImGui widgets |
| `Gui.h`, `Gui/*.h` | `BGui` wrappers around Virtools entities and behaviours |
| `InputHook.h` | Keyboard, mouse, controller state, and paired input-block tokens |
| `ExecuteBB.h` | v0.3 compatibility interface for executing or creating common Building Blocks |
| `ScriptHelper.h` | Find, connect, insert, and remove behaviour nodes and parameters |
| `Guids.h`, `Guids/*.h` | Virtools and Ballance Building Block GUIDs |

`BML::Result<T>` combines a BML status code with an optional value. Modules that
need structured diagnostics can use `BML::Result<T, ModuleStatus>`; for example,
`Behavior::Result<T>` supplies `Behavior::Status` without changing the shared
container or the C ABI. `Failure()` normalizes a non-error code to
`BML_ERROR_FAIL`, so a failed `Result<void>` cannot appear successful.

## Custom Mods menu pages

A Native Mod can append Ballance-styled entries to its own details page with
`BML::ModMenu::Page`. Config categories remain first; custom entries follow in
registration order and share the same four-item pagination. Selecting an entry
opens the Mod's fully custom page:

```cpp
#include <BML/Bui.h>
#include <BML/ModMenu.hpp>

class DiagnosticsPage final : public BML::ModMenu::Page {
public:
    DiagnosticsPage()
        : Page("diagnostics", "Diagnostics", "Show live runtime details") {}

protected:
    BML::ModMenu::PageAction OnFrame() override {
        Bui::Title("Diagnostics");
        // Draw ImGui or Bui widgets directly into the active page here.
        return Bui::NavBack() ? BML::ModMenu::PageAction::Back
                              : BML::ModMenu::PageAction::None;
    }
};

// Keep this object alive as part of the Mod.
DiagnosticsPage diagnostics;

void RegisterMenuPages() { (void) diagnostics.Register(); }
void UnregisterMenuPages() { (void) diagnostics.Unregister(); }
```

The loader has already opened the full-viewport ImGui page when `OnFrame` runs;
draw its contents directly and do not call `ImGui::NewFrame` or `ImGui::Render`.
Return `Back` to restore the Mod details page or `Close` to leave the Mods menu.
The loader copies the id, label, and description, but the page object and callbacks
must remain alive until unregistration. Remaining registrations are removed before
the owner DLL unloads. Version 1.0 exposes this capability to Native Mods only.
Call the two registration helpers from the owning Mod's `OnLoad` and `OnUnload`.
`ModMenu.h` remains usable from C and contains no C++ standard-library or class
surface; `ModMenu.hpp` is a one-way authoring facade over that C interface. At
the C seam, `BML_ModMenuPageDraw` returns a `BML_OK`/error status and writes a
deferred navigation request to the `Action` member of `BML_ModMenuPageFrame`. Navigation is
therefore never overloaded as an error result, and future frame inputs or outputs
can be appended behind `StructSize` without changing the 1.0 callback signature.

## Mod lifecycle and events

`IMod` inherits `IMessageReceiver`. A mod supplies its ID, version, name,
author, description, and required BML version, then overrides only the
callbacks it needs:

- lifecycle: `OnLoad`, `OnUnload`, `OnProcess`, and `OnRender`;
- configuration and commands: `OnModifyConfig`, `OnPreCommandExecute`,
  `OnPostCommandExecute`, and `OnCheatEnabled`;
- engine objects: `OnLoadObject`, `OnLoadScript`, `OnPhysicalize`, and
  `OnUnphysicalize`;
- game flow: menu, load, start, reset, pause, exit, next-level, death, finish,
  checkpoint, life, and navigation callbacks from `IMessageReceiver`.

`OnProcess` is the only callback that runs inside the active ImGui frame. Draw
every ImGui and `Bui` control from it, and never from `OnRender`. See
[Three UI surfaces](#three-ui-surfaces).

`OnRender` receives one `CK_RENDER_FLAGS` value. The native API does not expose
separately named before-render and after-render callbacks. Loader notifications
arrive synchronously through the `IMod` and `IMessageReceiver` virtuals listed
above. A mod that needs deferred handling should copy the data it needs into a
queue it owns.

## `IBML` services

`IBML` is the main loader service passed to a mod. It provides:

- the CK context and Attribute, Behavior, Collision, Input, Message, Path,
  Parameter, Render, Sound, and Time managers;
- `AddTimer` and `AddTimerLoop` scheduling by frames or by milliseconds;
- game state, cheat control, in-game messages, and command registration,
  lookup, and execution;
- named lookup for DataArrays, Groups, Materials, Meshes, 2D/3D Entities,
  Cameras, Lights, Sounds, Textures, and Behaviors;
- Initial Condition and visibility changes, plus skipping the next render tick;
- ball, floor, module, and transformation type registration and SR/HS scores;
- mod enumeration and lookup, plus dependency registration and queries.

Create timers through `IBML`. The SDK does not publish a standalone `Timer.h`;
the loader owns scheduling and callback processing.

`AddTimer` and `AddTimerLoop` are each overloaded on `CKDWORD` and `float`. The
`CKDWORD` overloads count frames, the `float` overloads count milliseconds, and
both units share one name, so an unsuffixed integer literal is ambiguous and
fails to compile. Write the suffix explicitly:

```cpp
bml->AddTimer(1ul, [] { /* next frame */ });
bml->AddTimer(1000.0f, [] { /* one second later */ });
bml->AddTimerLoop(1.0f, [] { return KeepRunning(); });
```

The loop callbacks keep running while they return `true`. Neither overload
returns a handle, so a scheduled timer cannot be cancelled; make the loop
callback return `false` instead.

## Native mod dependencies

Register dependencies before BML initializes mods. The constructor is the
usual place because it runs while `BMLEntry` creates the mod and before any
`OnLoad` callback:

```cpp
explicit MyMod(IBML *bml) : IMod(bml) {
    AddDependency("RequiredMod", BMLVersion(1, 2, 0));
    AddOptionalDependency("OptionalMod", BMLVersion(1, 0, 0));
}
```

BML orders mods so installed dependencies receive `OnLoad` before their
dependents. A missing optional dependency is ignored. A missing required
dependency or a dependency cycle prevents the mod initialization phase from
starting; the log identifies the requesting mod, required id and version, or
the mods affected by the cycle. If an installed dependency is older than the
requested version, BML skips the dependent mod's `OnLoad` and reports both
versions while continuing with other mods.

## Configuration, commands, and logging

`IConfig` retrieves an `IProperty` by category and key. Properties can be
String, Boolean, Integer, Float, or Keyboard Key and support current values,
defaults, comments, and category comments. There is no separate UTF-16
property API; use the explicit conversion functions in `BML.h` when needed.

`ICommand` provides the command name, aliases, description, cheat flag,
execution, Tab completion, and basic Integer, Float, and Boolean parsers.
`ILogger` provides three log levels.

`IBML::RegisterCommand` takes a raw `ICommand *` and the loader never deletes
it. Registration returns `void` and only writes to the log when it fails, which
happens for a null command, an invalid name or alias, and an already registered
name.

`IBML` has no unregister function, so removing a command goes through
`BML_UnregisterCommand` in `BML.h`:

```cpp
void MyMod::OnUnload() {
    if (BML_UnregisterCommand("mycmd") == BML_OK)
        delete m_Command;  // only now is deleting it safe
}
```

Only the DLL that registered a command may remove it, which the loader decides
by remembering which module called `RegisterCommand`. Asking about someone else's
command answers `BML_ERROR_ACCESS_DENIED`, an unknown name answers
`BML_ERROR_NOT_FOUND`, and the name is matched the way the console matches it, so
the alias names the command too. Call it from the game thread.

Without that call, a registered command stays in the command table until the
process ends. In particular, do not delete an `ICommand` in `OnUnload` while it is
still registered: unloading a mod does not remove its commands by itself, so a
deleted command leaves a dangling entry that the console will still try to run.

`ParseFloat` clamps to the whole finite float range by default. Earlier releases
defaulted its lower bound to `FLT_MIN`, the smallest positive normal value, so
negative input was silently clamped to about `1.17e-38`. Pass explicit bounds
when a command needs a narrower range.

## Loader capabilities

Capability added after the legacy C++ interfaces were frozen is published as a
versioned interface struct, fetched by id and major version through
`BML_GetInterface`. Each has its own header under `include/BML`, and each header
also declares an inline C++ namespace that folds the lookup and the argument
checks in:

- `BML::Runtime` for runtime state, clock, and scores;
- `BML::Scene` for object information, transforms, and named lookup;
- `BML::Gameplay` for level, energy, catalog, checkpoint, and reset data;
- `BML::UI` for the message board, mod/map menus, and HUD;
- `BML::Speedrun` for the shared speedrun timer;
- `BML::Behavior` for Virtools Building Block discovery, configured Runs,
  copied Frames, graph inspection, Watches, Patches, Plans, and revisioned
  installation bindings for installed Edit Nodes and parameter Ports.

`Interface.h` documents the version rules: a struct grows only by appending a
member and bumping its minor version, and `BML_IFACE_HAS` asks whether the
running loader has a member added after the header the mod was built against.

`bml.behavior` deliberately uses Virtools' own Prototype, Layout, Setting, Pin,
Local, In, Out, Pout, Graph, Patch, and Plan vocabulary. Its C interface is the
stable transport seam; Native C++ Mods should normally use `Behavior.hpp`, which
owns strings, arrays, callbacks, handles, and Frame bytes. See
[Behavior authoring](behavior-authoring.md) for the complete ownership, thread,
world-reset, error-handling, and hot-path rules.

The native `BML::Gameplay` collection reads return complete snapshots in a
caller-owned `std::vector`. Read the catalog during setup and refresh level
checkpoints or reset points when the level changes; these calls transfer the
complete collection and are not intended for per-frame polling.

The inline C++ operations return a BML status and are marked `[[nodiscard]]`.
Handle the returned status, or use an explicit `(void)` cast when deliberately
discarding the result of best-effort cleanup.

## Inter-mod communication

Prefer IMC for an ordinary API a Mod publishes to other Mods:

- a `.imc` file contains interface declarations only; field IDs are permanent
  wire identifiers rather than array positions;
- `bml_target_imc_api` generates C++ bindings and adds them to a target;
- RPC supports synchronous calls, futures, cancellation, timeouts, and
  completion callbacks;
- Topic supports bounded subscriber queues, unsubscribe, and drop counts;
- generated types and cached route IDs keep text parsing out of hot paths;
- a consumer discovers at runtime whether a route is there, so provider and
  consumer can ship separately.

C++ IMC operations that return a BML status are marked `[[nodiscard]]` as well.

A native base Mod that must expose direct function pointers or borrowed engine
objects may instead publish a plain-C function table with
`BML_RegisterInterface`. The table and its `InterfaceId` must be static data in
the provider DLL. Consumers declare that Mod as a required dependency, fetch the
table through `BML_GetInterface`, and never link an import library or resolve an
API export from the provider. BML rejects duplicate id/major pairs and removes a
provider's registrations before unloading its DLL.

New providers should start with the `interface-provider` profile and edit its
small definition instead of hand-writing the ABI header:

```text
interface yourname.value-provider.value 1.0

fn read_value(int input) -> int value
```

`bml_add_generated_interface_package` generates the plain-C table, Traits,
provider/version constants, and standalone header package. The adjacent lock
records method order and signatures. Run `.\bml interface update` only after
reviewing an intentional compatible edit; an appended method requires a higher
minor version, while a changed or reordered method requires a new major version.

The generated header is equivalent to the following advanced, hand-written
form. Traits contain only compile-time interface metadata: identity, major
version, and the minimum usable member.

```cpp
#include <BML/Interface.h>

struct ExampleInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *ReadValue)(int input, int *outValue);
};

#ifdef __cplusplus
#include <BML/Interface.hpp>
BML_DECLARE_INTERFACE_TRAITS(ExampleTraits, ExampleInterface,
                             "example.value", 1, ReadValue);
#endif
```

The provider uses `MakeInterface` to fill the header and `Publication` to own
the registration. The table itself must still have static storage:

```cpp
constexpr auto kExample =
    BML::Interfaces::MakeInterface<ExampleTraits>(0, &ReadValue);

BML::Interfaces::Publication<ExampleTraits> m_Example;

void OnLoad() override {
    const int status = m_Example.Open(kExample);
    // Handle status. Destruction is a cleanup fallback.
}

void OnUnload() override {
    (void)m_Example.Close();
}
```

The consumer binds dependency declaration and typed lookup with
`RequiredInterface`. Construct it as an `IMod` member, then open it from
`OnLoad`; it preserves the exact BML status and remains valid through the
consumer's `OnUnload` because dependencies unload in reverse order:

```cpp
#include <BML/ModInterface.hpp>

BML::Interfaces::RequiredInterface<ExampleTraits> m_Example;

ExampleConsumer(IBML *bml)
    : IMod(bml), m_Example(*this, "ExampleProvider", BMLVersion(1, 0, 0)) {}

void OnLoad() override {
    const int status = m_Example.Open();
    if (status == BML_OK) {
        int value = 0;
        (void)m_Example->ReadValue(35, &value);
    }
}
```

Use `OptionalInterface` in the same shape when the provider is optional. Its
`Open` may normally return `BML_ERROR_NOT_FOUND`. `Interface.h` keeps the raw C
functions for C consumers and advanced ownership tests; the C++ authoring layer
does not add an ABI or force process-local calls through IMC.

### Publish the interface header independently

Do not copy the shared header into every consumer and do not publish a provider
import library. The provider project can install a header-only CMake package:

```cmake
find_package(BML CONFIG REQUIRED)

bml_add_generated_interface_package(ExampleValue
    VERSION 1.0.0
    INPUT api/value.bml-interface
    PROVIDER_ID "yourname.value-provider"
    PROVIDER_VERSION "1.0.0"
    NAMESPACE Example
    OUTPUT_NAME ValueInterface.h
)

bml_add_mod(ExampleProvider src/Provider.cpp)
target_link_libraries(ExampleProvider PRIVATE ExampleValue::Interface)
```

The definition's adjacent lock preserves the order of the generated function
table. Installation preserves the generated relative header path and emits
`ExampleValueConfig.cmake`, a same-major version file, and the header-only
`ExampleValue::Interface` target. That target carries the BML SDK dependency,
not the provider binary.

`bml_add_interface_package` remains available when an advanced provider needs
to maintain its plain-C header manually.

The independent consumer project uses only the installed packages:

```cmake
find_package(BML CONFIG REQUIRED)
find_package(ExampleValue 1 CONFIG REQUIRED)

bml_add_mod(ExampleConsumer src/Consumer.cpp)
target_link_libraries(ExampleConsumer PRIVATE ExampleValue::Interface)
```

The SDK ships complete projects under `examples/native-interface-provider` and
`examples/native-interface-consumer`. They build from separate source trees;
the latter has no provider source, library, or binary on its link path.

`DataShare` is suitable for small named byte values when both sides obey its
reference-count and borrowed-pointer lifetime rules. Use IMC when an API has to
evolve on its own schedule, or needs RPC or Topic semantics.

## Three UI surfaces

- `Bui` draws Ballance-style ImGui widgets for native overlays.
- `BGui` creates in-game UI from Virtools 2D Entities and Behaviors.
- `BML::UI` does not draw widgets; it controls loader-owned messages, menus,
  and HUD state. Every one of its calls has to be made from the game thread.

These surfaces solve different problems and are not interchangeable.

### Draw ImGui from `OnProcess`

The loader owns the ImGui frame. It opens the frame before mod callbacks run and
ends it immediately after `OnProcess` returns:

1. the loader calls `ImGui::NewFrame` before the per-frame mod callbacks;
2. every mod's `OnProcess` runs inside that frame;
3. the loader calls `ImGui::Render`, which ends the frame;
4. `OnRender` runs;
5. the loader submits the recorded draw data.

So `ImGui` and `Bui` calls belong in `OnProcess`. The same calls made from
`OnRender` happen after the frame has ended: they draw nothing and may trip an
ImGui assertion. `BML::UI` message, menu, and HUD calls are not affected,
because they change loader state instead of recording draw commands.

## C API ownership

`BML.h` and `DataShare.h` are callable through the C ABI. Release strings,
wide strings, string arrays, wide-string arrays, and binary buffers allocated
by BML with the matching `BML_Free*` function. Do not call CRT `free` across a
DLL boundary.

`BML_DataShare_Get` returns a borrowed pointer. It becomes invalid when the
same key is set or removed or when the instance is destroyed. Use
`BML_DataShare_CopyEx` when a stable copy is required.

## Where the loader and your mod live

`BML.h` answers both questions a mod has about the file system:

```cpp
// The loader's own directories. Borrowed, valid for the process, never freed.
const char *loaderDir = BML_GetLoaderPathUtf8(BML_DIR_LOADER);

// Your own installation directory. Allocated, so release it.
char *modRoot = BML_GetModRootUtf8(nullptr);
// ... use modRoot ...
BML_FreeString(modRoot);
```

`BML_GetLoaderPathW` and `BML_GetLoaderPathUtf8` take a `BML_LoaderDirectory`:
`BML_DIR_WORKING`, `BML_DIR_TEMP`, `BML_DIR_GAME`, `BML_DIR_LOADER`, or
`BML_DIR_CONFIG`. They return a pointer the loader owns, so do not free it. Do
not confuse them with `BML_GetDirectoryA/W/Utf8`, which parse a path string and
return its directory part.

`BML_GetModRootW` and `BML_GetModRootUtf8` answer with the directory a mod is
installed in. Pass `nullptr` for your own: it resolves the calling DLL, needs no
registration, and therefore works from your constructor. Pass a mod id to ask
about another mod, which answers with its DLL's directory for a native mod and
with its script root for a script mod. Both allocate, so release the result with
`BML_FreeWString` or `BML_FreeString`.

Both return null while the loader is still initializing and when the directory
cannot be resolved. `BML_GetModRoot` also takes the loader's mod-registry lock,
so call it from your mod rather than from `DllMain`.

## Further reading

- [Which native API to use](native-api-routes.md)
- [Choose a mod development route](modding.md)
- [Inter-mod communication](imc.md)
- [Create a typed IMC API](imc-author-guide.md)
- [Native mod template](https://github.com/doyaGu/BallanceModLoaderPlus/tree/main/templates/native-mod-template)
