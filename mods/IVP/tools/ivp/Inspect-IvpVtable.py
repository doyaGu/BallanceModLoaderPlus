"""Print retail vtable entries and their decoded target instructions.

Run this through IDA's batch executable.  Names and imported types are hints;
the slot addresses and decoded instructions are the evidence this tool exposes.
Arguments are a vtable address and an optional slot count.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_idaapi
import ida_lines
import ida_nalt
import ida_name
import ida_pro
import idautils
import idc


def parse_address(value: str) -> int:
    try:
        address = int(value, 0)
    except ValueError:
        address = ida_name.get_name_ea(ida_idaapi.BADADDR, value)
        if address == ida_idaapi.BADADDR:
            raise ValueError(f"unknown vtable address or name: {value}")
    if address < ida_nalt.get_imagebase():
        address += ida_nalt.get_imagebase()
    return address


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
if not arguments:
    raise ValueError("expected a vtable address or name")
table = parse_address(arguments[0])
count = int(arguments[1], 0) if len(arguments) > 1 else 32
print(f"VTABLE\t0x{table:08X}\t{count}")
for slot in range(count):
    entry = table + slot * 4
    target = ida_bytes.get_dword(entry)
    name = idc.get_name(target) or idc.get_func_name(target)
    function = ida_funcs.get_func(target)
    print(f"SLOT\t{slot}\t0x{entry:08X}\t0x{target:08X}\t{name}")
    if function is None:
        continue
    for index, item in enumerate(idautils.FuncItems(function.start_ea)):
        if index == 8:
            break
        line = ida_lines.generate_disasm_line(item, 0) or ""
        print(f"INSN\t0x{item:08X}\t{ida_lines.tag_remove(line)}")
ida_pro.qexit(0)
