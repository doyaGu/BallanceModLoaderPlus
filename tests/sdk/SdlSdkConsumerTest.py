#!/usr/bin/env python3
"""Build all public workflow profiles against a relocated, installed SDL SDK."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


def run(*command: str, env=None) -> str:
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace")
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
    parser.add_argument("--generator", default="")
    parser.add_argument("--generator-platform", default="")
    parser.add_argument("--generator-toolset", default="")
    parser.add_argument("--toolchain", default="")
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
    if args.generator.startswith("Visual Studio") and not args.generator_platform:
        args.generator_platform = {"x86": "Win32", "x64": "x64", "arm64": "ARM64"}[abi["architecture"]]
    generator_options = []
    for option, value in (("-G", args.generator), ("-A", args.generator_platform),
                          ("-T", args.generator_toolset), ("--toolchain", args.toolchain)):
        if value:
            generator_options.extend((option, value))
    if sys.platform == "darwin":
        architecture = {"arm64": "arm64", "x64": "x86_64"}[abi["architecture"]]
        generator_options.append(f"-DCMAKE_OSX_ARCHITECTURES={architecture}")
    workflow_env = os.environ.copy()
    workflow_env["PYTHONIOENCODING"] = "utf-8"
    if args.generator_toolset:
        workflow_env["CMAKE_GENERATOR_TOOLSET"] = args.generator_toolset
    if args.toolchain:
        workflow_env["CMAKE_TOOLCHAIN_FILE"] = args.toolchain
    if os.name == "nt":
        loader = sdk / "bin/BMLPlus.dll"
    else:
        loader = next(sdk.rglob("libBMLPlus.dylib" if sys.platform == "darwin" else "libBMLPlus.so"))
    version_header = (sdk / "include/BML/Version.h").read_text(encoding="utf-8")
    version_match = re.search(r'^#define BML_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$',
                              version_header, re.MULTILINE)
    assert version_match, "SDK release version is missing"
    release = version_match.group(1)
    package_version = next(sdk.rglob("BMLConfigVersion.cmake")).read_text(encoding="utf-8")
    assert f'set(PACKAGE_VERSION "{release}")' in package_version
    for name in ("IMod.h", "IBML.h", "IConfig.h", "ICommand.h", "ILogger.h",
                 "IMessageReceiver.h", "InputHook.h", "ExecuteBB.h", "ModInterface.hpp", "RefCount.h"):
        assert not (sdk / "include/BML" / name).exists(), name

    # The inspection process is compiled for the SDK target. The orchestration
    # interpreter can have a different Windows architecture.
    probe_source = work / "native-probe"
    probe_source.mkdir()
    shutil.copy2(Path(__file__).with_name("NativeConsumerProbe.cpp"), probe_source)
    (probe_source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.25)
project(NativeSdkProbe LANGUAGES CXX)
add_executable(NativeSdkProbe NativeConsumerProbe.cpp)
target_compile_features(NativeSdkProbe PRIVATE cxx_std_20)
target_include_directories(NativeSdkProbe PRIVATE "${SDK}/include")
target_link_libraries(NativeSdkProbe PRIVATE ${CMAKE_DL_LIBS})
if(WIN32)
    target_compile_definitions(NativeSdkProbe PRIVATE _WIN32_WINNT=0x0A00)
endif()
if(MINGW)
    target_link_options(NativeSdkProbe PRIVATE -municode)
endif()
''', encoding="utf-8")
    probe_build = work / "native-probe-build"
    print(run(args.cmake, *generator_options, "-S", str(probe_source), "-B", str(probe_build),
              f"-DSDK={sdk}", f"-DCMAKE_BUILD_TYPE={args.configuration}"))
    print(run(args.cmake, "--build", str(probe_build), "--config", args.configuration))
    probe = next(probe_build.rglob("NativeSdkProbe.exe" if os.name == "nt" else "NativeSdkProbe"))
    libraries = [library.resolve() for library in args.runtime_library]
    probe_command = [str(probe), str(loader), release, str(len(libraries)), *map(str, libraries)]
    run(*probe_command, "loader")

    host = work / "中文宿主 with spaces"
    host_mods = host / "ModLoader/Mods"
    host_mods.mkdir(parents=True)
    if sys.platform == "darwin":
        # Match the layout expected by an SDK-built Mod while reusing one SDL.
        (host / "Bin").mkdir()
        dependency = next(library for library in libraries if library.name.startswith("libSDL3"))
        install_name = run("otool", "-D", str(dependency)).splitlines()[1].strip()
        (host / "Bin" / Path(install_name).name).symlink_to(dependency)
    projects = {}
    for profile in ("basic", "interface-provider", "interface-consumer", "imc-provider"):
        destination = work / "中文工作区" / profile
        command = workflow(tool, "new", "native", f"test.{profile}", "--profile", profile,
                           "--author", "SDK Test", "--destination", str(destination))
        if profile == "interface-consumer":
            command += ["--provider-id", "test.interface-provider"]
        print(run(*command, env=workflow_env))
        projects[profile] = destination
        command = workflow(destination / "bml.py", "build", "--verbose", "--configuration", args.configuration)
        if args.generator:
            command += ["--generator", args.generator]
        if profile == "interface-consumer":
            command += ["--package-root", str(projects["interface-provider"] / ".bml/stage")]
        print(run(*command, env=workflow_env))
        manifest = json.loads((destination / "bml.mod.json").read_text(encoding="utf-8"))
        module = destination / ".bml/stage/Mods" / f"{manifest['target']}.bmodp"
        settings = json.loads((destination / ".bml/settings.json").read_text(encoding="utf-8"))
        built = list(Path(settings["buildDirectory"]).rglob(module.name))
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
        print(run(*command, env=workflow_env))
        assert module.read_bytes() == original == built[0].read_bytes()
        print(run(*workflow(destination / "bml.py", "pack", "--verbose")))
        with zipfile.ZipFile(destination / "dist" / f"{manifest['target']}.zip") as archive:
            assert set(archive.namelist()) == {module.name, module.name + ".abi.json"}
            assert archive.read(module.name) == module.read_bytes()
        # Querying C metadata must not require a Player or construct a Mod.
        load_module = host_mods / module.name
        shutil.copy2(module, load_module)
        assert load_module.read_bytes() == module.read_bytes()
        run(*probe_command, "metadata", str(load_module), manifest["id"], manifest["name"],
            manifest["version"], "SDK Test", "2" if profile == "interface-consumer" else "1")

    # The installed pure C example has no C++ compiler or engine requirement.
    c_build = work / "pure-c-build"
    print(run(args.cmake, *generator_options, "-S", str(sdk / "examples/native-c-mod"), "-B", str(c_build),
              f"-DCMAKE_PREFIX_PATH={sdk}", f"-DCMAKE_BUILD_TYPE={args.configuration}"))
    print(run(args.cmake, "--build", str(c_build), "--config", args.configuration))
    c_module = next(c_build.rglob("BMLCExample.bmodp"))
    c_manifest = json.loads(Path(str(c_module) + ".abi.json").read_text())
    assert c_manifest["extensions"] == {}

    friendly_build = work / "native-example-build"
    print(run(args.cmake, *generator_options, "-S", str(sdk / "examples/native-mod"), "-B", str(friendly_build),
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
    print(run(args.cmake, *generator_options, "-S", str(fixture), "-B", str(build),
              f"-DCMAKE_PREFIX_PATH={prefixes}", f"-DCMAKE_BUILD_TYPE={args.configuration}",
              *(f"-D{name}_DIR={path}" for name, path in args.package_dir)))
    print(run(args.cmake, "--build", str(build), "--config", args.configuration))
    deployed_modules = []
    for target in ("Core", "Extended"):
        module = next(build.rglob(target + ".bmodp"))
        descriptor = json.loads(Path(str(module) + ".abi.json").read_text())
        assert descriptor["extensions"] == ({} if target == "Core" else abi["extension_abis"])
        deployed = host_mods / module.name
        shutil.copy2(module, deployed)
        deployed_modules.append(str(deployed))
    run(*probe_command, "isolation", *deployed_modules)

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
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
