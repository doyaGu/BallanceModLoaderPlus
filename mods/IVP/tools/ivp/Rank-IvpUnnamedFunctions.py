"""Rank unnamed retail functions by distinct incoming code references.

Run through Invoke-IvpIdbReadOnly.ps1.  Imported IDA names are only a search
aid: the report deliberately includes decoded callers and function sizes so a
candidate still has to be identified from machine-code evidence.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_funcs
import ida_lines
import ida_name
import ida_pro
import idautils
import idc


def is_unnamed(name: str) -> bool:
    return name.startswith(("sub_", "nullsub_", "unknown_libname_"))


def main() -> None:
    arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
    if not arguments:
        arguments = idc.ARGV[1:]
    if len(arguments) not in (2, 3):
        print(
            "USAGE\tRank-IvpUnnamedFunctions.py <start> <end> "
            "[minimum-distinct-callers]"
        )
        ida_pro.qexit(2)

    start = int(arguments[0], 0)
    end = int(arguments[1], 0)
    minimum_callers = int(arguments[2], 0) if len(arguments) == 3 else 2
    ida_auto.auto_wait()

    ranked = []
    for address in idautils.Functions(start, end):
        name = ida_name.get_name(address) or idc.get_func_name(address)
        if not is_unnamed(name):
            continue
        function = ida_funcs.get_func(address)
        if function is None:
            continue

        callers = {}
        for reference in idautils.CodeRefsTo(address, False):
            caller = ida_funcs.get_func_start(reference)
            if caller == idc.BADADDR:
                continue
            callers.setdefault(caller, []).append(reference)
        if len(callers) < minimum_callers:
            continue
        ranked.append(
            (
                -len(callers),
                -(function.end_ea - address),
                address,
                function.end_ea,
                name,
                callers,
            )
        )

    for _, _, address, end_address, name, callers in sorted(ranked):
        print(
            f"CANDIDATE\t0x{address:08X}\t0x{end_address:08X}\t"
            f"{end_address - address}\t{len(callers)}\t{name}\t"
            f"{idc.get_type(address) or ''}"
        )
        for caller, references in sorted(callers.items()):
            sites = ",".join(f"0x{site:08X}" for site in references)
            line = ida_lines.generate_disasm_line(references[0], 0) or ""
            print(
                f"CALLER\t0x{caller:08X}\t{idc.get_func_name(caller)}\t"
                f"{sites}\t{ida_lines.tag_remove(line)}"
            )

    ida_pro.qexit(0)


main()
