"""Audit IDA UDT alignment for direct Ballance IVP binary64 members.

IDA 9.x encodes an explicitly declared eight-byte UDT alignment as ``sda=4``.
Changing an imported member from IDA's ``long double`` spelling to ``double``
with ``set_udm_type`` can silently lower that owner to ``sda=3`` and truncate
tail padding.  This catches that semantic layout regression, not just spelling.
"""

from __future__ import annotations

import ida_auto
import ida_pro
import ida_typeinf


def is_direct_binary64(value: ida_typeinf.tinfo_t) -> bool:
    if value.is_double() or value.is_ldouble():
        return True
    if value.is_array():
        details = ida_typeinf.array_type_data_t()
        return value.get_array_details(details) and is_direct_binary64(
            details.elem_type
        )
    return False


ida_auto.auto_wait()
idati = ida_typeinf.get_idati()
owners = 0
problems = 0
for ordinal in range(1, ida_typeinf.get_ordinal_count(idati)):
    name = ida_typeinf.get_numbered_type_name(idati, ordinal) or ""
    if not (name.startswith("IVP_") or name.startswith("IVV_")):
        continue
    value = ida_typeinf.tinfo_t()
    if not value.get_numbered_type(idati, ordinal) or not value.is_udt():
        continue
    members = ida_typeinf.udt_type_data_t()
    if not value.get_udt_details(members):
        continue
    direct_members = [
        member.name for member in members if is_direct_binary64(member.type)
    ]
    if not direct_members:
        continue
    owners += 1
    # Natural eight-byte alignment is encoded as sda=0; source-imported
    # explicitly aligned records use sda=4.  Both are valid.  sda=3 with an
    # effective alignment of four is the destructive set_udm_type regression.
    valid = value.get_alignment() == 8 and value.get_size() % 8 == 0
    if not valid:
        problems += 1
    print(
        f"BINARY64_OWNER\t{name}\t0x{value.get_size():X}\t"
        f"effective={value.get_alignment()}\tdeclared={value.get_declalign()}\t"
        f"sda={members.sda}\tpack={members.pack}\t"
        f"members={','.join(direct_members)}\tvalid={int(valid)}"
    )

exact_layouts = {
    "IVP_Environment": 0x178,
    "IVP_Debug_Manager": 0x68,
    "IVP_Friction_Solver": 0x840,
    "IVP_Impact_Solver": 0x128,
    "IVP_Clustering_Visualizer_Shortrange_Callback": 0x68,
    "IVP_Extra_Info": 0x40,
    "IVP_Geompack": 0x90,
}
for name, expected_size in exact_layouts.items():
    value = ida_typeinf.tinfo_t()
    present = value.get_named_type(None, name, ida_typeinf.BTF_STRUCT)
    valid = (
        present
        and value.get_size() == expected_size
        and value.get_alignment() == 8
    )
    if not valid:
        problems += 1
    print(
        f"BINARY64_EXACT_LAYOUT\t{name}\t"
        f"0x{value.get_size() if present else 0:X}\tvalid={int(valid)}"
    )

environment = ida_typeinf.tinfo_t()
environment_members = ida_typeinf.udt_type_data_t()
environment_valid = (
    environment.get_named_type(None, "IVP_Environment", ida_typeinf.BTF_STRUCT)
    and environment.get_udt_details(environment_members)
)
expected_environment_members = {
    "reserved_34": (0x34, 0x04),
    "statistic_manager": (0x38, 0x60),
    "freeze_manager": (0x98, 0x04),
    "time_since_last_blocking": (0xB8, 0x08),
    "auth_costumer_name": (0x10C, 0x04),
    "auth_costumer_code": (0x110, 0x04),
    "pw_count": (0x114, 0x04),
    "environment_manager": (0x118, 0x04),
    "current_time": (0x120, 0x08),
    "integrated_energy_damp": (0x148, 0x08),
    "global_object_listeners": (0x150, 0x08),
    "collision_delegator_roots": (0x158, 0x08),
    "environment_magic_number": (0x170, 0x04),
    "reserved_174": (0x174, 0x04),
}
actual_environment_members = {
    member.name: (member.offset // 8, member.size // 8)
    for member in environment_members
}
environment_valid = environment_valid and all(
    actual_environment_members.get(name) == layout
    for name, layout in expected_environment_members.items()
)
environment_valid = (
    environment_valid and "constraint_listeners" not in actual_environment_members
)
if not environment_valid:
    problems += 1
print(f"ENVIRONMENT_RETAIL_LAYOUT\t{int(environment_valid)}")

print(f"BINARY64_UDT_OWNERS\t{owners}")
print(f"BINARY64_EXACT_LAYOUTS\t{len(exact_layouts)}")
print(f"PROBLEMS\t{problems}")
ida_pro.qexit(0 if problems == 0 else 1)
