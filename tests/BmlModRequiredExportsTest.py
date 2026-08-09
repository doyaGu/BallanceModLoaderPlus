#!/usr/bin/env python3
"""Verify that bml_add_mod rejects native Mods without loader entry points."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def quote_cmake(path: Path) -> str:
    return path.resolve().as_posix().replace('"', '\\"')


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
    args = parser.parse_args()

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
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(BmlModRequiredExports LANGUAGES CXX)\n"
            "add_library(BML::BML INTERFACE IMPORTED)\n"
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
