# Services for script Mods

Use the explicit `BML::ModContext` supplied to callbacks. `ctx.Config()`,
`ctx.Input()`, `ctx.Timers()` and `ctx.Commands()` return views of the same
Loader capabilities used by native Mods. A view remains valid throughout its
physical script runtime, including after `OnLoad` updates diagnostic generation
counters. Unload or replacement retires it permanently. A rollback binds a new
runtime; it does not reactivate views stored by the old runtime.

Script operations that can fail return `int`: compare with `BML::ERROR_OK`.
Outputs are usable only on success. Ref outputs are null on failure. Predicates,
fallback getters, UI interaction booleans and the existing Behavior, IMC and
DataShare contracts retain their documented meanings. An ordinary service error
does not disable the Mod. An uncaught callback exception still follows the
Loader's existing failure policy.

This interface intentionally breaks the old authoring API. There are no old
aliases, overloads or registration adapters. See [migration](migration.md).

## Configuration

```angelscript
BML::ConfigProperty@ enabled;

void Setup(const BML::ModContext &in ctx) {
    BML::ConfigPropertyDefinition definition;
    definition.Category = "General";
    definition.Key = "Enabled";
    definition.DefaultValue = BML::ConfigValue(true);
    definition.Comment = "Enable this feature";
    int code = ctx.Config().DeclareProperty(definition, enabled);
    if (code != BML::ERROR_OK) {
        ctx.LogError("DeclareProperty failed: " + code);
        return;
    }
    bool active = enabled.GetBoolean(true);
}
```

`ConfigValue` owns its payload and has a `Type` property. `ReadString`,
`ReadBoolean`, `ReadInteger`, `ReadFloat` and `ReadKey` require an exact type;
NONE or a different type returns `ERROR_TYPE_MISMATCH`. An ordinary integer
constructs INTEGER. Use `CKKEYBOARD` to construct a key value. `Get*(fallback)`
on a property provides an explicit fallback instead of strict reading.

`FindProperty` never creates a property. `Read(ConfigPropertyInfo &out)` returns
an owned snapshot. `Set`, `SetDefault`, `SetComment`, `SetEditor` and `SetChoices`
return status codes. Definition strings and choices are copied when accepted;
editing a read snapshot does not edit the host. `Choices` defaults to an empty
array; assigning null also means no choices.

A repeated declaration of an already-declared incompatible type fails. The
first typed declaration may replace a differently typed loaded value. Likewise,
`SetDefault` preserves a same-type current value, but an untyped declaration can
replace a differently typed current value, including with NONE. To inspect and
preserve an old value before deciding how to migrate, first declare with the
NONE default, then inspect `Read` and its strict value readers. Do not assume a
typed declaration or `SetDefault` is unconditionally non-destructive.

## Input

```angelscript
void Poll(const BML::ModContext &in ctx) {
    BML::KeyboardState@ keyboard;
    if (ctx.Input().ReadKeyboard(keyboard) == BML::ERROR_OK &&
        keyboard.IsKeyPressed(uint(CKKEY_F9))) {
        ctx.LogInfo("F9 pressed");
    }
}
```

`ReadKeyboard`, `ReadMouse` and `ReadJoystick` are the three state reads. Pass
`uint(BML::INPUT_RAW)` to read beneath blocking. Neither form processes the
engine or consumes its event queue. An absent device is a successful snapshot
with `Attached == false`; an absent input manager is `ERROR_UNAVAILABLE`.

`KeyboardState@` owns an immutable shared snapshot of all keys and events.
Holding it across frames or runtime retirement keeps reading that capture. New
reads share the Input module's cache; subsequent changes use copy-on-write.
`ReadKey` rejects an index outside 256 keys with `ERROR_INVALID_PARAMETER`;
`ReadEvent` rejects a missing event with `ERROR_NOT_FOUND`. The convenience
predicates use `IsKeyUp == true` and the other four predicates false out of range.
Native C/C++ keyboard spans still expire at the outer host invocation boundary.

```angelscript
BML::InputBlock@ keyboardBlock;
int code = ctx.Input().AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD), keyboardBlock);
if (code == BML::ERROR_OK) {
    // Release when the UI no longer needs the block.
    code = keyboardBlock.Release();
}
```

