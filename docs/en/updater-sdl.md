# SDL updater and doctor

The SDL updater is a separate executable. It uses SDL filesystem APIs, libcurl
7.68+ for HTTPS, and native crypto on Windows/macOS; Linux uses the statically compiled
RHash SHA-256 and p256-m primitives. The original engine's schema 1 updater is outside this SDL branch.

From an SDL installation, run:

```sh
./Bin/Updater doctor --game-root "/path/to/game"
./Bin/Updater configure https://example.org/bml --game-root "/path/to/game"
./Bin/Updater check --game-root "/path/to/game"
./Bin/Updater plan --game-root "/path/to/game"
./Bin/Updater update --game-root "/path/to/game"
```

Windows uses `Bin\Updater.exe` with the same SDL commands. `--game-root` selects
the game root, with either `Bin/Player` or a flat `Player` beside `base.cmo`.
When omitted, an updater in `Bin` selects its parent directory. Paths and console
text use UTF-8. POSIX Player files must have executable permission.

`doctor` checks the installed ABI descriptor, Player and shared-library image
format/architecture, SDL version, script runtime when enabled, duplicate Loader
files, process state, state-directory write access and interrupted transactions.
It does not execute plugin factories or replace gameplay acceptance tests.
For a static host it checks Player and metadata; embedded plugin registration
must be verified in the real Player.

## Targets and signatures

SDL manifests and channels require `schemaVersion: 2` and this target object:

```json
{"runtime":"sdl","platform":"macos","architecture":"arm64","host_linkage":"dynamic"}
```

Platforms are `windows`, `linux`, `macos`; architectures are `x86`, `x64`,
`arm64`, with x86 restricted to Windows. A configured base URL is resolved as
`<base>/sdl/<platform>/<architecture>/<host_linkage>/stable.json` (or `beta.json`).
The signed channel, signed manifest, installed `ModLoader/BML.abi.json` and
payload descriptor must agree. Missing descriptors and mismatched targets fail
before application. SDK compiler/configuration compatibility remains part of
native Mod loading; an updater cannot migrate existing Mod binaries to a new ABI.

Dynamic packages use `packageType: "bml-runtime"` and contain the Loader,
optional CKAngelScript, Loader fonts, ABI metadata and owned runtime dependencies.
They cannot update Player, CK2, VxMath or SDL. Static packages use
`packageType: "player-runtime"` and replace a matching, complete Player runtime;
they cannot be applied to a dynamic host. Static package routing is implemented;
building and validating a real BML static Player remains a separate integration task.

Every managed entry records `path`, SHA-256, exact byte `size`, and `mode`
(`420` for 0644 or `493` for 0755). User Mods, configs (including Player.ini), updater state and the
Updater executable itself are excluded. Explicit removals require a matching
file in the previous signed installed manifest and an unchanged content hash.

Detached signatures retain the win32 format: base64 of 64 bytes `r || s`,
ECDSA P-256 over SHA-256 of the exact JSON bytes. TLS certificates and hostnames
are verified; redirects remain HTTPS. `CURL_CA_BUNDLE`, or `SSL_CERT_FILE`, can
select a CA file. These variables do not disable verification. Failed downloads
remove their temporary file; rerunning downloads the package again.

## Failure recovery

Close Player before applying or rolling back. Apply/rollback take an exclusive
process lock. The updater copies and verifies backups, then flushes a journal
and pending hint before changing any managed file. File replacement is atomic
and POSIX permissions are recorded and restored. Metadata and the exact signed
installed manifest/signature participate in the same rollback.

```sh
./Bin/Updater verify-local /path/to/BMLPlus-Update-....zip --game-root /path/to/game
./Bin/Updater apply-local /path/to/BMLPlus-Update-....zip --game-root /path/to/game
./Bin/Updater rollback --game-root /path/to/game
```

An apply failure attempts rollback. Failed cleanup keeps the journal and pending
hint. After a process interruption, new application is blocked until `rollback`
finishes. Rollback is repeatable after an interruption of rollback itself.
Do not delete transaction backups to clear an error; restore write permissions
or missing backup files first. A successful transaction keeps its backups for
manual rollback. The Updater executable is bootstrapped from a full manual package.

## Producing a signed package

Use a fresh stage or the component's install manifest to select owned files from
a staged game. Never scan an existing user installation as an update payload.
The install manifest also avoids packaging the host libraries supplied by Player.

```sh
cmake --build build --target BML Updater BMLUpdateSigner
cmake --install build --component BMLRuntime --prefix /path/to/staged-game
python3 scripts/package_sdl_update.py \
  --stage /path/to/staged-game \
  --install-manifest build/install_manifest_BMLRuntime.txt \
  --output /path/to/release --version v0.3.14 \
  --signing-key /private/path/p256-key.pem \
  --signer build/Bin/BMLUpdateSigner \
  --base-url https://example.org/bml/artifacts
```

The stage must already provide its matching host SDL library for macOS relocation.
The packager writes the ZIP, manifest and signature, and optionally a signed
channel under the target-specific directory. Existing channel revocation arrays
are preserved. Existing output requires `--force`. Publish the versioned artifacts
at `--base-url` and the target channel tree under the configured updater base URL.
The signing public key must match the key compiled into the production updater.
Test keys are compiled into a separate, uninstalled test executable.

Current validation covers macOS ARM64 builds, disposable runtime fixtures,
loopback HTTPS, signed packages, rejection, permissions and interrupted recovery.
Windows/Linux native runs, real static Player integration and real game update
acceptance remain pending.

`BMLUpdateSigner` accepts unencrypted P-256 SEC1 or PKCS#8 keys in PEM/DER.
It signs with OS entropy and emits the existing raw 64-byte signature; the
Python packager applies base64. The publisher helper is separate from Player
and the runtime Updater. Third-party notices ship in `ModLoader/Licenses`.
