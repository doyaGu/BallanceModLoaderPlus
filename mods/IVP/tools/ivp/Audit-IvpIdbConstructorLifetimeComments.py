"""Audit retained complete-body lifetime notes in the Ballance IVP IDB."""

from __future__ import annotations

import ida_auto
import ida_bytes
import ida_funcs
import ida_pro


MARKER = "[BML complete-object lifetime]"
ADDRESSES = (
    0x1001D340,  # IVP_Friction_Core_Pair complete constructor
    0x1002DB50,  # IVP_BetterStatisticsmanager complete constructor
    0x10014D40,  # IVP_Meta_Collision_Filter complete constructor
    0x10014CE0,  # IVP_Meta_Collision_Filter complete destructor
    0x10014250,  # IVP_Actuator_Spring complete constructor
    0x100144D0,  # IVP_Actuator_Spring_Active complete constructor
    0x100186E0,  # IVP_Mindist_Manager complete constructor
    0x10018710,  # IVP_Mindist_Manager complete destructor
    0x10020140,  # IVP_U_Memory complete destructor
    0x10038290,  # IVP_SurfaceBuilder_Ledge_Soup complete constructor
    0x10038340,  # IVP_SurfaceBuilder_Ledge_Soup complete destructor
    0x100104C0,  # IVP_Attacher_To_Cores_Buoyancy complete constructor
    0x10010650,  # IVP_Attacher_To_Cores_Buoyancy complete destructor
    0x1002DC00,  # IVP_OV_Element complete constructor
    0x1002DC70,  # IVP_OV_Element complete destructor
    0x1002DEA0,  # IVP_OV_Node complete constructor
    0x1002DEF0,  # IVP_OV_Node complete destructor
    0x1002DFF0,  # IVP_OV_Tree_Manager complete constructor
    0x1002E0E0,  # IVP_OV_Tree_Manager complete destructor
)


ida_auto.auto_wait()
problems = []
for address in ADDRESSES:
    if ida_funcs.get_func_start(address) != address:
        problems.append((address, "missing-function-boundary"))
        continue
    if MARKER not in (ida_bytes.get_cmt(address, True) or ""):
        problems.append((address, "missing-lifetime-comment"))

print(f"CONSTRUCTOR_LIFETIME_COMMENTS\t{len(ADDRESSES) - len(problems)}")
print(f"PROBLEMS\t{len(problems)}")
for address, reason in problems:
    print(f"PROBLEM\t0x{address:08X}\t{reason}")

ida_pro.qexit(0)
