# Behavior authoring

`BML/Behavior.hpp` is the C++ interface for Native Mods that use Virtools Behaviors. It can create and execute any registered Building Block, create a top-level Script graph, and inspect or edit Behavior graphs. `BML/Behavior.h` exposes the same module as a C seam; C++ authors normally do not need its wire DTOs. This interface is still under pre-release development, currently supports Win32 Native C++ Mods only, and has not frozen source compatibility.

## 1. Object model

```text
Prototype -> Block -> Call / Task / Instance -> Frames
                         |
                         +--------------------> live CKBehavior

existing CKBehavior graph -> Graph snapshot -> Edit -> Patch / Plan

CKBeObject -> Script -> Graph snapshot -> Edit -> Patch
```

| Object | Meaning |
| --- | --- |
| `Prototype` | One registered BB provider, identified by GUID and provider generation |
| `Block` | A copyable BB configuration; it does not own a `CKBehavior` yet |
| `Call` / `Task` / `Instance` | Each owns one real `CKBehavior`; only execution ownership differs |
| `Frames` | Copied control-flow results and optional Pout values from native Execute |
| `Script` | One owner-scoped, top-level graph-backed `CKBehavior` |
| `Graph` | An immutable snapshot of a Behavior graph |
| `Edit` | A symbolic graph transformation that has not been installed |
| `Patch` | An Edit installed on one exact graph snapshot |
| `Plan` | An Edit reconciled against selected scripts across worlds |

## 2. Open a Session

Open a `Session` during Mod initialization:

```cpp
#include <BML/Behavior.hpp>
using namespace BML::Behavior;

auto opened = Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior unavailable: %s",
                       opened.GetStatus().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

The Loader verifies the calling DLL and binds the Session to the current Mod generation. A Session survives world reset; runs, Scripts, Watches, and Patches tied to that world do not. On Mod unload, the Loader stops new admission and retires callbacks and native Behaviors before unloading the DLL.

Objects created through a Session retain their own Session lease. Moving or closing the original `Session` value does not immediately invalidate a live Block, run, Script, Watch, Patch, or Plan.

### Find a Prototype

Use a known `CKGUID` directly, or query the catalog when the GUID is unknown or the current provider identity must be pinned:

```cpp
PrototypeQuery query;
query.Name = "Physicalize";

auto found = m_Behavior.Prototypes(query);
if (!found || found->size() != 1)
    return;

Prototype prototype = found->front().Ref;
auto declared = m_Behavior.Layout(prototype);
```

An empty `PrototypeQuery` returns every discoverable registration. `Session::Layout(prototype)` reads the declared Layout without creating a native Instance or sending lifecycle callbacks.

## 3. Configure any Building Block

`Session::Use` creates a `Block` from a Prototype:

```cpp
auto block = m_Behavior.Use(prototype)
    .TargetOwner()
    .Settings({{"Mode", 2}, {"Detail", 4}})
    .Settings({{"Created Later", 9}})
    .Pins({{"Strength", 12.0f}})
    .Locals({{"Accumulator", 0.0f}});

auto task = block.Start("Run", Signals(64).Pouts());
```

Each `Settings({...})` call is one Setting stage. Runtime sends `SETTINGSEDITED` after a stage; the BB may rebuild its interface in that callback, so the next stage is resolved against the new live Layout. Pins and Locals are applied after all Setting stages. Frame retention belongs to each Call, Task, or Instance rather than to the reusable Block.

Targets are explicit: `TargetOwner()`, `Target(type, object)`, or `NullTarget(type)`. Slot selectors are explicit too: `At(index)`, `Named(name, occurrence)`, or `Unique(name)`. A string overload means `Unique(name)` and never silently selects the first duplicate.

`Block::Validate()` is an optional declared-Layout check and does not create a `CKBehavior`. It can check only the Target, selectors, and types present in the Prototype's initial declaration. Slots created by Setting callbacks are checked during real run admission. Admission performs the complete native checks even when `Validate()` was not called.

The first validation or admission that can identify a provider pins its generation. If another provider later registers the same GUID, the old Block becomes stale instead of changing implementation. When provider retirement cannot be tracked, immediate runs may use generation zero, but such a Block cannot enter a durable cross-world Edit.

## 4. Choose who owns Execute

The three run types own the same native kind of object:

| Operation | First Execute | Later Execute |
| --- | --- | --- |
| `Call(input)` | Immediately, once | Only after moving the Call into `Continue()` |
| `Start(input)` | Immediately, once | Loader advances native continuation per game frame |
| `Spawn()` | None | The Mod drives it with `Pulse(input)` |
| `SpawnIn(graph)` | None | Like Spawn, but parked as an unconnected graph node |

```cpp
auto once = block.Call("Run");
auto task = block.Start("Run", Latest());
auto instance = block.Spawn();
auto parked = block.SpawnIn(graph);

