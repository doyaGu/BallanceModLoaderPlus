# Communication from script mods

Choose the smallest existing mechanism that matches the data and execution
model. BML+ does not expose a raw message codec or custom IMC Provider API to
scripts.

## Mod identity and dependencies

`ModRef` reports another mod's id, version, load state, and diagnostic. Declare
required and optional load-order relationships with `[bml.require]` and
`[bml.optional]`; use `ModRef` to inspect the result, not to build a second
runtime dependency graph.

## DataShare

Use DataShare for small named scalar or byte values in the same process.

Synchronous typed read:

```angelscript
bool enabled = BML::DataShareGetBool("feature.enabled", false, "BML");
int size = BML::DataShareSizeOf("feature.enabled", "BML");
```

Use a callback request when the value may not exist yet:

```angelscript
void ReceiveGreeting(const BML::ModContext &in ctx,
                     const BML::DataShareEvent &in event) {
  ctx.LogInfo(
      event.Exists ? event.StringValue : "missing " + event.Key);
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::DataShareRequestRef@ request = ctx.RequestDataShare(
      "remote.greeting", BML::DATASHARE_STRING, ReceiveGreeting);
  if (request is null)
    ctx.LogWarn("failed to request remote.greeting");
}
```

The callback is queued to a BML+ main-thread safe point. It does not run inside
the request or native trigger call stack. A request becomes inert after
completion, cancellation, unload, or successful reload replacement. Pending
callbacks are cancelled when their owner unloads.

Implement `BML::DataShareRequest` instead of a delegate when the request owns
state or needs explicit `Key`, `Type`, and optional namespace getters.

DataShare is not a schema system. Both sides must agree on the name, namespace,
and value type. Use it for a small piece of state, not request routing, event
streams, large payloads, or a protocol that must evolve independently.

## Built-in typed services

For BML+ runtime, gameplay, event, UI, and speedrun features, use the existing
typed script APIs. They project the loader's own interface structs, which is
where loader capability lives; IMC carries none of it, and no transport handle or
payload decoding is involved.

For communication among CKAngelScript runtime scripts and components, use
CKAngelScript `Message` or `Async` when that execution model fits. A BML+ fixed
callback cannot suspend.

## Generated IMC services

IMC is for an API one mod publishes for other mods. Use generated IMC when such a
service needs one or more of these properties:

- typed request and response messages;
- asynchronous completion;
- ordered Topic delivery with bounded capacity and dropped-message reporting;
- stable field and endpoint identities across versions;
- explicit thread policy; or
- throughput that should stay in native code.

A native mod writes the `.imc` interface, commits its `.imc.lock`, generates
the C++ binding, and registers the generated Provider. Another native mod uses
the generated Client. See [Inter-mod communication](../imc.md) and
[Create a typed IMC API](../imc-author-guide.md).

Script mods do not register custom RPC or Topic providers. If scripts also need
the native service, its owning plugin should register a small typed
CKAngelScript extension. Keep native ownership, thread policy, validation, and
high-frequency work behind that extension.

## Design a script-facing native service

A useful split is:

```text
generated IMC interface
        |
native Provider and service implementation
        |
small CKAngelScript extension
        |
BML+ script mod policy and UI
```

The extension should expose domain operations and values, not transport
handles, serialized payloads, allocator details, or raw worker-thread
callbacks. Report failure as a typed result, status, or script exception that
the author can diagnose.

Avoid a new service when DataShare or an existing BML+/CKAngelScript API already
owns the operation. Avoid a script façade when no script consumer exists.

## Performance rules

- Synchronous IMC handlers and BML+ callbacks normally execute on the game
  thread; keep them bounded.
- Move parsing, preparation, and large native work out of per-frame script
  callbacks.
- Do not rebuild stable gameplay snapshots or repeat scene scans every frame.
- Give Topics a bounded capacity, drain them regularly, and inspect dropped
  counts.
- Keep high-rate native data in native structures. Cross into script at a lower
  rate with the values the script actually uses.
- Do not hand-write JSON, byte codecs, or numeric field ids for a typed service;
  generated IMC already supplies the schema and bindings.

Return to [Script mods](index.md) or open the
[script API reference](api.md).
