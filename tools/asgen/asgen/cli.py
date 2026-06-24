from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .checks import (
    assert_approved_skip_reasons,
    assert_handwritten_bindings_synchronized,
    assert_negative_script_surface,
    assert_no_forbidden_symbols,
    parse_report_skip_reasons,
    write_if_changed,
)
from .generator import Generator
from .source import CImguiSourceError


def generated_files(root: Path, header: str, source: str, stub: str, report: str) -> dict[Path, str]:
    return {
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.h": header,
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.cpp": source,
        root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.report.md": report,
        root / "docs" / "bml-imgui-api.as": stub,
    }


def generated_paths(root: Path) -> dict[str, Path]:
    return {
        "header": root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.h",
        "source": root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.cpp",
        "report": root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.report.md",
        "stub": root / "docs" / "bml-imgui-api.as",
    }


def read_generated_snapshot(root: Path) -> tuple[str, str, str, str]:
    paths = generated_paths(root)
    missing = [str(path.relative_to(root)) for path in paths.values() if not path.exists()]
    if missing:
        raise FileNotFoundError("missing generated ImGui binding file(s): " + ", ".join(missing))
    return (
        paths["header"].read_text(encoding="utf-8"),
        paths["source"].read_text(encoding="utf-8"),
        paths["stub"].read_text(encoding="utf-8"),
        paths["report"].read_text(encoding="utf-8"),
    )


def validate_generated_snapshot(root: Path) -> None:
    header, source, stub, report = read_generated_snapshot(root)
    paths = generated_paths(root)
    assert_approved_skip_reasons(parse_report_skip_reasons(report))
    assert_no_forbidden_symbols({
        paths["header"]: header,
        paths["source"]: source,
    })
    assert_negative_script_surface(source)
    assert_handwritten_bindings_synchronized(source, stub)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="asgen")
    parser.add_argument("command", choices=["generate", "check", "negative-check"])
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()

    if args.command == "negative-check":
        try:
            validate_generated_snapshot(root)
        except (FileNotFoundError, RuntimeError) as exc:
            print(f"[asgen] ERROR {exc}", file=sys.stderr)
            return 1
        return 0

    if args.command == "generate":
        try:
            gen = Generator(root)
            header, source, stub, report = gen.generate()
        except CImguiSourceError as exc:
            print(f"[asgen] ERROR {exc}", file=sys.stderr)
            return 1
        assert_approved_skip_reasons(gen.report)
        files = generated_files(root, header, source, stub, report)
        assert_no_forbidden_symbols({
            root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.h": header,
            root / "src" / "AngelScript" / "generated" / "BMLImGuiAngelScriptBindings.cpp": source,
        })
        assert_negative_script_surface(source)
        assert_handwritten_bindings_synchronized(source, stub)
        changed = False
        for path, text in files.items():
            changed = write_if_changed(path, text) or changed
        return 0

    try:
        validate_generated_snapshot(root)
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"[asgen] ERROR {exc}", file=sys.stderr)
        return 1
    return 0
