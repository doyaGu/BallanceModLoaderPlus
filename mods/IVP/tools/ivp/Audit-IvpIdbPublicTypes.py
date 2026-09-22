"""Report exact-name public IVP bodies whose IDB prototype is empty.

Run through idat.exe in batch mode against the Ballance physics_RT database.
The input is the methods TSV emitted by Audit-IvpPublicInterface.py. Only rows
whose nearby decorated name resolves exactly in the corrected IDB are included;
nearby-source and inferred names are deliberately outside this audit.
"""

from __future__ import annotations

import csv
import json
import os
from pathlib import Path

import ida_auto
import ida_idaapi
import ida_name
import ida_pro
import idc


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
audit_path = Path(arguments[0] if arguments else
                  "build-dev/ivp-methods-current.tsv")
with audit_path.open("r", encoding="utf-8-sig", newline="") as stream:
    audit_rows = list(csv.DictReader(stream, delimiter="\t"))

records = {
    row["nearby_mangled_name"]: (row["class"], row["method"])
    for row in audit_rows
    if row["retail_exact_name"] == "true" and row["nearby_mangled_name"]
}

missing = []
unresolved = []
for mangled, (owner, method) in sorted(records.items()):
    address = ida_name.get_name_ea(ida_idaapi.BADADDR, mangled)
    if address == ida_idaapi.BADADDR:
        unresolved.append((mangled, owner, method))
    elif not (idc.get_type(address) or ""):
        missing.append((address, mangled, owner, method))

print(f"PUBLIC_EXACT_TYPES\t{len(records)}")
print(f"MISSING_TYPES\t{len(missing)}")
print(f"UNRESOLVED_NAMES\t{len(unresolved)}")
for address, mangled, owner, method in missing:
    print(f"MISSING\t0x{address:08X}\t{mangled}\t{owner}::{method}")
for mangled, owner, method in unresolved:
    print(f"UNRESOLVED\t{mangled}\t{owner}::{method}")

ida_pro.qexit(0)
