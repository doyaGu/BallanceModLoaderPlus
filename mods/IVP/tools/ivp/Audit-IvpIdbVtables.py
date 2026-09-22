"""Audit Ballance IVP vftable address points in the canonical IDB.

The imported ``*_vtbl`` UDTs are not treated as truth.  Address points come
from named MSVC tables or concrete constructor vptr stores; slot counts are a
curated result of checking the retail pointer run, the next address point, and
the applicable public virtual interface.  Selected high-risk multiple-
inheritance tables additionally pin every target address.
"""

from __future__ import annotations

import re

import ida_auto
import ida_bytes
import ida_funcs
import ida_idaapi
import ida_name
import ida_nalt
import ida_pro
import ida_segment
import ida_typeinf
import idautils
import idc


IMAGE_BASE = 0x10000000

# (address point, exact slot count).  Anonymous entries are retained because
# constructors install them even when the linker stripped the decorated name.
TABLES = (
    (0x100631E0, 11),
    (0x1006325C, 7),
    (0x10063390, 3),
    (0x1006339C, 1),
    (0x100633A0, 1),
    (0x100633A4, 1),
    (0x100633C0, 2),
    (0x100633C8, 2),
    (0x100633D0, 7),
    (0x100633EC, 7),
    (0x10063408, 7),
    (0x10063434, 6),
    (0x1006344C, 6),
    (0x10063464, 6),
    (0x10063508, 7),
    (0x10063524, 6),
    (0x1006353C, 4),
    (0x1006354C, 3),
    (0x10063558, 1),
    (0x1006355C, 2),
    (0x10063564, 2),
    (0x1006356C, 2),
    (0x10063574, 2),
    (0x1006357C, 2),
    (0x10063584, 2),
    (0x10063598, 7),
    (0x100635B8, 2),
    (0x100635C0, 8),
    (0x100635E0, 8),
    (0x10063600, 1),
    (0x10063604, 8),
    (0x10063624, 1),
    (0x10063628, 8),
    (0x1006364C, 3),
    (0x10063658, 3),
    (0x1006366C, 3),
    (0x10063678, 3),
    (0x10063698, 6),
    (0x100636B0, 6),
    (0x100636D0, 2),
    (0x100636D8, 15),
    (0x10063714, 1),
    (0x10063718, 2),
    (0x10063720, 2),
    (0x10063728, 1),
    (0x1006372C, 3),
    (0x10063738, 1),
    (0x1006373C, 1),
    (0x10063740, 3),
    (0x1006374C, 1),
    (0x10063778, 5),
    (0x100637A0, 5),
    (0x100637B4, 8),
    (0x10063848, 2),
    (0x10063890, 11),
    (0x10063930, 23),
    (0x100639E0, 3),
    (0x10063A00, 5),
    (0x10063A14, 4),
    (0x10063A24, 2),
    (0x10063A2C, 1),
    (0x10063A34, 2),
    (0x10063A3C, 5),
    (0x10063A50, 2),
    (0x10063A58, 7),
    (0x10063A84, 3),
    (0x10063ABC, 3),
    (0x10063AC8, 2),
    (0x10063AD0, 8),
    (0x10063B20, 23),
    (0x10063BB0, 2),
    (0x10063BB8, 5),
    (0x10063BF8, 2),
    (0x10063C68, 1),
    (0x10063D34, 2),
)

EXACT_TARGETS = {
    # Primary Mindist table. Ballance has no neighboring-source is_recursive
    # slot: invalidation and impact occupy slots six and seven directly.
    0x100637B4: (
        0x100181B0, 0x10016410, 0x10016430, 0x10016190,
        0x100162D0, 0x10019770, 0x10016F70, 0x100240A0,
    ),
    # OV Element final table. The adjacent 0x10063A14 table is the
    # base-construction address point and intentionally lacks the derived
    # scalar deleting destructor.
    0x10063A00: (
        0x1002DDA0, 0x1002DDC0, 0x1002DDB0, 0x1001A810, 0x1002DC50,
    ),
    # Private OV-tree hash helper. The manager constructor writes this address
    # into its separately allocated IVP_VHash-derived helper; the manager itself
    # is non-polymorphic.
    0x10063A24: (0x10037DD0, 0x1002E0C0),
    # Collision Delegator base and the separately installed Root Mindist table.
    0x10063A34: (0x10060762, 0x1002F580),
    0x10063A3C: (
        0x1002F3F0, 0x1002F5C0, 0x1002F3B0, 0x1002F430, 0x1002F3E0,
    ),
    # Active Spring construction, secondary listener and primary actuator.
    0x10063600: (0x10060762,),
    0x10063624: (0x10014440,),
    0x10063628: (
        0x10028950, 0x10004C10, 0x10013FA0, 0x10004C20,
        0x100146D0, 0x10013FB0, 0x100145F0, 0x10028950,
    ),
    # Active terminal delayed-listener, primary and construction address points.
    0x10063728: (0x10015D30,),
    0x1006372C: (0x10015C90, 0x10015DE0, 0x10015D80),
    0x10063738: (0x10060762,),
    0x1006373C: (0x10015D60,),
    0x10063740: (0x10015D00, 0x10015E40, 0x10015DF0),
    0x1006374C: (0x10060762,),
    # Secondary Collision Delegator subobjects in recursive mindist/OO watcher.
    0x10063AC8: (0x100309C0, 0x10030C50),
    0x10063AD0: (
        0x100181B0, 0x10016410, 0x10016430, 0x10016190,
        0x100308F0, 0x10028220, 0x10030710, 0x10030540,
    ),
    0x10063BB0: (0x100381D0, 0x10038280),
    0x10063BB8: (
        0x10016180, 0x10038240, 0x10038260, 0x10016190, 0x100380E0,
    ),
}

