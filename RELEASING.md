# Releasing the SDL branch

This runbook describes BML+ 0.4.0 and the tools present in this checkout.
The runtime is a Ballanced SDL **dynamic** host. Original-Player releases use their own branch;
static-package updater tests do not provide a working static BML Player.

The [build workflow](.github/workflows/build.yml) currently runs standalone SDK,
CLI and UI framework contracts on Windows, Linux and macOS. It does not build
or publish a complete seven-target runtime/SDK matrix.
The [docs workflow](.github/workflows/docs.yml)
strictly builds both languages and uploads a site artifact; it reserves `/updates`
for updater metadata. Neither workflow publishes release packages or deploys Pages.

## Freeze the source and acceptance record

- Use one reviewed source revision and matching Ballanced/CKAngelScript baselines
  and submodules. Record compiler, configuration, platform, architecture, linkage,
  script option, engine/ImGui identities and owned dependencies.
- Match the project version in `CMakeLists.txt`, package metadata and intended
  release tag. Never move a published tag or replace published versioned assets.
- Build in a clean directory for each target. Run script-on and script-off tests,
  installed/relocated SDK consumers, in-tree Mod builds and the real Player probe.
  Cross-compilation is a build result, not a native gameplay pass.
- Record full level/reset/death/map/input/UI/per-Mod acceptance separately from
  probes. Use the [validation record](docs/en/validation.md) as a dated local
  record, not evidence that every release target is accepted.

## Native C protocol release gate

The SDL Mod entry is `BML_GetModInterface` (`bml.mod`, major 1). Sidecars use schema
2. Old SDL `BMLEntry`/`IMod` binaries must be migrated and rebuilt; they are not an
alternate accepted entry or retained private backend.

Preserve released C table prefixes, calling conventions, record semantics and
ownership. C++ wrappers stay inside the author module. SDK content hashes are
diagnostic; runtime/platform/architecture/linkage, Mod protocol and opted-in
engine/ImGui identities are compatibility gates. A direct extension must also
match its dependency compiler/standard-library/runtime/configuration ABI.

Build installed C11 and C++ convenience examples and Native/Interface/IMC/Script
workflow profiles. Verify that modules reuse Player's SDL and staging preserves
module bytes and signatures. Check unload failure, retained callbacks, invalid
handles, world reset, duplicate IDs and incorrect/missing ABI sidecars.

## Stage runtime, SDK and Mods separately

Build with the selected configuration and `BML_BUILD_MODS=ON`; build updater tools
with `BML_BUILD_UPDATER=ON` and the declared libcurl dependency when they are part
of the delivery. The production signing public key must be the intended key.
Test executables with test keys are not production tools.

```sh
cmake --build build/sdl-release --config Release --parallel
cmake --install build/sdl-release --config Release --prefix /path/to/sdk
cmake --install build/sdl-release --config Release \
  --prefix /path/to/disposable-game --component BMLRuntime
cmake --install build/sdl-release --config Release \
  --prefix /path/to/disposable-game --component BMLMods
```

The runtime prefix is an isolated copy of the **matching complete Player**, with
host SDL available for macOS fixup. Keep CK2/VxMath/SDL supplied by Player. The
component manifest selects BML-owned files from that tree; do not make a dynamic
BML update out of Player, engine libraries or user files.

Keep runtime, SDK and Mods archives distinct. Test SDK relocation; preserve
POSIX executable modes, Linux load paths and macOS install names/signatures.
Perform distribution signing after build fixups. Launch a fresh unpacked runtime
on each accepted target. Do not ship user configuration as package defaults.

## Sign and construct the target channel

The publisher uses `scripts/package_sdl_update.py` and the separate native
`BMLUpdateSigner`. There are no PowerShell release/sign/channel-copy helpers on
this branch. Keep the private key outside the repository and ordinary CI logs.
The signer accepts unencrypted P-256 SEC1/PKCS#8 PEM or DER keys.

```sh
python scripts/package_sdl_update.py \
  --stage /path/to/disposable-game \
  --install-manifest build/sdl-release/install_manifest_BMLRuntime.txt \
  --output /path/to/review-output --version 0.4.0 \
  --signing-key /secure/p256-key.pem --signer /path/to/BMLUpdateSigner \
  --base-url https://example.invalid/bml --channel beta
```

Replace the sample version, paths, key and URL. The stage supplies its own target
from `ModLoader/BML.abi.json`; do not edit it to impersonate another target.
Review the ZIP, exact JSON manifest bytes, SHA-256/size/mode inventory and detached
signatures. The format is ECDSA P-256/SHA-256, base64 of raw 64-byte `r || s`.
The runtime and publisher public key must agree.

The channel lives under `sdl/<platform>/<architecture>/<host_linkage>/` as
`stable.json` or `beta.json`, with its detached signature. Review the generated
output tree rather than inventing names. Preserve prior revocation arrays and
signed installed-file ownership. `--force` may overwrite local review output;
it does not make replacing a published release safe.

## Publish only the reviewed artifacts

1. A maintainer reviews the tag, release notes, signatures, target descriptors and
   complete asset set. State unverified targets explicitly.
2. Publish the immutable versioned assets and verify their intended HTTPS URLs.
3. Update only the corresponding signed target channel and signature. Preserve
   unrelated Pages content and the `/updates` namespace. Do not force-push or
   discard a concurrent Pages change.
4. From an isolated game copy run the production updater's doctor/check/plan,
   then update. Verify file hashes, preserved Mods/configuration, executable
   modes, restart and game acceptance. Exercise signature/target rejection,
   interrupted recovery and rollback before declaring the channel accepted.

Documentation/build checks produce review artifacts. Publishing versioned assets
and switching signed channels are separate maintainer release steps.

## Failure and recovery

Before publication, stop and correct any failed check; a partial signed set must
not become a channel. After publication, fix a bad release with a new version
rather than changing existing assets or tags. A bad channel can be reverted to
previous valid signed metadata. Keep transaction journals/backups during recovery;
never “repair” verification by publishing unsigned metadata or disabling TLS.

See [Updater and doctor](docs/en/updater-sdl.md) for diagnostic/apply/rollback
commands and the distinction between implemented static-package routing and
unimplemented static Player integration.
