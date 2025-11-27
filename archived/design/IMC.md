# BML Inter-Mod Communication (IMC) - Design Document

**Scope:** IMC Core (L0–L4), Stable C ABI, C++ DX Layer, Schemas & Tooling, Policy/Permissions, Observability
**Design goals:** deterministic correctness, very low latency for common in-process flows, clean developer experience, safe evolution

---

## 1. IMC Guiding Principles

1. **Two physical planes, one logical API**

* **In-process plane:** ultra-low-latency data structures (lock-free rings/mailboxes) for the hot path-no syscalls in steady state. Proven design (LMAX Disruptor) achieves high throughput with predictable latency. ([lmax-exchange.github.io][1])
* **Inter-process plane (optional):** framed message channels on Unix domain sockets / Windows named pipes (message mode), plus pollers (epoll/IOCP/kqueue). Message boundaries preserved or reconstructed by L2 framing-never rely on best-effort atomic writes. ([Microsoft Learn][2])

2. **Dual-schema specialization at L3**

* **Events (Pub/Sub): FlatBuffers** for **in-place**, zero-copy reads on subscribers. ([flatbuffers.dev][3])
* **Commands (RPC): Protocol Buffers** for clear, widely tooled request/response; providers use **Arena allocation** to slash alloc/free overhead. ([protobuf.dev][4])

3. **Explicit semantics**

* **Events:** at-most-once; per-topic order; lossy under pressure (drop-newest).
* **RPC:** exactly-once within a provider process; timeouts and cancelation are first-class.
* **Backpressure:** publishers/callers get `{Ok, Dropped, QueueFull, Timeout}` (no unbounded buffering).

4. **Developer-centric DX**

* Stable C ABI for portability; C++ DX layer with RAII, `Result<T>`, awaitable `Future`, default main-thread handlers, optional executors.

5. **Evolvable & observable**

* Capabilities & rate hints from `mod.toml` enforced at runtime.
* Built-in counters, latency histograms, and tracing (`msg_id`) across layers.

---

## 2. Architecture Overview

```
+--------------------------------------------------------------------+
|                            Mod Code (DX)                           |
+--------------------------------------------------------------------+
|                      Stable C ABI  (extern "C")                    |
+=========================== Logical Layer ===========================+
| L4 Protocols:   Pub/Sub (Events) · RPC (Commands)                  |
| L3 Schemas:     FlatBuffers (Events) · Protobuf (RPC)              |
+========================== Physical Planes ==========================+
| In-Process Plane:  MPSC/MPMC Rings (Events), Mailboxes (RPC)       |
| Inter-Process:     Framed Streams · Msg Channels                   |
|                    (UDS / Named Pipes + epoll · IOCP · kqueue)     |
+--------------------------------------------------------------------+
```

### Pollers & transports (defaults)

* **Linux:** epoll **level-triggered** default (simpler, safer); edge-trigger opt-in with documented drain rules. ([lwn.net][5])
* **Windows:** IOCP + **message-type** named pipes to preserve frames and surface `ERROR_MORE_DATA` for partial reads. ([Microsoft Learn][2])
* **macOS/BSD:** kqueue for readiness; framing sits above.

*(Optional)* **io_uring** backend on Linux for specialized workloads (fewer syscalls/unified async I/O), but not required for v1. ([Red Hat Developer][6])

---

## 3. Message Semantics & QoS

* **Ordering:** Events are ordered per topic per publisher; RPC responses are correlated by `msg_id`.
* **Delivery:**

  * Events: at-most-once, best-effort; on overflow, **drop-newest** (keeps system fresh).
  * RPC: one response per request (success, error, or timeout).
* **QoS classes:**

  * **Notify:** coalescable; drop-newest on overflow.
  * **Event:** ordered; drop-newest.
  * **Command:** backpressure to caller-return `QueueFull`.

---

## 4. L2 Framing (Wire Format) & Fragmentation

**Frame header (little-endian):**

