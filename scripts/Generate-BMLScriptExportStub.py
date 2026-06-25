from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))

from bml_script_export_stub import ExportStubError, generate_export_stub_from_file


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="Generate-BMLScriptExportStub.py",
        description="Generate a pure AngelScript client wrapper for BML mod exports.",
    )
    parser.add_argument("--idl", type=Path, required=True, help="JSON export stub IDL to read")
    parser.add_argument("--out", type=Path, help="output .as file; stdout when omitted")
    args = parser.parse_args(argv)

    try:
        stub = generate_export_stub_from_file(args.idl)
    except ExportStubError as exc:
        print(f"[Generate-BMLScriptExportStub] ERROR {exc}", file=sys.stderr)
        return 1

    if args.out is None:
        print(stub, end="")
    else:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(stub, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
