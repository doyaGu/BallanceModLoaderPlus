# Create a typed IMC API

This guide shows how a native mod can expose a typed, in-process API to another
mod. Read [Inter-mod communication](imc.md) first for the transport, threading,
compatibility, and lifetime model.

The examples require an installed BML+ SDK, CMake 3.14 or newer, a C++20
compiler, and Python 3.10 or newer. New interfaces define RPCs and Topics.

## 1. Write the interface

Name the file after the API ID so the default generated filename is obvious:
`api/example.echo.imc`.

```text
api example.echo 1.0

enum echo_mode {
    normal = 0
    uppercase = 1
}

rpc echo(string text, enum<echo_mode> mode) -> (string text)
topic changed(int sequence, string text)
```

The file is a small IDL, not JSON. Whitespace separates declarations; commas
and semicolons are optional. Both `# comment` and `// comment` are accepted.
Only enum values need explicit numbers. Field IDs are assigned by
the generator and frozen in the adjacent `example.echo.imc.lock`. Do not
write transport bookkeeping in the source interface.

Inline request, response, and Topic payloads are preferred when used by one
endpoint. The generator names the corresponding value types `EchoRequestValue`,
`EchoReplyValue`, and `ChangedEventValue`. Use a named record when several
endpoints share a payload:

```text
record object_query {
    object target
    optional string label
}

rpc inspect(object_query) -> (string description)
```

A zero-input RPC uses empty parentheses: `rpc state() -> runtime_state`.
Add `optional` before a field type when evolving an existing record:
`optional string label`.

Omit `-> response` when the RPC only reports success or failure:
`rpc clear_cache()`. The generated client returns the RPC status directly and
does not allocate, register, encode, or decode an empty response payload.

The native authoring surface maps directly to the runtime:

| `.imc` entry | Generated IMC surface |
| --- | --- |
| `rpc name() -> response` | zero-input `Call*` RPC |
| `rpc name(request) -> response` | request/response `Call*` RPC |
| `rpc name(request)` | response-less `Call*` RPC |
| `topic name(message)` | `Publish*`/`Subscribe*` Topic |

Model object lookup as an ordinary request record containing an `object` field.
Put query/command intent in the RPC name and payload types, not in a transport
kind. One record can consume at most 64 permanent field IDs; the generator
reports this limit at generation time rather than emitting an incomplete codec.

Enum, record, field, enum-value, RPC, and Topic names may use ASCII letters,
digits, `_`, `-`, and `.` and must contain at least one letter or digit. API IDs
use a stricter canonical form: non-empty dot-separated segments containing only
lowercase ASCII letters and digits. This makes both the generated filename and
C++ namespace unambiguous across independently generated APIs.

Names beginning with a digit are supported: generated C++ identifiers receive
a leading `_`, while the original interface name and route are unchanged.
Codegen rejects punctuation-only names, empty API segments,
non-canonical API IDs, and two names that collapse to the same generated
identifier before the C++ compiler is invoked.

The grammar is closed: unknown declarations and malformed fields fail with the
input path, line, and column. Keep prose or project-specific metadata in
comments or outside the `.imc` file.

The IMC field vocabulary is:

| Interface type | Generated C++ type |
| --- | --- |
| `bool`, `int`, `float` | `bool`, 32-bit `int`, `float` |
| `int64`, `uint64`, `double` | `std::int64_t`, `std::uint64_t`, `double` |
| `string`, `bytes` | `std::string`, `std::vector<std::uint8_t>` |
| `object`, `vec2`, `vec3`, `mat4` | BML opaque object/math value |
| `array<T>` | `std::vector<T>` for bool, numeric, string, object, and math types |
| `enum<name>` | generated `enum class Name` with a fixed integer underlying type |

Wide numeric arrays support `array<int64>`, `array<uint64>`, and
`array<double>`. A byte blob uses `bytes`; there is no redundant
`array<bytes>` shape. The CMake helper selects typed IMC generation
automatically. Manual generation uses the same IMC-only generator.

An enum may specify `: int`, `: int64`, or `: uint64`; `int` is the default.
Values must have unique names and numbers and fit the
selected width. A record refers to it as `enum<echo_mode>`. Codegen emits
`enum class EchoMode` and `IsKnownEchoMode()` but reuses the corresponding
integer wire codec, so this adds no runtime registry, reflection, or wire tag.
Only scalar enum fields are supported; use a numeric array with an API-owned
conversion helper if a concrete interface needs repeated enum values.

