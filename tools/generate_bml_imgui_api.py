#!/usr/bin/env python3
"""Generate cimgui, the BML ImGui API table, and the C++ ImGui wrapper."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


FUNCTION_RE = re.compile(r"^CIMGUI_API\s+(.+?)\s+([A-Za-z_]\w*)\((.*)\);$")
DEFINED_RE = re.compile(r"defined\s*\(\s*([A-Za-z_]\w*)\s*\)")
DEFINED_BARE_RE = re.compile(r"defined\s+([A-Za-z_]\w*)")
IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")
REPO_IMCONFIG_MARKER = "#ifdef CIMGUI_GENERATOR"
MIRRORED_TYPES = {
    "ImColor_c": "ImColor",
    "ImRect_c": "ImRect",
    "ImTextureRef_c": "ImTextureRef",
    "ImVec2_c": "ImVec2",
    "ImVec2i_c": "ImVec2i",
    "ImVec4_c": "ImVec4",
}


@dataclass
class ConditionalFrame:
    parent_active: bool
    branch_taken: bool
    current_active: bool


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root. Defaults to the parent of tools/.",
    )
    parser.add_argument(
        "--compiler",
        default=None,
        help="Preprocessor/compiler frontend for cimgui generation (e.g. cl, clang, gcc). "
             "Defaults to auto-detection.",
    )
    parser.add_argument(
        "--luajit",
        type=Path,
        default=None,
        help="Path to LuaJIT executable. Defaults to deps/cimgui/luajit.exe.",
    )
    return parser.parse_args()


def evaluate_expr(expr: str, macros: dict[str, bool]) -> bool:
    expr = DEFINED_RE.sub(lambda match: "True" if macros.get(match.group(1), False) else "False", expr)
    expr = DEFINED_BARE_RE.sub(lambda match: "True" if macros.get(match.group(1), False) else "False", expr)
    expr = expr.replace("&&", " and ")
    expr = expr.replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    expr = IDENT_RE.sub(
        lambda match: match.group(0)
        if match.group(0) in {"True", "False", "and", "or", "not"}
        else "False",
        expr,
    )
    expr = expr.strip() or "False"
    return bool(eval(expr, {"__builtins__": {}}, {}))


def update_active(stack: list[ConditionalFrame]) -> bool:
    return all(frame.current_active for frame in stack)


def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8", newline="\n")


def copy_if_changed(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.read_bytes() == src.read_bytes():
        return
    shutil.copyfile(src, dst)


def snapshot_file(path: Path) -> tuple[bytes | None, tuple[float, float] | None]:
    if not path.exists():
        return None, None
    stat = path.stat()
    return path.read_bytes(), (stat.st_atime, stat.st_mtime)


def restore_timestamp_if_unchanged(
    path: Path,
    before_content: bytes | None,
    before_times: tuple[float, float] | None,
) -> None:
    if before_content is None or before_times is None or not path.exists():
        return
    if path.read_bytes() != before_content:
        return
    os.utime(path, before_times)


def detect_compiler(preferred: str | None) -> str:
    if preferred:
        return preferred

    for candidate in ("clang", "gcc", "cl"):
        if shutil.which(candidate):
            return candidate

    raise RuntimeError(
        "Unable to find a supported C/C++ preprocessor for cimgui generation. "
        "Pass --compiler or ensure one of clang/gcc/cl is available on PATH."
    )


def sync_imgui_config(repo_root: Path) -> None:
    src_config = repo_root / "src" / "Utils" / "imconfig.h"
    if REPO_IMCONFIG_MARKER not in src_config.read_text(encoding="utf-8"):
        raise RuntimeError(
            f"{src_config} is missing the CIMGUI_GENERATOR branch required for generated bindings."
        )

    copy_if_changed(src_config, repo_root / "deps" / "cimgui" / "imgui" / "imconfig.h")


def run_cimgui_generator(repo_root: Path, compiler: str, luajit_path: Path | None = None) -> None:
    cimgui_root = repo_root / "deps" / "cimgui"
    luajit = luajit_path if luajit_path else cimgui_root / "luajit.exe"
    generator_dir = cimgui_root / "generator"
    generator = generator_dir / "generator.lua"
    imgui_root = repo_root / "deps" / "cimgui" / "imgui"

    if not luajit.exists():
        raise RuntimeError(f"LuaJIT executable not found: {luajit}")
    if not generator.exists():
        raise RuntimeError(f"cimgui generator script not found: {generator}")
    if not imgui_root.exists():
        raise RuntimeError(f"Canonical imgui source tree not found: {imgui_root}")

    env = dict(os.environ)
    env["IMGUI_PATH"] = str(imgui_root)
    env["CIMGUI_GENERATOR"] = "1"

    compiler_name = Path(compiler).stem.lower()
    define_flag = "/DCIMGUI_GENERATOR" if compiler_name == "cl" else "-DCIMGUI_GENERATOR"
    generated_header = cimgui_root / "cimgui.h"
    generated_source = cimgui_root / "cimgui.cpp"
    header_before, header_times = snapshot_file(generated_header)
    source_before, source_times = snapshot_file(generated_source)

    command = [
        str(luajit),
        str(generator.name),
        compiler,
        "noimstrv",
        define_flag,
    ]

    result = subprocess.run(
        command,
        cwd=generator_dir,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, command)

    restore_timestamp_if_unchanged(generated_header, header_before, header_times)
    restore_timestamp_if_unchanged(generated_source, source_before, source_times)


def split_top_level(text: str, delimiter: str = ",") -> list[str]:
    if text.strip() == "":
        return []

    result: list[str] = []
    current: list[str] = []
    depth_paren = 0
    depth_angle = 0
    depth_bracket = 0

    for char in text:
        if char == "(":
            depth_paren += 1
        elif char == ")":
            depth_paren = max(0, depth_paren - 1)
        elif char == "<":
            depth_angle += 1
        elif char == ">":
            depth_angle = max(0, depth_angle - 1)
        elif char == "[":
            depth_bracket += 1
        elif char == "]":
            depth_bracket = max(0, depth_bracket - 1)

        if char == delimiter and depth_paren == 0 and depth_angle == 0 and depth_bracket == 0:
            result.append("".join(current).strip())
            current.clear()
            continue

        current.append(char)

    tail = "".join(current).strip()
    if tail:
        result.append(tail)
    return result


def strip_default_args(signature: str) -> str:
    if signature == "()":
        return signature

    inner = signature.strip()
    if not inner.startswith("(") or not inner.endswith(")"):
        return signature
    inner = inner[1:-1]

    result: list[str] = []
    depth_paren = 0
    depth_angle = 0
    depth_bracket = 0
    skipping_default = False

    for char in inner:
        if char == "(":
            depth_paren += 1
        elif char == ")":
            depth_paren = max(0, depth_paren - 1)
        elif char == "<":
            depth_angle += 1
        elif char == ">":
            depth_angle = max(0, depth_angle - 1)
        elif char == "[":
            depth_bracket += 1
        elif char == "]":
            depth_bracket = max(0, depth_bracket - 1)

        if skipping_default:
            if char == "," and depth_paren == 0 and depth_angle == 0 and depth_bracket == 0:
                skipping_default = False
                result.append(char)
            continue

        if char == "=" and depth_paren == 0 and depth_angle == 0 and depth_bracket == 0:
            skipping_default = True
            continue

        result.append(char)

    stripped = "".join(result).strip()
    return f"({stripped})" if stripped else "()"


def extract_c_param_type(param_text: str, param_name: str) -> str:
    if param_text == "...":
        return param_text

    result = param_text.strip()
    fn_ptr_pattern = re.compile(rf"\(\s*\*\s*{re.escape(param_name)}\s*\)")
    result = fn_ptr_pattern.sub("(*)", result)
    result = re.sub(rf"\b{re.escape(param_name)}\b\s*$", "", result).rstrip()
    return result


def map_c_type(type_name: str) -> str:
    mapped = type_name
    for c_name, cpp_name in MIRRORED_TYPES.items():
        mapped = re.sub(rf"\b{re.escape(c_name)}\b", cpp_name, mapped)
    return mapped


def find_mirrored_type(type_name: str) -> tuple[str, str] | None:
    for c_name, cpp_name in MIRRORED_TYPES.items():
        if re.search(rf"\b{re.escape(c_name)}\b", type_name):
            return c_name, cpp_name
    return None


CATEGORIES = None  # built dynamically from stname scopes

_IG_CONTEXT_RE = re.compile(
    r"^ig(CreateContext|DestroyContext|GetCurrentContext|SetCurrentContext|"
    r"NewFrame|EndFrame|Render$|GetDrawData|"
    r"GetIO|GetPlatformIO|GetStyle$|GetVersion|GetMainViewport|"
    r"GetBackgroundDrawList|GetForegroundDrawList|GetDrawListSharedData|"
    r"Show|DebugCheckVersionAndDataLayout|"
    r"GetAllocatorFunctions|SetAllocatorFunctions|MemAlloc|MemFree|"
    r"GetTime|GetFrameCount|"
    r"UpdatePlatformWindows|RenderPlatformWindowsDefault|DestroyPlatformWindows|"
    r"FindViewportByID|FindViewportByPlatformHandle)")

_IG_WINDOW_RE = re.compile(
    r"^ig(Begin$|Begin_|End$|BeginChild|EndChild|"
    r"SetNextWindow|GetWindow|IsWindow|SetWindow|"
    r"GetContentRegion|GetScroll|SetScroll|CalcWrapWidth|"
    r"BeginPopup|EndPopup|OpenPopup|CloseCurrentPopup|IsPopup)")

_IG_WIDGET_TEXT_RE = re.compile(
    r"^ig(Text$|Text[CUDVW]|LabelText|BulletText|Bullet$)")

_IG_WIDGET_BUTTON_RE = re.compile(
    r"^ig(Button|SmallButton|InvisibleButton|ArrowButton|Image|ProgressBar)")

_IG_WIDGET_SLIDER_RE = re.compile(
    r"^ig(Checkbox|RadioButton|"
    r"Slider|VSlider|Drag[FISM]|"
    r"Input[FTISDM]|InputScalar|"
    r"ColorEdit|ColorPicker|ColorButton|SetColorEdit)")

_IG_WIDGET_TREE_RE = re.compile(
    r"^ig(TreeNode|TreePush|TreePop|GetTreeNode|CollapsingHeader|SetNextItemOpen|Selectable)")

_IG_WIDGET_TABLE_RE = re.compile(
    r"^ig(BeginTable|EndTable|Table[NASGHR]|"
    r"BeginTabBar|EndTabBar|BeginTabItem|EndTabItem|TabItem|SetTabItem)")

_IG_WIDGET_POPUP_RE = re.compile(
    r"^ig(BeginCombo|EndCombo|Combo$|Combo_|"
    r"BeginListBox|EndListBox|ListBox|"
    r"BeginMenu|EndMenu|MenuItem|BeginMainMenu|EndMainMenu|BeginMenuBar|EndMenuBar|"
    r"BeginItemTooltip|BeginTooltip|EndTooltip|SetTooltip|SetItemTooltip|"
    r"Value|PlotLines|PlotHistogram|"
    r"BeginMultiSelect|EndMultiSelect|SetNextItemSelection|IsItemToggledSelection)")

_IG_LAYOUT_RE = re.compile(
    r"^ig(SameLine|NewLine|Separator|Spacing|Dummy|Indent|Unindent|"
    r"GetCursor|SetCursor|AlignText|GetTextLine|GetFrameHeight|"
    r"Columns|NextColumn|GetColumn|SetColumn|BeginGroup|EndGroup|CalcTextSize|"
    r"PushID|PopID|GetID|PushFont|PopFont|PushStyle|PopStyle|"
    r"PushItem|PopItem|PushTextWrapPos|PopTextWrapPos|PushClipRect|PopClipRect|"
    r"SetNextItemWidth|CalcItemWidth|GetStyleColor|ColorConvert|"
    r"GetFont$|GetFontSize|GetFontBaked$|GetFontTexUvWhitePixel|GetColorU32|"
    r"StyleColors|IsRectVisible)")

_IG_INPUT_RE = re.compile(
    r"^ig(IsKey|IsMouseButton|IsMouseClick|IsMouseDoubleClick|IsMouseDrag|"
    r"IsMouseRelease|IsMouseDown|IsMouseHoveringRect|IsMousePosValid|IsAnyMouseDown|"
    r"GetMousePos|GetMouseDrag|GetMouseClick|ResetMouseDrag|"
    r"GetMouseCursor|SetMouseCursor|GetClipboard|SetClipboard|"
    r"GetKeyName|GetKeyPressedAmount|Shortcut|SetNextItemShortcut|SetItemKeyOwner|"
    r"SetNextFrameWantCapture|"
    r"SetNextItemAllowOverlap|IsItem|GetItem|IsAnyItem|"
    r"SetItemDefaultFocus|SetNavCursorVisible|SetKeyboardFocus|SetNextItemStorageID|"
    r"BeginDragDrop|EndDragDrop|AcceptDragDrop|GetDragDrop|SetDragDrop)")


def _stname_to_category(stname: str) -> str:
    """Map a stname to a short snake_case category name.

    Strips the common Im/ImGui prefix, then converts CamelCase to snake_case.
    Examples: ImDrawList -> draw_list, ImGuiIO -> io, ImVec2 -> vec2
    """
    name = stname
    if name.startswith("ImGui"):
        name = name[5:]
    elif name.startswith("Im"):
        name = name[2:]
    result = re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", name)
    result = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", result)
    return result.lower()


def classify_function(name: str) -> str:
    """Classify an ig* (ImGui namespace) cimgui function by domain."""
    if _IG_CONTEXT_RE.match(name):
        return "context"
    if _IG_WINDOW_RE.match(name):
        return "window"
    if _IG_WIDGET_TEXT_RE.match(name):
        return "widget_text"
    if _IG_WIDGET_BUTTON_RE.match(name):
        return "widget_button"
    if _IG_WIDGET_SLIDER_RE.match(name):
        return "widget_slider"
    if _IG_WIDGET_TREE_RE.match(name):
        return "widget_tree"
    if _IG_WIDGET_TABLE_RE.match(name):
        return "widget_table"
    if _IG_WIDGET_POPUP_RE.match(name):
        return "widget_popup"
    if _IG_LAYOUT_RE.match(name):
        return "layout"
    if _IG_INPUT_RE.match(name):
        return "input"
    return "misc"


def classify_by_stname(stname: str, cimgui_name: str) -> str:
    """Classify by stname + cimgui function name."""
    if stname == "":
        return classify_function(cimgui_name)
    return _stname_to_category(stname)


def collect_api_entries(
    cimgui_source: Path,
) -> dict[str, tuple[list[str], list[str]]]:
    macros = {
        "CIMGUI_VARGS0": False,
        "IMGUI_HAS_DOCK": False,
        "IMGUI_ENABLE_FREETYPE": False,
    }

    stack: list[ConditionalFrame] = []
    active = True
    categorized: dict[str, tuple[list[str], list[str]]] = {}

    for raw_line in cimgui_source.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()

        if line.startswith("#ifdef "):
            name = line[7:].strip()
            cond = macros.get(name, False)
            stack.append(ConditionalFrame(active, cond, active and cond))
            active = update_active(stack)
            continue
        if line.startswith("#ifndef "):
            name = line[8:].strip()
            cond = not macros.get(name, False)
            stack.append(ConditionalFrame(active, cond, active and cond))
            active = update_active(stack)
            continue
        if line.startswith("#if "):
            expr = line[4:].strip()
            cond = evaluate_expr(expr, macros)
            stack.append(ConditionalFrame(active, cond, active and cond))
            active = update_active(stack)
            continue
        if line.startswith("#elif "):
            frame = stack[-1]
            cond = (not frame.branch_taken) and evaluate_expr(line[6:].strip(), macros)
            frame.current_active = frame.parent_active and cond
            frame.branch_taken = frame.branch_taken or cond
            active = update_active(stack)
            continue
        if line == "#else":
            frame = stack[-1]
            cond = not frame.branch_taken
            frame.current_active = frame.parent_active and cond
            frame.branch_taken = True
            active = update_active(stack)
            continue
        if line == "#endif":
            stack.pop()
            active = update_active(stack)
            continue

        if not active:
            continue

        match = FUNCTION_RE.match(line)
        if not match:
            continue

        ret, name, params = match.groups()
        if name.startswith("ig"):
            cat = classify_function(name)
            short_name = name[2:]  # strip "ig" prefix
        else:
            underscore = name.find("_")
            stname = name[:underscore] if underscore > 0 else name
            if stname == "ImVector" or name.startswith("ImVector_"):
                stname = "ImVector"
            cat = _stname_to_category(stname)
            # Strip "ClassName_" prefix for field name
            prefix = stname + "_"
            short_name = name[len(prefix):] if name.startswith(prefix) else name
            # Constructor: ClassName_ClassName -> New (avoid collision with type name)
            if short_name == stname or short_name.startswith(stname + "_"):
                short_name = "New" + short_name[len(stname):]
        if cat not in categorized:
            categorized[cat] = ([], [])
        fields, inits = categorized[cat]
        fields.append(f"    {ret} (*{short_name})({params});")
        inits.append(f"    {name},")  # init uses the real cimgui symbol

    return categorized


def parse_c_prototypes(cimgui_source: Path) -> dict[str, tuple[str, list[str]]]:
    prototypes: dict[str, tuple[str, list[str]]] = {}
    for raw_line in cimgui_source.read_text(encoding="utf-8").splitlines():
        match = FUNCTION_RE.match(raw_line.strip())
        if not match:
            continue
        ret, name, params = match.groups()
        prototypes[name] = (ret.strip(), split_top_level(params))
    return prototypes


def load_definition_items(definitions_path: Path) -> tuple[list[dict], set[tuple[str, str, str]]]:
    data = json.loads(definitions_path.read_text(encoding="utf-8"))
    items: list[dict] = []
    v_variants: set[tuple[str, str, str]] = set()

    for overloads in data.values():
        for item in overloads:
            items.append(item)
            scope = item.get("stname", "")
            namespace = item.get("namespace", "")
            v_variants.add((scope, namespace, item.get("funcname", "")))

    return items, v_variants


def resolve_header_location(repo_root: Path, location: str) -> tuple[Path, int] | None:
    source_name, _, line_text = location.partition(":")
    if not source_name or not line_text.isdigit():
        return None

    header_map = {
        "imgui": repo_root / "deps" / "imgui" / "imgui.h",
        "internal": repo_root / "deps" / "imgui" / "imgui_internal.h",
        "imgui_internal": repo_root / "deps" / "imgui" / "imgui_internal.h",
    }
    header_path = header_map.get(source_name)
    if not header_path or not header_path.exists():
        return None

    return header_path, int(line_text)


def is_inline_definition(item: dict, header_cache: dict[Path, list[str]], repo_root: Path) -> bool:
    location = item.get("location")
    if not location:
        return False

    resolved = resolve_header_location(repo_root, location)
    if not resolved:
        return False

    header_path, line_number = resolved
    lines = header_cache.setdefault(header_path, header_path.read_text(encoding="utf-8").splitlines())
    index = max(0, line_number - 1)
    snippet: list[str] = []

    while index < len(lines):
        snippet.append(lines[index].strip())
        joined = " ".join(snippet)
        brace_pos = joined.find("{")
        semicolon_pos = joined.find(";")
        if brace_pos != -1 and (semicolon_pos == -1 or brace_pos < semicolon_pos):
            return True
        if semicolon_pos != -1:
            return False
        index += 1

    return False


def has_qualified_definition(item: dict, header_cache: dict[Path, list[str]], repo_root: Path) -> bool:
    scope = item.get("stname", "")
    funcname = item.get("funcname", "")
    if not scope or not funcname:
        return False

    needle = f"{scope}::{funcname}("
    for header_path in (
        repo_root / "deps" / "imgui" / "imgui.h",
        repo_root / "deps" / "imgui" / "imgui_internal.h",
    ):
        lines = header_cache.setdefault(header_path, header_path.read_text(encoding="utf-8").splitlines())
        if any(needle in line for line in lines):
            return True
    return False


def original_return_type(item: dict) -> str:
    mapped = map_c_type(item["ret"])
    retref = item.get("retref")
    if retref and mapped.endswith("*"):
        mapped = mapped[:-1].rstrip() + retref
    return mapped


def call_scope(scope: str, name: str) -> str:
    return f"{scope}::{name}" if scope else name


def build_arg_call_expression(param_name: str, c_type: str, arg_info: dict) -> str:
    cpp_type = arg_info["type"]
    mirrored = find_mirrored_type(c_type)
    if arg_info.get("reftoptr"):
        if mirrored:
            c_name, cpp_name = mirrored
            return f"::bml::imgui::detail::ToCPtr<{cpp_name}, {c_name}>(&{param_name})"
        return f"&{param_name}"

    if not mirrored:
        if (
            "*" in c_type
            and "&" in cpp_type
            and "const" not in c_type
            and "..." not in c_type
        ):
            return f"&{param_name}"
        return param_name

    c_name, cpp_name = mirrored
    if "*" in c_type:
        return f"::bml::imgui::detail::ToCPtr<{cpp_name}, {c_name}>({param_name})"
    return f"::bml::imgui::detail::ToCValue<{cpp_name}, {c_name}>({param_name})"


def build_return_expression(call_expr: str, item: dict) -> str:
    mirrored = find_mirrored_type(item["ret"])
    if item.get("retref"):
        if mirrored:
            c_name, cpp_name = mirrored
            return f"*::bml::imgui::detail::FromCPtr<{cpp_name}, {c_name}>({call_expr})"
        return f"*({call_expr})"

    if mirrored:
        c_name, cpp_name = mirrored
        if "*" in item["ret"]:
            return f"::bml::imgui::detail::FromCPtr<{cpp_name}, {c_name}>({call_expr})"
        return f"::bml::imgui::detail::FromCValue<{cpp_name}, {c_name}>({call_expr})"

    return call_expr


def generate_wrapper_definition(
    item: dict,
    prototypes: dict[str, tuple[str, list[str]]],
    v_variants: set[tuple[str, str, str]],
) -> str | None:
    if item.get("constructor") or item.get("destructor") or item.get("templated"):
        return None

    scope = item.get("stname", "")
    namespace = item.get("namespace", "")
    if not scope and namespace != "ImGui":
        return None

    c_name = item["ov_cimguiname"]
    if c_name not in prototypes:
        return None

    c_ret, c_params = prototypes[c_name]
    args_original = item.get("argsoriginal", "()")
    signature = strip_default_args(args_original)
    if not signature:
        signature = "()"

    return_type = original_return_type(item)
    const_suffix = " const" if item.get("signature", "").endswith("const") else ""
    qualified_name = call_scope(scope, item["funcname"])
    lines = [f"inline {return_type} {qualified_name}{signature}{const_suffix} {{"]

    args_t = item.get("argsT", [])
    arg_offset = 0
    if scope and not item.get("is_static_function"):
        arg_offset = 1

    is_vararg = "..." in signature
    named_arg_infos = [arg for arg in args_t[arg_offset:] if arg["type"] != "..."]

    if is_vararg:
        v_key = (scope, namespace, item["funcname"] + "V")
        if v_key not in v_variants or not named_arg_infos:
            return None

        last_named = named_arg_infos[-1]["name"]
        named_args = ", ".join(arg["name"] for arg in named_arg_infos)
        v_call = f"{item['funcname']}V({named_args}, args)" if named_args else f"{item['funcname']}V(args)"
        lines.append("    va_list args;")
        lines.append(f"    va_start(args, {last_named});")
        if return_type == "void":
            lines.append(f"    {v_call};")
            lines.append("    va_end(args);")
            lines.append("}")
            return "\n".join(lines)

        lines.append(f"    {return_type} result = {v_call};")
        lines.append("    va_end(args);")
        lines.append("    return result;")
        lines.append("}")
        return "\n".join(lines)

    call_args: list[str] = []
    if scope and not item.get("is_static_function"):
        self_expr = "this"
        if (
            item.get("signature", "").endswith("const")
            and c_params
            and "*" in c_params[0]
            and "const" not in c_params[0]
        ):
            self_expr = f"const_cast<{scope} *>(this)"
        call_args.append(self_expr)

    if len(c_params) < len(args_t):
        return None

    for c_param_text, arg_info in zip(c_params[arg_offset:], named_arg_infos):
        c_param_type = extract_c_param_type(c_param_text, arg_info["name"])
        call_args.append(build_arg_call_expression(arg_info["name"], c_param_type, arg_info))

    stname = item.get("stname", "")
    category = classify_by_stname(stname, c_name)
    # Strip prefix for field name: "ig" for namespace, "ClassName_" for members
    if stname == "":
        field_name = c_name[2:]  # strip "ig"
    else:
        prefix = stname + "_"
        field_name = c_name[len(prefix):] if c_name.startswith(prefix) else c_name
        # Constructor: ClassName_ClassName -> New
        if field_name == stname or field_name.startswith(stname + "_"):
            field_name = "New" + field_name[len(stname):]
    api_call = f"::bml::imgui::detail::RequireCurrentApi()->{category}->{field_name}({', '.join(call_args)})"
    if return_type == "void":
        lines.append(f"    {api_call};")
    else:
        lines.append(f"    return {build_return_expression(api_call, item)};")
    lines.append("}")
    return "\n".join(lines)


def generate_cpp_wrapper(repo_root: Path, cimgui_source: Path, definitions_path: Path) -> str:
    prototypes = parse_c_prototypes(cimgui_source)
    items, v_variants = load_definition_items(definitions_path)
    header_cache: dict[Path, list[str]] = {}

    namespace_defs: list[str] = []
    member_defs: list[str] = []

    for item in items:
        if is_inline_definition(item, header_cache, repo_root):
            continue
        if has_qualified_definition(item, header_cache, repo_root):
            continue

        definition = generate_wrapper_definition(item, prototypes, v_variants)
        if not definition:
            continue

        if item.get("stname"):
            member_defs.append(definition)
        else:
            namespace_defs.append(definition)

    helper = """// Generated by tools/generate_bml_imgui_api.py from cimgui definitions.
