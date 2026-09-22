"""Find decoded instruction text across the Ballance physics_RT IDB.

Run through Invoke-IvpIdbReadOnly.ps1. Each argument is matched as a
case-insensitive substring after IDA tags are removed; the containing function
is printed once before its matching instructions. This is an evidence search,
not a type inference step.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_lines
import ida_pro
import idautils
import idc


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
needles = tuple(argument.casefold() for argument in arguments if argument)
if not needles:
    raise ValueError("expected at least one instruction-text substring")

matches = 0
for function_address in idautils.Functions():
    function_matches: list[tuple[int, str]] = []
    for item_address in idautils.FuncItems(function_address):
        line = ida_lines.tag_remove(
            ida_lines.generate_disasm_line(item_address, 0) or ""
        )
        if any(needle in line.casefold() for needle in needles):
            function_matches.append((item_address, line))
    if not function_matches:
        continue
    print(
        f"FUNCTION\t0x{function_address:08X}\t"
        f"{idc.get_func_name(function_address)}"
    )
    for item_address, line in function_matches:
        print(f"INSN\t0x{item_address:08X}\t{line}")
        matches += 1

print(f"MATCHES\t{matches}")
ida_pro.qexit(0)
