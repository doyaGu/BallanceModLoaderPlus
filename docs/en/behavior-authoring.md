# Behavior authoring

`BML/Behavior.hpp` is the Native C++ interface for using and editing Virtools
Behaviors. `BML/Behavior.h` is the underlying C ABI. The C++ interface follows
the Virtools object model instead of exposing the wire representation:

```text
Prototype -> configured Block -> Call / Task / Instance -> Frames
```

- A `Prototype` identifies one registered Building Block implementation.
- A `Block` is a copyable configuration. It has no `CKBehavior` yet.
- A `Call`, `Task`, or `Instance` owns one live `CKBehavior`.
- `Frames` owns the results copied after native executions.
- A `Node` is a Behavior that already exists in a graph snapshot.

## Open a Session

Open a Session during Mod setup. The Loader verifies the calling DLL and binds
the Session to that Mod generation. An optional owner id can confirm the
identity; it cannot impersonate another Mod.

```cpp
using namespace BML::Behavior;

auto opened = Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior unavailable: %s",
                       opened.GetStatus().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

Objects created through a Session retain the native Session they need. Moving
or closing the original `Session` value does not invalidate a live Block, run,
Watch, Patch, or Plan. The final owner closes the native Session.

Sessions survive world reset. Their runs and world-bound graph objects do not.
All objects become stale when the owning Mod generation retires.

## Configure a Block

`Session::Use` returns a `Block` directly. Configuration methods mutate that
value and return it for chaining:

```cpp
auto block = m_Behavior.Use(prototype)
    .TargetOwner()
    .Settings({{"Mode", 2}, {"Detail", 4}})
    .Settings({{"Created Later", 9}})
    .Pins({{"Strength", 12.0f}})
    .Locals({{"Accumulator", 0.0f}})
    .Frames(Signals(64));

auto checked = block.Validate();       // optional
auto call = block.Call("Run");          // validation is also automatic here
```

Each `Settings({...})` call is one Setting stage. Virtools sends
`SETTINGSEDITED` after a stage, and a Building Block may rebuild its layout in
that callback. The next stage is therefore resolved against the new layout.
Pins and Locals are applied after all Setting stages.

`Block` uses copy-on-write. Copies share their configuration and validated wire
form until one copy is changed. `Target`, `Settings`, `Pins`, `Locals`, and
`Frames` invalidate only the changed copy's validation cache. The first
successful validation fixes the Prototype provider generation; replacing the
provider makes the old Block stale instead of silently changing its native
implementation.

Selectors are explicit: use `At(index)`, `Named(name, occurrence)`, or
`Unique(name)`. A unique selector fails if the name occurs more than once.

## Choose who drives Execute

`Call`, `Task`, and `Instance` are separate move-only types. Each owns exactly
one native Behavior; they differ only in how later native executions happen.

- `Call` executes once. If it reports native continuation, move it into
  `Continue()` to let the Loader manage that same Instance.
- `Start` executes once and returns a `Task`; the Loader advances its native
  continuation on later game frames.
- `Spawn` creates an idle `Instance`; the Mod drives it with `Pulse`.
- `SpawnIn(graph)` creates the same driven Instance as an unconnected child of
  a live graph. Closing it removes the node again.

```cpp
auto once = block.Call("Run");
auto task = block.Start("Run", Latest());
auto instance = block.Spawn();
auto parked = block.SpawnIn(graph);

if (instance)
    instance->Pulse("Reset");
```

An optional policy passed to `Call`, `Start`, `Spawn`, or `SpawnIn` overrides
the Block's Frame policy for that run only.

One Instance executes at most once in one game frame. A same-frame or reentrant
Pulse is queued. Repeated admissions of the same logical In coalesce; distinct
Ins keep first-admission order. `Ready` means there is no native continuation
and no queued In. It does not mean that the Building Block has released state
held in Locals or in a manager.

All three run types provide `Info`, `Take`, `Layout`, `Inspect`, `Set`, `Bind`,
`Settings`, and `Close`. Only `Call` provides `Continue`; only `Task` and
`Instance` provide `Pulse`. Live `Slot` values carry a layout generation and
fail after a Setting or callback changes that layout.

## Take Frames

Every native Execute forms one immutable Frame before its active Outs are
cleared. It records sequence, game frame, native return code, continuation,
active Outs, copied Pouts, and diagnostics. Object Pouts contain an
`ObjectRef` issued while the object was live; reading them later never touches
the original parameter or CK object.

```cpp
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

`Take()` is the convenient allocating form. `Take(Frames&)` reuses the
existing header and payload capacity; when it is sufficient, the operation
performs one C call and allocates no record or string objects. A `Frame`, `Out`,
or `Pout` is a lightweight view into its owning `Frames`. Any modification of
that `Frames` invalidates all such views. The complete wire batch is validated
before any public view can be produced.

