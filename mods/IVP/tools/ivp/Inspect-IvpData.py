"""Print names, types, bytes, and references for retail IVP data addresses."""

from __future__ import annotations

import json
import os

import ida_auto
import ida_bytes
import ida_idaapi
import ida_name
import ida_pro
import idautils
import idc


def resolve(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError:
        return ida_name.get_name_ea(ida_idaapi.BADADDR, value)


def inspect(query: str) -> None:
    address = resolve(query)
    if address == ida_idaapi.BADADDR:
        print(f"NOT_FOUND\t{query}")
        return
    value = ida_bytes.get_bytes(address, 16) or b""
    print(
        f"DATA\t0x{address:08X}\t{ida_name.get_name(address)}\t"
        f"{idc.get_type(address) or ''}\t{value.hex().upper()}"
    )
    for reference in idautils.XrefsTo(address):
        print(f"XREF\t0x{reference.frm:08X}\t{reference.type}")


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
for argument in arguments:
    inspect(argument)
ida_pro.qexit(0)
