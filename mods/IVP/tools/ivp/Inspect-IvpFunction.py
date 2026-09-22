"""Print IDA's type and decoded instructions for named retail functions.

Run through idat.exe with the copied Ballance physics_RT database. Function
names are accepted as either IDA display names, mangled names, or hexadecimal
addresses. The database is evidence, not an authority: callers still need to
compare the decoded field accesses with the retail image and nearby source.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_idaapi
import ida_lines
import ida_name
import ida_pro
import ida_typeinf
import idautils
import idc


def resolve_function(value: str):
    try:
        address = int(value, 0)
    except ValueError:
        address = ida_name.get_name_ea(ida_idaapi.BADADDR, value)
        if address == ida_idaapi.BADADDR:
            for function_address in idautils.Functions():
                if idc.get_func_name(function_address) == value:
                    address = function_address
                    break
    if address == ida_idaapi.BADADDR:
        return None
    start = ida_funcs.get_func_start(address)
    return None if start == ida_idaapi.BADADDR else start


def print_function(query: str) -> None:
    function = resolve_function(query)
    if function is None:
        print(f"NOT_FOUND\t{query}")
        return

    address = function
    name = idc.get_func_name(address)
    declaration = idc.get_type(address) or ""
    comment = ida_bytes.get_cmt(address, True) or ""
    print(f"FUNCTION\t0x{address:08X}\t{name}")
    print(f"TYPE\t{declaration}")
    if comment:
        print(f"COMMENT\t{comment}")
    for item_address in idautils.FuncItems(address):
        line = ida_lines.generate_disasm_line(item_address, 0) or ""
        print(
            f"0x{item_address:08X}\t"
            f"{ida_lines.tag_remove(line)}"
        )


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
for argument in arguments:
    print_function(argument)
ida_pro.qexit(0)
