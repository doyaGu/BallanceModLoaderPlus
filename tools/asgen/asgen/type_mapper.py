from __future__ import annotations

import re
from typing import Any

from .model import ParamSpec
from .policy import (
    BML_AVAILABLE_VIEWPORT_FLAGS,
    BML_UNAVAILABLE_ENUM_VALUE_FRAGMENTS,
    BORROWED_HANDLE_RETURNS,
)

INTEGER_ALIASES = {
    "ImGuiID": ("ImGuiID", "uint"),
    "ImU8": ("ImU8", "uint8"),
    "ImS8": ("ImS8", "int8"),
    "ImU16": ("ImU16", "uint16"),
    "ImS16": ("ImS16", "int16"),
    "ImU32": ("ImU32", "uint"),
    "ImS32": ("ImS32", "int"),
    "ImU64": ("ImU64", "uint64"),
    "ImS64": ("ImS64", "int64"),
    "ImWchar": ("ImWchar", "uint"),
    "ImWchar16": ("ImWchar16", "uint16"),
    "ImWchar32": ("ImWchar32", "uint"),
    "ImTextureID": ("ImTextureID", "uint64"),
    "ImGuiKeyChord": ("ImGuiKeyChord", "int"),
    "ImGuiSelectionUserData": ("ImGuiSelectionUserData", "int64"),
}

PRIMITIVES = {
    "void": ("void", "void"),
    "bool": ("bool", "bool"),
    "char": ("char", "int8"),
    "short": ("short", "int16"),
    "int": ("int", "int"),
    "unsigned int": ("unsigned int", "uint"),
    "long": ("long", "int"),
    "unsigned long": ("unsigned long", "uint"),
    "float": ("float", "float"),
    "double": ("double", "double"),
    "size_t": ("size_t", "uint"),
}

VALUE_TYPES = {
    "ImVec2": ("ImVec2", "ImVec2"),
    "ImVec3": ("BMLImGuiASVec3", "ImVec3"),
    "ImVec4": ("ImVec4", "ImVec4"),
    "ImGuiInt2": ("BMLImGuiASInt2", "ImGuiInt2"),
    "ImGuiInt3": ("BMLImGuiASInt3", "ImGuiInt3"),
    "ImGuiInt4": ("BMLImGuiASInt4", "ImGuiInt4"),
}

ANGELSCRIPT_RESERVED_WORDS = {
    "and",
    "auto",
    "bool",
    "break",
    "case",
    "cast",
    "class",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "false",
    "float",
    "for",
    "from",
    "funcdef",
    "if",
    "import",
    "in",
    "inout",
    "int",
    "interface",
    "is",
    "mixin",
    "namespace",
    "not",
    "null",
    "or",
    "out",
    "private",
    "protected",
    "return",
    "shared",
    "super",
    "switch",
    "this",
    "true",
    "uint",
    "void",
    "while",
    "xor",
}


def enum_value_int(value: Any) -> int | None:
    text = str(value).strip()
    try:
        return int(text, 0)
    except ValueError:
        return None


def is_available_enum_value(enum_name: str, value_name: str) -> bool:
    if enum_name == "ImGuiViewportFlags":
        return value_name in BML_AVAILABLE_VIEWPORT_FLAGS
    return not any(fragment in value_name for fragment in BML_UNAVAILABLE_ENUM_VALUE_FRAGMENTS)


