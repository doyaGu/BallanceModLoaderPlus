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
The pre-release Win32 Native C++ interface for using Virtools Behaviors. A Native Mod opens an owner-scoped Session, selects a registered Prototype, configures a copyable Block, and creates a Call, Task, or Instance that owns one real `CKBehavior`. A Block contains its Prototype, Target, Setting stages, and initial Pin and Local values; it does not contain graph relations or Frame retention. Frame retention belongs to the Call, Task, or Instance that observes execution. The same interface creates owner-scoped top-level Scripts, inspects Behavior graphs, and composes symbolic Edits under one exact-world Patch or cross-world Plan. Script creation atomically admits the root and its initial Edit: failure exposes no Script, while success returns one inactive Script that owns the installed graph. Sessions survive world reset, but their world-bound Runs, Scripts, Watches, and Patches do not; all callback and native state retires before the owning Mod DLL unloads.
_Avoid_: Behavior transport, execution facade, Builder, compiler product, codec

**Behavior Runtime**:
The private native Instance implementation behind Behavior Authoring and loader-owned Behavior consumers. All private Behavior implementation types live in `BML::Behavior::Internal`; `BML::Behavior` is reserved for the public C++ interface, so the Loader can consume that interface without giving public and private types the same linkage identity. Runtime consumes a `BlockSpec`, creates and configures one Building Block `CKBehavior`, reflects its live Layout, applies that Block's Target and parameter relations, executes under the correct CK context, captures Frames, and completes CREATE/ATTACH/RESET/DETACH/DELETE at game-thread safe points. The private Block Module owns `BlockSpec`; it is not Runtime protocol state. Execution and Lifecycle are Runtime's CK2-independent internal seams; Runtime is their CK2 adapter. Runtime neither owns top-level Script roots nor creates graph-owned Parameter Operations: those belong to Behavior Graph Authoring. The private Script Module owns root owner, Scene, activity, and teardown lifecycle. A Ready Instance remains owned and may still hold Local or manager state until explicit teardown.
_Avoid_: Runtime Spec, Building Block singleton, Prototype-specific state table, synchronous-function wrapper, Virtools scheduler

**Behavior Graph Authoring**:
The Script, Graph, Edit, Patch, and Plan model used to create, observe, and change native Behavior graphs. Script owns one world-bound top-level graph root and enters its owner exactly once through `CKBeObject::AddScript`; its initial Edit is installed before the Script is published or returned, and closes before the root. Creation leaves it inactive, while activity and teardown requests complete at Behavior safe points. Graph is an immutable Logical or Live snapshot of Nodes, Links, Ports, and graph-owned Parameter Operations. Edit is an Edit-local symbolic transformation: `Edit::Graph()` denotes the target root, while added Blocks and appended root ports or Locals describe both a new Script body and changes to an existing graph. `AddOperation` declares one native `CKParameterOperation` by its operation GUID and exact result/input type tuple; its inputs use the same Bind relation as Block Pins, and Virtools retains its normal lazy result evaluation. `Replace` exchanges one idle child Node for a Block with the same public interface while retaining native Links, delays, public parameter relations, name, priority, and owner; the original Node is parked without lifecycle callbacks and restored before the replacement is destroyed, while private Settings and Locals are never copied. `Remove` parks one existing child Node and all incident Behavior Links without destroying them, disconnects the parked Links so active sibling outputs cannot traverse them, then restores their exact identities, endpoints, and delays when the Patch closes. Removal requires the selected Node, its control ports, and every incident Link source to be idle; an active non-target sink has already received its activation and does not depend on the Link. Because the retail SDK does not expose delayed-list membership, any positive remaining delay that differs from the Link's initial delay is treated as in-flight, including after the graph is deactivated without a reset. Unrelated graph work may remain active. Node edits exclude active Link overlays and prevent later Patches until they close. Edit copies each configured Block atomically, then describes graph structure and parameter relations around that Block. Settings belong only to the Block and are never represented as separate Edit steps. For a Node added by an Edit, all added Nodes exist first; its final graph parameter relations are then visible to its single `CKM_BEHAVIOREDITED` callback before control Flow makes the Node reachable. Existing Nodes whose parameters or parameter relations change receive one `CKM_BEHAVIOREDITED` after the transaction applies those changes and one after Patch teardown restores them; pure control Flow changes do not send that callback. Every such callback is followed by Layout reflection and relation validation. Patch binds an Edit to one exact graph fingerprint and may therefore use a Block, Node, Link, or Value from that world. Plan resolves the same durable intent against selected scripts in later worlds and rejects every non-null Object Reference retained by the Edit or one of its Blocks. CKEdit owns native mutation and its checked inverse, including operation ownership and Node replacement/removal, while Topology and Relations validate Link overlays and parameter claims without mutating CK objects.
_Avoid_: endpoint-pair Link identity, best-effort rollback, world-bound Plan, graph builder

