#!/usr/bin/env python3
"""Build all public workflow profiles against a relocated, installed SDL SDK."""

import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


class Header(ctypes.Structure):
    _fields_ = [("size", ctypes.c_size_t), ("major", ctypes.c_uint16),
                ("minor", ctypes.c_uint16), ("id", ctypes.c_char_p)]


class ModDefinition(ctypes.Structure):
    _fields_ = [("size", ctypes.c_size_t), ("id", ctypes.c_char_p),
                ("name", ctypes.c_char_p), ("version", ctypes.c_char_p),
                ("author", ctypes.c_char_p), ("description", ctypes.c_char_p),
                ("minimum_loader_version", ctypes.c_char_p),
                ("dependencies", ctypes.c_void_p), ("dependency_count", ctypes.c_size_t),
                ("dependency_stride", ctypes.c_size_t)]


class ModInterfacePrefix(ctypes.Structure):
    _fields_ = [("header", Header), ("definition", ctypes.POINTER(ModDefinition))]


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
    parser.add_argument("--engine-prefix-path", default="")
    parser.add_argument("--package-dir", action="append", nargs=2, default=[], metavar=("NAME", "DIR"))
    parser.add_argument("--runtime-library", action="append", type=Path, default=[])
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
        runtime_dirs = dict.fromkeys(path.resolve().parent for path in args.runtime_library)
        dll_directories = [os.add_dll_directory(str(directory)) for directory in (sdk / "bin", *runtime_dirs)]
        loader = sdk / "bin/BMLPlus.dll"
    else:
        loader = next(sdk.rglob("libBMLPlus.dylib" if sys.platform == "darwin" else "libBMLPlus.so"))
    # The SDK no longer installs CK2 or VxMath. Load the same engine libraries
    # as the target Player would, without copying them into the relocated SDK.
    _engine_libraries = [ctypes.CDLL(str(library.resolve()), mode=ctypes.RTLD_GLOBAL)
                         for library in args.runtime_library]
    host_library = ctypes.CDLL(str(loader), mode=ctypes.RTLD_GLOBAL)
    version_header = (sdk / "include/BML/Version.h").read_text(encoding="utf-8")
    version_match = re.search(r'^#define BML_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$',
                              version_header, re.MULTILINE)
    assert version_match, "SDK release version is missing"
    release = version_match.group(1)
    host_library.BML_GetVersionString.restype = ctypes.c_char_p
    assert host_library.BML_GetVersionString().decode("utf-8") == release
    version = [ctypes.c_int() for _ in range(3)]
    host_library.BML_GetVersion.argtypes = [ctypes.POINTER(ctypes.c_int)] * 3
    host_library.BML_GetVersion.restype = None
    host_library.BML_GetVersion(*(ctypes.byref(part) for part in version))
    assert tuple(part.value for part in version) == tuple(map(int, release.split(".")))
    package_version = next(sdk.rglob("BMLConfigVersion.cmake")).read_text(encoding="utf-8")
    assert f'set(PACKAGE_VERSION "{release}")' in package_version
    for name in ("CKGetPluginInfoCount", "CKGetPluginInfo", "RegisterBehaviorDeclarations",
                 "BML_GetInterface"):
        assert hasattr(host_library, name), f"Required host export is missing: {name}"
    # Obsolete C++ vtables and caller-address services must not remain a second
    # native entry route. Installed consumers use Context and versioned C tables.
    obsolete_headers = ("IMod.h", "IBML.h", "IConfig.h", "ICommand.h", "ILogger.h",
                        "IMessageReceiver.h", "InputHook.h", "ExecuteBB.h", "ModInterface.hpp",
                        "RefCount.h")
    for name in obsolete_headers:
        assert not (sdk / "include/BML" / name).exists(), name
    obsolete_symbols = [
        "BML_GetLoaderPathW", "BML_GetLoaderPathUtf8", "BML_GetModRootW",
        "BML_GetModRootUtf8", "BML_UnregisterCommand", "BML_SetCommandStatus",
        "BML_GetCommandInput", "BML_GetConfigPropertyEditor", "BML_SetConfigPropertyEditor",
        "BML_GetConfigPropertyChoiceCount", "BML_GetConfigPropertyChoice",
        "BML_SetConfigPropertyChoices",
    ]
    if os.name != "nt":
        obsolete_symbols += ["_ZTI4IMod", "_ZTI4IBML", "_ZTI9IProperty", "_ZTI7IConfig",
                             "_ZTI8ICommand", "_ZTI7ILogger", "_ZTI16IMessageReceiver"]
    for name in obsolete_symbols:
        assert not hasattr(host_library, name), f"Obsolete native export remains: {name}"
    host = work / "中文宿主 with spaces"
    host_mods = host / "ModLoader/Mods"
    host_mods.mkdir(parents=True)
    if sys.platform == "darwin":
        # This ctypes host uses the SDK's development libraries, rather than a
        # Player package. Supply its already-loaded SDL in the normal host Bin
        # directory without editing any library or consumer load commands.
        (host / "Bin").mkdir()
        dyld = ctypes.CDLL(None)
        dyld._dyld_image_count.restype = ctypes.c_uint32
        dyld._dyld_get_image_name.argtypes = [ctypes.c_uint32]
        dyld._dyld_get_image_name.restype = ctypes.c_char_p
        images = [Path(os.fsdecode(dyld._dyld_get_image_name(index)))
                  for index in range(dyld._dyld_image_count())]
        sdl_images = [image for image in images if image.name.startswith("libSDL3")]
        assert len(sdl_images) == 1, sdl_images
        dependency = sdl_images[0]
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
        descriptor = json.loads(module.with_suffix(".bmodp.abi.json").read_text(encoding="utf-8"))
        assert all(descriptor.get(key) == value for key, value in abi.items())
        assert descriptor["extensions"] == {}
        if sys.platform == "darwin":
            dependencies = [line.strip().split(" (", 1)[0]
                            for line in run("otool", "-L", str(module)).splitlines()[1:]]
            sdl_dependencies = [name for name in dependencies if Path(name).name.startswith("libSDL3")]
            assert all(name.startswith("@rpath/") for name in sdl_dependencies), dependencies
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
        # Querying C metadata must not require a Player or construct a Mod.
        load_module = host_mods / module.name
        shutil.copy2(module, load_module)
        assert load_module.read_bytes() == module.read_bytes()
        library = ctypes.CDLL(str(load_module))
        query = library.BML_GetModInterface
        query.argtypes = [ctypes.c_uint16, ctypes.POINTER(ctypes.c_void_p)]
        query.restype = ctypes.c_int
        interface = ctypes.c_void_p()
        assert query(1, ctypes.byref(interface)) == 0 and interface.value
        metadata = ctypes.cast(interface, ctypes.POINTER(ModInterfacePrefix)).contents.definition.contents
        assert metadata.id.decode("utf-8") == manifest["id"]
        assert metadata.name.decode("utf-8") == manifest["name"]
        assert metadata.version.decode("utf-8") == manifest["version"]
        assert metadata.minimum_loader_version.decode("utf-8") == release
        assert metadata.author.decode("utf-8") == "SDK Test"
        assert metadata.dependency_count == (2 if profile == "interface-consumer" else 1)
        original_interface = interface.value
        assert query(1, ctypes.byref(interface)) == 0 and interface.value == original_interface
        assert query(2, ctypes.byref(interface)) < 0 and interface.value is None

    # The installed pure C example has no C++ compiler or engine requirement.
    c_build = work / "pure-c-build"
    print(run(args.cmake, "-S", str(sdk / "examples/native-c-mod"), "-B", str(c_build),
              f"-DCMAKE_PREFIX_PATH={sdk}", f"-DCMAKE_BUILD_TYPE={args.configuration}"))
    print(run(args.cmake, "--build", str(c_build), "--config", args.configuration))
    c_module = next(c_build.rglob("BMLCExample.bmodp"))
    c_manifest = json.loads(Path(str(c_module) + ".abi.json").read_text())
    assert c_manifest["extensions"] == {}

    friendly_build = work / "native-example-build"
    print(run(args.cmake, "-S", str(sdk / "examples/native-mod"), "-B", str(friendly_build),
              f"-DCMAKE_PREFIX_PATH={sdk}", f"-DCMAKE_BUILD_TYPE={args.configuration}"))
    print(run(args.cmake, "--build", str(friendly_build), "--config", args.configuration))
    friendly = next(friendly_build.rglob("BMLNativeExample.bmodp"))
    assert json.loads(Path(str(friendly) + ".abi.json").read_text())["extensions"] == {}

    # Two DLLs deliberately use the same C++ names, template instantiations and
    # static state. A transitive extension dependency must enter the descriptor.
    fixture = work / "same-type-fixture"
    source = fixture / "consumer"
    source.mkdir(parents=True)
    (source / "Mod.cpp").write_text(r'''#include <BML/Mod.hpp>
#ifdef USE_EXTENSIONS
#include <BML/Engine.hpp>
#include <BML/Bui/ImGui.hpp>
#include <BML/Gui/Engine.hpp>
#endif
static int instances = 0;
class HelloMod final : public BML::Mod::Instance {
public:
    HelloMod() { ++instances; }
    ~HelloMod() noexcept override { --instances; }
};
BML_MOD_ENTRY(int) BML_TestInstances() { return instances; }
BML_DEFINE_MOD(HelloMod, {.Id = MOD_ID, .Name = "Same type", .Version = "1.0.0", .Author = "SDK tests"})
''', encoding="utf-8")
    (fixture / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.25)
