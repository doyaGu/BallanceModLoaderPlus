from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


FORBIDDEN_TOKENS = [
    "CKBehaviorContext",
    "CKBehaviorPrototype",
    "CKBehaviorIO",
    "CKBehaviorLink",
    "CKBehaviorPin",
    "CKObjectDeclaration",
    "CKParameter@",
    "CKParameterIn",
    "CKParameterOut",
    "CKBehaviorSlot",
    "BehaviorSlot",
    "InputParameter",
    "OutputParameter",
    "RegisterBehavior",
    "RegisterBB",
    "BB_",
    "generated helper",
    "GeneratedHelper",
    "asIScript",
    "CKAngelScript",
    "void *",
    "void*",
    "userData",
    "UserData",
    "BMLAS_",
    "BMLImGuiAS_",
]

REQUIRED_OWNERSHIP_DECLARATIONS = [
    "TimerRef@ AddTimer(Timer@+ timer) const",
    "CommandRef@ RegisterCommand(Command@+ command) const",
    "DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const",
    "TimerRef@ AddTimer(Timer@+ timer)",
    "CommandRef@ RegisterCommand(Command@+ command)",
    "DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request)",
]

STALE_OWNERSHIP_DECLARATIONS = [
    "TimerRef@ AddTimer(Timer@ timer)",
    "CommandRef@ RegisterCommand(Command@ command)",
    "DataShareRequestRef@ RequestDataShare(DataShareRequest@ request)",
]


def _read(path: Path) -> str:
    if not path.exists():
        raise RuntimeError(f"required file is missing: {path}")
    return path.read_text(encoding="utf-8")


def _extract_static_array_blocks(source: str) -> list[tuple[str, str]]:
    starts = re.finditer(r"static const\s+(?:Script\w+|BML::Script\w+)\s+(k\w+)\[\]\s*=\s*\{", source)
    blocks: list[tuple[str, str]] = []
    for start in starts:
        index = start.end()
        depth = 1
        while index < len(source) and depth:
            char = source[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            index += 1
        if depth != 0:
            raise RuntimeError(f"unterminated static array block: {start.group(1)}")
        blocks.append((start.group(1), source[start.start():index]))
    return blocks


def _extract_string_literals(text: str) -> list[str]:
    strings: list[str] = []
    for match in re.finditer(r'"((?:\\.|[^"\\])*)"', text):
        value = match.group(1)
        strings.append(bytes(value, "utf-8").decode("unicode_escape"))
    return strings


def _check_forbidden(label: str, text: str, errors: list[str]) -> None:
    for token in FORBIDDEN_TOKENS:
        if token in text:
            errors.append(f"{label}: forbidden token {token!r} in {text!r}")
    if re.search(r"\bBB\b", text):
        errors.append(f"{label}: forbidden token 'BB' in {text!r}")


def _check_registration_sources(root: Path, errors: list[str]) -> None:
    for relative in [
        Path("src/AngelScript/AngelScriptBindings.cpp"),
        Path("src/AngelScript/ScriptApiContract.cpp"),
    ]:
        path = root / relative
        source = _read(path)
        blocks = _extract_static_array_blocks(source)
        if not blocks:
            errors.append(f"{relative}: no static registration/contract arrays found")
            continue
        for block_name, block in blocks:
            for literal in _extract_string_literals(block):
                _check_forbidden(f"{relative}:{block_name}", literal, errors)


def _check_stub(root: Path, errors: list[str]) -> None:
    relative = Path("docs/bml-script-mod-api.as")
    text = _read(root / relative)
    _check_forbidden(str(relative), text, errors)
    for declaration in REQUIRED_OWNERSHIP_DECLARATIONS:
        if declaration not in text:
            errors.append(f"{relative}: missing ownership declaration {declaration!r}")
    for declaration in STALE_OWNERSHIP_DECLARATIONS:
        if declaration in text:
            errors.append(f"{relative}: stale ownership declaration {declaration!r}")


def main() -> int:
    parser = argparse.ArgumentParser(prog="check_bml_facade")
    parser.add_argument("--root", default=".")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    errors: list[str] = []
    _check_registration_sources(root, errors)
    _check_stub(root, errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("BML AngelScript facade negative check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
