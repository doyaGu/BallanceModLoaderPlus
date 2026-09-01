# Ballance Mod Loader Runtime Context

This context names the loader-owned runtime features that ship with BML itself. It distinguishes the built-in Mod's lifecycle role from the player-facing features it owns.

## Language

**Built-in Loader Mod**:
The loader-owned Mod registered with the id `BML`. It receives Mod lifecycle callbacks and owns the built-in runtime features without absorbing their state and behavior.
_Avoid_: BML service, built-in feature

**Built-in Console**:
The loader's in-game command surface, comprising the command bar, message board, command history, built-in command registration, and their configuration. One Built-in Loader Mod owns one Built-in Console.
_Avoid_: CommandBar, MessageBoard, console UI

**Built-in HUD**:
The loader's in-game status and custom overlay surface, comprising the HUD tree, title, FPS display, speedrun timer, cheat indicator, HUD command, and their configuration. One Built-in Loader Mod owns one Built-in HUD.
_Avoid_: HUD window, HUD host, HUD service

**Built-in Custom Maps**:
The loader's custom-map feature, comprising map discovery, map selection, temporary map preparation, game-script bindings, level selection, and its configuration. One Built-in Loader Mod owns one Built-in Custom Maps Module.
_Avoid_: map menu, map loader, custom map service

**Built-in Gameplay Tweaks**:
The loader's configurable game corrections, comprising the lantern alpha-test option, life-ball freeze fix, overclock patch, and their Virtools script bindings. One Built-in Loader Mod owns one Built-in Gameplay Tweaks Module.
_Avoid_: script patches, tweak settings, gameplay fixes

**Built-in Game Event Hooks**:
The loader-owned Virtools script adapters that translate game and menu transitions into Mod lifecycle and gameplay callbacks. One Built-in Loader Mod owns one Built-in Game Event Hooks Module.
_Avoid_: EventHookRegistrar, callback patches, event bridge

**Behavior Authoring**:
The public `bml.behavior` interface through which a Native Mod creates a Session, describes a Block from a Virtools Prototype, and obtains a Run as a Call, Task, or Instance. A Block names its Target, Setting stages, Pins, Locals, and Frame retention. A Run exposes Pulse/Continue state and owns copied Frames containing the active Outs and Pout parameter values observed for each native Execute. Object-valued Pouts hold a `BML_ObjectRef` issued while the value still names a live CK object; taking a Frame never reads the original CK parameter or object again. Sessions belong to one Native Mod generation, survive a world reset, and stop admitting work when that owner retires.
`BML::Behavior::Sessions` owns the owner-scoped Session and Run records behind this interface. It is not a second Behavior model: it delegates each native Instance to Behavior Runtime and preserves Frames after that Instance has closed.
`BML/Behavior.hpp` is the typed Native C++ facade over that same C seam. Its mutable Builder supports one-shot fluent Call/Start/Spawn use; its private Compiler can instead freeze an immutable Block for repeated runs across different owners. Compilation owns all author strings and values, pins the current provider generation when reliable Prototype discovery is available, checks only the first Setting stage against the declared Layout, and builds one stable C descriptor reused by the Block hot path. Later Setting stages, Pins, Locals, owner/target liveness, manager availability, and native type compatibility remain live Runtime admission concerns. The facade returns distinct Call, Task, and Instance handles and decodes the same Frame wire data; it does not implement a second scheduler, parameter model, or result store.
`BML::Behavior::PrototypeCatalog` is the private catalog behind Prototype discovery. It owns copied declaration and provider metadata, assigns a process-monotonic generation to each provider registration, and invalidates cached declared layouts when Virtools retires a declaration. A `PrototypeRef` is the Prototype GUID plus that provider generation; it prevents a previously discovered Block from silently binding to a replacement DLL. Discovery is exposed only when declaration retirement can be observed reliably, and provider reload requires all live Runs from that provider to be closed first. Detached compatibility belongs to the same provider generation: a known graph-only Prototype is rejected before lifecycle mutation, while an unclassified Prototype remains usable and marks its Run as `UnverifiedDetached`.
A declared Layout describes the Prototype contract without creating a `CKBehavior`; a live Layout describes one configured Run after Setting callbacks and dynamic layout changes. Both use the same Slot vocabulary and parameter descriptions. `BML::Behavior::Parameter` centralizes those descriptions through `CKParameterManager`, including Virtools type compatibility and the supported literal forms; it is not a second codec or type registry.
`Inspect` returns an owned Logical or Live view of a native Behavior graph: Nodes, Ports, Links, delay state, Prototype identity, and stable fingerprints contain no borrowed CK pointer. `Read` observes stored or direct/shared parameter values without evaluating a Parameter Operation; an operation is reported as indeterminate. A Watch compares graph, layout, or sampled values once per game frame and uses the shared callback lease protocol. Portable CK2.1 reports exact data-change observation and delayed-list membership as unavailable or unknown instead of inferring them from residual values.
_Avoid_: Behavior transport, Behavior service, execution facade, codec, object snapshotter, preflight capture

