#!/usr/bin/env python3
"""Verify that bml_add_mod rejects native Mods without loader entry points."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def quote_cmake(path: Path) -> str:
    return path.resolve().as_posix().replace('"', '\\"')


def missing_msvc_environment(generator: str) -> bool:
    """Report whether this generator needs a developer environment it lacks.

    Visual Studio generators locate the toolchain themselves. Ninja and
    Makefile generators invoke cl.exe directly and need the include and library
    search paths that vcvarsall exports, so without them the nested configure
    fails while detecting the compiler rather than while testing bml_add_mod.
    """
    if generator.startswith("Visual Studio"):
        return False
    return not os.environ.get("INCLUDE")


def run(command: list[str], *, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if (result.returncode == 0) != expect_success:
        raise AssertionError(
            f"command {'failed' if expect_success else 'unexpectedly succeeded'} "
            f"({result.returncode}): {' '.join(command)}\n{result.stdout}"
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--generator-platform", default="")
    parser.add_argument("--cxx-compiler", default="")
    args = parser.parse_args()

    if missing_msvc_environment(args.generator):
        print(
            f"BML_TEST_SKIPPED: the {args.generator} generator needs an MSVC "
            "developer environment; INCLUDE is not set."
        )
        return 0

    source_root = args.source_root.resolve()
    with tempfile.TemporaryDirectory(prefix="bml-mod-exports-") as temporary:
        root = Path(temporary)
        source = root / "source"
        build = root / "build"
        source.mkdir()

        (source / "missing_entry.cpp").write_text(
            'extern "C" __declspec(dllexport) void BMLExit(void *) {}\n',
            encoding="utf-8",
        )
        (source / "missing_exit.cpp").write_text(
            'extern "C" __declspec(dllexport) void *BMLEntry(void *) { return nullptr; }\n',
            encoding="utf-8",
        )
        # Mirror a packaged SDK consumer: a pinned MSVC runtime and a minimum
        # CMake version high enough for CMP0091.
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.15)\n"
            "project(BmlModRequiredExports LANGUAGES CXX)\n"
            "add_library(BML::BML INTERFACE IMPORTED)\n"
            'set(BML_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")\n'
            f'include("{quote_cmake(source_root / "cmake" / "BMLMod.cmake")}")\n'
            "bml_add_mod(MissingEntry missing_entry.cpp)\n"
            "bml_add_mod(MissingExit missing_exit.cpp)\n",
            encoding="utf-8",
        )

        configure = [
            str(args.cmake),
            "-S",
            str(source),
            "-B",
            str(build),
            "-G",
            args.generator,
        ]
        if args.generator_platform:
            configure.extend(["-A", args.generator_platform])
        # Without this the nested configure resolves the compiler from PATH,
        # which silently swaps the toolchain this test exists to validate.
        if args.cxx_compiler:
            configure.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")
        run(configure)

        for target, missing_symbol in (
            ("MissingEntry", "BMLEntry"),
            ("MissingExit", "BMLExit"),
        ):
            result = run(
                [
                    str(args.cmake),
                    "--build",
                    str(build),
                    "--config",
                    "Release",
                    "--target",
                    target,
                ],
                expect_success=False,
            )
            if missing_symbol not in result.stdout:
                raise AssertionError(
                    f"{target} failed without naming required symbol {missing_symbol}:\n"
                    f"{result.stdout}"
                )

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1) from error
