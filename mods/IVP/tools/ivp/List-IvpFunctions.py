"""List IDA function boundaries and names in an address range.

Run with idat.exe against a copied physics_RT database. Addresses are image
virtual addresses, not RVAs. The listing is evidence for locating unnamed
retail bodies; imported names remain non-authoritative.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_funcs
import ida_pro
import idautils
import idc


def main() -> None:
    arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
    if not arguments:
        arguments = idc.ARGV[1:]
    if len(arguments) != 2:
        print("USAGE\tList-IvpFunctions.py <start> <end>")
        ida_pro.qexit(2)
    start = int(arguments[0], 0)
    end = int(arguments[1], 0)
    ida_auto.auto_wait()
    for address in idautils.Functions(start, end):
        function = ida_funcs.get_func(address)
        if function is None:
            continue
        print(
            f"FUNCTION\t0x{address:08X}\t0x{function.end_ea:08X}\t"
            f"{function.end_ea - address}\t{idc.get_func_name(address)}\t"
            f"{idc.get_type(address) or ''}"
        )
    ida_pro.qexit(0)


main()