if (instance)
    instance->Pulse("Reset");
```

A Frame policy belongs to that run only; the reusable Block has no observation policy. `Call::Continue()` keeps the same native Instance; it does not create or reactivate another one.

An Instance executes at most once per game frame. Same-frame and reentrant Pulses queue; repeated admissions of the same logical In coalesce, while distinct Ins retain first-admission order. `Ready` means no native continuation and no queued In. It does not mean the BB released Local or manager state.

All run types provide `Info()`, `Take()`, `Layout()`, `Inspect()`, `Set()`, `Bind()`, `Settings()`, and `Close()`. Only `Call` provides `Continue()`; `Task` and `Instance` provide `Pulse()`.

## 5. Read Frames

Every native Execute creates one immutable Frame before Runtime clears that execution's active Outs. It records sequence, game frame, native result, continuation, active Outs, and diagnostics.

Frame retention and Pout capture are independent:

| Policy | Retained Frames |
| --- | --- |
| `Signals(n)` | First, signalled, failure, and non-continuing Frames |
| `EachFrame(n)` | Every Execute |
| `Latest()` | Latest continuing, failure, and non-continuing Frames |
| `Ignore()` | No ordinary Frames; failure and non-continuing Frames remain visible |

Runtime reads Pouts only when the policy includes `.Pouts()`:

```cpp
auto task = block.Start("Run", Signals(64).Pouts());

Frames frames;
frames.Reserve(16, 4096);
if (auto taken = task->Take(frames)) {
    for (Frame frame : frames) {
        if (frame.HasOut("Done")) {
            auto speed = frame.Pout<float>("Speed");
            if (speed)
                Use(speed.Value());
        }
    }
}
```

`Take()` allocates as needed. `Take(Frames&)` reuses existing header and payload buffers; with sufficient capacity it makes one C call and allocates no record or string objects. `Frame`, `Out`, and `Pout` are read-only views into their owning `Frames` and become invalid after that `Frames` is modified.

An object Pout receives an `ObjectRef` while the object is live. Later Frame reads never touch the original CK parameter or object. If any Pout cannot be read or encoded, Runtime discards that incomplete Pout batch but preserves the same Frame's active Outs and diagnostic.

A Frame without continuation does not mean its run handle has closed: a `Ready` Instance can still accept another Pulse. Admission stops only on failure, explicit close, or queue overflow.

A bounded store does not discard an old Frame to admit a new one. It stops the run and reports `FrameQueueFull` in a separate failure slot. Taking Frames releases ordinary queue capacity.

## 6. Use the live Layout

A run's `Layout()` describes its configured native interface. A `Slot` carries the Instance identity and layout generation and can be passed directly to `Set()` or `Bind()`:

```cpp
auto layout = instance->Layout();
if (layout) {
    if (auto strength = layout->Find(SlotKind::Pin, "Strength"))
        instance->Set(*strength, 20.0f);
}
```

A lifecycle callback boundary invalidates old Slots because the provider may rebuild a same-shaped interface. Ordinary Execute advances the layout generation only when Target, In/Out, Pin/Pout, Setting, or Local identity actually changes. A stale Slot or Port returns `LayoutChanged` instead of retargeting an old ordinal.

`Settings({...})` applies another live Setting stage. Runtime then reads the new Layout and restores Target, Pin, Local, and source relations that remain unique and type-compatible. Once native mutation begins, any write, callback, Layout, or relation failure makes the run `Failed`: `Info()` preserves the first `Status`, later mutation and execution are rejected with that same diagnostic, and the native Instance remains owned until `Close()`.

## 7. Inspect a graph

`Session::Inspect` and a run's `Inspect` return immutable `Graph` snapshots:

```cpp
auto snapshot = m_Behavior.Inspect(script);
if (!snapshot)
    return;

