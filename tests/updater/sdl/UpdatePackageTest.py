#!/usr/bin/env python3
"""Exercise the publisher before invoking the native signer."""

import argparse
from contextlib import redirect_stderr
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile


parser = argparse.ArgumentParser()
parser.add_argument("--packager", required=True, type=Path)
parser.add_argument("--cmake", default="cmake")
args, remaining = parser.parse_known_args()
spec = importlib.util.spec_from_file_location("packager", args.packager)
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PackageTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        self.stage = self.root / "stage"
        self.output = self.root / "output"
        self.inventory = self.root / "install_manifest_BMLRuntime.txt"
        self.abi_path = self.stage / "ModLoader/BML.abi.json"
        self.abi_path.parent.mkdir(parents=True)
        self.abi = {"schema": 2, "runtime": "sdl", "platform": "linux",
                    "architecture": "x64", "host_linkage": "dynamic", "version": "0.4.0",
                    "scripts": False, "mod_api": "bml.mod", "mod_api_major": 1, "sdk_abi": "a" * 64}
        self.loader = self.stage / "Managers/libBMLPlus.so"
        self.loader.parent.mkdir()
        self.loader.write_bytes(b"loader")
        self.descriptor()
        self.manifest(self.abi_path, self.loader)

    def descriptor(self):
        self.abi_path.write_text(json.dumps(self.abi), encoding="utf-8")

    def manifest(self, *paths):
        self.inventory.write_text("\n".join(str(path) for path in paths), encoding="utf-8")

    def invoke(self, version="0.4.0", error=None, inventory=True):
        argv = ["package", "--stage", str(self.stage), "--output", str(self.output),
                "--version", version, "--signing-key", str(self.root / "unused-key")]
        if inventory:
            argv += ["--install-manifest", str(self.inventory)]
        stderr = io.StringIO()
        with patch.object(sys, "argv", argv), redirect_stderr(stderr), \
                patch.object(packager.subprocess, "run") as signer:
            signer.return_value.stdout = bytes(64)
            if error:
                with self.assertRaises(SystemExit):
                    packager.main()
                self.assertIn(error, stderr.getvalue())
                signer.assert_not_called()
                self.assertFalse(self.output.exists())
            else:
                self.assertEqual(packager.main(), 0)

    def test_stage_version_is_authoritative(self):
        self.invoke(version="0.4.1", error="does not match the stage version")

    def test_manifest_requires_descriptor(self):
        self.manifest(self.loader)
        self.invoke(error="ABI descriptor")

    def test_manifest_requires_loader(self):
        self.manifest(self.abi_path)
        self.invoke(error="Loader or static Player")

    def test_manifest_requires_enabled_script_runtime(self):
        self.abi["scripts"] = True
        self.descriptor()
        plugin = self.stage / "BuildingBlocks/libAngelScript.so"
        plugin.parent.mkdir()
        plugin.write_bytes(b"script runtime")
        self.invoke(error="script runtime")

    def library_chain(self):
        binary = self.stage / "Bin/libexample.so.1.2"
        binary.parent.mkdir()
        binary.write_bytes(b"ELF test fixture")
        binary.chmod(0o755)
        soname = binary.with_name("libexample.so.1")
        soname.symlink_to(binary.name)
        return binary, soname

    @unittest.skipIf(sys.platform == "win32", "POSIX symlink fixture")
    def test_manifest_retains_soname_as_regular_payload(self):
        binary, soname = self.library_chain()
        self.manifest(self.abi_path, self.loader, binary, soname)
        self.invoke()
        with zipfile.ZipFile(next(self.output.glob("*.zip"))) as archive:
            self.assertEqual(archive.read("Bin/" + soname.name), binary.read_bytes())
            self.assertEqual(archive.read("ModLoader/BML.abi.json"), self.abi_path.read_bytes())
            mode = archive.getinfo("Bin/" + soname.name).external_attr >> 16
            self.assertTrue(stat.S_ISREG(mode))
            self.assertEqual(stat.S_IMODE(mode), 0o755)

    @unittest.skipIf(sys.platform == "win32", "POSIX symlink fixture")
    def test_link_target_must_be_in_manifest(self):
        _, soname = self.library_chain()
        self.manifest(self.abi_path, self.loader, soname)
        self.invoke(error="packaged library")

    @unittest.skipIf(sys.platform == "win32", "POSIX symlink fixture")
    def test_link_cannot_leave_stage(self):
        binary, soname = self.library_chain()
        outside = self.root / binary.name
        binary.rename(outside)
        soname.unlink()
        soname.symlink_to(outside)
        self.manifest(self.abi_path, self.loader, soname)
        self.invoke(error="packaged library")

    @unittest.skipIf(sys.platform == "win32", "POSIX symlink fixture")
    def test_link_to_game_asset_is_rejected(self):
        font = self.stage / "ModLoader/Fonts/font.ttf"
        font.parent.mkdir()
        font.write_bytes(b"font")
        link = font.with_name("link.ttf")
        link.symlink_to(font.name)
        self.invoke(error="symlink", inventory=False)

    @unittest.skipUnless(sys.platform == "linux", "native ELF loader fixture")
    def test_installed_elf_loads_using_packaged_soname(self):
        def run(*command, env=None):
            subprocess.run(command, env=env, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        sources = self.root / "sources"
        sources.mkdir()
        (sources / "dependency.c").write_text("int fixture(void) { return 42; }\n", encoding="utf-8")
        (sources / "loader.c").write_text("int fixture(void); int load(void) { return fixture(); }\n", encoding="utf-8")
        (sources / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.25)
project(SonameFixture LANGUAGES C)
if(NOT DEFINED SDK)
    add_library(fixture SHARED dependency.c)
    set_target_properties(fixture PROPERTIES VERSION 1.2 SOVERSION 1)
    install(TARGETS fixture LIBRARY DESTINATION lib)
else()
    add_library(fixture SHARED IMPORTED)
    set_property(TARGET fixture PROPERTY IMPORTED_LOCATION "${SDK}/lib/libfixture.so.1.2")
    add_library(BMLPlus SHARED loader.c)
    target_link_libraries(BMLPlus PRIVATE fixture)
    set_property(TARGET BMLPlus PROPERTY INSTALL_RPATH "$ORIGIN/../Bin")
    install(TARGETS BMLPlus RUNTIME_DEPENDENCY_SET Owned LIBRARY DESTINATION Managers)
    install(RUNTIME_DEPENDENCY_SET Owned LIBRARY DESTINATION Bin)
endif()
''', encoding="utf-8")
        dependency, sdk, build = (self.root / name for name in ("dependency", "sdk", "loader"))
        run(args.cmake, "-S", sources, "-B", dependency)
        run(args.cmake, "--build", dependency)
        run(args.cmake, "--install", dependency, "--prefix", sdk)
        run(args.cmake, "-S", sources, "-B", build, f"-DSDK={sdk}")
        run(args.cmake, "--build", build)
        run(args.cmake, "--install", build, "--prefix", self.stage)
        installed = (build / "install_manifest.txt").read_text(encoding="utf-8").splitlines()
        soname = self.stage / "Bin/libfixture.so.1"
        self.assertIn(str(soname), installed)
        self.assertTrue(soname.is_symlink())
        self.manifest(self.abi_path, *installed)
        self.invoke()
        unpacked = self.root / "unpacked"
        unpacked.mkdir()
        # Remove the development copy so only the ZIP can supply this SONAME.
        sdk.rename(self.root / "hidden-sdk")
        run(args.cmake, "-E", "chdir", unpacked, args.cmake, "-E", "tar", "xf",
            next(self.output.glob("*.zip")))
        environment = os.environ.copy()
        environment.pop("LD_LIBRARY_PATH", None)
        run(sys.executable, "-c", "import ctypes, sys; assert ctypes.CDLL(sys.argv[1]).load() == 42",
            unpacked / "Managers/libBMLPlus.so", env=environment)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *remaining])
