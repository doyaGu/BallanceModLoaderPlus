# BML services for script mods

`BML::ModContext` is the per-mod entry point for logging, configuration,
resources, input, commands, timers, mod lookup, and loader-owned services.
Prefer the callback parameter over a global helper when both exist.

## ModContext

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  BML::Logger@ logger = ctx.BorrowLogger();
  if (logger !is null)
    logger.Info("id=" + ctx.ModId);

  BML::Config@ config = ctx.BorrowConfig();
  if (config !is null) {
    BML::ConfigProperty@ greeting = config.GetProperty("General", "Greeting");
    if (greeting !is null)
      greeting.SetDefaultString("hello");
  }
}
```

Common capabilities include:

- the current mod id, root, logger, and configuration;
- BML/game state, directories, input, HUD, messages, and menus;
- `FindMod`, `GetModCount`, and `GetMod`;
- Timer and Command registration;
- DataShare reads and requests;
- CK manager and named-object borrowing;
- typed runtime, gameplay, and event snapshots.

`BorrowLogger()` and `BorrowConfig()` return BML+ wrappers that revalidate the
owning mod. Other `Borrow*` methods commonly expose non-owning CK or manager
handles; do not keep those across callbacks.

## Resources and paths

Resolve files through the mod root so directory, single-file, and zip packages
behave consistently:

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
  string root = ctx.GetModRootUtf8();
  string text = ctx.ReadModTextFileUtf8("Resources/readme.txt", "");
  if (ctx.ModFileExistsUtf8("Resources/settings.txt"))
    ctx.BorrowLogger().Info("resource root=" + root + " text=" + text);
}
```

`ResolveModPathUtf8`, `ModFileExistsUtf8`, `ModDirectoryExistsUtf8`, and
`ReadModTextFileUtf8` guard access to mod resources. `BML::Path` provides pure
path operations such as `Combine`, `Normalize`, `FileName`, `Extension`,
`RemoveExtension`, `IsAbsolute`, `IsRelative`, `Exists`, `IsFile`, and
`IsDirectory`.

Known BML+ paths are available through `GetDirectoryUtf8`, including
`DIR_GAME`, `DIR_LOADER`, `DIR_CONFIG`, and `DIR_TEMP`. Prefer mod-relative
resource paths for publishable assets.

## Logging and configuration

`Logger`, `Config`, and `ConfigProperty` expose `IsValid`. Stored wrapper
handles remain usable only while their owning mod and property remain valid.

```angelscript
BML::ConfigProperty@ enabledProp;
bool enabled = true;

void OnLoad(const BML::ModContext &in ctx) {
  BML::Config@ config = ctx.BorrowConfig();
  if (config is null)
    return;

  @enabledProp = config.GetProperty("General", "Enabled");
  if (enabledProp !is null) {
    enabledProp.SetDefaultBoolean(true);
    enabledProp.SetComment("Enable the example feature.");
    enabled = enabledProp.GetBoolean(true);
  }
}

void OnModifyConfig(const BML::ModContext &in ctx,
                    const BML::ConfigEvent &in event) {
  if (event.Category == "General" && event.Key == "Enabled") {
    BML::ConfigProperty@ prop = event.BorrowProperty();
    if (prop !is null)
      enabled = prop.GetBoolean(enabled);
  }
}
```

Use typed getters, setters, and defaults for string, boolean, integer, float,
and key values. `OnModifyConfig` suppresses recursive edits of the same property.

## Input, state, UI commands, and speedrun timing

Input is borrowed through `ctx.BorrowInputManager()` and used within the current
callback:

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
  BML::InputHook@ input = ctx.BorrowInputManager();
  if (input !is null && input.IsKeyPressed(CKKEY_F5))
    BML::UI::AddMessage("F5 pressed by script");
}
```

`ModContext` exposes `IsInGame`, `IsInLevel`, `IsPaused`, `IsPlaying`,
`IsCheatEnabled`, `EnableCheat`, `ExitGame`, `ExecuteCommand`,
`SkipRenderForNextTick`, `GetSRScore`, and `GetHSScore`.

Loader-owned presentation commands live under `BML::UI`: messages, mod/map
menus, HUD mode, title, and FPS. Speedrun timing is separate under
`BML::Speedrun`: visibility, start, pause, reset, and elapsed time. These
commands do not require an active ImGui frame. Drawing controls do; see
[Engine and UI](engine-ui.md).

Guard mutations that require an active level with an explicit state check.
Defaults and no-ops prevent a host crash, but an explicit branch produces a
clearer script and diagnostic.

## Built-in typed snapshots

`BML::Runtime` returns small in-process state, clock, score, and cheat values.
`BML::Gameplay` reads gameplay data that may be unavailable or use an
unsupported layout, so those calls return a status. Calling a built-in API
outside a valid script callback raises a script exception.

```angelscript
BML::Runtime::State runtime = BML::Runtime::GetState();
BML::Gameplay::LevelState level;

if (runtime.InLevel && BML::Gameplay::ReadLevel(level) == BML::ERROR_OK) {
  CKObject@ ball = level.BorrowActiveBall();
}
```

Catalog and checkpoint reads return complete script-owned arrays. Retain a
snapshot while its source is stable instead of rebuilding it every frame.
`BML::Events::Stream` yields immutable event snapshots in hook order. Poll only
when work is ready to consume events, handle `BML::ERROR_NOT_FOUND` as an empty
stream, and monitor `GetDroppedCount` for queue loss.

## Timers

BML+ owns registered timers for the script mod and cancels them at unload.
Prefer callback timers for simple delays and intervals:

```angelscript
void SayReady(const BML::ModContext &in ctx,
              const BML::TimerEvent &in event) {
  ctx.BorrowLogger().Info("ready");
}

bool Heartbeat(const BML::ModContext &in ctx,
               const BML::TimerEvent &in event) {
  ctx.BorrowLogger().Info("heartbeat " + event.CompletedIterations);
  return event.CompletedIterations < 5;
}

void OnLoad(const BML::ModContext &in ctx) {
  ctx.SetTimeout(1000.0f, SayReady, "ready");
  ctx.SetInterval(1000.0f, Heartbeat, "heartbeat");
}
```

Millisecond variants use `SetTimeout` and `SetInterval`; tick variants use
`SetTimeoutTicks` and `SetIntervalTicks`. An interval callback returns `false`
to stop. Use an AngelScript delegate to bind an object method.

Implement `BML::Timer` when the timer needs object state, start-paused,
repeat-count, or priority configuration. `TimerRef` supports validity checks,
pause, resume, cancel, state, completed/remaining iterations, and progress.

## Commands and completion

Use `CommandDefinition` and delegates for a small command:

```angelscript
void HelloExecute(const BML::ModContext &in ctx,
                  const BML::CommandEvent &in event) {
  ctx.BorrowLogger().Info("hello " + event.ArgsText);
}

void HelloComplete(const BML::ModContext &in ctx,
                   const BML::CommandEvent &in event,
                   BML::CommandCompletion &inout completions) {
  completions.Add("world");
}

void OnLoad(const BML::ModContext &in ctx) {
  BML::CommandDefinition def;
  def.Name = "hello";
  def.Alias = "h";
  def.Description = "Print hello";
  ctx.RegisterCommand(def, HelloExecute, HelloComplete);
}
```

Implement `BML::Command` when the command owns state or benefits from a single
object. `Name` and `Execute` are required; alias, description, usage, cheat
requirement, enabled state, and completion are optional. Duplicate names or
aliases fail registration. Self-unregister is delayed until the callback
returns.

Next: [Engine and UI](engine-ui.md).