// Do not edit manually.

#include <bit>

namespace bml::imgui::detail {
    template <typename CppT, typename CT>
    inline CT ToCValue(const CppT &value) noexcept {
        static_assert(sizeof(CppT) == sizeof(CT));
        return std::bit_cast<CT>(value);
    }

    template <typename CppT, typename CT>
    inline CppT FromCValue(const CT &value) noexcept {
        static_assert(sizeof(CppT) == sizeof(CT));
        return std::bit_cast<CppT>(value);
    }

    template <typename CppT, typename CT>
    inline CT *ToCPtr(CppT *value) noexcept {
        return reinterpret_cast<CT *>(value);
    }

    template <typename CppT, typename CT>
    inline const CT *ToCPtr(const CppT *value) noexcept {
        return reinterpret_cast<const CT *>(value);
    }

    template <typename CppT, typename CT>
    inline CppT *FromCPtr(CT *value) noexcept {
        return reinterpret_cast<CppT *>(value);
    }

    template <typename CppT, typename CT>
    inline const CppT *FromCPtr(const CT *value) noexcept {
        return reinterpret_cast<const CppT *>(value);
    }

    // Validation happens once in ScopedApiScope constructor (see bml_imgui.hpp).
    // This is a trivial getter for the hot path -- no per-call validation overhead.
    inline const BML_ImGuiApi *RequireCurrentApi() {
        return ::bml::imgui::GetCurrentApi();
    }
} // namespace bml::imgui::detail