auto counter = snapshot->Find(Named("Counter_Active", 0));
if (counter) {
    auto value = snapshot->Read(counter->Pout("Count"));
    // Use value while snapshot is alive.
}
```

`Find` and `FindAll` accept the same selectors as Block slots: `At(index)`,
`Named(name, occurrence)`, and `Unique(name)`. An optional Prototype GUID narrows
the match. `Node::Index()` is the direct-child position in that graph and
`Node::IsGraph()` distinguishes a graph-backed Behavior from a function-backed
BB. `Graph::Inspect(node)` enters a graph-backed child and returns a new snapshot
whose root is that Node. Native pointers enter the model only at the root through
`Session::Inspect(CKBehavior *)`; child Nodes and Links are obtained from the
snapshot, so an edit cannot silently mix identities from different graphs.

`Incoming` and `Outgoing` are zero-allocation Link views. `Entering`, `Leaving`,
`Previous`, and `Next` require a unique topology result and report ambiguity
instead of choosing an arbitrary branch. Node, Port, Link, and
ParameterOperation are lightweight views into one shared snapshot allocation.
`Graph::Operations()` reports each native `CKParameterOperation`, including its
operation GUID, exact result/input type tuple, graph owner, and object identity.
A Port retains its Node's layout generation, so it cannot be used against a
later incompatible Layout.

`Logical()` rereads the author-visible graph; `Live()` rereads the physical CK graph. Logical view keeps explicit Add and Flow edits, hides the exact infrastructure installed by Tap, Before, After, and Splice, and restores a splice anchor's logical endpoints. If foreign code changes a claimed after-image, Runtime reports `GraphChanged` instead of guessing by name or shape.

`Graph::Read` follows stored, direct, and shared sources without evaluating a Parameter Operation. Operation values are reported as indeterminate.

Watches sample a graph, Layout, or value once per game frame. Observation or callback failure retains the first `Status`, moves the Watch to `Failed`, and stops later callbacks.

## 8. Create a Script graph

`Session::CreateScript` creates a top-level Script owned by a live `CKBeObject` in the current Scene and installs its initial `Edit` before returning. The Script starts inactive, so Virtools cannot schedule a partially defined graph:

```cpp
Edit body;
auto graph = body.Root();
graph.AppendIn("Start");
graph.AppendOut("Done");
auto left = graph.AppendLocal("Left", CKPGUID_FLOAT);
auto sum = graph.AddOperation(
    addition, CKPGUID_FLOAT, CKPGUID_FLOAT, CKPGUID_FLOAT);
graph.Bind(left, 2.0f)
    .Bind(sum.Input(0), left)
    .Bind(sum.Input(1), 3.0f);

auto created = m_Behavior.CreateScript(owner, "My Script", body);
if (!created)
    return;
Script script = std::move(created).Value();
script.Activate(true);
```

`CreateScript` performs the one native `AddScript` operation that establishes both owner and Scene membership. It then compiles and applies the complete Edit. A validation, lifecycle, callback, or graph failure returns no Script handle, removes the unpublished root, and publishes nothing to Plans. The installed body belongs to the Script rather than to a second public Patch handle.

`Edit::Graph()` denotes the symbolic root of the graph being authored; the snapshot root must not be imported with `Use()` for this purpose. The root may own In, Out, Pin, Pout, and Local parameters. `AddOperation` adds a real graph-owned `CKParameterOperation`; Virtools selects its function from the operation GUID and the exact result/input type tuple, and evaluates its result lazily when a consumer reads it. Every declared operation input must be bound. `Script::Apply()` remains available for later incremental edits. An accepted activation request is applied at the next Behavior safe point, after pending Patches have reconciled. Passing `true` requests Virtools reset semantics even if the Script is already active.

Script closure is also completed at a safe point: `Close()` can first return `CloseState::Closing`, and a later call returns `Closed` after deactivation, owner removal, and native destruction. Close the Script before destroying its owner; Mod unload and world reset perform the same retirement automatically.

## 9. Edit a graph

`Edit` is one symbolic transformation with explicit graph scopes:

```cpp
Edit edit;
auto root = edit.Root();
auto highscoreNode = root.Require("Highscore");
auto highscore = highscoreNode.Graph();

