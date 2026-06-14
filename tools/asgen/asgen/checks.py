from __future__ import annotations

import re
from pathlib import Path

from .handwritten import draw_list_method_bindings, script_friendly_global_bindings


def write_if_changed(path: Path, text: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def check_file(path: Path, text: str) -> bool:
    return path.exists() and path.read_text(encoding="utf-8") == text


def assert_approved_skip_reasons(report: list[tuple[str, str]]) -> None:
    approved_exact = {
        "non-Imgui namespace",
        "imgui_internal",
        "templated",
        "varargs",
        "context/frame lifecycle is owned by BML",
        "platform lifecycle is owned by BML",
        "debug tooling is not part of BML ImGui script surface",
        "unsupported return type",
        "unsupported mutable char buffer",
        "hand-written overload",
        "not available in BML's compiled ImGui surface",
    }
    approved_patterns = [
        re.compile(r"^unsupported parameter [A-Za-z_][A-Za-z0-9_]*:.+$"),
        re.compile(r"^removed non-trailing default for [A-Za-z_][A-Za-z0-9_]*$"),
        re.compile(r"^duplicate AngelScript declaration$"),
    ]
    bad: list[tuple[str, str]] = []
    for name, reason in report:
        if reason in approved_exact:
            continue
        if any(pattern.match(reason) for pattern in approved_patterns):
            continue
        bad.append((name, reason))
    if bad:
        sample = ", ".join(f"{name}: {reason}" for name, reason in bad[:10])
        raise RuntimeError(f"unapproved AngelScript binding skip reason(s): {sample}")


def assert_no_forbidden_symbols(texts: dict[Path, str]) -> None:
    forbidden_literals = ["ImGuiBB_", "BBSlot", "BBConfig", "Behavior"]
    register_decl = re.compile(r'Register(?:GlobalFunction|ObjectType|ObjectMethod|ObjectProperty|Enum|EnumValue)\("([^"]*)"')
    for path, text in texts.items():
        for match in register_decl.finditer(text):
            declaration = match.group(1)
            for token in forbidden_literals:
                if token in declaration:
                    raise RuntimeError(f"forbidden AngelScript binding token {token!r} found in {path}: {declaration}")
            if re.search(r"\bBB\b", declaration):
                raise RuntimeError(f"forbidden AngelScript binding token 'BB' found in {path}: {declaration}")


def assert_negative_script_surface(source: str) -> None:
    forbidden_registered_fragments = [
        "ImGuiBB_",
        "BBSlot",
        "BBConfig",
        "Behavior",
        "void Debug",
        "bool Debug",
        "DebugCheckVersionAndDataLayout",
        "DestroyPlatformWindows",
        "RenderPlatformWindowsDefault",
        "UpdatePlatformWindows",
        "SliderScalar(",
        "DragScalar(",
        "InputScalar(",
        "SetDragDropPayload(",
        "AcceptDragDropPayload(",
    ]
    register_decl = re.compile(r'RegisterGlobalFunction\("([^"]*)"')
    bad: list[str] = []
    for match in register_decl.finditer(source):
        declaration = match.group(1)
        for fragment in forbidden_registered_fragments:
            if fragment in declaration:
                bad.append(declaration)
                break
        if re.search(r"\bBB\b", declaration):
            bad.append(declaration)
    if bad:
        sample = ", ".join(bad[:10])
        raise RuntimeError(f"negative AngelScript surface check failed: {sample}")


def _cpp_string(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\"", "\\\"")


def assert_handwritten_bindings_synchronized(source: str, stub: str) -> None:
    missing: list[str] = []
    for binding in script_friendly_global_bindings():
        declaration = binding.declaration
        cpp_declaration = _cpp_string(declaration)
        if f'RegisterGlobalFunction("{cpp_declaration}", asFUNCTION({binding.wrapper})' not in source:
            missing.append(f"cpp global {declaration}")
        if f"  {declaration};" not in stub:
            missing.append(f"stub global {declaration}")

    for binding in draw_list_method_bindings():
        declaration = binding.declaration
        cpp_declaration = _cpp_string(declaration)
        if f'RegisterObjectMethod("ImDrawList", "{cpp_declaration}", asFUNCTION({binding.function})' not in source:
            missing.append(f"cpp ImDrawList method {declaration}")
        if f"  {declaration};" not in stub:
            missing.append(f"stub ImDrawList method {declaration}")

    if missing:
        sample = "; ".join(missing[:10])
        raise RuntimeError(f"hand-written AngelScript binding table is not synchronized: {sample}")
