"""Read back Ballance bitfield/packed-state storage from a copied IDB.

The expected offsets are a curated ABI manifest.  The audit checks IDA against
that manifest; it deliberately does not treat the imported type database as the
source of truth.
"""

from __future__ import annotations

import ida_auto
import ida_pro
import ida_typeinf


# IDA intentionally represents a few byte-sized retail fields as scalars rather
# than C++ bitfields.  Offsets and occupied widths are what cross the DLL.
EXPECTED = {
    "IVP_Simulation_Unit": {
        "sim_unit_movement_type": (0x00 * 8, 8),
        "union_find_needed_for_sim_unit": (0x01 * 8, 2),
        "sim_unit_has_fast_objects": (0x01 * 8 + 2, 2),
        "sim_unit_just_slowed_down": (0x01 * 8 + 4, 2),
    },
    "IVP_VHash": {
        "nelems": (0x08 * 8, 24),
        "dont_free": (0x0B * 8, 8),
    },
    "IVP_Impact_Solver_Long_Term": {
        "coll_time_is_valid": (0x5C * 8, 8),
        "friction_is_broken": (0x5D * 8, 2),
    },
    "IVP_Contact_Point": {
        "two_friction_values": (0x34 * 8, 8),
        "cp_status": (0x60 * 8, 32),
    },
    "IVP_Friction_System": {
        "union_find_necessary": (0x44 * 8, 8),
        "fr_sys_simulated": (0x45 * 8, 8),
    },
    "IVP_Constraint": {"is_enabled": (0x04 * 8, 32)},
    "IVP_Constraint_Local": {"norm": (0x18C * 8, 8)},
    "IVP_Multidimensional_Interpolator": {
        "nr_of_vectors": (0x08 * 8, 8),
        "nr_of_elements_input": (0x09 * 8, 8),
        "nr_of_elements_solution": (0x0A * 8, 8),
    },
    "IVP_Actuator_Force": {
        "push_first_object": (0x74 * 8, 1),
        "push_second_object": (0x74 * 8 + 1, 1),
    },
    "IVP_Compact_Surface": {
        "max_factor_surface_deviation": (0x1C * 8, 8),
        "byte_size": (0x1D * 8, 24),
    },
    "IVP_Compact_Mopp": {
        "max_factor_surface_deviation": (0x1C * 8, 8),
        "byte_size": (0x1D * 8, 24),
    },
}

EXPECTED_SIZES = {
    "IVP_Simulation_Unit": 0x24,
    "IVP_VHash": 0x10,
    "IVP_Impact_Solver_Long_Term": 0xE0,
    "IVP_Contact_Point": 0x78,
    "IVP_Friction_System": 0x50,
    "IVP_Constraint": 0x18,
    "IVP_Constraint_Local": 0x190,
    "IVP_Multidimensional_Interpolator": 0x40,
    "IVP_Actuator_Force": 0x78,
    "IVP_Compact_Surface": 0x30,
    "IVP_Compact_Mopp": 0x30,
}


ida_auto.auto_wait()
problems: list[str] = []
field_count = 0

for owner, fields in EXPECTED.items():
    value = ida_typeinf.tinfo_t()
    if not value.get_named_type(None, owner, ida_typeinf.BTF_TYPEDEF):
        problems.append(f"MISSING_OWNER\t{owner}")
        continue
    actual_size = value.get_size()
    if actual_size != EXPECTED_SIZES[owner]:
        problems.append(
            f"SIZE\t{owner}\texpected=0x{EXPECTED_SIZES[owner]:X}"
            f"\tactual=0x{actual_size:X}"
        )
    members = ida_typeinf.udt_type_data_t()
    if not value.get_udt_details(members):
        problems.append(f"NO_UDT\t{owner}")
        continue
    by_name = {member.name: member for member in members if member.name}
    for name, expected in fields.items():
        field_count += 1
        member = by_name.get(name)
        actual = (member.offset, member.size) if member is not None else None
        if actual != expected:
            problems.append(
                f"FIELD\t{owner}::{name}\texpected={expected}\tactual={actual}"
            )

for problem in problems:
    print(problem)
print(f"BITFIELD_IDB_OWNERS\t{len(EXPECTED)}")
print(f"BITFIELD_IDB_STORAGE_FIELDS\t{field_count}")
print(f"PROBLEMS\t{len(problems)}")
ida_pro.qexit(1 if problems else 0)