auto done = highscore.AppendOut("Done");
auto activator = highscore.Require(Named("Activate Script", 0));
highscore.Flow(activator.Out(), done);
root.After(highscoreNode.Out("Done"), hook);
```

`Edit::Graph` carries all graph-local operations. `Edit::Node::Graph()` enters a
graph-backed Node. A parent graph may connect only the child's public ports;
internal ports from different scopes cannot be connected. `AddGraph(name,
priority)` creates a real graph-backed child, and the nested scope defines its
public interface and body in the same transaction. The root and every nested
scope are validated together and restored in child-before-parent order.

`Edit::Node`, `Edit::Port`, `Edit::Link`, and `Edit::Path` are authoring symbols,
not graph snapshot views. Symbols belong to the Edit and graph scope that
created them. `Require(snapshotNode)` and `Require(snapshotLink)` copy structural
identity: child position, name, Prototype, graph/function kind, port layout, and
Link endpoints. They can therefore be resolved again in another world, but
fail explicitly if that structure drifts. `Use(snapshotNode)` and
`Use(snapshotLink)` retain exact ObjectRefs and are valid only for an exact
Patch. A Plan rejects those identities and every non-null ObjectRef in its Edit
or Blocks.

`Add(block)` copies the Block configuration and pinned provider generation. Later changes to the source Block do not affect the Edit. Frame policy is not part of graph authoring.

During installation, every added Block completes `CREATE`, `ATTACH`, and its Setting stages before graph parameter relations are installed. Its one final `EDITED` callback sees those relations; only then does the Patch add control Flow. Existing Blocks whose values or parameter relations change likewise receive one `EDITED` after installation and one after successful restoration. A Patch containing only control Flow does not send block-level `EDITED` callbacks.

Common transformations are:

- `Flow` / `FlowCycle` for Behavior Links;
- `Bind` / `Share` / `Push` for parameter relations;
- `Tap` / `Before` / `After` for callbacks;
- `Splice` for routing an existing Link through a Block;
- `Redirect` for temporarily changing a Link destination;
- `AppendIn/Out/Pin/Pout` for dynamic interfaces;
- `AppendLocal` for the graph root or a Block added by the same Edit. A Local
  belongs to its implementation, so an Edit cannot add one to a borrowed Node;
- `AddGraph` for a graph-backed child whose nested scope is authored in place;
- `AddOperation` for a graph-owned, lazily evaluated Parameter Operation;
- `Replace` for exchanging one idle child Node for a configured Block with the
  same public interface;
- `Remove` for taking one existing child Node and all of its incoming and
  outgoing Behavior Links out of the graph for the lifetime of the Patch.

`Replace` preserves the original Link objects, delays, Pin and Target sources,
Pout destinations, name, priority, and owner. Settings and Locals are private
state, so the replacement uses its own Block configuration rather than copying
them. Closing the Patch restores the exact original Node and all of those
relations before the replacement Block is destroyed. Replacement refuses an
active Node or a public-interface mismatch instead of adapting by position.

`Remove` is reversible graph membership, not object destruction. The Patch
keeps the exact Node and incident Link objects, disconnects the Links while
they are parked, and restores their original endpoints and delays when it
closes. This prevents an active sibling's output port from traversing a Link
that no longer belongs to the graph. Applying a removal requires the selected
Node, its control ports, and every incident Link source to be idle;
an already-active non-target sink no longer depends on that Link. CK2 does not
expose delayed-list membership through the retail SDK. At an execution
boundary, a Link outside that list has a remaining delay of zero or its initial
delay; any other positive remaining delay is therefore treated as in-flight and
rejected. This remains true when a graph was deactivated without a reset, since
that operation leaves its delayed list intact. Unrelated graph work may remain
active. A Node edit is exclusive with active Link overlays and prevents later
Patches until it closes, so no overlay can retain a physical chain through a
parked Node.

Bind several current-world graphs to their Edits with `On(graph, edit)`. The
Session returns one Patch for the whole feature:

```cpp
auto patch = session.Apply(
    "overclock",
    On(deactivateGraph, deactivateEdit),
    On(newBallGraph, newBallEdit),
    On(energyGraph, energyEdit));
