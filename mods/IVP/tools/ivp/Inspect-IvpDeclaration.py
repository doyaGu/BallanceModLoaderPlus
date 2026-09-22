"""Parse C/C++ declarations in a disposable IDB without modifying it."""

from __future__ import annotations

import json
import os

import ida_auto
import ida_pro
import idc


ida_auto.auto_wait()
arguments = json.loads(os.environ.get("BML_IVP_IDA_SCRIPT_ARGUMENTS", "[]"))
if not arguments:
    arguments = idc.ARGV[1:]
for declaration in arguments:
    parsed = idc.parse_decl(declaration, idc.PT_SIL)
    print(f"PARSE\t{int(parsed is not None)}\t{declaration}")
ida_pro.qexit(0)