Decoding deliberately preserves an unrecognized underlying number instead of
failing the message. Check `IsKnownEchoMode(value)` before switching when the
sender may be newer. A compatible minor may add named values. It may not remove,
rename, or renumber an existing value, change the underlying type, or introduce
a numeric alias. The interface lock validates these rules. This lets an older
binding receive a newly added value, retain its number, and choose an
API-specific fallback.

### Evolve a compatible minor

For a same-major update, keep existing records, fields, RPCs, and Topics
unchanged. New fields must be optional. Increase the minor version, edit the
source, then explicitly update the interface lock:

```text
python imc_codegen.py --update-lock \
  --input api/example.echo.imc \
  --out-dir generated
```

The adjacent `.imc.lock` file owns permanent field IDs. It also keeps tombstones
for removed optional fields, so an old ID can never be reused by
accident. Declaration reordering does not change those IDs. The update rejects
a required new field, changed old field or endpoint, removed required field,
changed enum value, or a structural edit without a minor-version increase.

Commit `.imc` and `.imc.lock` together. Treat `.imc.lock` like a lock file:
review its diff, but do not edit it by hand. Ordinary generation and `--check`
never modify the lock; they fail with an actionable command when it is
missing or stale. Increase the major version for an intentional clean break;
that starts a new ID space.

## 2. Generate from CMake

The installed BML package includes the generator and
`bml_target_imc_api()`. The helper adds the generated
header to the target, adds its build directory to the include path, and applies
the required C++20 compile feature. It requires Python 3.10 or newer during
configuration, matching the installed generator. The source interface must have
an adjacent, committed `.imc.lock`.

```cmake
find_package(BML CONFIG REQUIRED)

bml_add_mod(EchoMod EchoMod.cpp)

bml_target_imc_api(EchoMod
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/api/example.echo.imc"
)
```

The default output is
`${CMAKE_CURRENT_BINARY_DIR}/bml-imc/example_echo_imc.hpp`. By default the
`.imc` filename must equal the interface's `api`; when it does not, pass
`API_ID example.echo`. The helper passes that expected identity to codegen, so
a typo now reports both IDs and the input path instead of surfacing later as a
missing generated header. `OUTPUT_DIR` can override the generated directory.

For a manual or committed-output workflow, the package exposes the
`BML_IMC_CODEGEN` path:

```text
python imc_codegen.py \
  --input api/example.echo.imc \
  --expected-api-id example.echo \
  --out-dir generated
```

`--expected-api-id` is optional outside the CMake helper but useful in scripts
that predict the output filename. Add `--check` in CI to fail when a committed
generated header or interface lock is stale. Use `--update-lock` only in the
author-controlled interface update step. Parse and validation errors include the
responsible input path, including when several interfaces are generated together.

## 3. Implement the provider

The generated namespace follows the API ID. Provider handlers receive typed
values and return an ordinary BML status code.

```cpp
#include "example_echo_imc.hpp"

namespace Echo = BML::Imc::Generated::Example::Echo;

Echo::Provider g_EchoProvider;

int HandleEcho(const Echo::EchoRequestValue &request,
               Echo::EchoReplyValue &reply,
               void *) {
    reply.Text = request.Text;
    return BML_OK;
}

int StartEchoProvider() {
    int status = g_EchoProvider.Open();
    if (status == BML_OK) {
        status = g_EchoProvider.RegisterEcho(
            &HandleEcho, nullptr, BML_IMC_EXECUTION_CALLER_THREAD);
    }
    return status;
}

void StopEchoProvider() {
    (void)g_EchoProvider.UnregisterEcho();
    (void)g_EchoProvider.Close();
}
```

For a response-less declaration such as `rpc clear_cache()`, the handler is
simply `int ClearCache(void *userdata)` and the client call is
`client.CallClearCache()`. With a request, the handler receives
`const RequestValue &` before `userdata`; neither form receives a dummy output.

Use caller-thread execution only for short, thread-safe code. A handler that
touches Virtools, BML UI, or other game-thread state must use
`BML_IMC_EXECUTION_GAME_THREAD` (the generated default).

`Open()` with no owner ID resolves the native mod from the calling DLL. If one
DLL contains multiple mods, pass that mod's explicit owner ID. Keep the
Provider and its callback userdata alive until unregister/close succeeds.

