#!/usr/bin/env python3
"""Fast behavior tests for the project-local Python Mod workflow."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


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
        config = sdk / "lib/cmake/BML/BMLConfig.cmake"
        config.parent.mkdir(parents=True)
        config.write_text("# Test SDK marker.\n", encoding="utf-8")
        tool = scripts / "bml.py"

        beginner_help = run(sys.executable, str(tool), "help").stdout
        assert "bml new" in beginner_help
        assert "bml run" in beginner_help
        assert "interface-provider" not in beginner_help
        assert "bml init" not in beginner_help

        advanced_help = run(sys.executable, str(tool), "help", "--verbose").stdout
        assert "bml init" in advanced_help
        assert "interface-provider" in advanced_help

        generated = Path(temporary) / "generated"
        run(
            sys.executable,
            str(tool),
            "new",
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

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
