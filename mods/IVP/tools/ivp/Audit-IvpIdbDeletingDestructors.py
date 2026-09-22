"""Audit typed IVP deleting destructors in the Ballance IDA database.

Run through idat.exe in batch mode. MSVC x86 scalar/vector deleting
destructors receive a 32-bit unsigned flags argument and return the original
object pointer. Imported ``char`` flags or an untyped ``void *this`` silently
change the public ABI even though RET 4 still looks plausible.
"""

from __future__ import annotations

import ida_auto
import ida_funcs
import ida_name
import ida_pro
import idautils
import idc


ida_auto.auto_wait()

records = []
problems = []
for address in idautils.Functions():
    name = ida_name.get_name(address) or ""
    if not (name.startswith("??_GIVP_") or name.startswith("??_EIVP_")):
        continue
    declaration = idc.get_type(address) or ""
    records.append((address, name, declaration))
    reasons = []
    if not declaration:
        reasons.append("missing-type")
    else:
        if " char" in declaration or ",char" in declaration:
            reasons.append("narrow-flags")
        if "void *this" in declaration or "void *self" in declaration:
            reasons.append("untyped-this")
        if not declaration.lstrip().startswith("void *"):
            reasons.append("non-pointer-return")
        if "unsigned int" not in declaration:
            reasons.append("missing-uint-flags")
    if reasons:
        problems.append((address, name, declaration, ",".join(reasons)))

print(f"DELETING_DESTRUCTORS\t{len(records)}")
print(f"PROBLEMS\t{len(problems)}")
for address, name, declaration, reasons in problems:
    print(f"PROBLEM\t0x{address:08X}\t{name}\t{declaration}\t{reasons}")

ida_pro.qexit(0)
