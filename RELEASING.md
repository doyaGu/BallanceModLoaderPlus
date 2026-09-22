# Releasing BML+

This runbook publishes an official BML+ version. GitHub Actions builds and tests
four ZIP files plus one updater manifest. A maintainer then signs that manifest
and `stable.json` on the release machine. The production private key never
enters GitHub Actions.

## Release rules

- `CMakeLists.txt` is the only source of the release version.
- A release tag is immutable. Never move or force-push a published tag.
- Build once: the ZIP files tested by CI are the ZIP files that get published.
- The signing command may create two signatures, `stable.json`, and
  `SHA256SUMS.txt`, but must not repackage a CI ZIP.
- GitHub Actions must not create or publish the GitHub release. The maintainer
  account creates the draft after all nine signed release files are ready.
- Publish the GitHub release before switching the updater channel to it.
- Update only `gh-pages:/updates/`; never rebuild or force-push the whole Pages
  branch while publishing a BML+ version.
- If channel validation fails, revert the channel commit. Do not move the tag or
  silently replace files in a published release.

Tags such as `v0.3.14-alpha.1` produce the same five unsigned CI artifacts for
candidate testing. They are not accepted by the signing or `stable.json`
publication scripts. The steps below apply to the final `vX.Y.Z` tag.

## 1. Prepare the release commit

1. Set `project(... VERSION X.Y.Z ...)` in `CMakeLists.txt`.
2. Build and test the intended release commit locally.
3. Ensure the worktree is clean and the commit is on `origin/main`.
4. Create an annotated `vX.Y.Z` tag at that commit and push it once.

```powershell
$version = 'vX.Y.Z'
$commit = git rev-parse HEAD
git tag -a $version $commit -m "BML+ $version"
git push origin "refs/tags/$version"
```

The tag starts `.github/workflows/build.yml`. CI validates the tag against the
CMake version, builds MSVC x86 Debug and Release with CKAngelScript enabled,
runs CTest, validates both packaged SDKs, and retains five unsigned files in
the tag's Build run. It does not create a GitHub release.

## 2. Download the five files from the Build run

The completed tag run contains exactly these five unsigned files:

```text
BMLPlus-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.manifest.json
BMLPlus-SDK-vX.Y.Z-Release.zip
BMLPlus-SDK-vX.Y.Z-Debug.zip
```

Resolve the Build run by the tag commit, require a successful result, and
download the named file collection without changing names or contents:

```powershell
$unsigned = Join-Path $env:TEMP "BMLPlus-$version-unsigned"
New-Item -ItemType Directory -Path $unsigned | Out-Null
$runs = @(
  gh run list `
    --repo doyaGu/BallanceModLoaderPlus `
    --workflow Build `
    --commit $commit `
    --event push `
    --limit 20 `
    --json databaseId,headBranch,status,conclusion |
    ConvertFrom-Json |
    Where-Object { $_.headBranch -ceq $version }
)
if ($runs.Count -ne 1) {
  throw "Expected one Build run for $version at $commit, found $($runs.Count)."
}
$run = $runs[0]
gh run watch $run.databaseId `
  --repo doyaGu/BallanceModLoaderPlus `
  --exit-status
gh run download $run.databaseId `
  --repo doyaGu/BallanceModLoaderPlus `
  --name "BMLPlus-$version-Unsigned-Release-Files" `
  --dir $unsigned
```

## 3. Sign the updater manifest and stable.json

Run this on the release machine. `OutputDir` must be new or empty so the CI
inputs cannot be modified in place.

Use the `stable.json` and `stable.json.sig` retained from the previous release.
Pass the JSON path as `PreviousChannelPath`; the signing command reads the
adjacent `.sig` file and verifies it with `SigningCngKeyName` before copying the
revocation lists. Use `PreviousChannelSignaturePath` only when the signature is
stored elsewhere.

```powershell
$signed = Join-Path $env:TEMP "BMLPlus-$version-signed"
$previousChannel = '<path-to-the-previous-verified-stable.json>'
.\scripts\Sign-BMLReleaseFiles.ps1 `
  -Version $version `
  -InputDir $unsigned `
  -OutputDir $signed `
  -SigningCngKeyName '<production-signing-key-name>' `
  -PreviousChannelPath $previousChannel
```

