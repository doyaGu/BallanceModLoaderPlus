"""Audit typed IVP complete-object destructors in the Ballance IDA database.

Run through idat.exe in batch mode. A complete destructor has a void return,
uses __thiscall, receives one concrete owner pointer in ECX, and has no explicit
stack argument. This is deliberately a prototype audit: a matching imported
type does not by itself prove that the attached owner name is correct.
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
    match = re.match(r"^\?\?1(IVP_[^@]+)@@", name)
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
        if not normalized.startswith("void __thiscall"):
            reasons.append("non-void-thiscall")
        if "void *this" in normalized or "void *self" in normalized:
            reasons.append("untyped-this")
        if owner not in normalized:
            reasons.append("owner-mismatch")
        arguments = normalized.rsplit("(", 1)[-1].split(")", 1)[0]
        if "," in arguments:
            reasons.append("unexpected-stack-argument")
    if reasons:
        problems.append((address, name, declaration, ",".join(reasons)))

print(f"COMPLETE_DESTRUCTORS\t{len(records)}")
print(f"PROBLEMS\t{len(problems)}")
for address, name, declaration, reasons in problems:
    print(f"PROBLEM\t0x{address:08X}\t{name}\t{declaration}\t{reasons}")

ida_pro.qexit(0)