project(IndependentModules LANGUAGES CXX)
add_subdirectory(consumer)
''', encoding="utf-8")
    (source / "CMakeLists.txt").write_text(r'''
find_package(BML CONFIG REQUIRED)
bml_add_mod(Core Mod.cpp)
target_compile_definitions(Core PRIVATE MOD_ID="test.core")
add_library(Extensions INTERFACE)
target_link_libraries(Extensions INTERFACE BML::Engine BML::ImGui)
bml_add_mod(Extended Mod.cpp)
target_compile_definitions(Extended PRIVATE USE_EXTENSIONS MOD_ID="test.extended")
target_link_libraries(Extended PRIVATE Extensions)
''', encoding="utf-8")
    build = fixture / "build"
    prefixes = ";".join(filter(None, (str(sdk), args.engine_prefix_path)))
    print(run(args.cmake, "-S", str(fixture), "-B", str(build),
              f"-DCMAKE_PREFIX_PATH={prefixes}", f"-DCMAKE_BUILD_TYPE={args.configuration}",
              *(f"-D{name}_DIR={path}" for name, path in args.package_dir)))
    print(run(args.cmake, "--build", str(build), "--config", args.configuration))
    Create = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_uint64, ctypes.POINTER(ctypes.c_void_p))
    Destroy = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
    class Protocol(ctypes.Structure):
        _fields_ = [("header", Header), ("definition", ctypes.c_void_p),
                    ("callbacks", ctypes.c_void_p), ("create", Create), ("destroy", Destroy)]
    libraries = []
    protocols = []
    objects = []
    for target in ("Core", "Extended"):
        module = next(build.rglob(target + ".bmodp"))
        descriptor = json.loads(Path(str(module) + ".abi.json").read_text())
        assert descriptor["extensions"] == ({} if target == "Core" else abi["extension_abis"])
        deployed = host_mods / module.name
        shutil.copy2(module, deployed)
        library = ctypes.CDLL(str(deployed))
        query = library.BML_GetModInterface
        query.argtypes = [ctypes.c_uint16, ctypes.POINTER(ctypes.POINTER(Protocol))]
        query.restype = ctypes.c_int
        interface = ctypes.POINTER(Protocol)()
        assert query(1, ctypes.byref(interface)) == 0
        assert library.BML_TestInstances() == 0
        libraries.append(library)
        protocols.append(interface.contents)
    for index, protocol in enumerate(protocols):
        instance = ctypes.c_void_p()
        assert protocol.create(0, ctypes.byref(instance)) == 0 and instance.value
        objects.append(instance)
        assert [lib.BML_TestInstances() for lib in libraries] == ([1, 0] if index == 0 else [1, 1])
    for protocol, instance in zip(protocols, objects):
        protocol.destroy(instance)
    assert [lib.BML_TestInstances() for lib in libraries] == [0, 0]

    if (sdk / "templates/script-mod-template").is_dir():
        destination = work / "中文工作区/script"
        print(run(*workflow(tool, "new", "script", "test.script", "--author", "SDK Test",
                            "--destination", str(destination))))
        print(run(*workflow(destination / "bml.py", "build")))
        print(run(*workflow(destination / "bml.py", "pack")))
        with zipfile.ZipFile(destination / "dist/ScriptMod.zip") as archive:
            assert "ScriptMod.mod.as" in archive.namelist()
            assert not {"bml", "bml.cmd", "bml.py"}.intersection(archive.namelist())
    print("Installed SDL SDK: four native profiles and pure C compiled; shared C++ names remained isolated; transitive Engine/ImGui validated; script staging/packing checked when enabled.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
