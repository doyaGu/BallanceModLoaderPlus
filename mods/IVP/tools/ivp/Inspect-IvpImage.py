"""Print the opened database image identity without modifying it."""

from __future__ import annotations

import ida_ida
import ida_nalt
import ida_pro


print(f"IMAGE_BASE\t0x{ida_nalt.get_imagebase():08X}")
print(f"MIN_EA\t0x{ida_ida.inf_get_min_ea():08X}")
print(f"MAX_EA\t0x{ida_ida.inf_get_max_ea():08X}")
print(f"INPUT_FILE\t{ida_nalt.get_root_filename()}")
ida_pro.qexit(0)
