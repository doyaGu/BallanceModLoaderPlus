# SDL tests

| Area | Scope |
| --- | --- |
| `tests/sdk` | Standalone SDK/CLI/ABI contracts and, in a full build, installed SDK consumers |
| `tests/ck2/sdl` | Real CK2, input and renderer component fixtures with SDL dummy video |
| `tests/platform` | SDL filesystem, module identity, ABI and script watching |
| `tests/updater/sdl` | doctor, signatures, HTTPS, transactions, rollback and recovery |
| `tests/player/sdl` | Installed-SDK Mod in disposable real Player runtimes |
| `tests/ui` | Optional headless ImGui, scenario catalog and automation Session checks |

The SDK contracts can run without engine development packages:

```sh
cmake -S tests/sdk -B build/sdk-tests
ctest --test-dir build/sdk-tests --output-on-failure
```

For the full Loader suite, configure with `-DBML_BUILD_TESTS=ON`, build, then run
`ctest --test-dir <build> -C Release --output-on-failure`. Use separate build
directories for `BML_ENABLE_ANGELSCRIPT=ON` and `OFF`. Optional UI checks additionally
require `-DBML_BUILD_UI_AUTOMATION=ON`; they are not real-game UI acceptance.

See [Player tests](player/README.md), [component tests](ck2/README.md),
[UI infrastructure](ui/README.md), and the [dated validation record](../docs/en/validation.md).
Native Player tests need a desktop and matching assets. Respect serial test
properties; do not start several Player or updater fixtures concurrently.
Unexecuted platforms remain unverified. Original Windows backend regressions
belong to its separate release branch.
