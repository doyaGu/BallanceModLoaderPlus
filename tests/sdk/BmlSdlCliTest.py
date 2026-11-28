#!/usr/bin/env python3
"""Workflow contracts that do not require an engine installation."""

import argparse
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile


parser = argparse.ArgumentParser()
parser.add_argument("--source-root", type=Path, required=True)
arguments, remaining = parser.parse_known_args()
source_root = arguments.source_root.resolve()
spec = importlib.util.spec_from_file_location("bml", source_root / "scripts/bml.py")
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class WorkflowTest(unittest.TestCase):
    def setUp(self):
        output = patch.object(sys, "stdout", io.StringIO())
        output.start()
        self.addCleanup(output.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix="bml-sdl-cli-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "中文项目 with spaces"
        self.root.mkdir()
        self.sdk = self.root / "sdk"
        self.config = self.sdk / "lib/cmake/BML/BMLConfig.cmake"
        self.config.parent.mkdir(parents=True)
        self.config.write_text('set(BML_RUNTIME "sdl")\n', encoding="utf-8")
        self.abi = {
            "schema": 1, "runtime": "sdl", "platform": "macos", "architecture": "arm64",
            "host_linkage": "dynamic", "sdk_abi": "a" * 64, "configuration": "Debug",
        }
        self.abi["platform"] = {"win32": "windows", "linux": "linux", "darwin": "macos"}[sys.platform]
        self.abi["architecture"] = {"amd64": "x64", "x86_64": "x64", "aarch64": "arm64", "arm64": "arm64",
                                    "x86": "x86", "i386": "x86", "i686": "x86"}[cli.platform.machine().lower()]
        cli.write_json(self.sdk / "share/BML/BML.abi.json", self.abi)
        self.manifest = {"format": 1, "kind": "native", "id": "test.cli", "target": "TestMod"}
        cli.write_json(self.root / "bml.mod.json", self.manifest)
        self.settings = {"bmlSdk": str(self.sdk)}
        cli.write_json(self.root / ".bml/settings.json", self.settings)
        _, self.args = cli.parse_command(["build", "--project", str(self.root)])
        self.args.generator = "Ninja"

    def build_with_recorder(self, args=None):
        commands = []

        def record(command, failure, verbose=False):
            commands.append(command)
            if "--build" in command:
                destination = self.root / ".bml/stage/Mods/TestMod.bmodp"
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(b"native module")
                cli.write_json(destination.with_suffix(".bmodp.abi.json"), self.abi)
            return ""

        with patch.object(cli, "run_checked", side_effect=record), patch.object(cli, "check_host"), \
                patch.object(cli.shutil, "which", side_effect=lambda name: f"/tools/{name}"):
            artifact = cli.native_build_or_run(args or self.args, source_root / "scripts/bml.py", False, False)
        return artifact, commands

    def test_sdl_single_config_and_cache_isolation(self):
        artifact, commands = self.build_with_recorder()
        configure, build = commands
        self.assertNotIn("-A", configure)
        self.assertFalse(any("VIRTOOLS_SDK_PATH" in part for part in configure))
        self.assertIn("-DCMAKE_BUILD_TYPE=Debug", configure)
        self.assertIn(f"sdl-{'a' * 64}-Debug", configure[configure.index("-B") + 1])
        self.assertEqual(build[build.index("--config") + 1], "Debug")
        self.assertEqual(artifact.read_bytes(), b"native module")

    def test_cli_reports_unicode_paths_to_legacy_output_streams(self):
        destination = self.root / "script"
        encoding = "cp1252"
        result = subprocess.run(
            [sys.executable, str(source_root / "scripts/bml.py"), "new", "script",
             "test.output", "--destination", str(destination)],
            env={**os.environ, "PYTHONIOENCODING": f"{encoding}:strict"},
            capture_output=True, encoding=encoding, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        displayed_path = str(destination).encode(encoding, "backslashreplace").decode(encoding)
        self.assertIn(displayed_path, result.stdout)
        self.assertTrue((destination / "bml.mod.json").is_file())

    def test_windows_sdl_target_is_not_forced_to_win32(self):
        self.abi.update(platform="windows", architecture="arm64")
        cli.write_json(self.sdk / "share/BML/BML.abi.json", self.abi)
        self.args.generator = "Visual Studio 18 2026"
        _, commands = self.build_with_recorder()
        configure = commands[0]
        self.assertEqual(configure[configure.index("-A") + 1], "ARM64")

    def test_sdl_missing_descriptor_is_rejected(self):
        (self.sdk / "share/BML/BML.abi.json").unlink()
        with self.assertRaisesRegex(cli.BmlError, "BML SDK ABI descriptor is missing"):
            cli.native_build_or_run(self.args, source_root / "scripts/bml.py", False, False)

    def test_virtools_sdk_selects_win32_and_a_distinct_cache(self):
        _, sdl_commands = self.build_with_recorder()
        sdl_configure = sdl_commands[0]
        self.abi.update(runtime="virtools", platform="windows", architecture="x86", host_linkage="dynamic")
        path = self.sdk / "share/BML/BML.abi.json"
        cli.write_json(path, self.abi)
        self.args.generator = "Visual Studio 17 2022"
        _, commands = self.build_with_recorder()
        configure = commands[0]
        self.assertEqual(configure[configure.index("-A") + 1], "Win32")
        self.assertNotEqual(configure[configure.index("-B") + 1],
                            sdl_configure[sdl_configure.index("-B") + 1])
        cli.write_json(path, {**self.abi, "architecture": "x64"})
        with self.assertRaisesRegex(cli.BmlError, "Invalid or unsupported BML ABI"):
            cli.sdk_abi(self.sdk)

    def test_old_runtime_descriptor_is_rejected(self):
        path = self.sdk / "share/BML/BML.abi.json"
        cli.write_json(path, {**self.abi, "runtime": "win32"})
        with self.assertRaisesRegex(cli.BmlError, "Invalid or unsupported BML ABI"):
            cli.sdk_abi(self.sdk)

    def test_linux_lib64_and_multiarch_sdk_layouts(self):
        for directory in ("lib64", "lib/aarch64-linux-gnu"):
            target = self.sdk / directory / "cmake/BML/BMLConfig.cmake"
            target.parent.mkdir(parents=True, exist_ok=True)
            self.config.rename(target)
            self.assertEqual(cli.find_sdk_config(self.sdk), target)
            self.assertEqual(cli.find_local_sdk(source_root / "scripts/bml.py", str(self.sdk)), self.sdk)
            target.rename(self.config)

    def test_multiple_sdk_architectures_in_one_prefix_are_rejected(self):
        target = self.sdk / "lib64/cmake/BML/BMLConfig.cmake"
        target.parent.mkdir(parents=True)
        shutil.copyfile(self.config, target)
        with self.assertRaisesRegex(cli.BmlError, "Multiple BML packages"):
            cli.find_sdk_config(self.sdk)

    def test_sdl_rejects_incompatible_configuration(self):
        self.args.configuration = "Release"
        with self.assertRaisesRegex(cli.BmlError, "requires configuration Debug"):
            cli.native_build_or_run(self.args, source_root / "scripts/bml.py", False, False)

    def test_host_target_validation_including_windows_x86(self):
        with patch.object(cli, "sys", SimpleNamespace(platform="win32")), \
                patch.object(cli.platform, "machine", return_value="AMD64"):
            cli.check_host({**self.abi, "platform": "windows", "architecture": "x86"})
            with self.assertRaisesRegex(cli.BmlError, "Use a matching"):
                cli.check_host({**self.abi, "platform": "linux", "architecture": "arm64"})

    def test_player_standard_flat_and_ambiguous_layouts(self):
        name = "Player.exe" if os.name == "nt" else "Player"
        player = self.root / "Bin" / name
        player.parent.mkdir()
        player.touch()
        player.chmod(0o755)
        self.assertEqual(cli.find_player(self.root), player)
        flat = self.root / name
        player.rename(flat)
        with self.assertRaisesRegex(cli.BmlError, "requires base.cmo"):
            cli.find_player(self.root)
        (self.root / "base.cmo").touch()
        self.assertEqual(cli.find_player(self.root), flat)
        player.touch()
        player.chmod(0o755)
        with self.assertRaisesRegex(cli.BmlError, "found 2"):
            cli.find_player(self.root)
        with patch.object(cli, "os", SimpleNamespace(name="nt")):
            windows = self.root / "windows-layout"
            executable = windows / "Bin/Player.exe"
            executable.parent.mkdir(parents=True)
            executable.touch()
            self.assertEqual(cli.find_player(windows), executable)

    @unittest.skipIf(os.name == "nt", "POSIX permissions")
    def test_non_executable_player_is_rejected(self):
        player = self.root / "Player"
        player.touch()
        with self.assertRaisesRegex(cli.BmlError, "not executable"):
            cli.find_player(self.root)

    def test_sdl_pack_keeps_module_and_exact_descriptor_together(self):
        artifact, _ = self.build_with_recorder()
        descriptor = artifact.with_suffix(".bmodp.abi.json")
        _, args = cli.parse_command(["pack", "--project", str(self.root)])
        with patch.object(cli, "native_build_or_run", return_value=artifact):
            cli.native_pack(args, source_root / "scripts/bml.py")
            with zipfile.ZipFile(self.root / "dist/TestMod.zip") as archive:
                self.assertEqual(set(archive.namelist()), {artifact.name, descriptor.name})
                self.assertEqual(archive.read(artifact.name), artifact.read_bytes())
                self.assertEqual(archive.read(descriptor.name), descriptor.read_bytes())
            with self.assertRaisesRegex(cli.BmlError, "Pass --force"):
                cli.native_pack(args, source_root / "scripts/bml.py")

    def test_abi_mismatch_is_rejected_before_build_or_deployment(self):
        cli.write_json(self.root / "ModLoader/BML.abi.json", {**self.abi, "sdk_abi": "b" * 64})
        self.args.ballance_root = str(self.root)
        with patch.object(cli.shutil, "which", return_value="cmake"), patch.object(cli, "run_checked") as run:
            with self.assertRaisesRegex(cli.BmlError, "ABI mismatch for sdk_abi"):
                cli.native_build_or_run(self.args, source_root / "scripts/bml.py", True, False)
            run.assert_not_called()
        self.assertFalse((self.root / "ModLoader/Mods").exists())

    def test_running_player_blocks_native_replacement(self):
        cli.write_json(self.root / "ModLoader/BML.abi.json", self.abi)
        player = self.root / ("Player.exe" if os.name == "nt" else "Player")
        player.touch()
        player.chmod(0o755)
        self.args.ballance_root = str(self.root)
        (self.root / "base.cmo").touch()
        with patch.object(cli, "player_is_running", return_value=True), \
                patch.object(cli.shutil, "which", return_value="cmake"), patch.object(cli, "run_checked") as run:
            with self.assertRaisesRegex(cli.BmlError, "Close it before replacing"):
                cli.native_build_or_run(self.args, source_root / "scripts/bml.py", True, False)
            run.assert_not_called()
        self.assertFalse((self.root / "ModLoader/Mods").exists())

    @unittest.skipIf(os.name == "nt", "POSIX process discovery")
    def test_running_player_is_detected_by_real_process(self):
        player = self.root / "Player"
        shutil.copy("/bin/sleep", player)
        process = subprocess.Popen([str(player), "20"])
        try:
            self.assertTrue(cli.player_is_running())
        finally:
            process.terminate()
            process.wait(timeout=5)
        with patch.object(cli, "run_checked", return_value="/tmp/OtherPlayer\n/tmp/PlayerHelper\n"):
            self.assertFalse(cli.player_is_running())

    def test_windows_tasklist_is_preserved(self):
        result = SimpleNamespace(returncode=0, stdout=b'Player.exe 123 Console\n')
        with patch.object(cli, "os", SimpleNamespace(name="nt")), \
                patch.object(cli.shutil, "which", return_value="tasklist"), \
                patch.object(cli.subprocess, "run", return_value=result) as run:
            self.assertTrue(cli.player_is_running())
            self.assertEqual(run.call_args.args[0], ["tasklist", "/FI", "IMAGENAME eq Player.exe", "/NH"])

    def test_script_runtime_uses_platform_plugin_names(self):
        cli.write_json(self.root / "ModLoader/BML.abi.json", self.abi)
        player = self.root / "Bin" / ("Player.exe" if os.name == "nt" else "Player")
        player.parent.mkdir()
        player.touch()
        player.chmod(0o755)
        prefix, suffix = {"windows": ("", ".dll"), "linux": ("lib", ".so"),
                          "macos": ("lib", ".dylib")}[self.abi["platform"]]
        for directory, name in (("Managers", "BMLPlus"), ("BuildingBlocks", "AngelScript")):
            plugin = self.root / directory / f"{prefix}{name}{suffix}"
            plugin.parent.mkdir()
            plugin.touch()
        _, args = cli.parse_command(["run", "--ballance-root", str(self.root)])
        self.assertEqual(cli.resolve_game(args, {}), self.root)
        plugin.unlink()
        with self.assertRaisesRegex(cli.BmlError, "Script Mod runtime is missing"):
            cli.resolve_game(args, {})

    @unittest.skipIf(os.name == "nt", "POSIX launcher")
    def test_script_run_deploys_and_waits_in_player_directory(self):
        game = self.root.parent / "游戏"
        player = game / "Bin/Player"
        player.parent.mkdir(parents=True)
        player.write_text('#!/bin/sh\npwd > launched.txt\n', encoding="utf-8")
        player.chmod(0o755)
        for relative in ("Managers/libBMLPlus", "BuildingBlocks/libAngelScript"):
            plugin = game / (relative + (".dylib" if sys.platform == "darwin" else ".so"))
            plugin.parent.mkdir(parents=True, exist_ok=True)
            plugin.touch()
        cli.write_json(game / "ModLoader/BML.abi.json", self.abi)
        cli.write_json(self.root / "bml.mod.json", {
            "format": 1, "kind": "script", "entry": "Script.mod.as", "deployName": "ScriptMod",
        })
        (self.root / "Script.mod.as").write_text(
            '[bml.mod id="test.script" name="Script" version="1.0.0"]\nclass Script {}\n', encoding="utf-8")
        _, args = cli.parse_command(["run", "--project", str(self.root), "--ballance-root", str(game)])
        with patch.object(cli, "player_is_running", return_value=False):
            cli.script_run(args, source_root / "scripts/bml.py")
        self.assertEqual((player.parent / "launched.txt").read_text().strip(), str(player.parent))
        self.assertTrue((game / "ModLoader/Mods/ScriptMod/Script.mod.as").is_file())


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *remaining])
