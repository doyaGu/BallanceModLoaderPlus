"""Audit IDB evidence for complete destructors directly exposed by the API."""

from __future__ import annotations

import ida_auto
import ida_bytes
import ida_funcs
import ida_pro


MARKER = "[BML direct complete destructor]"
ADDRESSES = (
    0x1000BE00,  # IVP_Material
    0x1000B6A0,  # IVP_Hash
    0x100148D0,  # IVP_Collision_Filter
    0x10014F70,  # IVP_PerformanceCounter
    0x100159D0,  # IVP_U_Active_Value
    0x100161D0,  # IVP_Synapse
    0x10018710,  # IVP_Mindist_Manager
    0x10020140,  # IVP_U_Memory
    0x10022160,  # IVP_SurfaceManager
    0x1002DC70,  # IVP_OV_Element
    0x1002DEF0,  # IVP_OV_Node
    0x1002E0E0,  # IVP_OV_Tree_Manager
    0x1002F570,  # IVP_Collision_Delegator
    0x10060C70,  # IVP_BetterDebugmanager
)


ida_auto.auto_wait()
problems = []
for address in ADDRESSES:
    if ida_funcs.get_func_start(address) != address:
        problems.append((address, "missing-function-boundary"))
        continue
    if MARKER not in (ida_bytes.get_cmt(address, True) or ""):
        problems.append((address, "missing-direct-destructor-comment"))

print(f"DIRECT_DESTRUCTOR_COMMENTS\t{len(ADDRESSES) - len(problems)}")
print(f"PROBLEMS\t{len(problems)}")
for address, reason in problems:
    print(f"PROBLEM\t0x{address:08X}\t{reason}")

ida_pro.qexit(0)
