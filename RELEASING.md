# Releasing BML+

This checklist records the decisions and checks required for an official
release. It deliberately does not record a release version, command transcript,
or artifact inventory. Use `CMakeLists.txt` for the project version and the
[CI workflow](.github/workflows/build.yml),
[packaging script](scripts/Package-BMLRelease.ps1),
[signing script](scripts/Sign-BMLReleaseFiles.ps1), and
[updater channel script](scripts/Copy-BMLUpdaterFilesToPages.ps1) for the
current build and publication details.

## Release checklist

### 1. Freeze the source

- Confirm the intended commit is reviewed, tested locally, on the release
  branch, and has a clean worktree. The project version and release tag must
  agree.
- Create and push the tag once. Do not move or reuse a published tag.
- Prerelease tags can exercise CI, but the stable signing and channel scripts
  accept only final release tags.

Gate: the tag identifies the exact source commit to be released.

### 2. Verify the CI output

- Confirm the successful build run belongs to that tag and commit. Check that
  the build, tests, package validation, and SDK consumer checks passed.
- Obtain the complete CI output without changing names or contents. Use the
  workflow and packaging checks for the expected inventory, not this document.

Gate: the files to publish are the files built and validated by CI. Do not
rebuild or repackage them on the release machine.

### 3. Sign in the release environment

- Keep the production signing key outside CI. Sign into a separate, empty
  output directory so the unsigned CI output remains untouched.
- Verify the previous signed channel metadata and carry forward its revocation
  information. An empty revocation list is only appropriate when creating the
  first channel.
- Let the signing script validate the package, manifest, signatures, and
  resulting file set. Confirm the CI archives remain byte-for-byte unchanged.

Gate: the complete signed output is ready for review; no unsigned or partially
signed set may be published.

### 4. Publish the GitHub Release

- A human maintainer creates the draft from the signed output. Review its tag,
  author, release notes, and full asset set before making it public.
- Confirm the published assets and updater manifest are publicly accessible
  before changing the updater channel. CI must not create or publish the
  Release on the maintainer's behalf.

Gate: the public Release is complete and usable independently of the updater
channel.

### 5. Switch and verify the updater channel

- Start from a clean checkout of the current Pages branch. Use the channel
  script, inspect the diff, and publish only the updater channel files. Do not
  force-push or replace unrelated Pages content.
- If Pages advances before the channel commit is published, refresh the
  checkout and repeat the copy rather than overwriting the newer branch.
- From a disposable game installation, verify that the public channel and its
  signature can be fetched, the updater accepts them, the update completes,
  and the installed files match the expected hashes.

Gate: the channel points to a published, signed Release that has passed an
end-to-end update check.

## Failure and recovery

- Before the Release is public, stop on any failed check. Keep or discard the
  draft, correct the issue, and repeat the affected checks before proceeding.
- Once the Release is public, do not replace its assets or move its tag. Fix
  it with a new release instead.
- If the channel update fails, revert the channel change to the previous
  signed metadata and investigate. Never publish unsigned channel metadata.