One Edit contains an explicit root graph scope and may enter graph-backed Nodes recursively. A nested scope owns only its internal Nodes and may publish ports to its parent; ports from different internal scopes never connect directly. `AddGraph` creates a real graph-backed child and its body in that same transaction. One Patch owns an ordered set of exact-world Graph targets and restores all of them in reverse if any target fails or disappears. One Plan owns several independent Script rules; each rule produces an atomic Patch for its root and nested scopes, while the Plan survives world reset. Patch and Plan retain their identity across Enable, Disable, and Replace. Replace preserves an unchanged prefix, replaces only the changed suffix, and restores the previous complete definition when the new definition fails.

**Named Building Block Adapters**:
Header-only definitions under `include/BML/Behavior/Blocks/*.hpp` that map one known retail BB's Prototype, Options, Settings, Pins, and Locals into an ordinary Behavior Block. They contain BB-specific parameter knowledge but do not own creation, execution, lifecycle, graph mutation, or teardown. Stateful loader consumers such as Physics Force may retain Runs separately without adding Prototype-specific behavior to Runtime.
Each installed header owns the single Prototype and configuration rule for that BB and contains only public dependencies. `BlockSpec::From(options)` applies that same rule directly inside the Loader; installed headers have no private compile mode or parallel private adapter.
_Avoid_: internal BB implementation, spec collection, action module, Runtime special case

**Physics Force**:
The private `BML::Behavior::Internal::PhysicsForce` module, which owns the options, spec construction, and target-indexed sessions for persistent Virtools Physics Force instances. Create and Shutdown use the same configured instance because the Building Block stores its native controller in a Local. `PhysicsCallbackContainer::Process(callback)` first invokes Create synchronously; only a target without a PhysicsObject leaves the callback pending for a later simulation. Same-frame Set or Clear may queue Shutdown under Runtime's one-Execute-per-frame rule, so the session remains in its stopping state until Shutdown clears the old Local; an updating Set also retains the next requested force and creates a fresh instance only after that point. A deleting target leaves the active index immediately, while its instance remains in a retiring queue until a pending callback has observed cancellation or native Shutdown is complete. The Ballance Player probe waits for `OnBallNavActive`, then verifies that a `Ready` Create leaves a live controller, motion continues while the Instance is idle, Shutdown clears the Local, the last same-frame Set wins, and repeated Clear retires the session.
_Avoid_: force action cache, independent Set/Unset calls, force manager

**Object References**:
`BML::ObjectRefs`, the private module at the C and script interface seam that issues and resolves opaque `BML_ObjectRef` values. It observes normal CK object-deletion notifications and world reset, and rejects stale domain, generation, address, and deletion state. Behavior Runtime and Physics Force do not depend on this module; they keep their ownership and delayed-lifetime knowledge local. Objects deliberately destroyed with `CK_DESTROY_NONOTIFY` are outside this interface because CK managers cannot observe that lifecycle.
_Avoid_: CK identity registry, universal CK identity, Behavior object handle

**ExecuteBB Adapter**:
`BML::ExecuteBBAdapter`, the stateful adapter behind the exported `ExecuteBB` interface. It translates API calls into `BlockSpec` values and delegates them to the Behavior Runtime. The adapter belongs to the API seam; it is not part of the Behavior module and the Behavior module never depends on it.
_Avoid_: ExecuteBB runtime, Behavior implementation, Virtools adapter

## Source layout

The private source tree follows these runtime concepts instead of collecting unrelated code under generic `Core`, `Runtime`, or `Builtin` directories:

- `src/BML.cpp` is the loader composition root.
- `src/Mods/` contains concrete bundled `IMod` implementations. `BMLMod` assembles the built-in modules, while `NewBallTypeMod` is an independent bundled Mod.
- `src/Loader/` owns Mod discovery, registration, invocation, lifecycle, and CK manager integration.
- `src/Api/` adapts the public BML interfaces to loader-owned implementations, including Behavior Authoring, Object References, the `ExecuteBB` facade, and its stateful adapter.
- `src/Behavior/` contains owner-scoped Behavior Sessions, the Prototype Catalog, the private `BlockSpec`, top-level Script lifecycle, graph inspection/editing and durable Plans, shared Layout and Parameter semantics, and the Behavior Runtime. Named Building Block definitions are header-only adapters under `include/BML/Behavior/Blocks/`; loader-owned consumers lower the same configuration into `BlockSpec`. Only stateful services such as Physics Force sessions and the live Text2D view remain private modules. These modules depend on CK infrastructure but never on `Loader` or `Api`; the Runtime never depends on a concrete Building Block.
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