```
u32 len;            // payload size (excl. header)
u16 schema_ver;     // schema version for payload
u8  schema_type;    // 0x01=Protobuf, 0x02=FlatBuffers
u8  flags;          // bit0=fragmented, bit1=last-fragment, bit2=compressed
u32 schema_hash;    // 32-bit hash of schema ID (fast dispatch/validate)
u64 msg_id;         // tracing & RPC correlation
```

**Fragmentation:** Senders **MUST** fragment payloads beyond `MAX_FRAME`; receivers **MUST** reassemble by `(channel_id, msg_id)` until `last-fragment`. This avoids relying on transport atomicity (POSIX only guarantees atomic pipe writes ≤ `PIPE_BUF`-commonly 4096 bytes; larger writes may interleave). ([man7.org][7])

---

## 5. Physical Planes

### 5.1 In-Process Plane (default hot path)

* **Events:** MPMC ring buffers with per-subscriber cursors; bounded capacity; `DropNewest`.
* **RPC:** request mailbox (MPSC) + reply mailbox (MPSC); correlation via `msg_id`.
* **Rationale:** Disruptor-style rings separate producer/consumer concerns, minimize contention, and deliver consistent low latency. ([lmax-exchange.github.io][1])

### 5.2 Inter-Process Plane

* **Windows:** Named pipes in **message mode** on both ends; reads may surface `ERROR_MORE_DATA`-callers must loop until the full message is drained. ([Microsoft Learn][2])
* **POSIX:** Unix stream sockets with L2 framing (do **not** assume write atomicity beyond `PIPE_BUF`). ([man7.org][7])
* **Pollers:** epoll (Linux), IOCP (Windows), kqueue (macOS/BSD). ET is optional with strict “drain-everything” guidance. ([lwn.net][5])

---

## 6. L3 Schemas & Evolution

* **Events → FlatBuffers**

  * Receivers **MUST** treat payload as immutable; read in-place (zero-copy). ([flatbuffers.dev][3])
  * Evolution: only add optional fields; never reorder existing fields.
* **RPC → Protobuf**

  * Use field numbers once; never reuse; unknown-field tolerant decoding.
  * Providers should use **Protobuf Arena** to reduce allocation churn. ([protobuf.dev][4])

**Schema tooling**

* `imc schema gen --fbs *.fbs --proto *.proto` (codegen) ([flatbuffers.dev][8])
* `imc schema lint` (FB reorder ban; PB field-number reuse ban)

---

## 7. L4 Protocols

### 7.1 Pub/Sub (Events)

* Topics resolved to integer IDs via registry; per-subscriber bounded queues.
* `publish(topic, event)` → `{Ok, Dropped, QueueFull}`.

### 7.2 RPC (Commands)

* Async request/response; `msg_id` correlation; timeouts & cancelation.
* Backpressure: provider queue full → caller gets `QueueFull` immediately.
* C ABI: `BML_Imc_RegisterRpc` exposes handlers, `BML_Imc_CallRpcAsync` returns a `BML_Future` handle supporting `Await/Cancel/GetResult/SetCallback` so callers can integrate with their own executors.

---

## 8. Security, Policy & Manifest

**`mod.toml` IMC section**

```toml
[imc]
provides_rpc   = ["inventory.get_items"]
calls_rpc      = ["player.get_health"]
publishes_to   = ["ui.button_clicked"]
subscribes_to  = ["player.on_damage"]
permissions    = ["allow_sync_rpc"]
rate_hints.rpc."inventory.get_items".qps_max = 200
rate_hints.topic."player.on_damage".pub_rps_max = 2000
```

**Runtime enforcement**

* On load, actual usage is validated against manifest scopes (e.g., `ui.*`); out-of-policy ops fail fast with a diagnostic.
* Rate hints guide scheduler/drop policies; violations yield structured warnings.

---

## 9. Observability & Tooling

