"""Run one IDAPython script against an existing IDB through idapro.

This deliberately enables unbuffered, visible script output so callers can
distinguish an IDB-open failure from a script refusal.  Most repository IDA
scripts terminate with ida_pro.qexit(), so this process is intentionally one
database and one script only.
"""

from __future__ import annotations

import idapro

import argparse
import json
import os
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--script", required=True, type=Path)
    parser.add_argument("--export", type=Path)
    parser.add_argument("--script-argument", action="append", default=[])
    parser.add_argument("--auto-analysis", action="store_true")
    args = parser.parse_args()

    database = args.database.resolve(strict=True)
    script = args.script.resolve(strict=True)
    if database.name.casefold() == "physics_rt-analysis.i64":
        raise SystemExit(
            "Refusing to open the canonical IDB directly; use "
            "Invoke-IvpIdbReadOnly.ps1 or Invoke-IvpIdbTransaction.ps1"
        )
    if args.export is not None:
        os.environ["BML_IVP_IDA_EXPORT_PATH"] = str(args.export.resolve())
    os.environ["BML_IVP_IDA_SCRIPT_ARGUMENTS"] = json.dumps(
        args.script_argument
    )

    idapro.enable_console_messages(False)
    result = idapro.open_database(str(database), args.auto_analysis)
    print(f"OPEN_RC\t{result}\t{database}", flush=True)
    if result != 0:
        raise SystemExit(3)

    import ida_idaapi

    ida_idaapi.IDAPython_ExecScript(str(script), globals())


if __name__ == "__main__":
    main()