Dropping a script Ref does not release its block. Explicit `Release` or runtime
retirement does. Each runtime releases only its own requests; nested blocks and
other owners remain intact. Pause, cursor visibility/shape and keyboard
repetition retain their shared-state semantics: callers must restore them.

## Timers

```angelscript
BML::TimerRef@ timer;
void SetupTimer(const BML::ModContext &in ctx) {
    int code = ctx.Timers().SetIntervalTicks(
        1, BML::TimerLoopCallback(OnTick), timer, "heartbeat");
    if (code != BML::ERROR_OK) ctx.LogError("Timer registration failed: " + code);
}
bool OnTick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    BML::TimerInfo info;
    if (event.Read(info) != BML::ERROR_OK) return false;
    return info.CompletedIterations < 10;
}
```

`Register(TimerDefinition, TimerLoopCallback, TimerRef &out)` supplies type,
time base, delay, repeat count, priority and start-paused state. Timeout helpers
use void callbacks; interval helpers use bool callbacks. Zero delay is accepted
and executes in a later timer pump, never during registration. Milliseconds are
finite, nonnegative doubles; ticks and notification counters retain 64 bits.
DEBOUNCE/THROTTLE remain unimplemented by the existing scheduler.

`TimerEvent.Read` reads a captured value, including after its owner retires.
`TimerRef.Read`, `Pause` and `Resume` require a live owning runtime. `Cancel`
stops new dispatch; an already-entered callback may finish. Returning true from
that callback cannot restart a cancelled timer. Dropping a script TimerRef does
not cancel; native C++ registrations retain their existing RAII cancellation.

## Commands

```angelscript
BML::CommandRef@ command;
void SetupCommand(const BML::ModContext &in ctx) {
    BML::CommandDefinition definition;
    definition.Name = "example";
    int code = ctx.Commands().Register(
        definition, BML::CommandExecute(Execute), command);
    if (code != BML::ERROR_OK) ctx.LogError("Command registration failed: " + code);
}
int Execute(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) {
    string argument;
    if (call.ArgumentCount != 0) call.ReadArgument(0, argument);
    return call.Write("input: " + call.Input) == BML::ERROR_OK ? 0 : 1;
}
```

The execute callback returns a nonnegative command status; a negative return
becomes command failure. `Commands.ExecuteLine(line, status &out)` separately
returns an API status. `HasInput` distinguishes no pipe from an empty pipe.
`Name` is canonical; `InvokedAs` is the typed name or alias. `ReadArgument`
uses zero-based arguments after the command name.

The optional complete delegate is
`int Complete(const BML::ModContext &in, const BML::CommandCompletionRequest &in, const BML::CommandCompletion &in)`.
It can inspect active argument and prefix and add/read candidates through the
existing collector. Failure discards this completion's candidates. Invocation,
request and completion copies expire together on callback return. Later sink
operations return `ERROR_INVALID_HANDLE`; copy text to retain it.

`Find` reads one `CommandInfo`. For collections pass an actual
`array<BML::CommandInfo>` object to `ReadCommands(? &out)`. Similarly, pass
`array<BML::ModInfo>` to `ctx.ReadMods(? &out)`. The runtime checks the exact
array type. A correct output is empty on failure and complete on success;
wrong types return `ERROR_TYPE_MISMATCH` without array writes.

## Resources, objects and cleanup

`ctx.Owner`, `FindMod`, `GetDirectory`, `GetResourcePath`, `ReadResource` and
`ResourceExists` use status plus outputs. `DIR_MOD_ROOT` is the entry/package
root, including for scripts. `GetResourcePath` resolves existing files only;
use the existing Path helpers to construct a path for a future file.

Use the Scene, Gameplay, Time, Speedrun, UI and Engine namespaces for their
capabilities. `BML::ObjectRef` is the existing domain/slot/generation identity,
not an engine pointer. Borrowed CK objects and managers remain engine-owned;
references reject deleted/reused objects and world resets. An owned record
snapshot does not keep its engine objects alive.

New work is frozen during unload. Cleanup remains available until service
retirement. For the same logical owner, successful Cancel/Release/Unregister is
idempotent, even after runtime retirement. Another owner is always denied.
Pause/Resume/Read have no terminal exception. Failed live cleanup keeps the
request for retry; the original host remains responsible for retired cleanup.
Do not use public diagnostic Generation properties as authorization or lifetime
checks. Gameplay type registration still requires the owner's `OnLoad` and uses
the existing capture/validation/rollback transaction.