namespace ImGui {
"""

    if namespace_defs:
        helper += "\n\n".join(namespace_defs) + "\n"
    helper += "} // namespace ImGui\n\n"
    if member_defs:
        helper += "\n\n".join(member_defs) + "\n"
    return helper


def _cat_to_struct_name(cat: str) -> str:
    """Convert a category name like 'im_draw_list' to 'BML_ImDrawListApi'."""
    parts = cat.split("_")
    return "BML_" + "".join(p.capitalize() for p in parts) + "Api"


def _generate_api_header(output_path: Path, all_cats: list[str]) -> None:
    """Generate bml_imgui_api.h with all sub-table structs."""
    lines = [
        "#ifndef BML_IMGUI_API_H",
        "#define BML_IMGUI_API_H",
        "",
        "/**",
        " * @file bml_imgui_api.h",
        f" * @brief Full BML_ImGuiApi struct definition with {len(all_cats)} sub-tables.",
        " *",
        " * Auto-generated by tools/generate_bml_imgui_api.py.",
        " * Does NOT include imgui_internal.h.",
        " */",
        "",
        '#include "imgui.h"',
        "",
        '#include "bml_imgui.h"',
        '#include "bml_errors.h"',
        '#include "bml_interface.h"',
        '#include "cimgui.h"',
        "",
        "BML_BEGIN_CDECLS",
        "",
    ]

    for cat in all_cats:
        struct_name = _cat_to_struct_name(cat)
        lines.append(f"typedef struct {struct_name} {{")
        lines.append("    size_t struct_size;")
        lines.append(f'#include "bml_imgui_{cat}_fields.inc"')
        lines.append(f"}} {struct_name};")
        lines.append("")

    lines.append("typedef struct BML_ImGuiApi {")
    lines.append("    BML_InterfaceHeader header;")
    lines.append("    const char *imgui_version_utf8;")
    lines.append("    const char *cimgui_version_utf8;")
    lines.append("    BML_Result (*ValidateMainThreadAccess)(void);")
    lines.append("    BML_Result (*ValidateCurrentFrameAccess)(void);")
    for cat in all_cats:
        struct_name = _cat_to_struct_name(cat)
        lines.append(f"    const {struct_name} *{cat};")
    lines.append("} BML_ImGuiApi;")
    lines.append("")
    lines.append("BML_END_CDECLS")
    lines.append("")
    lines.append("#endif /* BML_IMGUI_API_H */")
    lines.append("")

    write_if_changed(output_path, "\n".join(lines))


def _generate_subtable_init(output_path: Path, all_cats: list[str]) -> None:
    """Generate static sub-table instances and main struct initializer for UIMod.cpp."""
    lines = [
        "// Generated by tools/generate_bml_imgui_api.py.",
        "// Do not edit manually.",
        "",
    ]

    var_names: list[str] = []
    for cat in all_cats:
        struct_name = _cat_to_struct_name(cat)
        var_name = f"g_{cat.title().replace('_', '')}Api"
        var_names.append(var_name)
        lines.append(f"static const {struct_name} {var_name} = {{")
        lines.append(f"    sizeof({struct_name}),")
        lines.append(f'#include "bml_imgui_{cat}_init.inc"')
        lines.append("};")
        lines.append("")

    lines.append("const BML_ImGuiApi g_ImGuiApi = {")
    lines.append('    BML_IFACE_HEADER(BML_ImGuiApi, "bml.ui.imgui", 1, 0),')
    lines.append("    ImGui::GetVersion(),")
    lines.append('    "cimgui",')
    lines.append("    API_ValidateMainThreadAccess,")
    lines.append("    API_ValidateCurrentFrameAccess,")
    for var_name in var_names:
        lines.append(f"    &{var_name},")
    lines.append("};")
    lines.append("")

    write_if_changed(output_path, "\n".join(lines))


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    compiler = detect_compiler(args.compiler)

    cimgui_source = repo_root / "deps" / "cimgui" / "cimgui.h"
    public_cimgui = repo_root / "modules" / "BML_UI" / "include" / "cimgui.h"
    include_dir = repo_root / "modules" / "BML_UI" / "include"
    src_dir = repo_root / "modules" / "BML_UI" / "src"
    cpp_wrapper_output = include_dir / "bml_imgui_cpp_defs.inl"
    definitions_path = repo_root / "deps" / "cimgui" / "generator" / "output" / "definitions.json"

    sync_imgui_config(repo_root)
    run_cimgui_generator(repo_root, compiler, luajit_path=args.luajit)
    categorized = collect_api_entries(cimgui_source)

    header = (
        "// Generated by tools/generate_bml_imgui_api.py from deps/cimgui/cimgui.h.\n"
        "// Do not edit manually.\n"
    )

    # Sort categories: imgui_* first (alphabetical), then class-based (alphabetical)
    all_cats = sorted(categorized.keys(), key=lambda c: (0 if c.startswith("imgui_") else 1, c))

    total = 0
    counts: dict[str, int] = {}
    generated_field_files: set[str] = set()
    for cat in all_cats:
        fields, inits = categorized[cat]
        counts[cat] = len(fields)
        total += len(fields)
        write_if_changed(include_dir / f"bml_imgui_{cat}_fields.inc", header + "\n".join(fields) + "\n")
        write_if_changed(src_dir / f"bml_imgui_{cat}_init.inc", header + "\n".join(inits) + "\n")
        generated_field_files.add(f"bml_imgui_{cat}_fields.inc")
        generated_field_files.add(f"bml_imgui_{cat}_init.inc")

    # Clean up stale .inc files from previous runs
    for old in include_dir.glob("bml_imgui_*_fields.inc"):
        if old.name not in generated_field_files:
            old.unlink()
    for old in src_dir.glob("bml_imgui_*_init.inc"):
        if old.name not in generated_field_files:
            old.unlink()
    for legacy in ("bml_imgui_api_fields.inc",):
        p = include_dir / legacy
        if p.exists():
            p.unlink()
    for legacy in ("bml_imgui_api_init.inc",):
        p = src_dir / legacy
        if p.exists():
            p.unlink()

    write_if_changed(cpp_wrapper_output, generate_cpp_wrapper(repo_root, cimgui_source, definitions_path))

    # Generate bml_imgui_api.h with all sub-table structs
    _generate_api_header(include_dir / "bml_imgui_api.h", all_cats)

    # Generate subtable instances + main struct init for UIMod.cpp
    _generate_subtable_init(src_dir / "bml_imgui_subtables.inc", all_cats)

    copy_if_changed(cimgui_source, public_cimgui)

    cat_summary = ", ".join(f"{cat}={counts[cat]}" for cat in all_cats)
    print(
        f"Generated cimgui, {total} BML ImGui API entries across {len(all_cats)} tables "
        f"({cat_summary}), and the C++ ImGui wrapper with compiler '{compiler}'."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
