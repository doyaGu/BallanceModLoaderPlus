from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .checks import (
    assert_approved_skip_reasons,
    assert_handwritten_bindings_synchronized,
    assert_negative_script_surface,
    assert_no_forbidden_symbols,
    check_file,
    write_if_changed,
)
from .generator import Generator


def generated_files(root: Path, header: str, source: str, stub: str, report: str) -> dict[Path, str]:
    return {
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.h": header,
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.cpp": source,
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.report.md": report,
        root / "docs" / "bml-imgui-api.as": stub,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="asgen")
    parser.add_argument("command", choices=["generate", "check", "negative-check"])
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    gen = Generator(root)
    header, source, stub, report = gen.generate()
    assert_approved_skip_reasons(gen.report)
    files = generated_files(root, header, source, stub, report)
    assert_no_forbidden_symbols({
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.h": header,
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.cpp": source,
    })
    assert_negative_script_surface(source)
    assert_handwritten_bindings_synchronized(source, stub)

    if args.command == "generate":
        changed = False
        for path, text in files.items():
            changed = write_if_changed(path, text) or changed
        return 0

    if args.command == "negative-check":
        return 0

    failed = [str(path.relative_to(root)) for path, text in files.items() if not check_file(path, text)]
    if failed:
        for path in failed:
            print(f"[asgen] ERROR generated file is stale or missing: {path}", file=sys.stderr)
        return 1
    return 0
