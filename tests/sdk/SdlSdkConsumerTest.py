#!/usr/bin/env python3
"""Build all public workflow profiles against a relocated, installed SDL SDK."""

import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def run(*command: str) -> str:
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode or "install_name_tool:" in result.stdout:
        raise RuntimeError(f"Command failed ({result.returncode}): {command}\n{result.stdout}")
    return result.stdout


def workflow(tool: Path, *arguments: str) -> list[str]:
    if os.name == "nt":
        return [sys.executable, str(tool), *arguments]
    return [str(tool.with_name("bml")), *arguments]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--main-build-dir", type=Path)
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--configuration", default="Debug")
    args = parser.parse_args()
    args.work_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.work_root)).resolve()
    sdk = work / "已安装 SDK"
    if args.sdk:
        shutil.copytree(args.sdk.resolve(), sdk)
    elif args.main_build_dir:
        print(run(args.cmake, "--install", str(args.main_build_dir), "--prefix", str(sdk),
                  "--config", args.configuration))
    else:
        parser.error("Pass --sdk or --main-build-dir")
    # Rename after installation: no source/build-tree or old-prefix dependency may be required.
    relocated = work / "移动后的 SDK"
    sdk.rename(relocated)
    sdk = relocated
    tool = sdk / "scripts/bml.py"
    abi = json.loads((sdk / "share/BML/BML.abi.json").read_text(encoding="utf-8"))
    assert abi["runtime"] == "sdl" and abi["host_linkage"] == "dynamic"
    dll_directories = []
    if os.name == "nt":
        dll_directories = [os.add_dll_directory(str(sdk / directory)) for directory in ("Bin", "bin")]
        loader = sdk / "bin/BMLPlus.dll"
    else:
        loader = next(sdk.rglob("libBMLPlus.dylib" if sys.platform == "darwin" else "libBMLPlus.so"))
    host_library = ctypes.CDLL(str(loader), mode=ctypes.RTLD_GLOBAL)
    host = work / "中文宿主 with spaces"
    host_mods = host / "ModLoader/Mods"
    host_mods.mkdir(parents=True)
    if sys.platform == "darwin":
        # This ctypes host uses the SDK's development libraries, rather than a
        # Player package. Supply its already-loaded SDL in the normal host Bin
        # directory without editing any library or consumer load commands.
        (host / "Bin").mkdir()
        for line in run("otool", "-L", str(loader)).splitlines()[1:]:
            dependency = Path(line.strip().split(" (", 1)[0])
            if dependency.name.startswith("libSDL3"):
                assert dependency.is_absolute() and dependency.is_file(), dependency
                (host / "Bin" / dependency.name).symlink_to(dependency)
    projects = {}
    for profile in ("basic", "interface-provider", "interface-consumer", "imc-provider"):
        destination = work / "中文工作区" / profile
        command = workflow(tool, "new", "native", f"test.{profile}", "--profile", profile,
                           "--author", "SDK Test", "--destination", str(destination))
        if profile == "interface-consumer":
            command += ["--provider-id", "test.interface-provider"]
        print(run(*command))
        projects[profile] = destination
        command = workflow(destination / "bml.py", "build", "--verbose")
        if profile == "interface-consumer":
            command += ["--package-root", str(projects["interface-provider"] / ".bml/stage")]
        print(run(*command))
        manifest = json.loads((destination / "bml.mod.json").read_text(encoding="utf-8"))
        module = destination / ".bml/stage/Mods" / f"{manifest['target']}.bmodp"
        built = list((destination / ".bml/build").rglob(module.name))
        assert len(built) == 1, built
        assert built[0].read_bytes() == module.read_bytes(), "Install changed the SDK-built Mod"
        assert json.loads(module.with_suffix(".bmodp.abi.json").read_text(encoding="utf-8")) == abi
        if sys.platform == "darwin":
            dependencies = [line.strip().split(" (", 1)[0]
                            for line in run("otool", "-L", str(module)).splitlines()[1:]]
            sdl_dependencies = [name for name in dependencies if Path(name).name.startswith("libSDL3")]
            assert sdl_dependencies and all(name.startswith("@rpath/") for name in sdl_dependencies), dependencies
            run("codesign", "--verify", "--strict", str(module))
        # Rebuilding/reinstalling must preserve the deployable output, including
        # its signature; packaging must copy these same bytes as well.
        original = module.read_bytes()
        print(run(*command))
        assert module.read_bytes() == original == built[0].read_bytes()
        print(run(*workflow(destination / "bml.py", "pack", "--verbose")))
        with zipfile.ZipFile(destination / "dist" / f"{manifest['target']}.zip") as archive:
            assert set(archive.namelist()) == {module.name, module.name + ".abi.json"}
            assert archive.read(module.name) == module.read_bytes()
        # Native load plus both entry exports; factories need a real Player/IBML.
        load_module = host_mods / module.name
        shutil.copy2(module, load_module)
        assert load_module.read_bytes() == module.read_bytes()
        library = ctypes.CDLL(str(load_module))
        assert library.BMLEntry and library.BMLExit

    if (sdk / "templates/script-mod-template").is_dir():
        destination = work / "中文工作区/script"
        print(run(*workflow(tool, "new", "script", "test.script", "--author", "SDK Test",
                            "--destination", str(destination))))
        print(run(*workflow(destination / "bml.py", "build")))
        print(run(*workflow(destination / "bml.py", "pack")))
        with zipfile.ZipFile(destination / "dist/ScriptMod.zip") as archive:
            assert "ScriptMod.mod.as" in archive.namelist()
            assert not {"bml", "bml.cmd", "bml.py"}.intersection(archive.namelist())
    print("Installed SDL SDK: four native profiles compiled, packaged and loaded; script staging/packing checked when enabled.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