```

All targets are resolved and statically checked before the first graph changes.
They then commit in argument order; a later failure restores earlier targets in
reverse order. The same graph cannot appear twice at the top level. Deleting any
exact target retires the whole Patch and restores every surviving graph at the
next Behavior safe point. `Graph::Apply` and `Script::Apply` are single-graph
convenience forms of this operation.

A Plan owns several independent Script rules:

```cpp
auto plan = session.Plan(
    "game-events",
    On(Scripts::One("Event_handler"), eventEdit),
    On(Scripts::One("Gameplay_Ingame"), gameplayEdit),
    On(Scripts::One("Gameplay_Energy"), energyEdit));
```

Each rule reconciles only when its Script name changes; there is no per-frame
full graph scan. One rule may match `One` or `Each`, and its root plus nested
scopes install as one atomic Patch. `Partial` means at least one rule is
installed while another is unmatched; `Unsatisfied` means none is installed.
Definitions survive world reset and reconcile against the next world.

Patch and Plan both expose `Enable()`, `Disable()`, `Replace(...)`, `Info()`, and
`Close()`. Disable restores native graphs but keeps the handle and owned
definition. Enable validates again before installation. Replace preserves an
unchanged prefix, restores the changed suffix in reverse order, then installs
the new suffix. If new content fails, the old complete definition is restored;
if that inverse also conflicts, `Info()` reports `Conflicted` and retains the
journal. Several requests before the next safe point collapse to the last
definition and active state.

Close compares the Links, sources, and graph after-images still owned by an
installation. A foreign change produces `RevertConflict`; the handle remains
readable and Close can be retried after the conflict is repaired.

Hook callbacks run on the game thread. Exceptions do not cross the DLL seam. Self-close stops later admission immediately, while graph restoration, native teardown, and callback-state release finish at a safe point without waiting for the current invocation.

## 10. Named retail BBs

`BML/Behavior/Blocks.hpp` collects header-only adapters for known retail BBs, including Object Load, Physicalize, Physics Force, Physics Impulse, Physics Wake Up, Send Message, and 2D Text. Each header contains only that BB's Prototype, Options, and slot knowledge:

```cpp
#include <BML/Behavior/Blocks/Text2D.hpp>

Blocks::Text2D::Options options;
options.Text = "score";
options.FontIndex = 2;

auto made = Blocks::Text2D::Make(m_Behavior, options);
if (made) {
    auto text = std::move(made).Value().SpawnIn(graph);
}
```

These adapters return ordinary Blocks and do not bypass lifecycle, execution, or teardown. New Native Mod code should not use the legacy ExecuteBB interface.

## 11. Lifetime, errors, and performance

| Event | Session | Run | Script | Watch / Patch | Plan |
| --- | --- | --- | --- | --- | --- |
| Explicit Close | Last lease closes native Session | Admission stops; teardown finishes at a safe point | Closes its initial graph, deactivates, leaves its owner, then destroys at a safe point | Readable during closure or conflict | Readable while rules retire |
| World reset | Survives | Closes | Closes with the old world | Closes with the old graph | Survives and reconciles in the next world |
| Mod unload/reload | Owner generation retires | Closes before DLL unload | Leaves its owner and closes before DLL unload | Callback and graph state retire first | Retires before callback code unloads |

Except for Close, Behavior operations require the game thread. Every `Result<T>` carries both a stable error category and `Status`; branch on the error and phase, not on message text.

Reuse Blocks, Frames, and graph snapshots on hot paths. Blocks share compiled C descriptors, `Take(Frames&)` avoids allocation when capacity is sufficient, and Node, Port, Link, ParameterOperation, LinkRange, and Frame values are views rather than copied records. Plans reconcile only Script names reported as changed; disabled definitions and unchanged Replace prefixes do not rebuild native graphs.

The current public interface exposes native Parameter Operations but does not add a second expression language over them. It does not include an AngelScript Behavior projection or third-party parameter-format registration. Unsupported Virtools parameter types fail explicitly; they are never guessed to be arbitrary bytes.
