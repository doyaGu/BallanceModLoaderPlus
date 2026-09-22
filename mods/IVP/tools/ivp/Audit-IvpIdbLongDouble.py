"""List IVP UDT members still rendered as long double in the IDB."""

from __future__ import annotations

import ida_auto
import ida_pro
import ida_typeinf


ida_auto.auto_wait()
idati = ida_typeinf.get_idati()
count = 0
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
    for member in members:
        rendered = member.type.dstr()
        if "long double" not in rendered:
            continue
        print(
            f"LONG_DOUBLE\t{name}\t{member.name}\t"
            f"0x{member.offset // 8:X}\t{member.size}\t{rendered}"
        )
        count += 1
print(f"LONG_DOUBLE_MEMBERS\t{count}")
ida_pro.qexit(0)
