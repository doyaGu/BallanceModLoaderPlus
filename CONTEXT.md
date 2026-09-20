# Ballance Mod Loader Runtime Context

This file names the loader-owned features and their owners. It is a map, not an
implementation history. Use the source and focused guides for detailed behavior:
[Behavior authoring](docs/en/behavior-authoring.md) and
[UI automation](tests/ui/README.md).

## Authoring and public UI

**Mod Project** — An author-owned source tree for one Native or Script Mod.
Developer Workflow may add build metadata, but does not replace the Mod's runtime
identity.

**Developer Workflow** — The SDK Python interface for creating or adopting a
Mod Project, building it, starting Player, collecting diagnostics, and packaging
it. Native and Script adapters share the entry point but retain their different
build and reload rules.

**Bui** — The public C++ Interface for Ballance-styled ImGui controls and small
Window, Page, Pagination, and Menu primitives. It owns shared visual resources
and keyboard blocking, not the state of a loader feature.

**Built-in Loader Mod** — The loader-owned Mod named BML. It receives lifecycle
callbacks and forwards them to built-in features; it does not own their internal
state.

**Built-in Mod Menu** — The in-game Mod and configuration browser. Model combines
Mod metadata, Config categories, and registered Pages; Session owns navigation,
drafts, and reconciliation; Presentation owns the native-looking menu layout.
Bui::Menu owns route and Page lifecycle. An unregistered Mod still gets its
standard details and Config presentation.

**Config Property Editor** — Optional, non-persisted schema metadata attached
through `BML_GetConfigPropertyEditor` / `BML_SetConfigPropertyEditor`, keeping
the frozen IProperty vtable unchanged. It selects a Built-in Mod Menu editor
without changing the stored value type. The default follows PropertyType; the
colour editor keeps STRING storage while presenting `#RRGGBB` or `#RRGGBBAA`
through Bui.

**Mod Menu Page** — An optional Native Mod page registered with a stable id,
label, description, and draw callback. The loader keys it by owner and
registration generation, so removing and re-registering an id cannot revive a
stale route. The public C ABI lives in ModMenu.h and the C++ authoring layer in
ModMenu.hpp. Nested Mod-owned routing and AngelScript exposure are not part of
the 1.0 Interface.

## Built-in gameplay features

**Built-in Console** — Command bar, message board, history, commands, and
configuration. One viewport layout places the message board above the command
bar and a single transient completion, reverse-search, or IME row below it.
Continuation rows grow upward and move the message-board boundary with them;
they remain one logical command for completion and history. Clicking outside
an open command session or entering a new scene closes it and releases keyboard
capture.

**Shell Editing Analysis** — The private Built-in Console Module that interprets
incomplete shell source at a caret. It owns quote context, replacement ranges,
alias-expanded command/argument roles, and typed command-head ranges shared by
completion and syntax highlighting; execution remains owned by the strict
Parser.

**Command Bar Feature Policy** — The private Built-in Console policy that
enables or disables syntax highlighting, Tab completion, history suggestions,
reverse search, and history navigation as independent capabilities. CommandBar
owns the state transitions required when a capability is disabled.

**Command Bar Syntax Theme** — The private Built-in Console palette that maps
shell highlight roles to configurable RGBA colours. Atom One Dark is the
default; its Settings adapter owns role metadata and adapts persisted hex
strings into the palette, Console wires it to Config, and CommandBar only
consumes typed colours.

**Message Board Display Policy** — The private Built-in Console policy that
independently controls timed notifications with the command bar closed and
stored scrollback with it open. Messages remain recorded and logged even when
one or both presentations are disabled.

**Built-in HUD** — HUD tree, title, FPS, speedrun timer, cheat indicator,
commands, and configuration.

**Built-in Custom Maps** — Map discovery, selection, preparation, and load
orchestration. Menu selection stays separate from loading and Behavior edits.

**Custom Map Level Loader** — The private Custom Maps Module that reads the
values to be changed, installs a Behavior Plan, and restores the previous values
if loading fails or ends. It does not own menu state, files, or user messages.

**Built-in Gameplay Tweaks** — Configurable game corrections and their Virtools
script bindings.

**Built-in Game Event Hooks** — Adapters from retail script and menu transitions
to Mod lifecycle and gameplay callbacks. They do not own tweak state.

## UI and input

**UI Automation Session** — A test-only cross-process exchange between Player
and one visible UI runner. Player publishes a numbered checkpoint; the runner
performs the requested input or capture and acknowledges that checkpoint. Logs
are diagnostic, never the synchronization mechanism.

**UI Automation Journey** — One independently runnable Player scenario under
tests/ui/player/journeys/. Its descriptor selects the same source for the build
and runner. The user-facing action must pass through visible UI or native input;
fixtures may prepare data but cannot perform the action being tested.

**Overlay Platform Input** — The Win32 window adapter for ImGui's platform
backend. It observes the Player window tree and IME messages, but submits
ordinary input only from the backend window. Its narrow queue hook recovers
IME-consumed Tab navigation without rewriting or consuming the original
message. The ImGui Win32 backend alone submits committed characters.

