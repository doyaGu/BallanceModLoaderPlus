"""Inspect named IVP types in a copied IDA database.

Run with idat.exe in batch mode and pass one or more undecorated type names.
This deliberately reports both struct tags and typedef names because imported
source types in the Ballance database are not uniformly represented.
"""

from __future__ import annotations

import json
import os

import ida_auto
import ida_pro
import ida_typeinf
import idc


def describe(name: str) -> None:
    for label, kind in (
        ("typedef", ida_typeinf.BTF_TYPEDEF),
        ("struct", ida_typeinf.BTF_STRUCT),
    ):
        value = ida_typeinf.tinfo_t()
        present = value.get_named_type(None, name, kind)
        rendered = value.dstr() if present else ""
        size = value.get_size() if present else 0
        print(
            f"TYPE\t{name}\t{label}\t{int(present)}\t"
            f"0x{size:X}\t{rendered}"
        )
        if present and value.is_udt():
            members = ida_typeinf.udt_type_data_t()
            have_details = value.get_udt_details(members)
            print(
                f"ALIGN\t{name}\t{label}\t"
                f"effective={value.get_alignment()}\t"
                f"declared={value.get_declalign()}\t"
                f"sda={members.sda if have_details else -1}\t"
                f"pack={members.pack if have_details else -1}"
            )
            print(
                f"COMMENT\t{name}\t{label}\t"
                f"{value.get_type_cmt() or ''}"
            )
            if have_details:
                for member in members:
                    member_size = member.size
                    print(
                        f"MEMBER\t{name}\t{label}\t{member.name}\t"
                        f"0x{member.offset // 8:X}\t{member_size}\t"
                        f"{member.type.dstr()}"
                    )

    pointer_declaration = f"void __thiscall(struct {name} *this, int value)"
    parsed = idc.parse_decl(pointer_declaration, idc.PT_SIL)
    print(f"PARSE\t{name}\t{int(parsed is not None)}\t{pointer_declaration}")


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
for argument in arguments:
    describe(argument)
ida_pro.qexit(0)
