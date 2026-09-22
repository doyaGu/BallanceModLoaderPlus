"""List named IVP vtables and their concrete retail targets from a copied IDB.

Imported owner/type names are reported as hints.  Slot addresses, pointer
targets, code membership, xrefs, and leading instructions are the evidence.
"""

from __future__ import annotations

import json
import os
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


def slot_count_from_type(address: int, owner: str) -> int:
    value = ida_typeinf.tinfo_t()
    if ida_nalt.get_tinfo(value, address):
        pointed = ida_typeinf.tinfo_t()
        if value.is_ptr() and value.get_pointed_object(pointed):
            size = pointed.get_size()
            return size // 4 if size > 0 else 0
        size = value.get_size()
        if (value.is_array() or value.is_udt()) and size > 0:
            return size // 4

    # Most retail table data was never typed, but the imported source created
    # a separate `<owner>_vtbl` UDT.  It is useful as an explicit hint while the
    # concrete pointer run remains the binary check.
    if owner:
        table_type = ida_typeinf.tinfo_t()
        if table_type.get_named_type(
            None, owner + "_vtbl", ida_typeinf.BTF_STRUCT
        ):
            size = table_type.get_size()
            return size // 4 if size > 0 else 0
    return 0


def concrete_slot_count(address: int, typed_count: int, boundary_count: int) -> int:
    # A multiple-inheritance secondary vftable can share the same owner name as
    # the primary table while having fewer slots.  Never let an imported UDT
    # hint cross the next concrete vftable address.
    if typed_count > 0 and boundary_count > 0:
        maximum = min(typed_count, boundary_count)
    elif typed_count > 0:
        maximum = typed_count
    else:
        maximum = min(boundary_count, 64)
    count = 0
    for slot in range(maximum):
        target = ida_bytes.get_dword(address + slot * 4)
        function = ida_funcs.get_func(target)
        if function is None:
            break
        count += 1
    return count


def constructor_owner_from_xrefs(address: int) -> str:
    """Infer an anonymous vftable owner from a constructor vptr store."""
    owners: set[str] = set()
    for reference in idautils.XrefsTo(address):
        function = ida_funcs.get_func(reference.frm)
        if function is None:
            continue
        function_name = idc.get_func_name(function.start_ea) or ""
        match = re.match(r"^\?\?0([^@]+)@@", function_name)
        if match and match.group(1).startswith("IVP_"):
            owners.add(match.group(1))
    return next(iter(owners)) if len(owners) == 1 else ""


ida_auto.auto_wait()
image_base = ida_nalt.get_imagebase()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
summary_only = "summary" in arguments or "--summary" in arguments
tables: list[tuple[int, str, str, str, int, int, int]] = []

# MSVC vtables are frequently packed back-to-back.  Include every decorated
# vftable, including non-IVP Building Block classes, when deriving boundaries;
# otherwise an untyped IVP table would absorb the following class's slots.
decorated_vtable_addresses = {
    ida_name.get_nlist_ea(index)
    for index in range(ida_name.get_nlist_size())
    if (ida_name.get_nlist_name(index) or "").startswith("??_7")
}

# Link-time stripping leaves some concrete IVP tables named only `off_*`.
# Constructor vptr stores are the binary evidence for those address points.
# In particular, Root_Mindist's five-slot table begins in the middle of the
# apparent pointer run after the named two-slot Collision_Delegator base table.
anonymous_vtables: dict[int, str] = {}
if decorated_vtable_addresses:
    # Automatic `off_*` names are not part of IDA's public-name list.  Scan the
    # compact retail vftable band itself so stripped address points remain
    # visible to the audit.
    scan_start = min(decorated_vtable_addresses) & ~3
    scan_end = (max(decorated_vtable_addresses) + 0x100 + 3) & ~3
    for address in range(scan_start, scan_end, 4):
        if address in decorated_vtable_addresses:
            continue
        if ida_funcs.get_func(ida_bytes.get_dword(address)) is None:
            continue
        owner = constructor_owner_from_xrefs(address)
        if owner:
            anonymous_vtables[address] = owner

all_vtable_addresses = sorted(decorated_vtable_addresses | set(anonymous_vtables))

candidate_addresses = decorated_vtable_addresses | set(anonymous_vtables)
for address in candidate_addresses:
    name = ida_name.get_name(address) or idc.get_name(address) or ""
    inferred_owner = anonymous_vtables.get(address, "")
    if "IVP_" not in name and "ivp_" not in name and not inferred_owner:
        continue
    segment = ida_segment.getseg(address)
    if segment is None or ida_segment.get_segm_name(segment).casefold() == ".text":
        continue
    type_text = idc.get_type(address) or ""
    demangled = idc.demangle_name(name, idc.get_inf_attr(idc.INF_SHORT_DN)) or ""
    if not inferred_owner and not any(
        token in (name + " " + type_text + " " + demangled).casefold()
        for token in ("vtbl", "vftable")
    ):
        continue
    owner_match = re.match(r"^\?\?_7([^@]+)@@", name)
    owner = owner_match.group(1) if owner_match else inferred_owner
    typed_count = slot_count_from_type(address, owner)
    next_table = next(
        (candidate for candidate in all_vtable_addresses if candidate > address),
        address + 64 * 4,
    )
    boundary_count = max(0, (next_table - address) // 4)
    concrete_count = concrete_slot_count(address, typed_count, boundary_count)
    xrefs = sum(1 for _ in idautils.XrefsTo(address))
    tables.append(
        (address, name, owner, type_text, typed_count, concrete_count, xrefs)
    )

for address, name, owner, type_text, typed_count, concrete_count, xrefs in sorted(tables):
    print(
        f"VTABLE\t0x{address:08X}\t{name}\towner={owner}\ttyped={typed_count}"
        f"\tconcrete={concrete_count}\txrefs={xrefs}\t{type_text}"
    )
    if summary_only:
        continue
    for slot in range(concrete_count):
        entry = address + slot * 4
        target = ida_bytes.get_dword(entry)
        target_name = idc.get_name(target) or idc.get_func_name(target)
        function = ida_funcs.get_func(target)
        first = ""
        if function is not None:
            first_item = next(iter(idautils.FuncItems(function.start_ea)), ida_idaapi.BADADDR)
            if first_item != ida_idaapi.BADADDR:
                first = idc.generate_disasm_line(first_item, 0) or ""
        print(
            f"SLOT\t0x{address:08X}\t{slot}\t0x{entry:08X}"
            f"\t0x{target:08X}\t{target_name}\t{first}"
        )

print(f"VTABLES\t{len(tables)}")
ida_pro.qexit(0)