## 4. Call synchronously or asynchronously

The ordinary generated method performs the complete typed call:

```cpp
Echo::Client client;
int status = client.Open();

Echo::EchoRequestValue request;
request.Text = "hello";
request.Mode = Echo::EchoMode::Normal;
Echo::EchoReplyValue reply;
if (status == BML_OK)
    status = client.CallEcho(request, reply, 1000);
```

An optional integration can avoid a speculative call:

```cpp
bool available = false;
if (client.IsEchoAvailable(available) == BML_OK && available) {
    // Show or enable the integration. The normal call still handles unload.
}
```

`IsEchoAvailable` is one read-only lookup for the already cached RPC ID. It is
an advisory point-in-time snapshot: the provider may unload immediately after
the check, so `CallEcho`/`BeginCallEcho` must still handle
`BML_ERROR_IMC_ENDPOINT_NOT_FOUND`. It does not enumerate providers or
create a future.

Every generated RPC also has a typed future method. It avoids manual request
encoding, raw future handles, payload checks, and result decoding:

```cpp
Echo::Client::EchoFuture pending;
int status = client.BeginCallEcho(request, pending, 1000);

// A later tick or worker iteration:
if (status == BML_OK) {
    status = pending.AwaitResult(reply, 0); // zero means poll
    if (status == BML_ERROR_BUSY) {
        // Still pending; keep the future and try again later.
    }
}
```

`RpcFuture<T>` is move-only and releases its raw future automatically. It also
offers `GetState`, `Await`, `Cancel`, `GetError`, `GetResult`, and explicit
`Release`. A `Begin*` method returns `BML_ERROR_BUSY` rather than silently
overwriting a future that is still owned by the caller.

Do not perform a nonzero wait on the game thread for pending game-thread work.
A zero-time poll is safe.

## 5. Publish and subscribe

```cpp
std::size_t subscribers = 0;
if (client.GetChangedSubscriberCount(subscribers) == BML_OK && subscribers) {
    Echo::ChangedEventValue event;
    event.Sequence = 1;
    event.Text = "updated";
    (void)client.PublishChanged(event);
}
```

Checking the count first is useful when constructing the typed event itself is
expensive. The runtime still handles a zero-subscriber publish safely without
allocating an internal queued message.

```cpp
void OnChanged(int status, Echo::ChangedEventValue *event,
               const BML_ImcMessage *, void *) noexcept {
    if (status == BML_OK && event) {
        // The typed value is borrowed for this callback only.
    }
}

Echo::ChangedSubscription subscription;
int status = client.SubscribeChanged(subscription, &OnChanged, nullptr, 256,
    BML_IMC_BACKPRESSURE_DROP_OLDEST,
    BML_IMC_EXECUTION_GAME_THREAD);
```

Keep the subscription and callback userdata alive until `Close()` succeeds.
Use `DroppedCount()` to observe backpressure loss.

## 6. Diagnose failures

All generated methods return BML status codes. Log both the code and
`BML_GetErrorString(status)`. The common integration failures are:

- `BML_ERROR_IMC_ENDPOINT_NOT_FOUND`: no provider registered that RPC route;
- `BML_ERROR_IMC_API_MISMATCH`: the received payload type does not match the
  generated endpoint;
- `BML_ERROR_WRONG_THREAD`: a pending future was synchronously waited on the
  game thread;
- `BML_ERROR_WOULD_BLOCK`: a bounded queue using FAIL backpressure is full;
- `BML_ERROR_BUSY`: a teardown or future replacement would violate lifetime
  rules.

Close subscriptions and providers before destroying callback userdata. Close
clients before unloading the native DLL. Owner cleanup is the final safety net,
not the normal lifecycle mechanism.

## 7. Release checklist

Before publishing an API:

- verify that the API, record, field, RPC, Topic, and enum identities are final;
- commit the generated `.imc.lock` beside its `.imc` source;
- run generation with `--check` in CI when generated headers are committed;
- inspect the interface-lock diff for a compatible minor;
- use game-thread execution for every callback that touches Virtools or BML UI;
- choose explicit RPC timeouts and Topic queue/backpressure settings;
- test provider unload while clients, futures, and subscriptions exist;
- log and handle BML status codes instead of treating availability as permanent.