The script requires the exact five-file CI list, verifies every updater ZIP
entry against the unsigned manifest, preserves every ZIP byte-for-byte, signs
the manifest, carries the previous channel's revocation lists forward, creates
and signs `stable.json`, and writes `SHA256SUMS.txt`. Use
`-AllowEmptyRevocations` instead of `-PreviousChannelPath` only when creating
the first updater channel.

The signed directory must contain exactly nine files:

```text
BMLPlus-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.manifest.json
BMLPlus-Update-vX.Y.Z.manifest.json.sig
BMLPlus-SDK-vX.Y.Z-Release.zip
BMLPlus-SDK-vX.Y.Z-Debug.zip
stable.json
stable.json.sig
SHA256SUMS.txt
```

## 4. Create the draft release with the maintainer account

Confirm that GitHub CLI is authenticated as a human maintainer, then create the
draft with the nine signed files and the reviewed release notes. A bot-created
draft remains bot-authored after a maintainer publishes it, so do not use one.

```powershell
$publisher = gh api user --jq '.login'
if (-not $publisher -or $publisher.EndsWith('[bot]')) {
  throw "Refusing to create a release with bot account '$publisher'."
}
$releaseNotes = '<path-to-reviewed-release-notes.md>'
$releaseFiles = Get-ChildItem -LiteralPath $signed -File |
  Sort-Object Name |
  ForEach-Object { $_.FullName }
gh release create $version @releaseFiles `
  --repo doyaGu/BallanceModLoaderPlus `
  --draft `
  --verify-tag `
  --title $version `
  --notes-file $releaseNotes
$release = gh release view $version `
  --repo doyaGu/BallanceModLoaderPlus `
  --json author,isDraft,assets |
  ConvertFrom-Json
if ($release.author.login -cne $publisher -or -not $release.isDraft) {
  throw 'Draft release author or state is incorrect.'
}
if ($release.assets.Count -ne 9) {
  throw "Expected nine release files, found $($release.assets.Count)."
}
gh release view $version `
  --repo doyaGu/BallanceModLoaderPlus `
  --web
```

Publish the draft only after the tag, notes, and all nine files have been
reviewed:

```powershell
gh release edit $version `
  --draft=false `
  --latest `
  --repo doyaGu/BallanceModLoaderPlus
```

Verify that the public versioned package and manifest URLs work before changing
the updater channel.

## 5. Copy stable.json and its signature to gh-pages

Use a clean checkout of the current `gh-pages` branch. The copy script
refuses a dirty checkout and modifies only `updates/index.html`,
`updates/stable.json`, and `updates/stable.json.sig`.

```powershell
$pages = Join-Path $env:TEMP "BMLPlus-$version-gh-pages"
git fetch origin gh-pages
git worktree add --detach $pages origin/gh-pages

.\scripts\Copy-BMLUpdaterFilesToPages.ps1 `
  -Version $version `
  -ReleaseDir $signed `
  -GhPagesCheckout $pages `
  -ExpectedBaseRef origin/gh-pages

git -C $pages status --short
git -C $pages add updates
git -C $pages diff --cached --check
git -C $pages diff --cached --stat
git -C $pages commit -m "Publish updater channel $version"
git -C $pages push origin HEAD:gh-pages
git worktree remove $pages
```

Do not use `--force`. If another Pages deployment wins the race, remove the
temporary worktree, fetch the new branch, and copy the three `/updates` files again.

## 6. Validate the public path

The following URLs must return HTTP 200:

```text
https://doyagu.github.io/BallanceModLoaderPlus/updates/
https://doyagu.github.io/BallanceModLoaderPlus/updates/stable.json
https://doyagu.github.io/BallanceModLoaderPlus/updates/stable.json.sig
```

From a disposable validation game install, run:

```powershell
Bin\Updater.exe source set `
  'https://doyagu.github.io/BallanceModLoaderPlus/updates' `
  --channel stable
Bin\Updater.exe check
Bin\Updater.exe update --force
Bin\Updater.exe status
```

Require `channel signature verified`, `OK: remote channel checked`, a completed
apply, and hashes matching `ModLoader/Updater/installed.manifest.json`.

## Rollback

- Before the channel switch: leave the maintainer-created release as a draft
  or delete the draft without deleting the tag.
- After the release is public but before the channel switch: fix forward with a
  new version; do not reuse the published version.
- After the channel switch: revert the `gh-pages` channel commit to the previous
  signed `stable.json`, then investigate. Never publish an unsigned `stable.json`.
