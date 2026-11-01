# Player acceptance tests

These tests start the real Ballance Player. Run only the scenario affected by a
change; unit tests and headless UI checks do not verify Virtools behavior.

Build the loader and the targets named by the selected runner, then pass the
loader DLL to that runner. For example:

```powershell
cmake --build build-dev --config RelWithDebInfo --target BML PlayerFlowDriver ExecuteBBTest
powershell -ExecutionPolicy Bypass -File tests/player/Invoke-ExecuteBBTest.ps1 `
  -BallanceRoot "<Ballance-root>" `
  -BuildDll "build-dev/bin/RelWithDebInfo/BMLPlus.dll"
```

Other focused runners are `Invoke-BehaviorAcceptanceTest.ps1`,
`Invoke-BehaviorDeletionTest.ps1`,
`Invoke-GameplayRouteTest.ps1`, `Invoke-InterfaceProviderTest.ps1`,
`Invoke-NewBallTypeTest.ps1`, `Invoke-NativeModProfilesTest.ps1`,
`Invoke-NoExitModTest.ps1`,
`Invoke-PublicAuthoringTest.ps1`, and `Invoke-ScriptImcInteropTest.ps1`. Read
each runner's required build targets before invoking it. `GameplayRouteTest`
is not part of the 0.3.14 release scope and reports SKIPPED while its authored
route is under review; a skipped result is never release evidence.

The full Behavior acceptance runner requires a dedicated instrumented build
configured with `-DBML_ENABLE_BEHAVIOR_TEST_INTERFACE=ON`. This private bridge
is disabled by default and must not be used for release packages.

`Invoke-BehaviorDeletionTest.ps1` needs only `PlayerFlowDriver` and
`BehaviorDeletionTest`. It accepts an ordinary release loader with or without
AngelScript; no private bridge or fixture DLL is needed. The probe exercises
mixed deferred deletion through the shipped BGui and Behavior interfaces,
including Instance cleanup, nested graphs, root and owner deletion, deletion of
an edited dynamic graph, orphan Links with surviving source IOs, journal-owned
value snapshots, parked Nodes and their incident Links, replacement originals,
and copied port and Link relations. It checks retained CK objects, not just
stale interface handles, then returns through the
game's Exit Level message before exiting. The full Behavior acceptance runner
also includes this probe.

`PlayerFlowDriver` drives the retail menu and level flow. Each installed probe
Mod reports one result through `BMLPlayerProbeRead`; probes that need gameplay
input also expose `BMLPlayerProbeStart`. The driver does not decide which probes
to run. `BMLPlayerHarness.psm1` backs up and restores touched game files and
logs, starts Player, and collects the probe results. Close an existing Player
process before a runner replaces its DLL.

For visible ImGui journeys, use the separate [UI automation guide](../ui/README.md).

`Invoke-NativeZipResourcesTest.ps1` checks the package contract with ordinary
native and script ZIPs. Configure with `-DBML_ENABLE_ANGELSCRIPT=ON`, build `BML`,
`PlayerFlowDriver`, and `NativeZipResourcesTest`, then pass the loader DLL with
`-BuildDll`. The native Mod reads its bundled file using `BML_GetModRootW`; the
script Mod uses `ReadModTextFileUtf8`. Both resources must remain readable after
startup, and the native resource must still be readable during unload. This
test does not inspect loader state or game Behavior graphs.

Add `-Scenario mixed-reload` to put both Mods in one ZIP and reload the script
twice through the public `script reload` command. `-Scenario wrapped-reload`
places the script in a subdirectory. In both layouts the native and script
resources must stay readable across both reloads and through native unload.
The engine must also keep resolving native data from the native Mod's own root,
even when the script moves away from their initially shared search paths.
`-Scenario nested` places different `Shared.zip` archives in two outer ZIPs;
neither may overwrite the other's native Mod or resource.

Build `ScriptZipSearchPathsTest` for `-Scenario search-reload` and
`-Scenario search-rollback`. These update an actual script ZIP, then resolve
new data, bitmap and sound files through the public CKPathManager API, including
inside the new script's OnLoad callback. A rejected OnLoad must restore the old
resources, and neither case may leak search-path entries. The lifecycle signal
uses a normal public command registration.
Each scenario repeats three times. Add `-ResourceLayout Add` or `Remove` to
introduce or remove the data, texture and sound subdirectories during reload.
Counts must change by the exact number of directories, and rollback must
restore the original layout and counts on every attempt.

Pass `-ScriptSupport Disabled` with a loader built using
`-DBML_ENABLE_ANGELSCRIPT=OFF` to check native startup and unload for `separate`,
`mixed-reload`, `wrapped-reload`, and `nested`. Mixed packages still contain
their scripts, but no reload is requested: the public Mod lookup must find no
script and the public `script status` command must report failure. Reload
scenarios require enabled support.

`-Scenario named` checks spaces in archive names and uppercase `.ZIP`
extensions. `-Scenario malformed` includes an invalid archive beside the valid
packages; its extraction must fail without hiding the valid Mods or creating a
failed Script Mod. Both scenarios support either script build mode.
