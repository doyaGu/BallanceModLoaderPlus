# Real-Game Testing

All real-game testing uses a single tool: `tools/bml_test.py` (Python 3.11+, stdlib only).

```bash
python tools/bml_test.py <subcommand> [options]
```

## Quick Reference

| Goal | Command |
|------|---------|
| Deploy + launch specific modules | `python tools/bml_test.py run --modules BML_UI,BML_Console` |
| Deploy + launch all builtins | `python tools/bml_test.py run --all-builtin` |
| Build first, then deploy | `python tools/bml_test.py run --all-builtin --build` |
| Deploy without launching | `python tools/bml_test.py run --all-builtin --no-launch` |
| Test a third-party mod | `python tools/bml_test.py run --extra C:\MyMods\MyMod` |
| Run integration test module | `python tools/bml_test.py run --all-builtin --extra BML_IntegrationTest` |
| Batch crash detection | `python tools/bml_test.py smoke` |
| Per-module health analysis | `python tools/bml_test.py analyze --all-builtin` |
| Check dependency resolution | `python tools/bml_test.py resolve --modules BML_Console` |

## CMake Targets

After `cmake` configure, these targets are available:

```bash
cmake --build build --target bml-run           # Deploy all builtins + launch
cmake --build build --target bml-deploy        # Deploy only (no launch)
cmake --build build --target bml-smoke         # Batch smoke test
cmake --build build --target bml-analyze       # Per-module analysis
cmake --build build --target bml-run-BML_Console   # Deploy BML_Console + deps + launch
```

Configure with cache variables:

```bash
cmake -B build -DBML_GAME_DIR="D:/Games/Ballance" -DBML_TEST_CONFIG=Debug
```

## Subcommands

### `run` - Deploy and Launch

The core workflow for daily development. Deploys specified modules to the game, launches it, waits for exit or timeout, collects artifacts, then restores the game directory.

```bash
python tools/bml_test.py run --modules BML_UI,BML_Console [options]
python tools/bml_test.py run --all-builtin [options]
python tools/bml_test.py run --extra BML_IntegrationTest [options]
```

**Module selection** (at least one required):

| Flag | Meaning |
|------|---------|
| `--modules A,B` | Deploy these modules (+ auto-resolved dependencies) |
| `--all-builtin` | Deploy all non-test modules from `modules/` |
| `--extra X` | Add module X on top of `--modules` or `--all-builtin`. Repeatable. Accepts a build module name or a directory path containing `mod.toml` |

`--modules` and `--all-builtin` are mutually exclusive. `--extra` is always additive.

**Options:**

| Flag | Default | Meaning |
|------|---------|---------|
| `--build` | off | Run `cmake --build` before deploying |
| `--target A,B` | all | CMake build targets (only with `--build`) |
| `--timeout N` | 300 | Seconds to wait before killing the game. 0 = wait forever |
| `--screenshot` | off | Capture a window screenshot before game exits |
| `--no-launch` | off | Deploy only, don't start the game (implies `--no-restore`) |
| `--no-restore` | off | Don't restore the game directory after the run |
| `--input-scenario X` | none | Inject a predefined key sequence (e.g. `console-basics`) |
| `--game-dir` | `$BML_GAME_DIR` env var | Game installation path |
| `--build-dir` | `<repo>/build` | CMake build directory |
| `--config` | `Release` | Build configuration |

**Flow:**

1. Backup `BML.dll`, `ModLoader.dll`, `ModLoader/Mods/`
2. Resolve module dependencies from `mod.toml`
3. Clear `ModLoader/Mods/` and deploy requested modules
4. Launch `Player.exe`
5. Wait for game exit or timeout
6. Collect logs and reports to `artifacts/run-<timestamp>/`
7. Restore game directory

**Artifacts:**

```
artifacts/run-<timestamp>/
  summary.json       # Metadata: modules, timeout, exit code
  window.bmp         # Screenshot (if --screenshot)
  reports/           # JSON reports from ModLoader/ (IntegrationTestReport.json etc.)
  logs/              # Per-module logs
    BML_Console/
    BML_UI/
```

### `smoke` - Batch Crash Detection

Runs the game multiple times with different module combinations to check for crashes. Uses `BML_MODS_DIR` environment variable so the game's own `ModLoader/Mods/` is not modified.

```bash
python tools/bml_test.py smoke [options]
```

**Case selection** (choose one, or omit for auto-generation):

| Flag | Meaning |
|------|---------|
| *(none)* | Auto-generate: each builtin module + its deps as one case, all builtins as final case |
| `--cases "A,B;C,D"` | Semicolon-separated module groups |
| `--cases-file tools/smoke-cases.toml` | Read cases from TOML file |

**Options:**

| Flag | Default | Meaning |
|------|---------|---------|
| `--duration N` | 15 | Seconds to run each case before checking if process is alive |
| `--build` | off | Run `cmake --build` first |

