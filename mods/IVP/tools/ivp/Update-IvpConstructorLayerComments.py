"""Update the canonical IDB comments for verified layered constructors.

Run through IDA in batch mode. This intentionally changes only repeatable
comments; it does not reapply the repository's wider UDT correction batch.
"""

import ida_auto
import ida_bytes
import ida_loader
import ida_name
import ida_pro
import idc


ida_auto.auto_wait()

COMMENTS = {
    0x100375B0: (
        "??0IVP_Constraint@@QAE@XZ",
        "Retail IVP_Constraint complete constructor: installs the 23-slot "
        "vtable, constructs the inline two-Core vector at +0x08/+0x0C/+0x10, "
        "and enables the constraint. Public direct construction and the nested "
        "Local path each enter this body exactly once.",
    ),
    0x10028210: (
        "??0IVP_Constraint_Local_Anchor@@QAE@XZ",
        "Retail Local Anchor constructor. It writes only rot=null at +0x84; "
        "object at +0x80 remains owned by the enclosing Local initializer. "
        "Direct public construction and each nested Local anchor use this body.",
    ),
    0x10028270: (
        "??0IVP_Constraint_Local@@QAE@ABVIVP_Template_Constraint@@@Z",
        "Retail Local complete constructor: calls Constraint at 0x100375B0, "
        "constructs anchors at +0x70/+0xF8, writes identity mappings at "
        "+0x180/+0x183, then calls 0x100284D0 and activate. Both the public "
        "constructor and raw-memory factory safely use this complete route.",
    ),
}

for address, (expected_name, comment) in COMMENTS.items():
    actual_name = ida_name.get_name(address) or ""
    if actual_name != expected_name:
        print(
            f"NAME_MISMATCH\t0x{address:08X}\t{expected_name}\t{actual_name}"
        )
        ida_pro.qexit(1)
    if not idc.get_type(address):
        print(f"MISSING_TYPE\t0x{address:08X}\t{actual_name}")
        ida_pro.qexit(1)
    if not ida_bytes.set_cmt(address, comment, True):
        print(f"COMMENT_FAILED\t0x{address:08X}\t{actual_name}")
        ida_pro.qexit(1)
    print(f"COMMENTED\t0x{address:08X}\t{actual_name}")

ida_loader.save_database(idc.get_idb_path(), ida_loader.DBFL_COMP)
print(f"SAVED\t{idc.get_idb_path()}")
ida_pro.qexit(0)