**Behavior Runtime**:
`BML::Behavior::Runtime`, the private Instance implementation behind Behavior Authoring and the loader's concrete Building Block modules. It resolves a Virtools Building Block Prototype, creates a configured instance, reflects its live layout, binds parameter values or sources, runs its callback lifecycle, and distinguishes immediate, persistent, graph-resident, and cross-frame execution. Settings are applied before final input binding because they may rebuild the instance layout. One loader runtime owns one Behavior Runtime.
Resolved slots carry the configured instance id and layout generation; a settings callback or execution makes old slot handles stale instead of silently retargeting an ordinal. Runtime records keep private CK ID-and-address stamps for owned objects; the public object-reference interface is not an internal lifetime mechanism. Literal sources are independent, runtime-owned CK parameters rather than extra behavior or parent locals. Normal release is delayed past native manager callbacks and sends DETACH/DELETE without misusing RESET; world reset explicitly closes retained state before discarding runtime records.
Detached execution is implemented by the private `BML::Behavior::Execution` module. It owns the Instance state machine, logical In admission, one-Execute-per-frame rule, function-retry versus graph-activity continuation, immutable active-Out Frames, bounded retention, and terminal diagnostics. `Runtime` is its CK2 adapter; deterministic tests use a fake adapter at the same seam. Building Blocks consume their own active In, while the execution implementation clears only active Out after capture.
Native configuration and teardown are implemented by the private `BML::Behavior::Lifecycle` module. It owns callback ordering, Setting replay, the placed/created/attached ledger, callback revalidation, compensating teardown, and deferred close admission; `Runtime` is its only CK2 adapter. Callback code is held through the private `BML::Behavior::Callback` lease protocol, which closes admission without waiting for the active invocation and retires callback state only at a game-thread safe point. Script Hook Blocks use the same lease and lifecycle path rather than a separate active-call protocol.
_Avoid_: action cache, Building Block singleton, fixed pin table, synchronous-function wrapper, Virtools runtime

**Behavior Spec**:
`BML::Behavior::Spec`, the declarative interface between a named Building Block module and the Behavior Runtime. A spec contains only the Prototype GUID, slot selectors, values or sources, and setting stages; all creation, ownership, callback, execution, and destruction behavior remains in the Behavior Runtime. Each concrete Building Block keeps its options and spec construction in its own `src/Behavior/<BuildingBlock>.*` module rather than a shared spec collection.
_Avoid_: preset object, graph recipe, Building Block implementation, action module

**Physics Force**:
The `BML::Behavior::PhysicsForce` module, which owns the options, spec construction, and target-indexed sessions for persistent Virtools Physics Force instances. Create and Shutdown use the same configured instance because the Building Block stores its native handle in a local parameter. Replacement and shutdown wait until a later physics epoch because the Building Block may retain its `CKBehavior` in a pre-simulation callback before writing that handle. A deleting target leaves the active index immediately, while its instance remains in a retiring queue until native shutdown is safe; a replacement target therefore cannot inherit the old session.
_Avoid_: force action cache, independent Set/Unset calls, force manager

