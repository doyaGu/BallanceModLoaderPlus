"""Audit typed IVP complete constructors in the Ballance IDA database.

Run through idat.exe in batch mode. This audit checks only facts encoded by
the retained constructor symbol and MSVC x86 implementation ABI: a concrete
owner pointer returned in EAX, __thiscall, and the same owner pointer in ECX.
Explicit parameters are intentionally left to decorated-name/RET/callsite
analysis rather than inferred from imported source types.
"""

from __future__ import annotations

import re

import ida_auto
import ida_name
import ida_pro
import idautils
import idc


ida_auto.auto_wait()

records = []
problems = []
for address in idautils.Functions():
    name = ida_name.get_name(address) or ""
    match = re.match(r"^\?\?0(IVP_[^@]+)@@", name)
    if match is None:
        continue

    owner = match.group(1)
    declaration = idc.get_type(address) or ""
    records.append((address, name, declaration))
    reasons = []
    if not declaration:
        reasons.append("missing-type")
    else:
        normalized = " ".join(declaration.split())
        return_prefixes = (f"{owner} *__thiscall", f"struct {owner} *__thiscall")
        if not normalized.startswith(return_prefixes):
            reasons.append("return-owner-mismatch")
        if "__thiscall" not in normalized:
            reasons.append("non-thiscall")
        if "void *this" in normalized or "void *self" in normalized:
            reasons.append("untyped-this")
        if normalized.count(owner) < 2:
            reasons.append("this-owner-mismatch")
    if reasons:
        problems.append((address, name, declaration, ",".join(reasons)))

print(f"CONSTRUCTORS\t{len(records)}")
print(f"PROBLEMS\t{len(problems)}")
for address, name, declaration, reasons in problems:
    print(f"PROBLEM\t0x{address:08X}\t{name}\t{declaration}\t{reasons}")

ida_pro.qexit(0)
