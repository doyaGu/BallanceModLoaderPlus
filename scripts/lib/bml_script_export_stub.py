from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class ExportStubError(ValueError):
    pass


@dataclass(frozen=True)
class TypeInfo:
    raw: str
    kind: str
    wrapper_type: str
    signature_type: str


@dataclass(frozen=True)
class ParamInfo:
    name: str
    type: TypeInfo


@dataclass(frozen=True)
class ExportInfo:
    export_name: str
    method_name: str
    field_name: str
    signature: str
    return_type: TypeInfo
    params: list[ParamInfo]


_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_DOTTED_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*$")
_NAMESPACE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*$")
_ARRAY_TYPES = {"bool", "int", "float", "string", "uint8"}
_AS_KEYWORDS = {
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
    "final",
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
    "override",
    "private",
    "protected",
    "return",
    "shared",
    "string",
    "switch",
    "true",
    "uint",
    "uint8",
    "void",
    "while",
}


def load_export_stub_idl(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ExportStubError(f"{path}: cannot read IDL: {exc.strerror}") from exc
    except json.JSONDecodeError as exc:
        raise ExportStubError(f"{path}: invalid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ExportStubError("export stub IDL root must be an object")
    return data


def generate_export_stub_from_file(path: Path) -> str:
    return generate_export_stub(load_export_stub_idl(path))


def generate_export_stub(spec: dict[str, Any]) -> str:
    provider_mod_id = _required_string(spec, "providerModId")
    if not _DOTTED_IDENTIFIER.match(provider_mod_id):
        raise ExportStubError("providerModId must be a dot-separated identifier")

    class_name = _required_string(spec, "className")
    _require_identifier(class_name, "className")

    namespace = _optional_string(spec, "namespace")
    if namespace is not None and not _NAMESPACE.match(namespace):
        raise ExportStubError("namespace must be an AngelScript namespace path")
    if namespace is not None:
        for part in namespace.split("::"):
            _require_identifier(part, "namespace")

    exports_data = spec.get("exports")
    if not isinstance(exports_data, list) or not exports_data:
        raise ExportStubError("exports must be a non-empty array")

    exports = _build_exports(exports_data)
    return _emit_stub(provider_mod_id, namespace, class_name, exports)


def _build_exports(exports_data: list[Any]) -> list[ExportInfo]:
    exports: list[ExportInfo] = []
    method_names: set[str] = set()
    field_names: set[str] = set()
    signatures: set[tuple[str, str]] = set()

    for index, item in enumerate(exports_data):
        if not isinstance(item, dict):
            raise ExportStubError(f"exports[{index}] must be an object")

        export_name = _required_string(item, "name", f"exports[{index}]")
        if not _DOTTED_IDENTIFIER.match(export_name):
            raise ExportStubError(f"exports[{index}].name must be a dot-separated identifier")

        method_name = _optional_string(item, "method") or _default_method_name(export_name)
        _require_identifier(method_name, f"exports[{index}].method")
        if method_name in method_names:
            raise ExportStubError(f"duplicate wrapper method name: {method_name}")
        method_names.add(method_name)

        return_type, param_types, signature = _read_signature_parts(item, export_name, index)
        params = _read_params(item, param_types, index)
        signature = signature or _build_signature(export_name, return_type, params)

        key = (export_name, signature)
        if key in signatures:
            raise ExportStubError(f"duplicate export signature: {export_name} {signature}")
        signatures.add(key)

        field_name = _make_unique_field_name(method_name, field_names)
        field_names.add(field_name)

        exports.append(ExportInfo(
            export_name=export_name,
            method_name=method_name,
            field_name=field_name,
            signature=signature,
            return_type=return_type,
            params=params,
        ))

    return exports


def _read_signature_parts(item: dict[str, Any],
                          export_name: str,
                          index: int) -> tuple[TypeInfo, list[TypeInfo], str | None]:
    signature = _optional_string(item, "signature")
    if signature is not None:
        parsed_name, return_type, param_types = _parse_signature(signature)
        if parsed_name and _IDENTIFIER.match(export_name) and parsed_name != export_name:
            raise ExportStubError(
                f"exports[{index}].signature name '{parsed_name}' does not match export name '{export_name}'")
        return return_type, param_types, signature

    return_type = _type_info(_required_string(item, "return", f"exports[{index}]"), allow_void=True)
    raw_params = item.get("params", [])
    if not isinstance(raw_params, list):
        raise ExportStubError(f"exports[{index}].params must be an array")
    param_types = []
    for param_index, raw_param in enumerate(raw_params):
        if not isinstance(raw_param, dict):
            raise ExportStubError(f"exports[{index}].params[{param_index}] must be an object")
        param_types.append(_type_info(_required_string(raw_param, "type",
                                                       f"exports[{index}].params[{param_index}]"),
                                      allow_void=False))
    return return_type, param_types, None


def _read_params(item: dict[str, Any], param_types: list[TypeInfo], index: int) -> list[ParamInfo]:
    raw_params = item.get("params")
    if raw_params is None:
        return [ParamInfo(name=f"arg{i}", type=param_type) for i, param_type in enumerate(param_types)]
    if not isinstance(raw_params, list):
        raise ExportStubError(f"exports[{index}].params must be an array")
    if len(raw_params) != len(param_types):
        raise ExportStubError(f"exports[{index}].params count does not match signature")

    params: list[ParamInfo] = []
    used_names: set[str] = set()
    for param_index, (raw_param, param_type) in enumerate(zip(raw_params, param_types)):
        if not isinstance(raw_param, dict):
            raise ExportStubError(f"exports[{index}].params[{param_index}] must be an object")
        name = _optional_string(raw_param, "name") or f"arg{param_index}"
        _require_identifier(name, f"exports[{index}].params[{param_index}].name")
        if name in used_names:
            raise ExportStubError(f"exports[{index}] has duplicate parameter name: {name}")
        used_names.add(name)
        explicit_type = _optional_string(raw_param, "type")
        if explicit_type is not None and _type_info(explicit_type, allow_void=False).kind != param_type.kind:
            raise ExportStubError(f"exports[{index}].params[{param_index}].type does not match signature")
        params.append(ParamInfo(name=name, type=param_type))
    return params


def _emit_stub(provider_mod_id: str,
               namespace: str | None,
               class_name: str,
               exports: list[ExportInfo]) -> str:
    lines: list[str] = [
        "// This file is generated by scripts/Generate-BMLScriptExportStub.py. Do not edit by hand.\n",
        "// Pure script client wrapper. Runtime calls still go through BML::ExportResolver.\n\n",
    ]

    namespace_parts = namespace.split("::") if namespace else []
    for part in namespace_parts:
        lines.append(f"namespace {part} {{\n")
    if namespace_parts:
        lines.append("\n")

    lines.append(f"class {class_name} {{\n")
    for export in exports:
        lines.append(f"  private BML::ExportResolver@ {export.field_name};\n")
    lines.append("\n")
    lines.append(f"  {class_name}() {{\n")
    lines.append(f"    Bind({_as_string(provider_mod_id)});\n")
    lines.append("  }\n\n")
    lines.append(f"  {class_name}(const string &in providerModId) {{\n")
    lines.append("    Bind(providerModId);\n")
    lines.append("  }\n\n")
    lines.append("  void Clear() {\n")
    for export in exports:
        lines.append(f"    if ({export.field_name} !is null)\n")
        lines.append(f"      {export.field_name}.Clear();\n")
    lines.append("  }\n\n")
    lines.append("  int Rebind() {\n")
    lines.append("    int status = BML::ERROR_OK;\n")
    for export in exports:
        lines.append(f"    int {export.field_name}Status = {export.field_name}.Rebind();\n")
        lines.append(f"    if ({export.field_name}Status != BML::ERROR_OK && status == BML::ERROR_OK)\n")
        lines.append(f"      status = {export.field_name}Status;\n")
    lines.append("    return status;\n")
    lines.append("  }\n")

    for export in exports:
        lines.append("\n")
        lines.extend(_emit_method(export))

    lines.append("\n")
    lines.append("  private void Bind(const string &in providerModId) {\n")
    for export in exports:
        lines.append(
            f"    @{export.field_name} = BML::ExportResolver(providerModId, "
            f"{_as_string(export.export_name)}, {_as_string(export.signature)});\n")
    lines.append("  }\n")
    lines.append("}\n")

    if namespace_parts:
        lines.append("\n")
        for part in reversed(namespace_parts):
            lines.append(f"}} // namespace {part}\n")
    return "".join(lines)


def _emit_method(export: ExportInfo) -> list[str]:
    params = [f"{param.type.wrapper_type} {param.name}" for param in export.params]
    if export.return_type.kind != "void":
        params.append(f"{_result_param_type(export.return_type)} result")

    lines = [f"  int {export.method_name}({', '.join(params)}) {{\n"]
    if not export.params and export.return_type.kind == "void":
        lines.append(f"    return {export.field_name}.CallVoid();\n")
        lines.append("  }\n")
        return lines

    lines.append("    BML::CallFrame@ frame = BML::CallFrame();\n")
    for index, param in enumerate(export.params):
        lines.append(f"    int status = frame.{_set_method(param.type)}({index}, {param.name});\n")
        lines.append("    if (status != BML::ERROR_OK)\n")
        lines.append("      return status;\n")

    lines.append(f"    int status = {export.field_name}.Call(frame);\n")
    lines.append("    if (status != BML::ERROR_OK)\n")
    lines.append("      return status;\n")
    if export.return_type.kind == "void":
        lines.append("    return status;\n")
    else:
        lines.append(f"    return frame.{_get_result_method(export.return_type)}(result);\n")
    lines.append("  }\n")
    return lines


def _parse_signature(signature: str) -> tuple[str, TypeInfo, list[TypeInfo]]:
    open_paren = signature.find("(")
    close_paren = signature.rfind(")")
    if open_paren == -1 or close_paren == -1 or close_paren < open_paren:
        raise ExportStubError(f"invalid export signature: {signature}")
    if signature[close_paren + 1:].strip():
        raise ExportStubError(f"invalid export signature trailing text: {signature}")

    prefix = signature[:open_paren].strip()
    if not prefix:
        raise ExportStubError(f"missing export signature return type: {signature}")
    parts = prefix.rsplit(None, 1)
    if len(parts) == 1:
        return_name = ""
        return_type = parts[0]
    else:
        return_type, return_name = parts

    params_text = signature[open_paren + 1:close_paren]
    param_types = [_type_info(param, allow_void=False) for param in _split_parameters(params_text)]
    return return_name, _type_info(return_type, allow_void=True), param_types


def _split_parameters(parameters: str) -> list[str]:
    if not parameters.strip():
        return []
    out: list[str] = []
    depth = 0
    start = 0
    for index, ch in enumerate(parameters + ","):
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
            if depth < 0:
                raise ExportStubError("invalid generic parameter list")
        elif ch == "," and depth == 0:
            item = parameters[start:index].strip()
            if not item:
                raise ExportStubError("empty signature parameter")
            out.append(item)
            start = index + 1
    if depth != 0:
        raise ExportStubError("unterminated generic parameter list")
    return out


def _type_info(raw_type: str, *, allow_void: bool) -> TypeInfo:
    prepared = _remove_trailing_parameter_name(raw_type)
    compact = _compact(prepared)
    if compact == "void":
        if not allow_void:
            raise ExportStubError("void is not a valid export parameter type")
        return TypeInfo(raw=raw_type, kind="void", wrapper_type="void", signature_type="void")
    if compact in {"bool", "int", "float"}:
        return TypeInfo(raw=raw_type, kind=compact, wrapper_type=compact, signature_type=compact)
    if compact in {"string", "conststring&in", "string&in"}:
        return TypeInfo(raw=raw_type, kind="string", wrapper_type="const string &in", signature_type="const string &in")
    if compact == "CKObject@":
        return TypeInfo(raw=raw_type, kind="object", wrapper_type="CKObject@", signature_type="CKObject@")

    array_inner = _array_inner(compact)
    if array_inner:
        kind = "buffer" if array_inner == "uint8" else f"{array_inner}_array"
        return TypeInfo(raw=raw_type,
                        kind=kind,
                        wrapper_type=f"const array<{array_inner}> &in",
                        signature_type=f"array<{array_inner}>@" if allow_void else f"const array<{array_inner}> &in")

    raise ExportStubError(f"unsupported export type: {raw_type}")


def _array_inner(compact: str) -> str | None:
    match = re.fullmatch(r"array<([^<>]+)>@", compact)
    if match and match.group(1) in _ARRAY_TYPES:
        return match.group(1)
    match = re.fullmatch(r"([^<>\[\]]+)\[\]@", compact)
    if match and match.group(1) in _ARRAY_TYPES:
        return match.group(1)
    match = re.fullmatch(r"constarray<([^<>]+)>&in", compact)
    if match and match.group(1) in _ARRAY_TYPES:
        return match.group(1)
    match = re.fullmatch(r"const([^<>\[\]]+)\[\]&in", compact)
    if match and match.group(1) in _ARRAY_TYPES:
        return match.group(1)
    return None


def _build_signature(export_name: str, return_type: TypeInfo, params: list[ParamInfo]) -> str:
    param_types = []
    for param in params:
        param_types.append(f"{param.type.signature_type} {param.name}")
    name = f" {export_name}" if _IDENTIFIER.match(export_name) else ""
    return f"{return_type.signature_type}{name}({', '.join(param_types)})"


def _result_param_type(type_info: TypeInfo) -> str:
    if type_info.kind in {"bool", "int", "float"}:
        return f"{type_info.kind} &out"
    if type_info.kind == "string":
        return "string &out"
    if type_info.kind == "object":
        return "CKObject@ &out"
    inner = _result_array_inner(type_info)
    if inner:
        return f"array<{inner}>@ &out"
    raise ExportStubError(f"unsupported result type: {type_info.raw}")


def _set_method(type_info: TypeInfo) -> str:
    mapping = {
        "bool": "SetBool",
        "int": "SetInt",
        "float": "SetFloat",
        "string": "SetString",
        "object": "SetObject",
        "bool_array": "SetArray",
        "int_array": "SetArray",
        "float_array": "SetArray",
        "string_array": "SetArray",
        "buffer": "SetArray",
    }
    return mapping[type_info.kind]


def _get_result_method(type_info: TypeInfo) -> str:
    mapping = {
        "bool": "GetResultBool",
        "int": "GetResultInt",
        "float": "GetResultFloat",
        "string": "GetResultString",
        "object": "GetResultObject",
        "bool_array": "GetResultArray",
        "int_array": "GetResultArray",
        "float_array": "GetResultArray",
        "string_array": "GetResultArray",
        "buffer": "GetResultArray",
    }
    return mapping[type_info.kind]


def _result_array_inner(type_info: TypeInfo) -> str | None:
    if type_info.kind == "buffer":
        return "uint8"
    suffix = "_array"
    if type_info.kind.endswith(suffix):
        return type_info.kind[:-len(suffix)]
    return None


def _remove_trailing_parameter_name(type_name: str) -> str:
    value = type_name.strip()
    if not value:
        return value
    parts = value.rsplit(None, 1)
    if len(parts) != 2:
        return value
    maybe_type, maybe_name = parts
    if _IDENTIFIER.match(maybe_name) and maybe_name not in {"in", "out", "inout", "const"}:
        return maybe_type.strip()
    return value


def _compact(value: str) -> str:
    return "".join(ch for ch in value if ch not in " \t\r\n")


def _default_method_name(export_name: str) -> str:
    name = re.sub(r"[^A-Za-z0-9_]", "_", export_name)
    if not name or name[0].isdigit():
        name = "_" + name
    if name in _AS_KEYWORDS:
        name += "_"
    return name


def _make_unique_field_name(method_name: str, used: set[str]) -> str:
    field_name = method_name[0].lower() + method_name[1:] + "Resolver"
    if field_name not in used:
        return field_name
    suffix = 2
    while f"{field_name}{suffix}" in used:
        suffix += 1
    return f"{field_name}{suffix}"


def _required_string(data: dict[str, Any], key: str, owner: str | None = None) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        label = f"{owner}.{key}" if owner else key
        raise ExportStubError(f"{label} must be a non-empty string")
    return value.strip()


def _optional_string(data: dict[str, Any], key: str) -> str | None:
    value = data.get(key)
    if value is None:
        return None
    if not isinstance(value, str) or not value.strip():
        raise ExportStubError(f"{key} must be a non-empty string when present")
    return value.strip()


def _require_identifier(value: str, label: str) -> None:
    if not _IDENTIFIER.match(value) or value in _AS_KEYWORDS:
        raise ExportStubError(f"{label} must be an AngelScript identifier")


def _as_string(value: str) -> str:
    return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""
