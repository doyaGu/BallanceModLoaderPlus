"""Print code and data cross-references to retail IVP addresses.

Run with idat.exe against a copied physics_RT database. Arguments can be
hexadecimal virtual addresses or current IDA names. Imported names and types
remain clues; the decoded caller and operand are the evidence reported here.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_funcs
import ida_idaapi
import ida_lines
import ida_name
import ida_pro
import idautils
import idc


def resolve(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError:
        return ida_name.get_name_ea(ida_idaapi.BADADDR, value)


def print_xrefs(query: str) -> None:
    address = resolve(query)
    if address == ida_idaapi.BADADDR:
        print(f"NOT_FOUND\t{query}")
        return

    print(f"TARGET\t0x{address:08X}\t{ida_name.get_name(address)}")
    for reference in idautils.XrefsTo(address):
        function_address = ida_funcs.get_func_start(reference.frm)
        function_name = (
            idc.get_func_name(function_address)
            if function_address != ida_idaapi.BADADDR else ""
        )
        line = ida_lines.generate_disasm_line(reference.frm, 0) or ""
        print(
            f"XREF\t0x{reference.frm:08X}\t{reference.type}\t"
            f"0x{function_address:08X}\t{function_name}\t"
            f"{ida_lines.tag_remove(line)}"
        )


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
for argument in arguments:
    print_xrefs(argument)
ida_pro.qexit(0)