**Script ImGui Ownership** — Tracks ImGui interaction created by each Script
Mod call. Script window ids include the owning Mod identity while preserving
the requested visible title, so equal names from different Mods remain
independent. It releases only that Mod's remaining interaction state on
unload; the shared ImGui context belongs to Overlay.

**ImGui Callback Recovery** — Captures ImGui stack depths before each Mod
callback and restores unbalanced windows, styles, ids, and related stacks
before the next Mod runs. Native and Script callbacks share the same recovery
mechanism; recovery contains a failed Mod without replacing normal balanced
window lifecycle.

**IME Runtime** — Mirrors composition through IMM32 and obtains modern
candidate state and control through a UI-less TSF sink. It publishes immutable,
revisioned frames and clears stale state on composition, focus, or lifecycle
changes. It never draws ImGui or submits text.

**IME Presentation** — Draws one in-game composition and candidate rail in
windowed and true exclusive full-screen modes. Console may reserve the row
below its command bar; other text fields use their caret and viewport. It owns
measurement, layout, and drawing, not native IME state or character submission.

**ANSI Text** — Parses terminal-style formatting and prepares reusable
font-aware wrapped layouts for Console drawing. Opacity and palette can vary
without repeating line layout.

**Built-in UI Font Runtime** — Resolves the configured ImGui font profile,
fallback order, coverage, and live atlas replacement for loader-owned text.
Independent Native Mod fonts remain untouched. The font command is its user
interface.

**Game Font Catalog** — Resolves retail menu font names to indices created by
the current world. It is separate from the ImGui font atlas.

**Legacy GUI Text** — The private style registry for BML::Gui::Text sprites
backed by Windows font faces. It is separate from both ImGui and retail menu
fonts. See [font domain decision](docs/adr/0001-separate-font-domains.md).

## Behavior and engine access

**Behavior Authoring** — The public Win32 C function table and its C++ and
AngelScript authoring layers. An owner-scoped Session configures Blocks from
registered Prototypes, runs them as Calls, Tasks, or Instances, and builds
graph Edits for Patches and Plans. Sessions survive world reset; world-bound
objects and callbacks do not. The C, C++, and script layers use one native
implementation. The legacy IMod interface still bootstraps Native Mods.

**Behavior Plan Instance** — A revisioned view of one Plan installation in a
world. Symbolic Nodes and Ports can be resolved to checked Object References or
read and written through the installed parameter relation. World reset,
replacement, disable, or rebuild invalidates an older view; no CK pointer is
kept alive for the caller.

**Behavior Runtime** — The private CK adapter that instantiates a configured
Block as a real CKBehavior, executes it, captures Frames, and completes its
lifecycle at safe points. It does not own top-level Script roots or graph
mutation. Private implementation types live in BML::Behavior::Internal.

**Behavior Graph Authoring** — Script owns a top-level root; Graph captures an
immutable snapshot; Edit describes symbolic changes. Patch applies one exact
world, while Plan resolves rules across later worlds. Installation and
restoration are checked and atomic for each requested change; rejected or
partially restored work remains visible as explicit status.

**Graph Pattern** — A serializable structural selector inside an Edit. It
matches observable Nodes, Ports, Links, and bounded paths without retaining a
live Graph or an author callback.

**Named Building Block Adapters** — Header-only definitions under
include/BML/Behavior/Blocks/ for known retail Prototypes and their parameters.
They provide configuration, not a second Runtime.

**Physics Force** — The private target-indexed owner of persistent Physics
Force Instances. It retains the same Instance through Create and Shutdown,
handles pending callbacks, and retires deleted targets safely.

**Object References** — BML::ObjectRefs issues and resolves opaque
BML_ObjectRef values across the C and script seams. It observes CK deletion and
world reset, rejecting stale identities. Destruction without CK notification is
outside what it can observe.

**ExecuteBB Adapter** — The API-side adapter that translates exported
ExecuteBB calls into Blocks and delegates execution to Behavior Runtime.
Behavior Runtime does not depend on ExecuteBB.

## Source ownership

- src/BML.cpp composes the loader; src/Mods/ contains its bundled Mods.
- src/Loader/ owns Mod discovery, invocation, and CK manager integration.
- src/Api/ adapts public BML interfaces to private implementations.
- src/Behavior/ owns Sessions, Prototypes, graph authoring, and Runtime.
  src/Behavior/Blocks/ holds private implementations tied to individual BBs.
- src/Console/, src/HUD/, src/CustomMaps/, src/Gameplay/, and src/ModMenu/
  own their named built-in features.
- src/UI/ owns shared Overlay, input, text, fonts, and IME infrastructure.
- src/Config/, src/DataShare/, src/Imc/, src/Logging/, src/Hooks/, src/Virtools/,
  src/AngelScript/, and src/Utils/ keep their respective supporting code local.
- tests/unit/ groups in-process tests by domain. tests/abi/, tests/codegen/,
  and tests/sdk/ check public headers, generators, and installed consumers.
  tests/player/ drives gameplay acceptance; tests/ui/ owns ImGui and visible UI
  acceptance. Test-only Player automation does not live in src/UI/.