**Judgment:** process still alive after `duration` seconds = PASS, process exited = FAIL (crash).

**Artifacts:**

```
artifacts/smoke-<timestamp>/
  summary.json
  runs/
    <case_name>/
      Mods/            # Preserved temp Mods directory
      reports/
      logs/
```

**`smoke-cases.toml` format:**

```toml
[cases]
minimal = ["BML_UI", "BML_Input"]
core_five = ["BML_UI", "BML_Input", "BML_Console", "BML_ObjectLoad", "BML_GameEvent"]
```

Dependencies are resolved automatically. Specifying `BML_Console` implicitly includes `BML_UI`, `BML_Menu`, and `BML_Input`.

### `analyze` - Per-Module Isolation Probe

Runs a full-stack baseline, then probes each module in isolation with its minimal dependency set. Intended for pre-release health checks.

```bash
python tools/bml_test.py analyze --all-builtin [options]
python tools/bml_test.py analyze --modules BML_Console,BML_MapMenu [options]
```

**Flow:**

1. Deploy `BML.dll` + `ModLoader.dll`
2. Run full-stack baseline (all builtins + `BML_IntegrationTest`)
3. For each target module:
   a. Resolve its minimal dependency set
   b. Run `BMLCoreDriver.exe` bootstrap check
   c. Launch game with `BML_INTTEST_MODE=probe` + `BML_PROBE_TARGET=<id>`
   d. Collect probe report
4. Restore and output health summary

**Options:**

| Flag | Default | Meaning |
|------|---------|---------|
| `--probe-timeout N` | 90 | Seconds to wait per probe |
| `--probe-min-duration N` | 8 | Minimum probe duration in seconds |
| `--pause-between N` | 3 | Seconds between probes |
| `--build` | off | Build first |

Probe reports are self-describing JSON. The script displays them as-is without interpreting signal names.

**Artifacts:**

```
artifacts/analyze-<timestamp>/
  builtin-module-health.json    # Summary with all probe results
  runs/
    baseline/
    bml_console/
    bml_ui/
    ...
```

### `resolve` - Dry Run

Outputs the resolved module set as JSON to stdout. No game launch, no file changes. Useful for CI validation and debugging dependency resolution.

```bash
python tools/bml_test.py resolve                          # All builtins
python tools/bml_test.py resolve --modules BML_Console    # Single module + deps
```

**Output:**

```json
{
  "builtin_modules": ["BML_Console", "BML_GameEvent", ...],
  "resolved_modules": ["BML_UI", "BML_Menu", "BML_Input", "BML_Console"],
  "dependency_graph": {
    "BML_Console": ["com.bml.ui", "com.bml.menu", "com.bml.input"],
    "BML_UI": []
  }
}
```

`builtin_modules` is always the full list (unaffected by `--modules`). `resolved_modules` is the requested set with transitive dependencies in topological order.

## Key Concepts

### Dependency Resolution

Dependencies are read from each module's `mod.toml` `[dependencies]` section. The script builds a package ID index, then resolves transitive dependencies via topological sort.

- `optional = true` dependencies are only included if the optional module is already in the deployment set
- Resolution checks `build/Mods/<Name>/<Config>/mod.toml` first, falls back to `modules/<Name>/mod.toml`

### Module Exclusion

Modules with `category = "test"` in their `mod.toml` are excluded from `--all-builtin`. Add them explicitly with `--extra`:

```bash
python tools/bml_test.py run --all-builtin --extra BML_IntegrationTest
```

### Backup and Restore

`run` backs up and restores the entire `ModLoader/Mods/` directory plus `BML.dll` and `ModLoader.dll`. The backup lives in `artifacts/<subcommand>-<timestamp>/backup/`.

`smoke` and `analyze` only deploy `BML.dll` + `ModLoader.dll` to the game directory. Modules are placed in temporary directories and loaded via the `BML_MODS_DIR` environment variable.

### Concurrency Safety

A lock file at `<game_dir>/.bml-test.lock` prevents concurrent runs. Stale locks (dead PID) are automatically cleaned up.

## Manual Verification

Scripted passes are not enough for UI-heavy modules. After deploying, manually verify:

- `/` opens the console
- Mods option opens ModMenu
- Custom map loading works
- Pause/reset/exit behave correctly

Recommended flow:

1. `python tools/bml_test.py run --all-builtin --no-launch` to deploy
2. Launch the game manually from the game directory
3. Test interactively
4. Close the game
5. Re-run `python tools/bml_test.py run --all-builtin --no-launch` to re-deploy (or use `--no-restore` on step 1 to keep the deployment)

## Rule of Thumb

| Question | Subcommand |
|----------|------------|
| Does this module set load without crashing? | `smoke` |
| Does this specific change behave correctly in-game? | `run` |
| Are all builtin modules healthy before release? | `analyze` |
| What dependencies does this module pull in? | `resolve` |
