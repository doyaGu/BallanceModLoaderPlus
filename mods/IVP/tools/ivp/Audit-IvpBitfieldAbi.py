#!/usr/bin/env python3
"""Audit IVP bitfields with the i686 MSVC C++ layout rules.

The declarations are parsed from the reconstructed public umbrella.  A fixed
manifest records the layouts established from the Ballance IDB and retained
field accesses; the compiler, rather than a source-text regex, supplies the
actual byte/bit allocation.  This does not promote imported IDA types to ground
truth.  The independent IDB readback lives in Audit-IvpIdbBitfields.py.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


# owner, field -> (C++ type spelling, width, byte offset, first bit, last bit)
EXPECTED = {
    ("IVP_VHash", "nelems"): ("std::uint32_t", 24, 0x08, 0, 23),
    ("IVP_VHash", "dont_free"): ("std::uint32_t", 8, 0x0B, 0, 7),
    ("IVP_Compact_Mopp", "max_factor_surface_deviation"):
        ("unsigned int", 8, 0x1C, 0, 7),
    ("IVP_Compact_Mopp", "byte_size"): ("int", 24, 0x1D, 0, 23),
    ("IVP_Impact_Solver_Long_Term", "coll_time_is_valid"):
        ("IVP_BOOL", 8, 0x5C, 0, 7),
    ("IVP_Impact_Solver_Long_Term", "friction_is_broken"):
        ("IVP_BOOL", 2, 0x5D, 0, 1),
    ("IVP_Impact_Solver_Long_Term", "reserved_flags"):
        ("IVP_BOOL", 22, 0x5D, 2, 23),
    ("IVP_Contact_Point", "two_friction_values"):
        ("IVP_BOOL", 8, 0x34, 0, 7),
    ("IVP_Contact_Point", "cp_status"):
        ("IVP_CONTACT_POINT_BREAK_STATUS", 8, 0x60, 0, 7),
    ("IVP_Friction_System", "union_find_necessary"):
        ("IVP_BOOL", 8, 0x44, 0, 7),
    ("IVP_Friction_System", "fr_sys_simulated"):
        ("IVP_BOOL", 8, 0x45, 0, 7),
    ("IVP_Simulation_Unit", "sim_unit_movement_type"):
        ("IVP_Movement_Type", 8, 0x00, 0, 7),
    ("IVP_Simulation_Unit", "union_find_needed_for_sim_unit"):
        ("IVP_BOOL", 2, 0x01, 0, 1),
    ("IVP_Simulation_Unit", "sim_unit_has_fast_objects"):
        ("IVP_BOOL", 2, 0x01, 2, 3),
    ("IVP_Simulation_Unit", "sim_unit_just_slowed_down"):
        ("IVP_BOOL", 2, 0x01, 4, 5),
    ("IVP_Constraint", "is_enabled"):
        ("std::uint32_t", 2, 0x04, 0, 1),
    ("IVP_Constraint_Local", "norm"):
        ("IVP_NORM", 8, 0x18C, 0, 7),
    ("IVP_Multidimensional_Interpolator", "nr_of_vectors"):
        ("int", 8, 0x08, 0, 7),
    ("IVP_Multidimensional_Interpolator", "nr_of_elements_input"):
        ("int", 8, 0x09, 0, 7),
    ("IVP_Multidimensional_Interpolator", "nr_of_elements_solution"):
        ("int", 8, 0x0A, 0, 7),
    ("IVP_Actuator_Force", "push_first_object"):
        ("IVP_BOOL", 1, 0x74, 0, 0),
    ("IVP_Actuator_Force", "push_second_object"):
        ("IVP_BOOL", 1, 0x74, 1, 1),
}


def clang_command(
    clang: Path, include_root: Path, bml_include_root: Path,
) -> list[str]:
    return [
        str(clang),
        "--target=i686-pc-windows-msvc",
        "-x", "c++", "-std=c++17", "-fms-extensions",
        "-fno-delayed-template-parsing", "-Wno-everything",
        "-ferror-limit=0", "-DWIN32", "-DNDEBUG",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        # LLVM 18 predates the newest installed MSVC STL spelling.
        "-D__builtin_verbose_trap(...)=__builtin_trap()",
        f"-I{include_root}",
        f"-I{bml_include_root}",
    ]


def run_clang(command: list[str], source: str) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, input=source, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"clang failed with exit {result.returncode}:\n{result.stderr[-4000:]}"
        )
    return result


def collect_layouts(
    output: str,
) -> dict[tuple[str, str], tuple[str, int, int, int, int]]:
    layouts: dict[tuple[str, str], tuple[str, int, int, int, int]] = {}
    owners = {owner for owner, _ in EXPECTED}
    for chunk in output.split("*** Dumping AST Record Layout"):
        lines = chunk.strip().splitlines()
        if not lines:
            continue
        match = re.match(r"^\s*0 \| (?:class|struct) (IVP_[A-Za-z0-9_]+)\s*$", lines[0])
        if not match or match.group(1) not in owners:
            continue
        owner = match.group(1)
        for line in lines[1:]:
            bitfield = re.match(
                r"^\s*(\d+):(\d+)-(\d+) \|\s+(.+?)\s+"
                r"([A-Za-z_][A-Za-z0-9_]*)\s*$",
                line,
            )
            if not bitfield:
                continue
            key = (owner, bitfield.group(5))
            if key in EXPECTED:
                byte, first_bit, last_bit = map(
                    int, bitfield.group(1, 2, 3)
                )
                layouts[key] = (
                    bitfield.group(4).strip(),
                    last_bit - first_bit + 1,
                    byte,
                    first_bit,
                    last_bit,
                )
    return layouts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--clang", type=Path,
                        default=Path(shutil.which("clang++") or "clang++"))
    parser.add_argument("--bml-include-root", type=Path, required=True)
    args = parser.parse_args()
    source_root = args.source_root.resolve()
    clang = args.clang.resolve()
    if not clang.is_file():
        raise RuntimeError(f"clang not found: {clang}")

    source = '#include "BML/IVP/IVP.h"\n'
    base = clang_command(
        clang, source_root / "include", args.bml_include_root.resolve(),
    )
    owners = sorted({owner for owner, _ in EXPECTED})
    layout_source = source + "int bml_ivp_bitfield_sizes[] = {" + \
        ",".join(f"sizeof({owner})" for owner in owners) + "};\n"
    layout_result = run_clang(
        base + ["-Xclang", "-fdump-record-layouts", "-fsyntax-only", "-"],
        layout_source,
    )
    layouts = collect_layouts(layout_result.stdout + layout_result.stderr)

    problems: list[str] = []
    for key, expected in EXPECTED.items():
        actual = layouts.get(key)
        wanted = expected
        if actual != wanted:
            problems.append(
                f"LAYOUT\t{key[0]}::{key[1]}\texpected={wanted}\tactual={actual}"
            )

    for problem in problems:
        print(problem)
    print(f"BITFIELD_DECLARATIONS\t{len(layouts)}")
    print(f"BITFIELD_OWNERS\t{len({owner for owner, _ in layouts})}")
    print(f"BITFIELD_LAYOUTS\t{len(layouts)}")
    print(f"PROBLEMS\t{len(problems)}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
