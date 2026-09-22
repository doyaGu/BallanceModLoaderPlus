"""Report direct retail-contract functions whose IDB prototype is empty.

Run through idat.exe in batch mode against the Ballance physics_RT database.
The contract supplies the bounded RVA set; this script is read-only and emits
only missing prototypes so routine audits do not generate huge disassemblies.
"""

from __future__ import annotations

import json
from pathlib import Path

import ida_auto
import ida_pro
import idc


ida_auto.auto_wait()
contract_path = Path("tools/ivp/retail-contract.json")
contract = json.loads(contract_path.read_text(encoding="utf-8"))

missing = []
for dependency in contract["direct_dependencies"]:
    address = 0x10000000 + int(dependency["rva"], 16)
    declaration = idc.get_type(address) or ""
    if not declaration:
        missing.append((address, idc.get_func_name(address), dependency["symbol"]))

print(f"DIRECT_TYPES\t{len(contract['direct_dependencies'])}")
print(f"MISSING_TYPES\t{len(missing)}")
for address, ida_name, contract_name in missing:
    print(f"MISSING\t0x{address:08X}\t{ida_name}\t{contract_name}")

ida_pro.qexit(0)
