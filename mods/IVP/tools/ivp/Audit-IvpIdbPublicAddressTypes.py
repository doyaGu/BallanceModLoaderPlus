"""Audit every retail function address used by the public IVP headers.

This is the reverse-closure counterpart to the curated dependency audit: the
headers are scanned first, then every Address::* use must resolve through the
retail contract to a function RVA with an IDB prototype.  Address aliases are
counted once at the RVA boundary.  IDB types remain evidence, not ground truth.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

import ida_auto
import ida_pro
import idc


ida_auto.auto_wait()

root = Path.cwd()
contract = json.loads(
    (root / "tools/ivp/retail-contract.json").read_text(encoding="utf-8")
)
address_by_id = {
    entry["id"]: int(entry["rva"], 16) for entry in contract["addresses"]
}

used_ids: set[str] = set()
pattern = re.compile(r"\bAddress::([A-Za-z_][A-Za-z0-9_]*)\b")
for header in (root / "include/BML/IVP").rglob("*.h"):
    used_ids.update(pattern.findall(header.read_text(encoding="utf-8")))

unresolved_ids = sorted(used_ids - address_by_id.keys())
rvas = sorted({address_by_id[name] for name in used_ids if name in address_by_id})
missing_types = []
declarations: dict[int, str] = {}
for rva in rvas:
    address = 0x10000000 + rva
    declaration = idc.get_type(address) or ""
    declarations[rva] = declaration
    if not declaration:
        missing_types.append((rva, idc.get_func_name(address)))

abi_problems = []
random_declaration = declarations.get(0x0002FCD0, "").lower()
if ("float" not in random_declaration or "double" in random_declaration or
        "__cdecl" not in random_declaration):
    abi_problems.append((0x0002FCD0, random_declaration))

print(f"PUBLIC_ADDRESS_IDS\t{len(used_ids)}")
print(f"PUBLIC_ADDRESS_RVAS\t{len(rvas)}")
print(f"UNRESOLVED_IDS\t{len(unresolved_ids)}")
print(f"MISSING_TYPES\t{len(missing_types)}")
print(f"PUBLIC_ADDRESS_ABI_PROBLEMS\t{len(abi_problems)}")
for name in unresolved_ids:
    print(f"UNRESOLVED\t{name}")
for rva, name in missing_types:
    print(f"MISSING\t0x{rva:08X}\t{name}")
for rva, declaration in abi_problems:
    print(f"PROBLEM\t0x{rva:08X}\t{declaration}")

ida_pro.qexit(0)
