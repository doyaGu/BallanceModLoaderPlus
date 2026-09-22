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
`Invoke-GameplayRouteTest.ps1`, `Invoke-InterfaceProviderTest.ps1`,
`Invoke-NewBallTypeTest.ps1`, `Invoke-NativeModProfilesTest.ps1`,
`Invoke-PublicAuthoringTest.ps1`, and `Invoke-ScriptImcInteropTest.ps1`. Read
each runner's required build targets before invoking it. `GameplayRouteTest`
is not part of the 0.3.14 release scope and reports SKIPPED while its authored
route is under review; a skipped result is never release evidence.

`PlayerFlowDriver` drives the retail menu and level flow. Each installed probe
Mod reports one result through `BMLPlayerProbeRead`; probes that need gameplay
input also expose `BMLPlayerProbeStart`. The driver does not decide which probes
to run. `BMLPlayerHarness.psm1` backs up and restores touched game files and
logs, starts Player, and collects the probe results. Close an existing Player
process before a runner replaces its DLL.

For visible ImGui journeys, use the separate [UI automation guide](../ui/README.md).
