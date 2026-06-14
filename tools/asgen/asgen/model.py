from __future__ import annotations

from dataclasses import dataclass


@dataclass
class ParamSpec:
    name: str
    cpp_type: str
    as_type: str
    call_expr: str
    default: str | None = None


@dataclass
class FunctionBinding:
    wrapper_name: str
    script_decl: str
    cpp_return_type: str
    return_kind: str
    params: list[ParamSpec]
    call_args: list[str]
    imgui_name: str
    char_buffer_name: str | None = None
    needs_frame_scope: bool = True


@dataclass
class HandWrittenGlobalBinding:
    declaration: str
    wrapper: str
    diagnostic_name: str | None = None
    call_convention: str = "asCALL_CDECL"


@dataclass
class ObjectMethodBinding:
    declaration: str
    function: str
