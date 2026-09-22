#!/usr/bin/env python3
"""Audit Ballance-version-specific Mindist source ABI.

The exact DLL table topology is checked independently by
Audit-IvpIdbVtables.py.  This compiler-AST pass prevents the public headers
from silently reintroducing virtual slots found only in neighboring IVP
sources.  The compiler record-layout pass additionally locks the retail
recursive-mindist secondary-base and tail-field offsets: these determine both
the secondary vptr location and the adjusted ``this`` passed across the DLL
boundary.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


EXPECTED = {
    ("IVP_Mindist", "simulate_time_event"): True,
    ("IVP_Mindist", "mindist_rescue_push"): True,
    ("IVP_Mindist", "is_recursive"): False,
    ("IVP_Mindist", "exact_mindist_went_invalid"): True,
    ("IVP_Mindist", "do_impact"): True,
    ("IVP_Mindist_Recursive", "collision_is_going_to_be_deleted_event"): True,
    ("IVP_Mindist_Recursive", "mindist_rescue_push"): True,
    ("IVP_Mindist_Recursive", "rec_hull_limit_exceeded_event"): False,
    ("IVP_Mindist_Recursive", "exact_mindist_went_invalid"): True,
    ("IVP_Mindist_Recursive", "do_impact"): True,
}

EXPECTED_RECURSIVE_LAYOUT = {
    "primary_base": 0x00,
    "secondary_base": 0x88,
    "recursive_status": 0x8C,
    "mindists": 0x90,
    "size": 0x98,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source-root", type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument(
        "--clang", type=Path, default=Path(shutil.which("clang++") or "clang++"),
    )
    parser.add_argument("--bml-include-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    clang = args.clang.resolve()
    if not clang.is_file():
        raise RuntimeError(f"clang not found: {clang}")

    command = [
        str(clang), "--target=i686-pc-windows-msvc",
        "-x", "c++", "-std=c++17", "-fms-extensions",
        "-fno-delayed-template-parsing", "-Wno-everything",
        "-ferror-limit=0", "-DWIN32", "-DNDEBUG",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        "-D__builtin_verbose_trap(...)=__builtin_trap()",
        f"-I{root / 'include'}",
        f"-I{args.bml_include_root.resolve()}",
        "-Xclang", "-ast-dump",
        "-Xclang", "-ast-dump-filter=IVP_Mindist",
        "-fsyntax-only", "-",
    ]
    completed = subprocess.run(
        command,
        input='#include "BML/IVP/IVP.h"\n',
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"clang failed with exit {completed.returncode}:\n"
            f"{completed.stderr[-4000:]}"
        )

    actual: dict[tuple[str, str], bool] = {}
    ast_output = completed.stdout + completed.stderr
    block_pattern = re.compile(
        r"(?m)^Dumping (IVP_Mindist(?:_Recursive)?):\r?$"
    )
    block_matches = list(block_pattern.finditer(ast_output))
    for index, block_match in enumerate(block_matches):
        owner = block_match.group(1)
        block_end = (
            block_matches[index + 1].start()
            if index + 1 < len(block_matches)
            else len(ast_output)
        )
        lines = ast_output[block_match.end():block_end].splitlines()
        for line_index, line in enumerate(lines):
            if "CXXMethodDecl" not in line:
                continue
            for expected_owner, method in EXPECTED:
                if expected_owner != owner:
                    continue
                if not re.search(rf"\b{re.escape(method)}\s+'", line):
                    continue
                following = "\n".join(lines[line_index + 1:line_index + 4])
                actual[(owner, method)] = (
                    " virtual " in f" {line} " or "Overrides:" in following
                )
    problems = []
    for key, expected in EXPECTED.items():
        observed = actual.get(key)
        if observed is not expected:
            problems.append(
                f"{key[0]}::{key[1]} expected virtual={expected}, "
                f"actual={observed}"
            )

    layout_command = [
        str(clang), "--target=i686-pc-windows-msvc",
        "-x", "c++", "-std=c++17", "-fms-extensions",
        "-fno-delayed-template-parsing", "-Wno-everything",
        "-ferror-limit=0", "-DWIN32", "-DNDEBUG",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        "-D__builtin_verbose_trap(...)=__builtin_trap()",
        f"-I{root / 'include'}",
        f"-I{args.bml_include_root.resolve()}",
        "-Xclang", "-fdump-record-layouts", "-fsyntax-only", "-",
    ]
    layout_completed = subprocess.run(
        layout_command,
        input=(
            '#include "BML/IVP/Mindist.h"\n'
            "int bml_force_recursive_mindist_layout = "
            "sizeof(IVP_Mindist_Recursive);\n"
        ),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if layout_completed.returncode != 0:
        raise RuntimeError(
            f"clang layout dump failed with exit "
            f"{layout_completed.returncode}:\n"
            f"{layout_completed.stderr[-4000:]}"
        )

    layout_output = layout_completed.stdout + layout_completed.stderr
    layout_match = re.search(
        r"(?ms)^\s*0 \| class IVP_Mindist_Recursive\r?\n"
        r"(?P<body>.*?)"
        r"^\s*\| \[sizeof=(?P<size>\d+), align=(?P<align>\d+),\r?\n"
        r"^\s*\|\s+nvsize=(?P<nvsize>\d+), nvalign=(?P<nvalign>\d+)\]",
        layout_output,
    )
    observed_layout: dict[str, int] = {}
    if layout_match is None:
        problems.append("compiler did not emit IVP_Mindist_Recursive layout")
    else:
        body = layout_match.group("body")
        layout_patterns = {
            "primary_base":
                r"(?m)^\s*(\d+) \|   class IVP_Mindist \(primary base\)$",
            "secondary_base":
                r"(?m)^\s*(\d+) \|   class IVP_Collision_Delegator \(base\)$",
            "recursive_status":
                r"(?m)^\s*(\d+) \|   IVP_MINDIST_RECURSIVE_TYPES recursive_status$",
            "mindists":
                r"(?m)^\s*(\d+) \|   class IVP_U_FVector<class IVP_Collision> mindists$",
        }
        for name, pattern in layout_patterns.items():
            field_match = re.search(pattern, body)
            if field_match is None:
                problems.append(f"compiler layout omitted {name}")
            else:
                observed_layout[name] = int(field_match.group(1))
        observed_layout["size"] = int(layout_match.group("size"))

    for name, expected in EXPECTED_RECURSIVE_LAYOUT.items():
        observed = observed_layout.get(name)
        if observed != expected:
            problems.append(
                f"IVP_Mindist_Recursive {name} expected 0x{expected:X}, "
                f"actual={None if observed is None else f'0x{observed:X}'}"
            )

    for problem in problems:
        print(f"PROBLEM\t{problem}")
    print(f"VTABLE_SURFACE_METHODS\t{len(actual)}")
    print(
        "MINDIST_RECURSIVE_LAYOUT\t"
        + "\t".join(
            f"{name}=0x{observed_layout[name]:X}"
            for name in EXPECTED_RECURSIVE_LAYOUT
            if name in observed_layout
        )
    )
    print(f"PROBLEMS\t{len(problems)}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