* **Counters per topic/RPC:** publish rate, drops, queue depth p50/p95/p99, RPC latency histograms, timeout counts.
* **Tracing:** 64-bit `msg_id` injected at publish/call; propagated through handlers; surfaced in logs/CLI.
* **CLI tools:**

  * `imc tap --topic player.on_damage --to json`
  * `imc trace --msg 0xDEADBEEFCAFEBABE`
  * `imc dump --schema payload.bin --proto inventory.proto`

---

## 10. Stable C ABI (abridged)

```c
// Version & features
uint32_t   BML_Imc_GetVersion(void);
BML_Result BML_Imc_GetFeatures(uint64_t* out_mask);

// Pump & threading
void       BML_Imc_PumpEvents(void); // Handlers run on main thread by default

// Pub/Sub
BML_Result BML_Imc_Publish(const char* topic,
                           const void* payload, size_t len);
BML_Result BML_Imc_Subscribe(const char* topic,
                             BML_ImcPubSubHandler cb, void* user_data,
                             BML_ImcSubscriptionHandle* out);

// RPC
BML_Result BML_Imc_RegisterRpc(const char* name,
                               BML_ImcRpcHandler cb, void* user_data);
BML_Result BML_Imc_CallRpcAsync(const char* name,
                                const void* payload, size_t len,
                                BML_ImcFutureHandle* out);
BML_Result BML_Imc_FutureCancel(BML_ImcFutureHandle);
BML_Result BML_Imc_SetDefaultTimeoutMs(uint32_t);

// Discovery
BML_Result BML_Imc_ListRpcs(const char*** names, size_t* count);
BML_Result BML_Imc_ListTopics(const char*** names, size_t* count);
void       BML_Imc_FreeNameList(const char** names, size_t count);

// Telemetry
BML_Result BML_Imc_GetCounter(const char* name, uint64_t* out_value);
```

**Threading contract:** All callbacks are marshaled to the main thread and executed during `BML_Imc_PumpEvents()` unless a custom executor is specified.

---

## 11. C++ DX Layer (the “sweet” API)

```cpp
namespace IMC {
  struct Error { int code; std::string_view msg; };

  template<class T>
  class Result {
  public:
    bool ok() const;
    T& value(); const T& value() const;
    Error& error(); const Error& error() const;
  };

  namespace PubSub {
    enum class PublishStatus { Ok, Dropped, QueueFull };

    class [[nodiscard]] Subscription; // RAII: auto-unsubscribe

    template<class T_FlatBuffer>
    PublishStatus publish(std::string_view topic, const T_FlatBuffer& event);

    template<class T_FlatBuffer>
    std::unique_ptr<Subscription> subscribe(
      std::string_view topic,
      std::function<void(const T_FlatBuffer&)> handler,
      Executor* exec = MainThread());
  }

  namespace RPC {
    template<class T> class Future; // awaitable

    template<class Resp, class Req>
    Future<Result<Resp>> call(std::string_view name, const Req& request);

    template<class Resp, class Req>
    void provide(std::string_view name,
                 std::function<Result<Resp>(const Req&)> handler,
                 Executor* exec = MainThread());

    template<class T>
    class Future {
    public:
      Future& then(std::function<void(Result<T>)> cont);
      Future& with_timeout(std::chrono::milliseconds);
      void cancel();
      auto operator co_await();
    };
  }
}
```

**DX guarantees**

* RAII resources; no manual `Release`.
* Default handler affinity: **main thread** (UI-safe); custom executors allowed.
* `publish()`/`call()` surface backpressure & timeouts to the caller.

---

## 12. Performance & Tuning Notes

