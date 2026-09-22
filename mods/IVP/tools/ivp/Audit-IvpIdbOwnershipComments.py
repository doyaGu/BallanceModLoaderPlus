"""Audit cross-DLL allocation evidence retained in the Ballance IVP IDB."""

from __future__ import annotations

import ida_auto
import ida_bytes
import ida_funcs
import ida_pro


MARKER = "[BML cross-DLL ownership]"
ADDRESSES = (
    0x1000BF00,  # IVP_Material_Simple complete constructor
    0x1000BF50,  # IVP_Material_Simple scalar deleting destructor
    0x10060C30,  # IVP_BetterDebugmanager complete constructor
    0x10060C50,  # IVP_BetterDebugmanager scalar deleting destructor
    0x100104C0,  # IVP_Attacher_To_Cores_Buoyancy complete constructor
    0x10010790,  # IVP_Attacher_To_Cores_Buoyancy deleting destructor
    0x1000D400,  # IVP_Core complete constructor
    0x1000D580,  # IVP_Core complete destructor
    0x10060756,  # legacy MSVCRT operator delete import thunk
    0x1006075C,  # legacy MSVCRT operator new import thunk
)


ida_auto.auto_wait()
problems = []
for address in ADDRESSES:
    if ida_funcs.get_func_start(address) != address:
        problems.append((address, "missing-function-boundary"))
        continue
    if MARKER not in (ida_bytes.get_cmt(address, True) or ""):
        problems.append((address, "missing-ownership-comment"))

print(f"OWNERSHIP_COMMENTS\t{len(ADDRESSES) - len(problems)}")
print(f"PROBLEMS\t{len(problems)}")
for address, reason in problems:
    print(f"PROBLEM\t0x{address:08X}\t{reason}")

ida_pro.qexit(0)