**Object References**:
`BML::ObjectRefs`, the private module at the C and script interface seam that issues and resolves opaque `BML_ObjectRef` values. It observes normal CK object-deletion notifications and world reset, and rejects stale domain, generation, address, and deletion state. Behavior Runtime and Physics Forces do not depend on this module; they keep their ownership and delayed-lifetime knowledge local. Objects deliberately destroyed with `CK_DESTROY_NONOTIFY` are outside this interface because CK managers cannot observe that lifecycle.
_Avoid_: CK identity registry, universal CK identity, Behavior object handle

**ExecuteBB Adapter**:
`BML::ExecuteBBAdapter`, the stateful adapter behind the exported `ExecuteBB` interface. It translates API calls into Behavior Specs and delegates them to the Behavior Runtime. The adapter belongs to the API seam; it is not part of the Behavior module and the Behavior module never depends on it.
_Avoid_: ExecuteBB runtime, Behavior implementation, Virtools adapter

## Source layout

The private source tree follows these runtime concepts instead of collecting unrelated code under generic `Core`, `Runtime`, or `Builtin` directories:

- `src/BML.cpp` is the loader composition root.
- `src/Mods/` contains concrete bundled `IMod` implementations. `BMLMod` assembles the built-in modules, while `NewBallTypeMod` is an independent bundled Mod.
- `src/Loader/` owns Mod discovery, registration, invocation, lifecycle, and CK manager integration.
- `src/Api/` adapts the public BML interfaces to loader-owned implementations, including Behavior Authoring, Object References, the `ExecuteBB` facade, and its stateful adapter.
- `src/Behavior/` contains owner-scoped Behavior Sessions, the Prototype Catalog, shared Layout and Parameter semantics, the Behavior Runtime, and one module per concrete Building Block (`HookBlock`, `ObjectLoad`, `Physicalize`, `PhysicsForce`, `PhysicsImpulse`, `PhysicsWakeUp`, `SendMessage`, and `Text2D`). Physics Force keeps its persistent session implementation beside its spec. These modules depend on CK infrastructure but never on `Loader` or `Api`; the Runtime never depends on a concrete Building Block.
- `src/Console/`, `src/HUD/`, and `src/CustomMaps/` contain the Built-in Console, Built-in HUD, and Built-in Custom Maps modules respectively.
- `src/Gameplay/` contains the loader-owned game session, game event hooks, and gameplay tweaks.
- `src/Config/`, `src/DataShare/`, `src/Imc/`, and `src/Logging/` each keep one cross-cutting runtime concern local.
- `src/UI/` contains concrete UI such as the Mod menu; `src/Hooks/` contains process and engine hooks; `src/Virtools/` contains only shared low-level CK graph helpers that do not belong to a deeper runtime module. The Hook Block Prototype, registration, execution, and spec all belong to `src/Behavior/HookBlock.*`.
- `src/AngelScript/` and `src/Utils/` remain independently navigable implementation families.

Private includes use these directory names explicitly, so a caller reveals which module interface it crosses.

## Example dialogue

> **Developer:** Should command history be saved by the Built-in Loader Mod?
>
> **Domain expert:** No. Command history belongs to the Built-in Console; the Built-in Loader Mod only tells it when loading and unloading occur.
>
> **Developer:** Should the Built-in Loader Mod update the speedrun timer and HUD elements itself?
>
> **Domain expert:** No. Those belong to the Built-in HUD; the Built-in Loader Mod only forwards game lifecycle events.

> **Developer:** Should the Built-in Loader Mod know which Virtools parameters a custom map load changes?
>
> **Domain expert:** No. Those bindings and the loading sequence belong to Built-in Custom Maps; the Built-in Loader Mod only forwards the relevant object, script, and game lifecycle events.

> **Developer:** Does the Overclock graph state belong to Built-in Game Event Hooks because both inspect gameplay scripts?
>
> **Domain expert:** No. Built-in Game Event Hooks only translate game transitions into callbacks. Overclock and the other configurable corrections belong to Built-in Gameplay Tweaks.