class TypeMapper:
    def __init__(self, enum_groups: dict[str, tuple[str, list[dict[str, Any]]]], used_enums: set[str], opaque_handle_types: set[str]) -> None:
        self.enum_groups = enum_groups
        self.used_enums = used_enums
        self.opaque_handle_types = opaque_handle_types

    def enum_default_for(self, as_type: str, raw_default: str) -> str | None:
        values = self.enum_groups.get(as_type)
        if not values:
            return None
        default_value = enum_value_int(raw_default)
        if default_value is None:
            return None
        _, entries = values
        preferred_none = f"{as_type}_None"
        for entry in entries:
            name = str(entry.get("name", ""))
            if not is_available_enum_value(as_type, name):
                continue
            if name == preferred_none and enum_value_int(entry.get("value", 0)) == default_value:
                return preferred_none
        for entry in entries:
            name = str(entry.get("name", ""))
            if not is_available_enum_value(as_type, name):
                continue
            if enum_value_int(entry.get("value", 0)) == default_value:
                return name
        return None


    @staticmethod
    def clean_type(type_name: str) -> str:
        t = type_name.strip()
        t = t.replace("CONST ", "const ")
        t = re.sub(r"\s+", " ", t)
        t = t.replace("const ", "")
        t = t.replace("&", "")
        return t.strip()


    @staticmethod
    def base_type(type_name: str) -> str:
        t = TypeMapper.clean_type(type_name)
        t = re.sub(r"\[[0-9]+\]$", "", t)
        t = t.replace("*", "")
        if t.endswith("_c"):
            t = t[:-2]
        return t.strip()


    @staticmethod
    def is_pointer(type_name: str) -> bool:
        return "*" in type_name


    @staticmethod
    def is_array(type_name: str) -> bool:
        return re.search(r"\[[0-9]+\]$", type_name.strip()) is not None


    @staticmethod
    def is_const_char_ptr(type_name: str) -> bool:
        return type_name.strip().replace("CONST", "const") == "const char*"


    @staticmethod
    def is_char_ptr(type_name: str) -> bool:
        return type_name.strip() == "char*"


    @staticmethod
    def safe_identifier(name: str) -> str:
        cleaned = re.sub(r"[^A-Za-z0-9_]", "_", name)
        if not cleaned or cleaned[0].isdigit():
            cleaned = "_" + cleaned
        if cleaned in ANGELSCRIPT_RESERVED_WORDS:
            cleaned = f"{cleaned}_value"
        return cleaned


    def default_for(self, cpp_type: str, raw_default: str | None, as_type: str) -> str | None:
        if raw_default is None:
            return None
        d = raw_default.strip()
        if d in {"NULL", "nullptr", "((void*)0)"}:
            if as_type.endswith("@"):
                return "null"
            if as_type == "string":
                return "\"\""
            return None
        if "string" in as_type:
            if d.startswith("\"") and d.endswith("\""):
                return d
            return None
        if d in {"true", "false"}:
            return d
        if d == "-FLT_MIN":
            return "-3.402823466e38f"
        if d == "FLT_MAX":
            return "3.402823466e38f"
        enum_default = self.enum_default_for(as_type, d)
        if enum_default is not None:
            return enum_default
        if re.fullmatch(r"[+-]?[0-9]+", d):
            return d
        if re.fullmatch(r"[+-]?[0-9]+\.[0-9]+f?", d):
            return d.rstrip("f")
        if d.startswith("\"") and d.endswith("\""):
            return d
        if d.startswith("ImVec2("):
            return d.replace(" ", "").replace("-FLT_MIN", "-3.402823466e38f").replace("FLT_MAX", "3.402823466e38f")
        if d.startswith("ImVec4("):
            return d.replace(" ", "")
        if d.startswith("ImVec3("):
            return d.replace(" ", "")
        return None


    def map_value_or_primitive(self, type_name: str) -> tuple[str, str, str] | None:
        base = self.base_type(type_name)
        if base in VALUE_TYPES:
            cpp, as_name = VALUE_TYPES[base]
            return cpp, as_name, "value"
        if base in PRIMITIVES:
            cpp, as_name = PRIMITIVES[base]
            return cpp, as_name, "primitive"
        if base in INTEGER_ALIASES:
            cpp, as_name = INTEGER_ALIASES[base]
            return cpp, as_name, "primitive"
        if base in self.enum_groups:
            self.used_enums.add(base)
            return base, base, "enum"
        return None


    def map_param(self, arg: dict[str, Any], defaults: dict[str, str]) -> ParamSpec | None:
        name = self.safe_identifier(arg["name"])
        type_name = str(arg["type"]).strip()
        raw_default = defaults.get(arg["name"])

        if self.base_type(type_name) == "ImTextureRef":
            if arg["name"] == "tex_ref":
                return ParamSpec(
                    name="texture",
                    cpp_type="CKTexture *",
                    as_type="CKTexture@",
                    call_expr="ImTextureRef((void *)texture)",
                    default=None,
                )
            return None

        if self.is_const_char_ptr(type_name):
            default = self.default_for(type_name, raw_default, "string")
            return ParamSpec(
                name=name,
                cpp_type="const std::string &",
                as_type="const string &in",
                call_expr=f"{name}.c_str()" if raw_default not in {"NULL", "((void*)0)"} else f"({name}.empty() ? nullptr : {name}.c_str())",
                default=default,
            )

        if self.is_array(type_name):
            match = re.search(r"\[([0-9]+)\]$", type_name.strip())
            if not match:
                return None
            size = int(match.group(1))
            base = self.base_type(type_name)
            if base == "float" and size in {2, 3, 4}:
                as_name = {2: "ImVec2", 3: "ImVec3", 4: "ImVec4"}[size]
                cpp = VALUE_TYPES[as_name][0]
                return ParamSpec(name=name, cpp_type=f"{cpp} &", as_type=f"{as_name} &inout", call_expr=f"&{name}.x")
            if base == "int" and size in {2, 3, 4}:
                as_name = {2: "ImGuiInt2", 3: "ImGuiInt3", 4: "ImGuiInt4"}[size]
                cpp = VALUE_TYPES[as_name][0]
                return ParamSpec(name=name, cpp_type=f"{cpp} &", as_type=f"{as_name} &inout", call_expr=f"&{name}.x")
            return None

        if self.is_pointer(type_name):
            base = self.base_type(type_name)
            if base in {"bool", "int", "unsigned int", "float", "double"}:
                mapped = self.map_value_or_primitive(base)
                if not mapped:
                    return None
                cpp, as_name, _ = mapped
                call_expr = name if arg.get("reftoptr") else f"&{name}"
                if raw_default in {"NULL", "((void*)0)"} and arg["name"].startswith("p_"):
                    return ParamSpec(name=name, cpp_type=f"{cpp} &", as_type=f"{as_name} &inout", call_expr=call_expr)
                return ParamSpec(name=name, cpp_type=f"{cpp} &", as_type=f"{as_name} &inout", call_expr=call_expr)
            base = self.base_type(type_name)
            if base in VALUE_TYPES or base.endswith("Func"):
                return None
            if base.startswith("Im") and not base.endswith("Callback"):
                self.opaque_handle_types.add(base)
                default = self.default_for(type_name, raw_default, f"{base}@")
                return ParamSpec(name=name, cpp_type=f"{base} *", as_type=f"{base}@".replace("_c@", "@"), call_expr=name, default=default)
            return None

        mapped = self.map_value_or_primitive(type_name)
        if mapped:
            cpp, as_name, kind = mapped
            if kind == "value":
                cpp_type = f"const {cpp} &"
                as_type = f"const {as_name} &in"
            else:
                cpp_type = cpp
                as_type = as_name
            return ParamSpec(name=name, cpp_type=cpp_type, as_type=as_type, call_expr=name, default=self.default_for(type_name, raw_default, as_type))

        return None


    def map_return(self, ret: str, funcname: str) -> tuple[str, str, str] | None:
        ret = ret.strip()
        if ret == "void":
            return "void", "void", "void"
        if self.is_const_char_ptr(ret):
            return "std::string", "string", "string"
        if self.is_pointer(ret):
            base = self.base_type(ret)
            if base in BORROWED_HANDLE_RETURNS:
                self.opaque_handle_types.add(base)
                return f"{base} *", f"{base}@", "handle"
            return None
        mapped = self.map_value_or_primitive(ret)
        if mapped:
            cpp, as_name, kind = mapped
            return cpp, as_name, kind
        return None


    def can_drop_trailing_arg(self, arg: dict[str, Any], defaults: dict[str, str]) -> bool:
        if arg["name"] not in defaults:
            return False
        t = str(arg["type"])
        base = self.base_type(t)
        if arg["name"] == "text_end" and self.is_const_char_ptr(t):
            return defaults.get(arg["name"]) in {"NULL", "((void*)0)", "nullptr"}
        if self.is_pointer(t) and base in VALUE_TYPES:
            return defaults.get(arg["name"]) in {"NULL", "((void*)0)", "nullptr"}
        return base.endswith("Callback") or base in {"void", "ImGuiInputTextCallback", "ImGuiSizeCallback"}
