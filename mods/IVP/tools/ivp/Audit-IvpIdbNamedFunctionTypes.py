"""Report named IVP functions whose IDB prototype is empty.

This is the broad internal counterpart to the bounded public/direct audits.
It includes decorated names containing IVP_ and manually recovered ivp_
functions, but excludes unnamed functions and bundled qhull/runtime helpers.
Names remain evidence to be checked, not proof that an imported type is right.
"""

from __future__ import annotations

import ida_auto
import ida_name
import ida_pro
import idautils
import idc


ida_auto.auto_wait()

records = []
missing = []
for address in idautils.Functions():
    name = ida_name.get_name(address) or ""
    if "IVP_" not in name and not name.lower().startswith("ivp_"):
        continue
    declaration = idc.get_type(address) or ""
    records.append((address, name, declaration))
    if not declaration:
        missing.append((address, name))

print(f"NAMED_IVP_FUNCTIONS\t{len(records)}")
print(f"MISSING_TYPES\t{len(missing)}")
for address, name in missing:
    print(f"MISSING\t0x{address:08X}\t{name}")

ida_pro.qexit(0)
