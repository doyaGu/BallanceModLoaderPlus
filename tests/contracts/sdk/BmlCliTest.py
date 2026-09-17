#!/usr/bin/env python3
"""Fast behavior tests for the project-local Python Mod workflow."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def run(*arguments: str, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(arguments, capture_output=True, text=True, check=False)
    if expect_success and result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    if not expect_success and result.returncode == 0:
        raise AssertionError(f"Command unexpectedly succeeded: {arguments}")
    return result


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    source_root = args.source_root.resolve()

    with tempfile.TemporaryDirectory(prefix="bml-cli-test-") as temporary:
        sdk = Path(temporary) / "sdk"
        scripts = sdk / "scripts"
        scripts.mkdir(parents=True)
        shutil.copy2(source_root / "scripts/bml.py", scripts / "bml.py")
        shutil.copy2(source_root / "scripts/bml.cmd", scripts / "bml.cmd")
        shutil.copytree(
            source_root / "templates/native-mod-template",
            sdk / "templates/native-mod-template",
        )
        shutil.copytree(
            source_root / "templates/script-mod-template",
            sdk / "templates/script-mod-template",
        )
        config = sdk / "lib/cmake/BML/BMLConfig.cmake"
        config.parent.mkdir(parents=True)
        config.write_text("# Test SDK marker.\n", encoding="utf-8")
        tool = scripts / "bml.py"

        beginner_help = run(sys.executable, str(tool), "help").stdout
        assert "bml new script" in beginner_help
        assert "bml new native" in beginner_help
        assert "bml run" in beginner_help
        assert "bml pack" in beginner_help
        assert "interface-provider" not in beginner_help
        assert "bml init" not in beginner_help

        advanced_help = run(sys.executable, str(tool), "help", "--verbose").stdout
        assert "bml init" in advanced_help
        assert "interface-provider" in advanced_help

        removed_syntax = run(
            sys.executable,
            str(tool),
            "new",
            "test.unspecified-kind",
            expect_success=False,
        )
        assert "invalid choice" in removed_syntax.stderr

        generated = Path(temporary) / "generated"
        run(
            sys.executable,
            str(tool),
            "new",
            "native",
            "test.first-mod",
            "--author",
            "CLI Test",
            "--destination",
            str(generated),
        )
        assert (generated / "src/FirstMod.cpp").is_file()
        assert (generated / "bml.py").is_file()
        assert (generated / "bml.cmd").is_file()
        assert not (generated / "bml.ps1").exists()
        native_manifest = json.loads((generated / "bml.mod.json").read_text(encoding="utf-8"))
        assert native_manifest["kind"] == "native"

        existing = Path(temporary) / "existing"
        source = existing / "src/ExistingTarget.cpp"
        source.parent.mkdir(parents=True)
        cmake = existing / "CMakeLists.txt"
        cmake.write_text(
            """cmake_minimum_required(VERSION 3.15)
project(ExistingTarget VERSION 3.2.1 LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(ExistingTarget src/ExistingTarget.cpp)
bml_install_mod(ExistingTarget)
""",
            encoding="utf-8",
        )
        source.write_text("// Existing source must stay byte-for-byte identical.\n", encoding="utf-8")
        before = (digest(cmake), digest(source))
        run(
            sys.executable,
            str(tool),
            "init",
            "test.existing",
            "--project",
            str(existing),
        )
        assert before == (digest(cmake), digest(source))
        manifest = (existing / "bml.mod.json").read_text(encoding="utf-8")
        assert '"target": "ExistingTarget"' in manifest
        assert '"version": "3.2.1"' in manifest
        assert '"kind": "native"' in manifest
        assert not (existing / "bml.ps1").exists()

        repeated = run(
            sys.executable,
            str(existing / "bml.py"),
            "init",
            "test.existing",
            "--project",
            str(existing),
            expect_success=False,
        )
        assert "already initialized" in repeated.stderr

        script_project = Path(temporary) / "script-project"
        run(
            sys.executable,
            str(tool),
            "new",
            "script",
            "test.script-mod",
            "--author",
            "CLI Test",
            "--destination",
            str(script_project),
        )
        script_entry = script_project / "ScriptMod.mod.as"
        assert script_entry.is_file()
        script_manifest = json.loads(
            (script_project / "bml.mod.json").read_text(encoding="utf-8")
        )
        assert script_manifest["kind"] == "script"
        assert script_manifest["entry"] == "ScriptMod.mod.as"
        assert "id" not in script_manifest

        script_helper = script_project / "helper.as"
        script_helper.write_text("void Helper() {}\n", encoding="utf-8")
        run(sys.executable, str(script_project / "bml.py"), "build")
        staged_root = script_project / ".bml/stage/Mods/ScriptMod"
        staged_entry = staged_root / "ScriptMod.mod.as"
        assert staged_entry.is_file()
        assert (staged_root / "helper.as").is_file()
        unmanaged = staged_root / "runtime-created.txt"
        unmanaged.write_text("keep\n", encoding="utf-8")
        script_helper.write_text("void Helper(int value) {}\n", encoding="utf-8")
        run(sys.executable, str(script_project / "bml.py"), "build")
        assert (staged_root / "helper.as").read_text(encoding="utf-8") == (
            "void Helper(int value) {}\n"
        )
        script_helper.unlink()
        run(sys.executable, str(script_project / "bml.py"), "build")
        assert not (staged_root / "helper.as").exists()
        assert unmanaged.is_file()

        (script_project / ".vscode").mkdir()
        (script_project / ".vscode/settings.json").write_text("{}\n", encoding="utf-8")
        (script_project / "as.predefined").write_text("editor only\n", encoding="utf-8")
        run(sys.executable, str(script_project / "bml.py"), "pack")
        script_package = script_project / "dist/ScriptMod.zip"
        with zipfile.ZipFile(script_package) as archive:
            entries = set(archive.namelist())
        assert "ScriptMod.mod.as" in entries
        assert "README.md" in entries
        assert "bml.py" not in entries
        assert "bml.cmd" not in entries
        assert "bml.mod.json" not in entries
        assert "as.predefined" not in entries
        assert ".vscode/settings.json" not in entries
        existing_package = run(
            sys.executable,
            str(script_project / "bml.py"),
            "pack",
            expect_success=False,
        )
        assert "Pass --force" in existing_package.stderr
        run(sys.executable, str(script_project / "bml.py"), "pack", "--force")

        existing_script = Path(temporary) / "existing-script"
        existing_script.mkdir()
        existing_entry = existing_script / "OldScript.mod.as"
        existing_entry.write_text(
            '[bml.mod id="test.old-script" name="Old Script" version="2.4.0"]\n'
            "class OldScript {}\n",
            encoding="utf-8",
        )
        helper = existing_script / "helper.as"
        helper.write_text("void ExistingHelper() {}\n", encoding="utf-8")
        before_script = (digest(existing_entry), digest(helper))
        run(
            sys.executable,
            str(tool),
            "init",
            "--project",
            str(existing_script),
        )
        assert before_script == (digest(existing_entry), digest(helper))
        adopted_manifest = json.loads(
            (existing_script / "bml.mod.json").read_text(encoding="utf-8")
        )
        assert adopted_manifest["kind"] == "script"
        assert adopted_manifest["entry"] == "OldScript.mod.as"
        assert "id" not in adopted_manifest

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
