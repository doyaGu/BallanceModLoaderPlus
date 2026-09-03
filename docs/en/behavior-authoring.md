# Behavior authoring

`BML/Behavior.hpp` is the primary Native C++ authoring interface for Virtools
Building Blocks. `BML/Behavior.h` is its stable C seam for other languages and
toolchains. Both describe the same Sessions, Prototypes, Layouts, Blocks, Runs,
Frames, Graphs, Watches, Patches, and Plans.

## Start with a Session

Open one Session during Mod setup. The Loader verifies the calling DLL and
binds the Session to that Mod generation; an `ownerId` can confirm that identity
but cannot impersonate another Mod.

```cpp
auto opened = BML::Behavior::Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior is unavailable: %s",
                       opened.Detail().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

Values created from a Session share its native ownership. Releasing the
original `Session` value does not invalidate a live Block, Run, Watch, Patch, or
Plan. The last value closes the native Session.

Prototype discovery and declared Layout reads do not create a live
`CKBehavior`. A discovered Prototype carries its provider generation. Keep that
generation in the `Prototype` passed to `Use`; a provider reload then fails
explicitly instead of silently binding an old definition to a new DLL.

## Compile reusable Blocks

A Builder describes the Target, Setting stages, Pins, Locals, and Frame policy.
Settings are staged because a `SETTINGSEDITED` callback may rebuild the native
Layout before later values are resolved.

```cpp
using namespace BML::Behavior;

auto compiled = m_Behavior.Use(Prototype(myPrototype, providerGeneration))
    .TargetOwner()
    .Setting("Mode", 2)
    .NextStage()
    .Setting("Detail", 4)
    .Pin("Strength", 12.0f)
    .Frames(signals(64))
    .Compile();
```

The Builder is convenient for one-off calls. For repeated or per-frame work,
call `Compile()` during setup and retain the immutable Block. A compiled Block
owns its strings and values, reuses one C descriptor, and does not rediscover
the declared Layout for every Run.

Selectors are explicit. Use an index, a name plus occurrence, or a unique name.
A unique-name selector fails on duplicates; it never chooses the first match.
A live Layout Slot also carries its Layout generation and becomes stale when a
Setting or native callback changes that Layout.

## Choose the Run that matches the Behavior

- `Call` executes once. If native continuation remains, move the Call into
  `Continue()` explicitly; this preserves the same native Instance.
- `Start` executes once and lets the Loader advance native continuation on later
  game frames.
- `Spawn` creates an idle Instance. `Pulse` supplies logical Ins as the Mod needs
  them.

One Instance executes at most once per game frame. A second same-frame or
reentrant Pulse is queued; repeated Pulses of the same logical In coalesce,
while different Ins retain admission order. `Ready` means that no native or
queued continuation remains. It does not mean that the Building Block has
released state stored in a Local or registered with a manager; the Run owns the
native Instance until it is closed or its world ends.

Native execution failure is recorded in a Frame. A failure that occurs before a
safe native Execute—such as an invalid selector, incompatible type, stale
Layout, or unavailable Prototype—returns a failed `Result` and creates no Run.

## Read Frames

Each native Execute produces one immutable Frame before its active Outs are
cleared. A Frame records its sequence, game frame, native result, continuation,
active Outs, copied Pout values, and diagnostics. Object-valued Pouts contain a
`BML_ObjectRef` issued while the object was still live; reading the Frame later
does not access that object again.

Frame policies have exact retention semantics:

- `signals(n)` retains the first Execute, Executes with active Outs, failures,
  and the non-continuing Frame, up to `n` regular Frames.
- `eachFrame(n)` retains every Execute, up to `n` regular Frames.
- `latest()` retains the latest continuing Frame plus the latest failure and
  non-continuing Frame when they are distinct.
- `ignore()` omits ordinary Frames but still retains a failure and the
  non-continuing Frame so execution cannot fail silently.

A bounded store never discards an older Frame to make room. It records a
terminal `FrameQueueFull` diagnostic in the independent terminal slot and stops
the Run. Take Frames often enough, or choose `latest()`/`ignore()` when every
intermediate value is not needed.

The C `TakeFrames` call uses a non-consuming two-phase protocol. The first call
measures the complete header and payload buffers. Insufficient capacity writes
only the required counts and consumes nothing; a successful second call copies
whole Frames in sequence order and consumes that exact batch atomically. The
C++ `Take()` method owns and decodes the returned data.

## Inspect and watch graphs

`Inspect` returns an owned Logical or Live graph view without exposing CK
pointers. Live shows the physical CK graph. Logical shows what an author edited:
explicit Blocks and Links remain visible, while the Loader restores a spliced
anchor's original endpoints and delay and hides its exact continuation Links and
Tap/After Hook Blocks. If one of those Loader-owned physical relations changes
behind the Patch, Logical inspection reports `GraphChanged` instead of guessing.
Node names are not identities and may repeat: enumerate all matches or ask for a
unique match and handle ambiguity explicitly. Parameter reads follow
stored/direct/shared sources without evaluating a Parameter Operation.

A Watch samples graph, Layout, or value state once per game frame. Portable
CK2.1 has no exact parameter-data notification seam, so the public interface
offers sampled value changes only. If reading the source or invoking the author
callback fails, the Watch becomes `Failed`, stops polling, and retains its first
diagnostic until it is read and closed. It never disappears silently.

## Patch one graph or maintain a Plan

A Patch changes one exact live Graph and owns the journal needed to restore it.
A Plan keeps symbolic intent for scripts selected by exact name and reconciles
fresh Patch installations as worlds and script instances change. Their builders
are deliberately distinct: Patch authoring ends in `Apply`; Plan authoring adds
its target selection and ends in `Submit`.

Graph edits use explicit node, port, link, and path queries. Duplicate names,
ambiguous paths, cross-graph references, unconfirmed same-frame cycles, source
conflicts, and ordering cycles are errors before native mutation. Closing
compares the graph with the Patch after-image. A foreign edit produces
`RevertConflict`; restore the expected relation and retry instead of forcing a
destructive inverse.

Hook and Watch callbacks run on the game thread. The C++ thunks catch every
exception before it can cross the C/DLL seam. A Hook callback that throws is
reported as `HookResult::Fault`: the Loader keeps the first fault as the Hook
diagnostic, stops invoking that callback, and the Hook Block passes the
activation through, so a Mod bug cannot stop the host script's chain. Returning
`HookResult::Error` deliberately leaves every Out inactive and keeps the
callback installed. A
callback or another thread may request Close: new callback admission stops
immediately, while graph restoration, native teardown, and callback Release run
later at a game-thread Behavior safe point. Close never waits for its own active
callback. The same deferral applies inside the Loader's own inverse: a native
teardown or EDITED callback that closes the Patch being torn down is answered
`Busy`, and one that closes or applies another Patch has that request queued for
the next safe point rather than nested under the running restoration.

## Lifetimes

| Event | Session | Run | Watch | Graph Patch | Plan |
| --- | --- | --- | --- | --- | --- |
| Explicit Close | Closes after its last facade owner | Becomes stale; native teardown is deferred when needed | Becomes stale; callback state retires at a safe point | Remains readable while restoration is pending or conflicted | Remains readable while retirement is pending or conflicted |
| World reset | Survives | Closed | Closed | Closed with the old graph | Survives and reconciles in the next world |
| Mod unload/reload | Owner generation retires; all handles become stale | Closed before the DLL unloads | Closed before callback code unloads | Restoration is completed or retained by the Loader until safe | Retired before callback code unloads |

Except for Close requests, operations require the game thread. Never retain
borrowed pointers from a callback or a C descriptor after that call returns.