Frame policies are:

- `Signals(n)`: first Execute, signalled Executes, failures, and terminal Frame.
- `EachFrame(n)`: every Execute.
- `Latest()`: the latest continuing, failure, and terminal Frames when distinct.
- `Ignore()`: no ordinary Frames, but failures and terminal state remain visible.

A bounded store does not discard an older Frame to admit a newer one. It stops
the run and records `FrameQueueFull` in the terminal slot. Taking Frames frees
regular capacity.

## Read a graph

`Session::Inspect` and a run's `Inspect` return immutable graph snapshots.
`Graph::Root()` returns a `Node`; every Node directly names its `In`, `Out`,
`Pin`, `Pout`, `Setting`, `Local`, and `Target` ports.

```cpp
auto snapshot = m_Behavior.Inspect(script);
auto counter = snapshot->Find("Counter_Active");
if (counter) {
    auto value = snapshot->Read(counter->Pout("Count"));
}
```

`Logical()` rereads the author-visible graph; `Live()` rereads the physical CK
graph. Logical inspection keeps explicit Blocks and Links but hides the exact
Hook Blocks and continuation Links owned by Tap/Before/After and restores a
spliced anchor's logical endpoints. A foreign change to claimed infrastructure
is reported as `GraphChanged` rather than guessed around.

Node names are not identities. `FindAll` returns all matches; `Find` requires
exactly one. Parameter reads follow stored, direct, and shared sources without
evaluating a Parameter Operation.

Watches sample a graph, layout, or value once per game frame:

```cpp
auto watch = snapshot->Watch(GraphChanged{}, [](const Change &change) {
    OnGraphChanged(change);
});
```

Use `Info()` to read Watch state. Observation or callback failure leaves it in
`Failed` with its first `Status` and stops later callbacks.

## Describe one symbolic Edit

`Edit` is the sole symbolic graph transformation. The same Edit can be applied
to one exact snapshot or retained as a cross-world Plan:

```cpp
Edit edit;
auto source = edit.Require("Counter_Active");
auto added = edit.Add(block);
edit.Flow(source.Out(), added.In());

auto patch = graph.Apply("extra-life", edit);
auto plan = m_Behavior.Plan(
    "extra-life", Scripts::One("Gameplay_Events"), edit);
```

`Edit::Node`, `Edit::Port`, `Edit::Link`, and `Edit::Path` are symbolic values;
they are intentionally different from snapshot `Node`, `Port`, and `Link`.
`Use(snapshotNode)` and `Use(snapshotLink)` import exact live identities for a
one-graph Patch. A Plan rejects those world-bound identities because it must
resolve against new scripts in later worlds.

`Add(block)` copies the Block's native configuration at that call. Later
changes to the original Block do not alter the Edit, and its Frame policy is
not part of graph authoring. `Graph::Apply` verifies the snapshot fingerprint
before mutation and returns a one-use `Patch`. `Session::Plan` accepts
`Scripts::Each(name)` or `Scripts::One(name)` and returns a `Plan` reconciled as
matching scripts appear, reset, or disappear.

Patch and Plan use `Info()` and `Close()`. Closing performs a checked inverse;
a foreign change yields `RevertConflict` and keeps the handle readable so the
conflict can be repaired and closure retried.

Hooks, splices, redirects, dynamic ports, data relations, and ordering are
methods on `Edit`. Hook callbacks run on the game thread. Exceptions are caught
before crossing the DLL seam, callback admission closes immediately, and graph
restoration plus native teardown occur at a Behavior safe point without waiting
for the callback that requested closure.

## Named retail Building Blocks

`BML/Behavior/Blocks.hpp` collects header-only adapters for the retail Building
Blocks used by BML+: Object Load, Physicalize, Physics Force, Physics Impulse,
Physics Wake Up, Send Message, and 2D Text. Each individual header defines an
`Options` value and `Make(Session, Options) -> Result<Block>`.

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

The adapters contain only the Prototype and parameter knowledge of that
specific BB. Creation, lifecycle, execution, and teardown still go through the
same Behavior Runtime as an arbitrary `Session::Use` Block. New Native Mod code
should not use the legacy ExecuteBB API.

## Lifetime summary

| Event | Session | Run | Watch/Patch | Plan |
| --- | --- | --- | --- | --- |
| Explicit Close | Last owner closes native Session | Admission closes; teardown may finish at a safe point | Remains readable while closure or conflict is pending | Remains readable while installations retire |
| World reset | Survives | Closed | Closed with the old graph | Survives and reconciles in the next world |
| Mod unload/reload | Owner generation retires | Closed before DLL unload | Callback and graph state retire before DLL unload | Retired before callback code unloads |

Except for Close requests, Behavior operations require the game thread. Never
retain a callback's borrowed pointers or C descriptors after that call returns.
