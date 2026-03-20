#!/usr/bin/env python3
"""
package_sdk.py - BML SDK Packaging Tool

Assembles a standalone SDK zip from the build tree and source tree.
The SDK allows third-party mod authors to create BML modules without
cloning the full repository.

Usage:
    python tools/package_sdk.py --build-dir build --config Release
    python tools/package_sdk.py --build-dir build --config Release --output BML_SDK.zip
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path
from typing import Optional, List


def find_repo_root() -> Path:
    p = Path(__file__).resolve().parent.parent
    if (p / "CMakeLists.txt").exists() and (p / "cmake").is_dir():
        return p
    return Path.cwd()


def read_version(repo_root: Path) -> str:
    """Extract version from root CMakeLists.txt."""
    cmake_file = repo_root / "CMakeLists.txt"
    for line in cmake_file.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("VERSION") and not line.startswith("VERSION_"):
            parts = line.split()
            if len(parts) >= 2:
                return parts[1]
        if "VERSION" in line and "project(" in line:
            # project(... VERSION 0.4.0 ...)
            import re
            m = re.search(r"VERSION\s+(\d+\.\d+\.\d+)", line)
            if m:
                return m.group(1)
    return "0.0.0"


def copy_tree(src: Path, dst: Path, patterns: Optional[List[str]] = None) -> int:
    """Copy directory tree, optionally filtering by glob patterns. Returns file count."""
    count = 0
    if patterns:
        for pattern in patterns:
            for f in src.rglob(pattern):
                if f.is_file():
                    rel = f.relative_to(src)
                    dest = dst / rel
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(f, dest)
                    count += 1
    else:
        if src.is_dir():
            shutil.copytree(src, dst, dirs_exist_ok=True)
            count = sum(1 for _ in dst.rglob("*") if _.is_file())
    return count


def package_sdk(
    repo_root: Path,
    build_dir: Path,
    config: str,
    output_path: Optional[Path],
) -> int:
    version = read_version(repo_root)
    sdk_name = f"BML_SDK_v{version}"

    staging = build_dir / "sdk_staging" / sdk_name
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    total = 0

    # 1. Core headers from include/
    print("Copying core headers...")
    core_inc = staging / "include"
    n = copy_tree(repo_root / "include", core_inc, ["*.h", "*.hpp", "*.inc", "*.inl"])
    total += n
    print(f"  {n} files")

    # 2. Module public headers
    modules_with_api = [
        "BML_Input", "BML_UI", "BML_Console", "BML_Menu",
        "BML_NewBallType", "BML_Event",
    ]
    for mod_name in modules_with_api:
        mod_inc = repo_root / "modules" / mod_name / "include"
        if mod_inc.is_dir():
            dst = staging / "modules" / mod_name / "include"
            n = copy_tree(mod_inc, dst, ["*.h", "*.hpp", "*.inc", "*.inl"])
            total += n
            print(f"  {mod_name}: {n} headers")

    # 3. Import library (BML.lib)
    lib_dir = build_dir / "lib" / config
    bml_lib = lib_dir / "BML.lib"
    if bml_lib.exists():
        dst = staging / "lib"
        dst.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bml_lib, dst / "BML.lib")
        total += 1
        print(f"  BML.lib copied")
    else:
        print(f"  Warning: BML.lib not found at {bml_lib}")

    # 4. CMake modules
    print("Copying CMake modules...")
    cmake_dst = staging / "lib" / "cmake" / "BML"
    cmake_dst.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo_root / "cmake" / "BMLMod.cmake", cmake_dst / "BMLMod.cmake")
    total += 1

    # BMLConfig.cmake and BMLConfigVersion.cmake from build dir
    for f in ["BMLConfig.cmake", "BMLConfigVersion.cmake"]:
        src = build_dir / f
        if src.exists():
            shutil.copy2(src, cmake_dst / f)
            total += 1
        else:
            print(f"  Warning: {f} not found in build dir")

    # 5. Templates
    print("Copying templates...")
    n = copy_tree(repo_root / "templates", staging / "share" / "BML" / "templates")
    total += n
    print(f"  {n} files")

    # 6. Examples (native DLL and script mods)
    print("Copying examples...")
    examples_dst = staging / "share" / "BML" / "examples"
    example_exts = {".cpp", ".h", ".hpp", ".toml", ".txt", ".md", ".as"}
    for example_dir in (repo_root / "examples").iterdir():
        if not example_dir.is_dir():
            continue
        # Include if it has CMakeLists.txt (native) or mod.toml (script)
        has_cmake = (example_dir / "CMakeLists.txt").exists()
        has_manifest = (example_dir / "mod.toml").exists()
        if not has_cmake and not has_manifest:
            continue
        dst = examples_dst / example_dir.name
        dst.mkdir(parents=True, exist_ok=True)
        for f in example_dir.iterdir():
            if f.is_file() and f.suffix in example_exts:
                shutil.copy2(f, dst / f.name)
                total += 1

    # 7. Documentation
    print("Copying docs...")
    docs_dst = staging / "share" / "BML" / "docs"
    docs_dst.mkdir(parents=True, exist_ok=True)
    for doc in [
        repo_root / "docs" / "mod-manifest-schema.md",
        repo_root / "docs" / "api" / "scripting-api.md",
    ]:
        if doc.exists():
            shutil.copy2(doc, docs_dst / doc.name)
            total += 1

    # 7b. Script development files
    bml_api_stub = repo_root / "modules" / "BML_Scripting" / "include" / "bml_api.as"
    if bml_api_stub.exists():
        script_dst = staging / "share" / "BML" / "scripting"
        script_dst.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bml_api_stub, script_dst / "bml_api.as")
        total += 1

    # 8. Tools
    print("Copying tools...")
    tools_dst = staging / "tools"
    tools_dst.mkdir(parents=True, exist_ok=True)
    create_mod = repo_root / "tools" / "bml_create_mod.py"
    if create_mod.exists():
        shutil.copy2(create_mod, tools_dst / create_mod.name)
        total += 1

    # 9. Create zip
    if output_path is None:
        output_path = build_dir / f"{sdk_name}.zip"
    else:
        output_path = Path(output_path)

    print(f"\nPackaging {total} files into {output_path}...")
    archive_base = output_path.with_suffix("")
    shutil.make_archive(str(archive_base), "zip", staging.parent, sdk_name)

    final_size = output_path.stat().st_size
    print(f"SDK packaged: {output_path} ({final_size:,} bytes)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Package BML SDK for distribution.")
    parser.add_argument(
        "--build-dir",
        default="build",
        help="CMake build directory (default: build)",
    )
    parser.add_argument(
        "--config",
        default="Release",
        help="Build configuration (default: Release)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output zip path (default: <build-dir>/BML_SDK_v<version>.zip)",
    )

    args = parser.parse_args()
    repo_root = find_repo_root()
    build_dir = Path(args.build_dir).resolve()

    if not build_dir.exists():
        print(f"Error: Build directory '{build_dir}' does not exist.", file=sys.stderr)
        return 1

    return package_sdk(repo_root, build_dir, args.config, args.output)


if __name__ == "__main__":
    sys.exit(main())