EXPECTED_NAMES = {
    0x10063A00: "??_7IVP_OV_Element@@6B@",
    0x10063A24: "??_7IVP_ov_tree_hash@@6B@",
    0x100637B4: "??_7IVP_Mindist@@6B@",
    0x10063AC8:
        "??_7IVP_Mindist_Recursive@@6BIVP_Collision_Delegator@@@",
    0x10063AD0: "??_7IVP_Mindist_Recursive@@6BIVP_Mindist@@@",
    0x10063A34: "??_7IVP_Collision_Delegator@@6B@",
    0x10063A3C: "??_7IVP_Collision_Delegator_Root_Mindist@@6B@",
    0x10063624:
        "??_7IVP_Actuator_Spring_Active@@6BIVP_U_Active_Float_Listener@@@",
    0x10063628:
        "??_7IVP_Actuator_Spring_Active@@6BIVP_Actuator_Spring@@@",
}

EXPECTED_UDT_SIZES = {
    "IVP_Mindist_vtbl": 0x20,
    "IVP_Mindist_Recursive": 0x98,
    "IVP_Mindist_Recursive_vtbl": 0x20,
    "IVP_Mindist_Recursive_Collision_Delegator_vtbl": 0x08,
    "IVP_Collision_Delegator_vtbl": 0x08,
    "IVP_Collision_Delegator_Root_vtbl": 0x14,
    "IVP_Collision_Delegator_Root_Mindist_vtbl": 0x14,
}


def constructor_owner(address: int) -> str:
    owners: set[str] = set()
    for reference in idautils.XrefsTo(address):
        function_start = ida_funcs.get_func_start(reference.frm)
        if function_start == ida_idaapi.BADADDR:
            continue
        match = re.match(r"^\?\?0([^@]+)@@", idc.get_func_name(function_start) or "")
        if match and match.group(1).startswith("IVP_"):
            owners.add(match.group(1))
    return next(iter(owners)) if len(owners) == 1 else ""


def discover_address_points() -> set[int]:
    named = {
        ida_name.get_nlist_ea(index)
        for index in range(ida_name.get_nlist_size())
        if (ida_name.get_nlist_name(index) or "").startswith("??_7IVP_")
    }
    if not named:
        return set()
    result = set(named)
    for address in range(min(named) & ~3, (max(named) + 0x100 + 3) & ~3, 4):
        target = ida_bytes.get_dword(address)
        if ida_funcs.get_func_start(target) != target:
            continue
        if constructor_owner(address):
            result.add(address)
    return result


ida_auto.auto_wait()
problems: list[str] = []

if ida_nalt.get_imagebase() != IMAGE_BASE:
    problems.append(f"image-base=0x{ida_nalt.get_imagebase():X}")

expected_addresses = {address for address, _ in TABLES}
discovered_addresses = discover_address_points()
for address in sorted(expected_addresses - discovered_addresses):
    problems.append(f"missing-address-point 0x{address:08X}")
for address in sorted(discovered_addresses - expected_addresses):
    problems.append(f"unexpected-address-point 0x{address:08X}")

for index, (address, count) in enumerate(TABLES):
    if not any(True for _ in idautils.XrefsTo(address)):
        problems.append(f"no-xref 0x{address:08X}")
    if index + 1 < len(TABLES) and address + count * 4 > TABLES[index + 1][0]:
        problems.append(f"overlap 0x{address:08X} count={count}")
    for slot in range(count):
        target = ida_bytes.get_dword(address + slot * 4)
        if ida_funcs.get_func_start(target) != target:
            problems.append(
                f"non-function-slot 0x{address:08X}[{slot}]=0x{target:08X}"
            )

for address, targets in EXACT_TARGETS.items():
    actual = tuple(
        ida_bytes.get_dword(address + slot * 4)
        for slot in range(len(targets))
    )
    if actual != targets:
        problems.append(
            f"target-drift 0x{address:08X} expected={targets} actual={actual}"
        )

for address, expected_name in EXPECTED_NAMES.items():
    actual_name = ida_name.get_name(address) or ""
    if actual_name != expected_name:
        problems.append(
            f"name-drift 0x{address:08X} expected={expected_name} actual={actual_name}"
        )

for name, expected_size in EXPECTED_UDT_SIZES.items():
    value = ida_typeinf.tinfo_t()
    if not value.get_named_type(None, name, ida_typeinf.BTF_STRUCT):
        problems.append(f"missing-udt {name}")
    elif value.get_size() != expected_size:
        problems.append(
            f"udt-size {name} expected=0x{expected_size:X} actual=0x{value.get_size():X}"
        )

print(f"IVP_VTABLE_ADDRESS_POINTS\t{len(expected_addresses)}")
print(f"IVP_VTABLE_SLOTS\t{sum(count for _, count in TABLES)}")
print(f"IVP_VTABLE_EXACT_TARGET_TABLES\t{len(EXACT_TARGETS)}")
print(f"PROBLEMS\t{len(problems)}")
for problem in problems:
    print(f"PROBLEM\t{problem}")

ida_pro.qexit(0 if not problems else 2)
