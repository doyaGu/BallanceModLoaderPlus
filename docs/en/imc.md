# Inter-mod communication (IMC)

IMC is BML+'s typed, in-process transport for communication between mods and
loader-provided services. It provides two operations:

- RPC for request/response calls;
- Topic for publish/subscribe notifications.

Use IMC when two independently built native modules need a stable API without
sharing C++ objects, STL containers, allocators, or Virtools pointers.

For a complete interface, provider, client, and Topic example, see
[Create a typed IMC API](imc-author-guide.md).

## Programming model

IMC keeps transport concerns separate from domain terminology:

| API intent | IMC representation |
| --- | --- |
| Read current state | RPC with no request payload |
| Look up an object | RPC taking an opaque `BML_ObjectRef` |
| Run a query | Request/response RPC |
| Execute a command | Request/response RPC with an explicit result |
| Return a collection | RPC whose response contains a counted array |
| Notify observers | Topic |

Resources, components, commands, and collections are API design patterns, not
additional transport types. A small transport surface keeps threading,
lifetime, and failure handling consistent across APIs.

## Recommended workflow

1. Define records, RPCs, and Topics in a versioned `.imc` interface.
2. Add the interface to a CMake target with `bml_target_imc_api()`.
3. Generate or refresh its adjacent `.imc.lock` with the
   `bml_update_imc_locks` CMake target, then review and commit the diff.
4. Implement the generated `Provider` callbacks.
5. Call the API through the generated `Client`.
6. Keep providers, clients, subscriptions, futures, and callback data alive
   until their corresponding close or release operation succeeds.

Generated bindings are the normal API surface. They encode and validate
payloads, cache route IDs, manage opaque handles, and return BML status codes.
Most mods should not construct `BML_ImcMessage` values directly.

## Public API layers

IMC has three public layers:

| Layer | Purpose |
| --- | --- |
| Generated `*_imc.hpp` | Typed payloads, codecs, clients, providers, futures, and subscriptions |
| `BML/ImcCpp.hpp` | Generic C++ RAII wrappers for custom integrations |
| `BML/Imc.h` | Fixed-layout C ABI used across DLL boundaries |

The C ABI exports `BML_Imc_*` functions and uses only C scalars, fixed-layout
structures, byte spans, callbacks, and opaque handles. C++ classes, exceptions,
RTTI objects, and allocator-owned values never cross the module boundary.

`BML/ImcWire.hpp` defines the little-endian field encoding used by generated
bindings. A callback may borrow the bytes in a `BML_ImcMessage` only for the
duration of that callback. Generated decoders copy strings, arrays, and blobs
into their typed result values. The message already carries its payload type,
so the payload does not repeat a schema ID or descriptor hash. Each field uses
a Protobuf-style varint tag that combines its permanent ID and physical wire
kind. Fixed-width scalars carry no redundant length; only strings, composite
values, and packed arrays are length-delimited.

## Interface identity and compatibility

An API ID uses lowercase alphanumeric dot-separated segments, such as
`example.echo`. The generated header for that ID is
`example_echo_imc.hpp`, and its C++ namespace is
`BML::Imc::Generated::Example::Echo`.

Authors do not write field numbers in `.imc`. The generator assigns permanent
field IDs in the adjacent `.imc.lock`, which is committed beside the
source interface. Once released:

- do not change an existing field's type or required/optional status;
- add new fields as optional fields;
- preserve existing RPC and Topic names and payload records;
- preserve existing enum names, numeric values, and underlying types.

For a compatible minor release, increase the interface minor version and run the
generator once with `--update-lock`. It checks evolution against the lock,
preserves assigned IDs, and reserves IDs of removed optional fields. Normal
builds only verify the lock. Unknown fields are skipped, and unknown enum
numbers are preserved so API-specific code can choose a fallback.

Increase the interface's major version when a change cannot follow those rules.
Do not rely on runtime route IDs for compatibility: route IDs are process-local
cache keys. The API ID, major version, endpoint payload kind, and frozen field
layout define the interoperable interface.

## RPC execution and waiting

Every RPC provider selects an execution mode:

- `BML_IMC_EXECUTION_CALLER_THREAD` runs immediately on the caller's thread.
  Use it only for short, thread-safe work.
- `BML_IMC_EXECUTION_GAME_THREAD` queues the request for the BML game-thread
  pump. Use it for Virtools objects, BML UI, and other game-thread-only state.

Synchronous generated calls wait for their typed result up to the supplied
timeout. Generated `Begin*` methods return a move-only typed future for polling,
cancellation, or bounded waiting.

Do not wait with a nonzero timeout on the game thread for game-thread work. A
zero-time wait is a safe poll and returns `BML_ERROR_BUSY` while the operation
is still pending.

An RPC name has at most one live provider. Calls must handle
`BML_ERROR_IMC_ENDPOINT_NOT_FOUND` because a provider can unload after an
availability check.

## Topic delivery and backpressure

A Topic may have any number of subscribers. Caller-thread subscriptions run
inline; game-thread subscriptions use a bounded per-subscription queue.

Choose an overflow policy when subscribing:

- `BML_IMC_BACKPRESSURE_DROP_OLDEST` keeps recent messages;
- `BML_IMC_BACKPRESSURE_DROP_NEWEST` preserves queued messages;
- `BML_IMC_BACKPRESSURE_FAIL` reports `BML_ERROR_WOULD_BLOCK` to the publisher.

Select a capacity that matches how quickly the consumer can drain its queue.
Use the subscription's dropped-message count to detect sustained overload. If
building an event payload is expensive, query the subscriber count before
constructing it.

## Ownership and shutdown

Clients and providers are associated with a mod owner. BML revokes an owner's
IMC state during unload, but explicit shutdown is still required:

1. stop producing new calls or messages;
2. cancel or release outstanding futures;
3. close subscriptions before destroying callback data;
4. unregister provider callbacks;
5. close providers and clients.

Closing a client, unregistering a provider, or closing a subscription is a
callback-quiescence boundary. Calling the same teardown operation recursively
from its active callback returns `BML_ERROR_BUSY` instead of deadlocking.

Opaque handles become invalid immediately after successful release. Treat
`BML_ERROR_INVALID_HANDLE` as a stale-owner or double-release programming
error, not as a recoverable route failure.

## Built-in APIs

BML+ provides the following typed service facades:

| Namespace | Service |
| --- | --- |
| `BML::Runtime` | Runtime state, clock, and score |
| `BML::Scene` | Object information, transforms, and lookup |
| `BML::Gameplay` | Level, energy, checkpoint, and reset-point data |
| `BML::UI` | HUD state and UI commands |
| `BML::Speedrun` | Shared timer state and controls |
| `BML::Events` | Typed event Topic |

Use those facades directly when they already cover the required operation.
Create a new interface only for a capability owned by your mod.

## Performance characteristics

Generated clients resolve names once and reuse numeric route IDs. Small
payloads use inline storage, direct caller-thread RPC avoids queueing, and
game-thread work uses bounded queues and per-frame pump budgets. Topic publish
has a zero-subscriber fast path.

These optimizations do not relax the public rules: callback code still needs a
clear execution mode, queues still need a backpressure policy, and payloads
still need stable records.

## Reference

- [Create a typed IMC API](imc-author-guide.md)
- C ABI: `BML/Imc.h`
- C++ wrappers: `BML/ImcCpp.hpp`
- wire codec: `BML/ImcWire.hpp`
