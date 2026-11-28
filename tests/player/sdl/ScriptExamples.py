"""Stage the repository's complete script examples for real-Player compilation."""
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[3]


def stage_examples(mods, tutorials=False):
    identities = []
    sources = list((ROOT / "examples/script-mod").iterdir())
    sources += [ROOT / "templates/script-mod-template"]
    sources += [ROOT / "tests/smoke/AngelScript" / name for name in (
        "BMLAngelScriptSmoke", "BMLAngelScriptSingleFileSmoke", "BMLAngelScriptZipSmoke")]
    for source in sources:
        if not source.is_dir():
            continue
        target = mods / source.name
        if source.name == "BMLAngelScriptZipSmoke":
            with zipfile.ZipFile(target.with_suffix(".zip"), "w", zipfile.ZIP_DEFLATED) as archive:
                for path in source.rglob("*"):
                    if path.is_file():
                        archive.write(path, path.relative_to(source))
        else:
            shutil.copytree(source, target, symlinks=False)
        entries = [p for p in source.rglob("*.as") if "[bml.mod" in p.read_text(encoding="utf-8")]
        if len(entries) != 1:
            raise RuntimeError(f"Expected one Mod definition: {source}")
        identities.append(re.search(r'\[bml\.mod\s+id="([^"]+)"', entries[0].read_text(encoding="utf-8"))[1])
    if tutorials:
        course = ROOT / "docs/zh-CN/script-mod-tutorial"
        for source in sorted(course.glob("*.md")):
            for index, block in enumerate(re.findall(r"```angelscript\n(.*?)```", source.read_text(), re.S)):
                # Exclude metadata-only or abbreviated publishing illustrations.
                if "[bml.mod" not in block or "class " not in block or len(block.splitlines()) < 5:
                    continue
                identity = f"docs.api.{source.stem}.{index}"
                block = re.sub(r'(\[bml\.mod\s+id=")[^"]+', r'\g<1>' + identity, block, count=1)
                (mods / f"{identity}.mod.as").write_text(block)
                identities.append(identity)
        complete = course / "assets/CompleteMod.mod.as"
        shutil.copy2(complete, mods / complete.name)
        identities.append("complete.mod")
    # Validate reload candidate source variants without executing their
    # intentional exit/throw paths. Runtime rollback is covered separately.
    smoke = ROOT / "tests/smoke/AngelScript"
    variants = [smoke / "BMLAngelScriptStateReloadSmoke" / name for name in (
        "runtime.as", "runtime.v2.txt", "runtime.restore-fail.txt", "runtime.migrate-fail.txt")]
    variants.append(smoke / "BMLAngelScriptCompileErrorSmoke/runtime.recovery.txt")
    for index, source in enumerate(variants):
        identity = f"examples.reload.compile.{index}"
        text = re.sub(r'(\[bml\.mod\s+id=")[^"]+', r'\g<1>' + identity, source.read_text(), count=1)
        text = re.sub(r'\b(OnLoad|OnUnload|OnProcess|SaveState|MigrateState|RestoreState)\b',
                      r'CompileOnly\1', text)
        (mods / f"{identity}.mod.as").write_text(text)
        identities.append(identity)
    return identities
