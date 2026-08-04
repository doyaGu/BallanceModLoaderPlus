# Bilingual Documentation Release Process

This document records how the English and Simplified Chinese sites are built
and published. It is a maintenance note and must not be included in either
public MkDocs navigation.

## Published Site

The public site is:

```text
https://doyagu.github.io/BallanceModLoaderPlus/
```

The MkDocs output is deployed to the root of the `gh-pages` branch. The
`updates/` directory on `gh-pages` is reserved for updater channel metadata and
must be preserved during documentation deployment.

## Source Files

Public documentation source is separated by language:

```text
docs/en/
docs/zh-CN/
docs/hooks/pygments_angelscript.py
mkdocs.yml
mkdocs.zh-CN.yml
requirements-docs.txt
.github/workflows/docs.yml
```

Repository maintenance notes live under `docs/internal/`. They are tracked for
maintainers but are not inputs to either MkDocs site or the SDK package.

`docs/en/` contains English prose only. `docs/zh-CN/` contains Simplified
Chinese prose only. Identifiers, code, file names, and quoted diagnostics keep
their original language. A page that is unavailable in one language must be
linked as an explicitly labelled external-language resource; do not place the
other language's prose inside the local documentation tree.

Do not publish draft or private material:

```text
docs/script-mod-tutorial/archive/
docs/script-mod-tutorial/libs/
docs/script-mod-tutorial-materials/
docs/script-mod-tutorial-outline.md
docs/script-mod-tutorial-research.md
docs/script-mod-tutorial-gap-audit.md
docs/superpowers/
docs/bml-script-api-redesign-plan.md
```

Do not add a standalone tutorial examples directory. Tutorial examples should
be inline in the relevant chapter or omitted.

## Local Build

Install documentation dependencies:

```powershell
python -m pip install -r requirements-docs.txt
```

Build both sites strictly, in this order:

```powershell
mkdocs build --strict --config-file mkdocs.yml
mkdocs build --strict --config-file mkdocs.zh-CN.yml
```

The English build owns `site/` and cleans it first. The Chinese build writes
only `site/zh-CN/`, so reversing the order would delete the Chinese output.

Preview one language at a time:

```powershell
mkdocs serve --config-file mkdocs.yml -a 127.0.0.1:8000
mkdocs serve --config-file mkdocs.zh-CN.yml -a 127.0.0.1:8001
```

## Pre-Commit Checks

Scan source files for local paths, personal names, private key names, and old
draft references. Keep the sensitive words in variables so this process note
does not itself contain the exact strings being searched:

```powershell
$localUserPath = 'C:' + '\\Users\\'
$programFilesPath = 'C:' + '\\Program Files'
$gamePath = 'Games' + '\\Ballance'
$workPathA = 'Works' + '\\Ballance'
$workPathB = 'Works' + '\\Virtools'
$releaseWorktree = 'BallanceModLoaderPlus' + '-release'
$ckasStaging = 'build-ci-' + 'ckas-release'
$productionKeyPrefix = 'BMLPlus-Updater-' + 'Production'
$validationAuthorA = 'Game' + 'piaynmo'
$validationAuthorB = 'Ying' + 'Che'
$validationAuthorC = 'Ka' + 'kuty'
$oldDraftText = '旧稿' + '和素材'
$maintenanceText = '维护' + '资料'

$patterns = @(
  $localUserPath,
  $programFilesPath,
  $gamePath,
  $workPathA,
  $workPathB,
  $releaseWorktree,
  $ckasStaging,
  $productionKeyPrefix,
  $validationAuthorA,
  $validationAuthorB,
  $validationAuthorC,
  $oldDraftText,
  $maintenanceText
) -join '|'

rg --no-ignore -n $patterns docs .github mkdocs.yml mkdocs.zh-CN.yml requirements-docs.txt .gitignore
```

Scan the generated site:

```powershell
$internalDocPatterns = @(
  'internal/scripting/validation',
  'internal/scripting/facade-coverage',
  'bml-script-api-redesign-plan',
  'internal/release/packaging-process',
  'internal/release/updater-design',
  'script-mod-tutorial-materials'
) -join '|'

$removedExamplePattern = 'script-mod-tutorial' + '/examples'
$sitePatterns = $patterns + '|' + $internalDocPatterns + '|' + $removedExamplePattern
$matches = Get-ChildItem -LiteralPath site -Recurse -File -Include *.html,*.xml,*.json,*.as | Select-String -Pattern $sitePatterns -List
if ($matches) { $matches | Select-Object Path,LineNumber,Line } else { 'site-clean' }
```

Check staged filenames before committing:

```powershell
git diff --cached --name-only
```

The staged set must not include ignored draft directories, `site/`,
`__pycache__/`, or dependency directories.

## Normal Deployment

Commit documentation source to `main` and push:

```powershell
git push origin main
```

The `Docs` GitHub Actions workflow then:

1. Installs MkDocs dependencies.
2. Builds the English site with `mkdocs.yml`.
3. Builds the Simplified Chinese site with `mkdocs.zh-CN.yml` into
   `site/zh-CN/`.
4. Checks out `gh-pages`.
5. Deletes the old Pages root contents except `.git`, `.nojekyll`, and
   `updates/`.
6. Copies `site/` into the `gh-pages` root.
7. Commits and pushes only when generated content changed.

## Manual Pages Deployment

Use this only when the public site must be updated before Actions finishes.

Build first:

```powershell
mkdocs build --strict --config-file mkdocs.yml
mkdocs build --strict --config-file mkdocs.zh-CN.yml
```

Then deploy from a temporary `gh-pages` worktree. Preserve `.git`, `.nojekyll`,
and `updates/`, delete the other root entries, copy `site/` into the worktree,
commit, and push `HEAD:gh-pages`.

Before pushing, verify that the worktree does not contain:

```text
old subpath deployment directory
standalone tutorial examples directory
internal maintenance pages
old draft or material directories
```

## Post-Deployment Verification

Check the public site:

```powershell
Invoke-WebRequest -UseBasicParsing "https://doyagu.github.io/BallanceModLoaderPlus/"
Invoke-WebRequest -UseBasicParsing "https://doyagu.github.io/BallanceModLoaderPlus/imc/"
Invoke-WebRequest -UseBasicParsing "https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/"
Invoke-WebRequest -UseBasicParsing "https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/tutorial-01-setup/"
Invoke-WebRequest -UseBasicParsing "https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/api/"
```

Removed paths should return 404. Use placeholders instead of recording removed
URLs in this file:

```powershell
Invoke-WebRequest -UseBasicParsing "<removed-subpath-deployment-url>"
Invoke-WebRequest -UseBasicParsing "<removed-tutorial-example-url>"
```

Also verify raw `main` when source visibility matters:

```powershell
Invoke-WebRequest -UseBasicParsing "https://raw.githubusercontent.com/doyaGu/BallanceModLoaderPlus/main/docs/zh-CN/script-mod-tutorial/tutorial-01-setup.md"
```

## Rules

- Keep the tutorial source in `main` synchronized with the published site.
- Keep English and Simplified Chinese prose in their respective source trees.
- Build English before Simplified Chinese so `site/zh-CN/` is preserved.
- Keep old drafts and private materials out of both MkDocs output and staged
  commits.
- Keep `updates/` on `gh-pages`.
- Do not deploy to a subdirectory; the documentation site lives at the Pages
  root.
- Do not expose local absolute paths, local machine names, key names, or
  validation-only author strings.