* **Events (FB):** zero-copy reads minimize allocations & parsing-ideal for hot, high-fanout topics. ([flatbuffers.dev][3])
* **RPC (PB):** Protobuf **Arena** aggregates allocations and frees en masse at scope end-ideal for short-lived RPC messages. ([protobuf.dev][9])
* **Linux poller choice:** prefer epoll **LT** unless you can rigorously implement ET “drain until EAGAIN” everywhere. ([lwn.net][5])
* **Windows IPC:** use **PIPE_TYPE_MESSAGE** to preserve frames; handle `ERROR_MORE_DATA` as the partial-read signal. ([Microsoft Learn][2])
* **POSIX atomicity:** treat `PIPE_BUF` as the only write-atomic bound; rely on L2 fragmentation for larger messages. ([man7.org][7])
* **io_uring:** potential win for heavy small-I/O workloads; keep optional. ([Red Hat Developer][6])

---

## 13. Tooling & Developer Workflow

* **Codegen:** `flatc --cpp` and `protoc --cpp_out` pinned in the build; `ENABLE_ARENA=ON` for PB. ([flatbuffers.dev][8])
* **Schema lint:** enforce FB field-order stability and PB field-number uniqueness.
* **CLI inspectors:** `tap`, `trace`, `dump` for payloads (FB → JSON view; PB → text format).

---

## 14. Testing Strategy

* **Unit:** frame parser/assembler, ring queues, subscription teardown, timeout/cancel.
* **Fuzz:** deframer + fragmented inputs (malformed headers, out-of-order fragments).
* **Perf:**

  * Events fan-out (in-proc rings) throughput & tail latency. (Disruptor-style patterns are the baseline.) ([lmax-exchange.github.io][1])
  * RPC latency with Arena vs heap. ([protobuf.dev][9])
* **Soak:** 24h workload mixing bursty events, steady RPC, induced stalls (verify drop policies) & timeouts.

---

## 15. Migration & Compatibility

* **Semantic versioning:** wire format and ABI changes bump MAJOR; schema additions bump MINOR.
* **Shim layer:** optional adapter mapping legacy IMC calls for gradual adoption.

---

### Appendix A - Minimal Examples

**Event publish (C++ DX)**

```cpp
PlayerDamagedEvent fb_event = BuildPlayerDamaged(builder, dmg);
using PS = IMC::PubSub::PublishStatus;
if (IMC::PubSub::publish("player.on_damage", fb_event) != PS::Ok) {
  // coalesce or log drop
}
```

**RPC call with timeout (C++20 coroutine)**

```cpp
auto r = co_await IMC::RPC::call<GetInventoryResponse>("inventory.get_items", req)
                             .with_timeout(250ms);
if (!r.ok()) { ShowToast("Inventory timeout"); co_return; }
UpdateUI(r.value().items());
```

**Windows pipe creation (message mode)**

```cpp
CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                 PIPE_UNLIMITED_INSTANCES, outBuf, inBuf, 0, NULL);
```

(Ensures message boundaries; handle `ERROR_MORE_DATA` on reads.) ([Microsoft Learn][2])

[1]: https://lmax-exchange.github.io/disruptor/user-guide/index.html?utm_source=chatgpt.com "LMAX Disruptor User Guide"
[2]: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes?utm_source=chatgpt.com "Named Pipe Type, Read, and Wait Modes - Win32 apps"
[3]: https://flatbuffers.dev/white_paper/?utm_source=chatgpt.com "White Paper - FlatBuffers Docs"
[4]: https://protobuf.dev/reference/cpp/arenas/?utm_source=chatgpt.com "C++ Arena Allocation Guide"
[5]: https://lwn.net/Articles/865400/?utm_source=chatgpt.com "The edge-triggered misunderstanding"
[6]: https://developers.redhat.com/articles/2023/04/12/why-you-should-use-iouring-network-io?utm_source=chatgpt.com "Why you should use io_uring for network I/O"
[7]: https://man7.org/linux/man-pages/man7/pipe.7.html?utm_source=chatgpt.com "pipe(7) - Linux manual page"
[8]: https://flatbuffers.dev/tutorial/?utm_source=chatgpt.com "Tutorial - FlatBuffers Docs"
[9]: https://protobuf.dev/reference/cpp/api-docs/google.protobuf.arena/?utm_source=chatgpt.com "arena.h | Protocol Buffers Documentation"
