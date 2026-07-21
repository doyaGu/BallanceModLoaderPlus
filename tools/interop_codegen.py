#!/usr/bin/env python3
"""Generate deterministic Interop API descriptors from .bmlapi JSON.

The loader never reads .bmlapi files at runtime.  This tool validates the
authoring representation, canonicalizes it, and emits fixed C-ABI descriptor
tables plus reference material consumed by native and AngelScript authors.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


FIELD_TYPES = {
    "bool": "BML_INTEROP_FIELD_BOOL",
    "int": "BML_INTEROP_FIELD_INT",
    "float": "BML_INTEROP_FIELD_FLOAT",
    "string": "BML_INTEROP_FIELD_STRING",
    "object": "BML_INTEROP_FIELD_OBJECT",
    "vec2": "BML_INTEROP_FIELD_VEC2",
    "vec3": "BML_INTEROP_FIELD_VEC3",
    "mat4": "BML_INTEROP_FIELD_MAT4",
    "array<bool>": "BML_INTEROP_FIELD_BOOL_ARRAY",
    "array<int>": "BML_INTEROP_FIELD_INT_ARRAY",
    "array<float>": "BML_INTEROP_FIELD_FLOAT_ARRAY",
    "array<string>": "BML_INTEROP_FIELD_STRING_ARRAY",
    "array<object>": "BML_INTEROP_FIELD_OBJECT_ARRAY",
    "array<vec2>": "BML_INTEROP_FIELD_VEC2_ARRAY",
    "array<vec3>": "BML_INTEROP_FIELD_VEC3_ARRAY",
    "array<mat4>": "BML_INTEROP_FIELD_MAT4_ARRAY",
}

ENDPOINT_KINDS = {
    "resource": "BML_INTEROP_ENDPOINT_RESOURCE",
    "component": "BML_INTEROP_ENDPOINT_COMPONENT",
    "collection": "BML_INTEROP_ENDPOINT_COLLECTION",
    "stream": "BML_INTEROP_ENDPOINT_STREAM",
    "query": "BML_INTEROP_ENDPOINT_QUERY",
    "command": "BML_INTEROP_ENDPOINT_COMMAND",
}

AS_FIELD_TYPES = {
    field_type: field_type_name.removeprefix("BML_INTEROP_")
    for field_type, field_type_name in FIELD_TYPES.items()
}

AS_ENDPOINT_KINDS = {
    endpoint_kind: endpoint_kind_name.removeprefix("BML_INTEROP_")
    for endpoint_kind, endpoint_kind_name in ENDPOINT_KINDS.items()
}

# Generated bindings deliberately use the same shallow value vocabulary on
# both sides.  The C++ form is header-only (InteropClient.h); the AngelScript
# form targets the private host bridge registered as BML::Interop.
CPP_FIELD_TYPES = {
    "bool": "bool",
    "int": "int",
    "float": "float",
    "string": "std::string",
    "object": "::BML::Interop::ObjectRef",
    "vec2": "::BML::Interop::Vec2",
    "vec3": "::BML::Interop::Vec3",
    "mat4": "::BML::Interop::Mat4",
    "array<bool>": "std::vector<bool>",
    "array<int>": "std::vector<int>",
    "array<float>": "std::vector<float>",
    "array<string>": "std::vector<std::string>",
    "array<object>": "std::vector<::BML::Interop::ObjectRef>",
    "array<vec2>": "std::vector<::BML::Interop::Vec2>",
    "array<vec3>": "std::vector<::BML::Interop::Vec3>",
    "array<mat4>": "std::vector<::BML::Interop::Mat4>",
}

AS_VALUE_TYPES = {
    "bool": "bool",
    "int": "int",
    "float": "float",
    "string": "string",
    "object": "::BML::Interop::ObjectRef",
    "vec2": "::BML::Vec2",
    "vec3": "::BML::Vec3",
    "mat4": "::BML::Mat4",
    "array<bool>": "array<bool>",
    "array<int>": "array<int>",
    "array<float>": "array<float>",
    "array<string>": "array<string>",
    "array<object>": "array<::BML::Interop::ObjectRef>",
    "array<vec2>": "array<::BML::Vec2>",
    "array<vec3>": "array<::BML::Vec3>",
    "array<mat4>": "array<::BML::Mat4>",
}

FIELD_GETTERS = {
    "bool": "GetBool",
    "int": "GetInt",
    "float": "GetFloat",
    "string": "GetString",
    "object": "GetObject",
    "vec2": "GetVec2",
    "vec3": "GetVec3",
    "mat4": "GetMat4",
    "array<bool>": "GetBoolArray",
    "array<int>": "GetIntArray",
    "array<float>": "GetFloatArray",
    "array<string>": "GetStringArray",
    "array<object>": "GetObjectArray",
    "array<vec2>": "GetVec2Array",
    "array<vec3>": "GetVec3Array",
    "array<mat4>": "GetMat4Array",
}

# Win32 defines GetObject as a macro in many native mod translation units.
# The C++ transport names its object accessor GetObjectRef; AngelScript keeps
# the more idiomatic GetObject spelling in its separate map above.
CPP_FIELD_GETTERS = dict(FIELD_GETTERS)
CPP_FIELD_GETTERS["object"] = "GetObjectRef"

FIELD_SETTERS = {
    "bool": "SetBool",
    "int": "SetInt",
    "float": "SetFloat",
    "string": "SetString",
    "object": "SetObject",
    "vec2": "SetVec2",
    "vec3": "SetVec3",
    "mat4": "SetMat4",
    "array<bool>": "SetBoolArray",
    "array<int>": "SetIntArray",
    "array<float>": "SetFloatArray",
    "array<string>": "SetStringArray",
    "array<object>": "SetObjectArray",
    "array<vec2>": "SetVec2Array",
    "array<vec3>": "SetVec3Array",
    "array<mat4>": "SetMat4Array",
}

KEY_RE = re.compile(r"^[A-Za-z0-9_.-]+$")
CPP_RE = re.compile(r"[^A-Za-z0-9_]")


class ApiDefinitionError(ValueError):
    pass


def parse_hash64(value: Any, what: str) -> int:
    if isinstance(value, int) and not isinstance(value, bool):
        result = value
    elif isinstance(value, str) and value.startswith(("0x", "0X")):
        try:
            result = int(value, 16)
        except ValueError as error:
            raise ApiDefinitionError(f"{what} must be a 64-bit hash") from error
    else:
        raise ApiDefinitionError(f"{what} must be an integer or 0x-prefixed hexadecimal string")
    if not 0 < result <= 0xFFFFFFFFFFFFFFFF:
        raise ApiDefinitionError(f"{what} must be a non-zero 64-bit hash")
    return result


def key(value: Any, what: str) -> str:
    if not isinstance(value, str) or not KEY_RE.fullmatch(value):
        raise ApiDefinitionError(f"{what} must match {KEY_RE.pattern}")
    return value


def positive(value: Any, what: str, *, allow_zero: bool = False) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < (0 if allow_zero else 1):
        comparison = "non-negative" if allow_zero else "positive"
        raise ApiDefinitionError(f"{what} must be a {comparison} integer")
    return value


def uint32(value: Any, what: str, *, allow_zero: bool = False) -> int:
    result = positive(value, what, allow_zero=allow_zero)
    if result > 0xFFFFFFFF:
        raise ApiDefinitionError(f"{what} must fit in uint32")
    return result


def identifier(value: str) -> str:
    result = CPP_RE.sub("_", value).strip("_")
    if not result:
        result = "api"
    if result[0].isdigit():
        result = "_" + result
    return result


def camel(value: str) -> str:
    parts = [part for part in re.split(r"[_\W]+", value) if part]
    return "".join(part[:1].upper() + part[1:] for part in parts) or "ApiDefinition"


def validate_generated_identifiers(schemas: list[Record], endpoints: list[Endpoint]) -> None:
    top_level: dict[str, str] = {
        name: "generated runtime helper"
        for name in ("ApiId", "Major", "Minor", "Hash", "Schemas", "Endpoints",
                     "CompatibleApiHashes", "Descriptor", "Require", "RegisterProvider")
    }

    def add(symbols: dict[str, str], symbol: str, source: str) -> None:
        previous = symbols.get(symbol)
        if previous is not None:
            raise ApiDefinitionError(
                f"generated identifier collision for {symbol}: {previous} and {source}"
            )
        symbols[symbol] = source

    input_schema_ids = {endpoint.input_schema for endpoint in endpoints if endpoint.input_schema}
    for record in schemas:
        record_name = camel(record.name)
        source = f"schema {record.name}"
        add(top_level, record_name, source)
        add(top_level, f"{record_name}Field", source)
        add(top_level, value_name(record), source)
        add(top_level, f"Decode{record_name}", source)
        if record.id in input_schema_ids:
            add(top_level, f"Encode{record_name}", source)

        members: dict[str, str] = {}
        for field in record.fields:
            member = camel(field.name)
            add(members, member, f"field {record.name}.{field.name}")
            if field.optional:
                add(members, f"Has{member}", f"optional flag for {record.name}.{field.name}")

    endpoint_symbols: dict[str, str] = {}
    for endpoint in endpoints:
        prefix = {
            "resource": "Read",
            "component": "Read",
            "collection": "Open",
            "stream": "Open",
            "query": "Query",
            "command": "Command",
        }[endpoint.kind]
        add(endpoint_symbols, f"{prefix}{camel(endpoint.name)}", f"endpoint {endpoint.name}")


@dataclass(frozen=True)
class Field:
    id: int
    name: str
    type: str
    optional: bool


@dataclass(frozen=True)
class Record:
    id: int
    name: str
    fields: tuple[Field, ...]


@dataclass(frozen=True)
class Endpoint:
    name: str
    kind: str
    input_schema: int
    output_schema: int
    requires_probe: bool


@dataclass(frozen=True)
class ApiDefinition:
    api_id: str
    major: int
    minor: int
    schemas: tuple[Record, ...]
    endpoints: tuple[Endpoint, ...]
    compatible_hashes: tuple[int, ...]
    canonical: str
    hash64: int


def parse_api_definition(raw: Any) -> ApiDefinition:
    if not isinstance(raw, dict):
        raise ApiDefinitionError("top-level value must be an object")
    api_id = key(raw.get("api"), "api")
    version = raw.get("version")
    if not isinstance(version, dict):
        raise ApiDefinitionError("version must be an object")
    major = uint32(version.get("major"), "version.major")
    minor = uint32(version.get("minor", 0), "version.minor", allow_zero=True)

    raw_schemas = raw.get("schemas")
    if not isinstance(raw_schemas, list) or not raw_schemas:
        raise ApiDefinitionError("schemas must be a non-empty array")
    schemas: list[Record] = []
    record_ids: set[int] = set()
    record_names: set[str] = set()
    for index, raw_record in enumerate(raw_schemas):
        if not isinstance(raw_record, dict):
            raise ApiDefinitionError(f"schemas[{index}] must be an object")
        record_id = uint32(raw_record.get("id"), f"schemas[{index}].id")
        record_name = key(raw_record.get("name"), f"schemas[{index}].name")
        if record_id in record_ids or record_name in record_names:
            raise ApiDefinitionError(f"duplicate record id or name: {record_name}")
        raw_fields = raw_record.get("fields", [])
        if not isinstance(raw_fields, list):
            raise ApiDefinitionError(f"schemas[{index}].fields must be an array")
        fields: list[Field] = []
        field_ids: set[int] = set()
        field_names: set[str] = set()
        for field_index, raw_field in enumerate(raw_fields):
            if not isinstance(raw_field, dict):
                raise ApiDefinitionError(f"schemas[{index}].fields[{field_index}] must be an object")
            field_id = uint32(raw_field.get("id"), f"schemas[{index}].fields[{field_index}].id")
            field_name = key(raw_field.get("name"), f"schemas[{index}].fields[{field_index}].name")
            field_type = raw_field.get("type")
            if field_type not in FIELD_TYPES:
                raise ApiDefinitionError(f"unsupported field type {field_type!r} in {record_name}.{field_name}")
            optional = raw_field.get("optional", False)
            if not isinstance(optional, bool):
                raise ApiDefinitionError(f"{record_name}.{field_name}.optional must be boolean")
            if field_id in field_ids or field_name in field_names:
                raise ApiDefinitionError(f"duplicate field id or name in {record_name}: {field_name}")
            field_ids.add(field_id)
            field_names.add(field_name)
            fields.append(Field(field_id, field_name, field_type, optional))
        record_ids.add(record_id)
        record_names.add(record_name)
        schemas.append(Record(record_id, record_name, tuple(sorted(fields, key=lambda item: item.id))))

    known_schema_ids = {record.id for record in schemas}
    raw_endpoints = raw.get("endpoints")
    if not isinstance(raw_endpoints, list) or not raw_endpoints:
        raise ApiDefinitionError("endpoints must be a non-empty array")
    endpoints: list[Endpoint] = []
    endpoint_names: set[str] = set()
    for index, raw_source in enumerate(raw_endpoints):
        if not isinstance(raw_source, dict):
            raise ApiDefinitionError(f"endpoints[{index}] must be an object")
        endpoint_name = key(raw_source.get("name"), f"endpoints[{index}].name")
        kind = raw_source.get("kind")
        if kind not in ENDPOINT_KINDS:
            raise ApiDefinitionError(f"unsupported endpoint kind {kind!r} for {endpoint_name}")
        input_schema = uint32(raw_source.get("input", 0), f"endpoints[{index}].input", allow_zero=True)
        output_schema = uint32(raw_source.get("output"), f"endpoints[{index}].output")
        requires_probe = raw_source.get("requires_probe", False)
        if not isinstance(requires_probe, bool):
            raise ApiDefinitionError(f"endpoints[{index}].requires_probe must be boolean")
        accepts_input = kind in {"query", "command"}
        if accepts_input != (input_schema != 0):
            raise ApiDefinitionError(f"{kind} endpoint {endpoint_name} must {'have' if accepts_input else 'not have'} input")
        if input_schema and input_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown input schema {input_schema}")
        if output_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown output schema {output_schema}")
        if endpoint_name in endpoint_names:
            raise ApiDefinitionError(f"duplicate endpoint name {endpoint_name}")
        endpoint_names.add(endpoint_name)
        endpoints.append(Endpoint(endpoint_name, kind, input_schema, output_schema, requires_probe))

    validate_generated_identifiers(schemas, endpoints)

    raw_compatible_hashes = raw.get("compatible_hashes", [])
    if not isinstance(raw_compatible_hashes, list):
        raise ApiDefinitionError("compatible_hashes must be an array when present")
    compatible_hashes = tuple(
        parse_hash64(value, f"compatible_hashes[{index}]")
        for index, value in enumerate(raw_compatible_hashes)
    )
    if len(set(compatible_hashes)) != len(compatible_hashes):
        raise ApiDefinitionError("compatible_hashes must not contain duplicates")

    # Compatibility permits an older descriptor hash to be accepted at
    # runtime.  It intentionally does not participate in the structural
    # descriptor hash: it changes rollout policy, not api semantics.
    canonical_object = {
        "api": api_id,
        "version": {"major": major, "minor": minor},
        "schemas": [
            {
                "id": record.id,
                "name": record.name,
                "fields": [
                    {"id": field.id, "name": field.name, "type": field.type, "optional": field.optional}
                    for field in record.fields
                ],
            }
            for record in sorted(schemas, key=lambda item: item.id)
        ],
        "endpoints": [
            {
                "name": endpoint.name,
                "kind": endpoint.kind,
                "input": endpoint.input_schema,
                "output": endpoint.output_schema,
                "requires_probe": endpoint.requires_probe,
            }
            for endpoint in sorted(endpoints, key=lambda item: item.name)
        ],
    }
    canonical = json.dumps(canonical_object, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    descriptor_hash = int.from_bytes(hashlib.sha256(canonical.encode("utf-8")).digest()[:8], "big")
    if descriptor_hash in compatible_hashes:
        raise ApiDefinitionError("compatible_hashes must not repeat the current descriptor hash")
    return ApiDefinition(api_id, major, minor, tuple(sorted(schemas, key=lambda item: item.id)),
                    tuple(sorted(endpoints, key=lambda item: item.name)), compatible_hashes, canonical, descriptor_hash)


def value_name(record: Record) -> str:
    """A snapshot value must not collide with its descriptor constant."""
    return f"{camel(record.name)}Value"


def append_cpp_decode(lines: list[str], record: Record) -> None:
    descriptor = camel(record.name)
    value = value_name(record)
    lines.append(f"struct {value} {{")
    for field in record.fields:
        field_name = camel(field.name)
        if field.optional:
            lines.append(f"    bool Has{field_name} = false;")
        lines.append(f"    {CPP_FIELD_TYPES[field.type]} {field_name}{{}};")
    lines.append("};")
    lines.append("")
    lines.append(f"inline int Decode{descriptor}(const ::BML::Interop::Record &record, {value} &out) {{")
    lines.append("    std::uint32_t schema = 0;")
    lines.append("    int status = record.Schema(schema);")
    lines.append("    if (status != BML_OK)")
    lines.append("        return status;")
    lines.append(f"    if (schema != {descriptor}.Id)")
    lines.append("        return BML_ERROR_INTEROP_SCHEMA_MISMATCH;")
    lines.append(f"    {value} decoded{{}};")
    for field in record.fields:
        field_name = camel(field.name)
        getter = CPP_FIELD_GETTERS[field.type]
        field_id = f"{descriptor}Field::{field_name}"
        if field.optional:
            lines.append("    if (status == BML_OK) {")
            lines.append(f"        const int fieldStatus = record.{getter}({field_id}, decoded.{field_name});")
            lines.append("        if (fieldStatus == BML_ERROR_NOT_FOUND) {")
            lines.append(f"            decoded.Has{field_name} = false;")
            lines.append("        } else if (fieldStatus != BML_OK) {")
            lines.append("            status = fieldStatus;")
            lines.append("        } else {")
            lines.append(f"            decoded.Has{field_name} = true;")
            lines.append("        }")
            lines.append("    }")
        else:
            lines.append(f"    if (status == BML_OK) status = record.{getter}({field_id}, decoded.{field_name});")
    lines.append("    if (status == BML_OK)")
    lines.append("        out = std::move(decoded);")
    lines.append("    return status;")
    lines.append("}")
    lines.append("")


def append_cpp_encode(lines: list[str], record: Record) -> None:
    descriptor = camel(record.name)
    value = value_name(record)
    lines.append(f"inline int Encode{descriptor}(const {value} &value, ::BML::Interop::Detail::InputRecord &out) {{")
    lines.append(f"    int status = out.Create(ApiId, {descriptor}.Id);")
    for field in record.fields:
        field_name = camel(field.name)
        setter = FIELD_SETTERS[field.type]
        field_id = f"{descriptor}Field::{field_name}"
        condition = f"status == BML_OK && value.Has{field_name}" if field.optional else "status == BML_OK"
        lines.append(f"    if ({condition}) status = out.{setter}({field_id}, value.{field_name});")
    lines.append("    return status;")
    lines.append("}")
    lines.append("")


def append_cpp_consumers(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    lines.extend([
        "/*",
        " * Typed consumer bindings are generated from the same descriptor. They",
        " * remain inline and use only the fixed C ABI through InteropClient.h.",
        " */",
        "",
    ])
    for record in api.schemas:
        append_cpp_decode(lines, record)

    input_ids = sorted({endpoint.input_schema for endpoint in api.endpoints if endpoint.input_schema})
    for schema_id in input_ids:
        append_cpp_encode(lines, schemas[schema_id])

    for endpoint in api.endpoints:
        endpoint_name = camel(endpoint.name)
        output = schemas[endpoint.output_schema]
        output_value = value_name(output)
        if endpoint.kind == "resource":
            lines.extend([
                f"inline int Read{endpoint_name}({output_value} &out) {{",
                "    ::BML::Interop::Record record;",
                "    int status = Require();",
                f"    if (status == BML_OK) status = ::BML::Interop::ReadResource(ApiId, \"{endpoint.name}\", record);",
                f"    if (status == BML_OK) status = Decode{camel(output.name)}(record, out);",
                "    return status;",
                "}",
                "",
            ])
        elif endpoint.kind == "component":
            lines.extend([
                f"inline int Read{endpoint_name}(::BML::Interop::ObjectRef object, {output_value} &out) {{",
                "    ::BML::Interop::Record record;",
                "    int status = Require();",
                f"    if (status == BML_OK) status = ::BML::Interop::ReadComponent(ApiId, \"{endpoint.name}\", object, record);",
                f"    if (status == BML_OK) status = Decode{camel(output.name)}(record, out);",
                "    return status;",
                "}",
                "",
            ])
        elif endpoint.kind == "collection":
            lines.extend([
                f"inline int Open{endpoint_name}(::BML::Interop::Collection &out) {{",
                "    const int status = Require();",
                f"    return status == BML_OK ? ::BML::Interop::OpenCollection(ApiId, \"{endpoint.name}\", out) : status;",
                "}",
                "",
            ])
        elif endpoint.kind == "stream":
            lines.extend([
                f"inline int Open{endpoint_name}(int capacity, ::BML::Interop::Stream &out) {{",
                "    const int status = Require();",
                f"    return status == BML_OK ? ::BML::Interop::OpenStream(ApiId, \"{endpoint.name}\", capacity, out) : status;",
                "}",
                "",
            ])
        else:
            input_record = schemas[endpoint.input_schema]
            input_value = value_name(input_record)
            invoke = "InvokeQuery" if endpoint.kind == "query" else "InvokeCommand"
            verb = "Query" if endpoint.kind == "query" else "Command"
            lines.extend([
                f"inline int {verb}{endpoint_name}(const {input_value} &input, {output_value} &out) {{",
                "    ::BML::Interop::Detail::InputRecord builder;",
                "    int status = Require();",
                f"    if (status == BML_OK) status = Encode{camel(input_record.name)}(input, builder);",
                "    ::BML::Interop::Record record;",
                f"    if (status == BML_OK) status = ::BML::Interop::Detail::{invoke}(ApiId, \"{endpoint.name}\", builder, record);",
                f"    if (status == BML_OK) status = Decode{camel(output.name)}(record, out);",
                "    return status;",
                "}",
                "",
            ])


def emit_header(api: ApiDefinition) -> str:
    namespace = "::".join(camel(part) for part in api.api_id.split("."))
    c_prefix = f"BML_INTEROP_{identifier(api.api_id).upper()}"
    c_symbol = "BML_InteropGenerated_" + "_".join(
        camel(part) for part in api.api_id.split(".")
    )
    lines = [
        "// Generated by tools/interop_codegen.py. Do not edit by hand.",
        "#pragma once",
        "",
        '#include "BML/InteropApi.h"',
        "",
        "/*",
        " * C++ exposes header-only namespaced convenience constants below.",
        " * C receives the same descriptor through static C data and macros in",
        " * the #else branch. Neither form exports a C++ symbol from the DLL.",
        " */",
        "#ifdef __cplusplus",
        "#include <cstdint>",
        '#include "BML/InteropClient.h"',
        "",
        f"namespace BML::Interop::Generated::{namespace} {{",
        f"inline constexpr const char ApiId[] = \"{api.api_id}\";",
        f"inline constexpr unsigned int Major = {api.major};",
        f"inline constexpr unsigned int Minor = {api.minor};",
        f"inline constexpr std::uint64_t Hash = 0x{api.hash64:016X}ULL;",
        "",
    ]
    for record in api.schemas:
        record_id = camel(record.name)
        field_pointer = "nullptr"
        if record.fields:
            lines.append(f"inline constexpr BML_InteropFieldDescriptor {record_id}Fields[] = {{")
            for field in record.fields:
                optional = "1" if field.optional else "0"
                lines.append(f"    {{{field.id}, \"{field.name}\", {FIELD_TYPES[field.type]}, {optional}}},")
            lines.append("};")
            field_pointer = f"{record_id}Fields"
        lines.append(f"inline constexpr BML_InteropSchemaDescriptor {record_id} = "
                     f"{{{record.id}, \"{record.name}\", {field_pointer}, {len(record.fields)}}};")
        lines.append("")
    lines.append("inline constexpr BML_InteropSchemaDescriptor Schemas[] = {")
    for record in api.schemas:
        lines.append(f"    {camel(record.name)},")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr BML_InteropEndpointDescriptor Endpoints[] = {")
    for endpoint in api.endpoints:
        probe = "1" if endpoint.requires_probe else "0"
        lines.append(f"    {{\"{endpoint.name}\", {ENDPOINT_KINDS[endpoint.kind]}, {endpoint.input_schema}, "
                     f"{endpoint.output_schema}, {probe}}},")
    lines.append("};")
    lines.append("")
    compatible_pointer = "nullptr"
    if api.compatible_hashes:
        lines.append("inline constexpr std::uint64_t CompatibleApiHashes[] = {")
        for compatible_hash in api.compatible_hashes:
            lines.append(f"    0x{compatible_hash:016X}ULL,")
        lines.append("};")
        lines.append("")
        compatible_pointer = "CompatibleApiHashes"
    lines.append("inline constexpr BML_InteropApiDescriptor Descriptor = {")
    lines.append("    sizeof(BML_InteropApiDescriptor), ApiId, Major, Minor, Hash,")
    lines.append("    Schemas, sizeof(Schemas) / sizeof(Schemas[0]),")
    lines.append("    Endpoints, sizeof(Endpoints) / sizeof(Endpoints[0]),")
    lines.append(f"    {compatible_pointer}, {len(api.compatible_hashes)},")
    lines.append("};")
    lines.append("")
    lines.extend([
        "/* Header-only helpers: still no C++ symbol crosses the DLL boundary. */",
        "inline int Require() {",
        "    return BML_Interop_RequireApi(ApiId, Major, Hash);",
        "}",
        "inline int RegisterProvider(const BML_InteropProviderCallbacks *callbacks, void *userdata) {",
        "    return BML_Interop_RegisterProvider(&Descriptor, callbacks, userdata);",
        "}",
        "",
    ])
    for record in api.schemas:
        record_id = camel(record.name)
        lines.append(f"namespace {record_id}Field {{")
        for field in record.fields:
            lines.append(f"inline constexpr std::uint32_t {camel(field.name)} = {field.id};")
        lines.append("}")
        lines.append("")
    append_cpp_consumers(lines, api)
    lines.append(f"}} // namespace BML::Interop::Generated::{namespace}")
    lines.append("")
    lines.extend([
        "#else",
        f"#define {c_prefix}_API_ID \"{api.api_id}\"",
        f"#define {c_prefix}_MAJOR {api.major}u",
        f"#define {c_prefix}_MINOR {api.minor}u",
        f"#define {c_prefix}_HASH UINT64_C(0x{api.hash64:016X})",
        "",
    ])
    for record in api.schemas:
        record_id = camel(record.name)
        if record.fields:
            lines.append(f"static const BML_InteropFieldDescriptor {c_symbol}_{record_id}Fields[] = {{")
            for field in record.fields:
                optional = "1" if field.optional else "0"
                lines.append(f"    {{{field.id}u, \"{field.name}\", {FIELD_TYPES[field.type]}, {optional}}},")
            lines.append("};")
            lines.append("")
    lines.append(f"static const BML_InteropSchemaDescriptor {c_symbol}_Schemas[] = {{")
    for record in api.schemas:
        field_pointer = f"{c_symbol}_{camel(record.name)}Fields" if record.fields else "0"
        lines.append(f"    {{{record.id}u, \"{record.name}\", {field_pointer}, {len(record.fields)}u}},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const BML_InteropEndpointDescriptor {c_symbol}_Endpoints[] = {{")
    for endpoint in api.endpoints:
        probe = "1" if endpoint.requires_probe else "0"
        lines.append(f"    {{\"{endpoint.name}\", {ENDPOINT_KINDS[endpoint.kind]}, {endpoint.input_schema}u, "
                     f"{endpoint.output_schema}u, {probe}}},")
    lines.append("};")
    lines.append("")
    compatible_pointer = "0"
    if api.compatible_hashes:
        lines.append(f"static const uint64_t {c_symbol}_CompatibleApiHashes[] = {{")
        for compatible_hash in api.compatible_hashes:
            lines.append(f"    UINT64_C(0x{compatible_hash:016X}),")
        lines.append("};")
        lines.append("")
        compatible_pointer = f"{c_symbol}_CompatibleApiHashes"
    lines.append(f"static const BML_InteropApiDescriptor {c_symbol}_Descriptor = {{")
    lines.append(f"    sizeof(BML_InteropApiDescriptor), {c_prefix}_API_ID, "
                 f"{c_prefix}_MAJOR, {c_prefix}_MINOR, {c_prefix}_HASH,")
    lines.append(f"    {c_symbol}_Schemas, sizeof({c_symbol}_Schemas) / sizeof({c_symbol}_Schemas[0]),")
    lines.append(f"    {c_symbol}_Endpoints, sizeof({c_symbol}_Endpoints) / sizeof({c_symbol}_Endpoints[0]),")
    lines.append(f"    {compatible_pointer}, {len(api.compatible_hashes)}u,")
    lines.append("};")
    lines.append(f"#define {c_prefix}_DESCRIPTOR {c_symbol}_Descriptor")
    lines.extend([
        "#endif // __cplusplus",
        "",
    ])
    return "\n".join(lines)


def append_as_decode(lines: list[str], record: Record) -> None:
    descriptor = camel(record.name)
    value = value_name(record)
    lines.append(f"class {value} {{")
    for field in record.fields:
        field_name = camel(field.name)
        if field.optional:
            lines.append(f"  bool Has{field_name} = false;")
        lines.append(f"  {AS_VALUE_TYPES[field.type]} {field_name};")
    lines.append("}")
    lines.append("")
    lines.append(f"int Decode{descriptor}(::BML::Interop::Record@ record, {value} &out value) {{")
    lines.append("  if (record is null)")
    lines.append("    return ::BML::ERROR_INTEROP_RECORD_INVALID;")
    lines.append(f"  if (record.Schema != {record.id})")
    lines.append("    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;")
    lines.append(f"  {value} decoded;")
    lines.append("  int status = ::BML::ERROR_OK;")
    for field in record.fields:
        field_name = camel(field.name)
        getter = FIELD_GETTERS[field.type]
        if field.optional:
            lines.extend([
                "  if (status == ::BML::ERROR_OK) {",
                f"    const int fieldStatus = record.{getter}({field.id}, decoded.{field_name});",
                "    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {",
                f"      decoded.Has{field_name} = false;",
                "    } else if (fieldStatus != ::BML::ERROR_OK) {",
                "      status = fieldStatus;",
                "    } else {",
                f"      decoded.Has{field_name} = true;",
                "    }",
                "  }",
            ])
        else:
            lines.append(f"  if (status == ::BML::ERROR_OK) status = record.{getter}({field.id}, decoded.{field_name});")
    lines.extend([
        "  if (status == ::BML::ERROR_OK)",
        "    value = decoded;",
        "  return status;",
        "}",
        "",
    ])


def append_as_encode(lines: list[str], api: ApiDefinition, record: Record) -> None:
    descriptor = camel(record.name)
    value = value_name(record)
    lines.append(f"int Encode{descriptor}(const {value} &in value, ::BML::Interop::Input@ &out input) {{")
    lines.append(f"  int status = ::BML::Interop::CreateInput(\"{api.api_id}\", {record.id}, input);")
    for field in record.fields:
        field_name = camel(field.name)
        setter = FIELD_SETTERS[field.type]
        condition = f"status == ::BML::ERROR_OK && value.Has{field_name}" if field.optional else "status == ::BML::ERROR_OK"
        lines.append(f"  if ({condition}) status = input.{setter}({field.id}, value.{field_name});")
    lines.extend([
        "  return status;",
        "}",
        "",
    ])


def append_as_consumers(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    lines.extend([
        "// Typed consumer facade.  It is generated from this API and uses",
        "// BML::Interop only as its private transport layer.",
        f"const uint Major = {api.major};",
        f"const uint64 Hash = uint64(0x{api.hash64:016X});",
        "",
        "int Require() {",
        f"  return ::BML::Interop::RequireApi(\"{api.api_id}\", Major, Hash);",
        "}",
        "",
    ])
    for record in api.schemas:
        append_as_decode(lines, record)

    input_ids = sorted({endpoint.input_schema for endpoint in api.endpoints if endpoint.input_schema})
    for schema_id in input_ids:
        append_as_encode(lines, api, schemas[schema_id])

    for endpoint in api.endpoints:
        endpoint_name = camel(endpoint.name)
        output = schemas[endpoint.output_schema]
        output_value = value_name(output)
        if endpoint.kind == "resource":
            lines.extend([
                f"int Read{endpoint_name}({output_value} &out value) {{",
                "  ::BML::Interop::Record@ record;",
                "  int status = Require();",
                f"  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource(\"{api.api_id}\", \"{endpoint.name}\", record);",
                f"  if (status == ::BML::ERROR_OK) status = Decode{camel(output.name)}(record, value);",
                "  return status;",
                "}",
                "",
            ])
        elif endpoint.kind == "component":
            lines.extend([
                f"int Read{endpoint_name}(const ::BML::Interop::ObjectRef &in object, {output_value} &out value) {{",
                "  ::BML::Interop::Record@ record;",
                "  int status = Require();",
                f"  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadComponent(\"{api.api_id}\", \"{endpoint.name}\", object, record);",
                f"  if (status == ::BML::ERROR_OK) status = Decode{camel(output.name)}(record, value);",
                "  return status;",
                "}",
                "",
            ])
        elif endpoint.kind == "collection":
            lines.extend([
                f"int Open{endpoint_name}(::BML::Interop::Cursor@ &out cursor) {{",
                "  const int status = Require();",
                f"  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenCollection(\"{api.api_id}\", \"{endpoint.name}\", cursor) : status;",
                "}",
                "",
                f"int Next{endpoint_name}(::BML::Interop::Cursor@ cursor, {output_value} &out value, bool &out hasValue, bool &out complete) {{",
                "  hasValue = false;",
                "  complete = false;",
                "  if (cursor is null)",
                "    return ::BML::ERROR_INTEROP_CURSOR_STALE;",
                "  ::BML::Interop::Record@ record;",
                "  int status = cursor.Next(record, hasValue, complete);",
                f"  if (status == ::BML::ERROR_OK && hasValue) status = Decode{camel(output.name)}(record, value);",
                "  return status;",
                "}",
                "",
            ])
        elif endpoint.kind == "stream":
            lines.extend([
                f"int Open{endpoint_name}(::BML::Interop::Stream@ &out stream, int capacity = 256) {{",
                "  const int status = Require();",
                f"  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenStream(\"{api.api_id}\", \"{endpoint.name}\", stream, capacity) : status;",
                "}",
                "",
                f"int Poll{endpoint_name}(::BML::Interop::Stream@ stream, {output_value} &out value, bool &out hasValue) {{",
                "  hasValue = false;",
                "  if (stream is null)",
                "    return ::BML::ERROR_INTEROP_HANDLE_STALE;",
                "  ::BML::Interop::Record@ record;",
                "  int status = stream.Poll(record);",
                "  hasValue = record !is null;",
                f"  if (status == ::BML::ERROR_OK && hasValue) status = Decode{camel(output.name)}(record, value);",
                "  return status;",
                "}",
                "",
            ])
        else:
            input_record = schemas[endpoint.input_schema]
            input_value = value_name(input_record)
            invoke = "InvokeQuery" if endpoint.kind == "query" else "InvokeCommand"
            verb = "Query" if endpoint.kind == "query" else "Command"
            lines.extend([
                f"int {verb}{endpoint_name}(const {input_value} &in inputValue, {output_value} &out value) {{",
                "  ::BML::Interop::Input@ input;",
                "  int status = Require();",
                f"  if (status == ::BML::ERROR_OK) status = Encode{camel(input_record.name)}(inputValue, input);",
                "  ::BML::Interop::Record@ record;",
                f"  if (status == ::BML::ERROR_OK) status = ::BML::Interop::{invoke}(\"{api.api_id}\", \"{endpoint.name}\", input, record);",
                f"  if (status == ::BML::ERROR_OK) status = Decode{camel(output.name)}(record, value);",
                "  return status;",
                "}",
                "",
            ])


def emit_stub(api: ApiDefinition) -> str:
    namespace = camel(api.api_id.replace(".", "_"))
    lines = [
        "// Generated Interop AngelScript binding. Do not edit by hand.",
        "// Include this API binding from a mod. CreateApi() is for providers;",
        "// the typed facade below is for consumers.",
        "",
        "namespace BMLInteropGenerated {",
        f"namespace {namespace} {{",
        "",
        "::BML::Interop::ApiBuilder@ CreateApi() {",
        "  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(",
        f"      \"{api.api_id}\", {api.major}, {api.minor}, uint64(0x{api.hash64:016X}));",
        "  if (api is null)",
        "    return null;",
    ]
    for record in api.schemas:
        lines.extend([
            f"  if (api.AddSchema({record.id}, \"{record.name}\") != ::BML::ERROR_OK)",
            "    return null;",
        ])
        for field in record.fields:
            optional = "true" if field.optional else "false"
            lines.extend([
                f"  if (api.AddField({record.id}, {field.id}, \"{field.name}\", "
                f"::BML::Interop::{AS_FIELD_TYPES[field.type]}, {optional}) != ::BML::ERROR_OK)",
                "    return null;",
            ])
    for endpoint in api.endpoints:
        probe = "true" if endpoint.requires_probe else "false"
        lines.extend([
            f"  if (api.AddEndpoint(\"{endpoint.name}\", ::BML::Interop::{AS_ENDPOINT_KINDS[endpoint.kind]}, "
            f"{endpoint.input_schema}, {endpoint.output_schema}, {probe}) != ::BML::ERROR_OK)",
            "    return null;",
        ])
    for compatible_hash in api.compatible_hashes:
        lines.extend([
            f"  if (api.AddCompatibleApiHash(uint64(0x{compatible_hash:016X})) != ::BML::ERROR_OK)",
            "    return null;",
        ])
    lines.extend([
        "  return api;",
        "}",
        "",
    ])
    append_as_consumers(lines, api)
    lines.extend([
        f"}} // namespace {namespace}",
        "} // namespace BMLInteropGenerated",
        "",
    ])
    return "\n".join(lines)


def emit_docs(api: ApiDefinition) -> str:
    lines = [f"# `{api.api_id}` API", "", f"Compatibility: `{api.major}.{api.minor}`", "",
             f"Descriptor hash: `0x{api.hash64:016X}`", "", "## Schemas", ""]
    for record in api.schemas:
        lines.extend([f"### `{record.name}` ({record.id})", "", "| Field ID | Name | Type | Optional |", "| ---: | --- | --- | :---: |"])
        for field in record.fields:
            lines.append(f"| {field.id} | `{field.name}` | `{field.type}` | {'yes' if field.optional else 'no'} |")
        lines.append("")
    if api.compatible_hashes:
        lines.extend(["## Accepted older descriptor hashes", ""])
        lines.extend(f"- `0x{compatible_hash:016X}`" for compatible_hash in api.compatible_hashes)
        lines.append("")
    lines.extend(["## Endpoints", "", "| Name | Kind | Input schema | Output schema | Probe |", "| --- | --- | ---: | ---: | :---: |"])
    for endpoint in api.endpoints:
        input_schema = str(endpoint.input_schema) if endpoint.input_schema else "-"
        lines.append(f"| `{endpoint.name}` | `{endpoint.kind}` | {input_schema} | {endpoint.output_schema} | {'yes' if endpoint.requires_probe else 'no'} |")
    lines.append("")
    return "\n".join(lines)


def emit_test_metadata(api: ApiDefinition) -> str:
    """Stable machine-readable expectations for consumer/provider smoke tests."""
    metadata = {
        "api": api.api_id,
        "version": {"major": api.major, "minor": api.minor},
        "hash": f"0x{api.hash64:016X}",
        "compatible_hashes": [f"0x{compatible_hash:016X}" for compatible_hash in api.compatible_hashes],
        "schemas": [
            {
                "id": record.id,
                "name": record.name,
                "fields": [
                    {"id": field.id, "name": field.name, "type": field.type, "optional": field.optional}
                    for field in record.fields
                ],
            }
            for record in api.schemas
        ],
        "endpoints": [
            {
                "name": endpoint.name,
                "kind": endpoint.kind,
                "input": endpoint.input_schema,
                "output": endpoint.output_schema,
                "requires_probe": endpoint.requires_probe,
            }
            for endpoint in api.endpoints
        ],
    }
    return json.dumps(metadata, ensure_ascii=False, sort_keys=True, indent=2) + "\n"


def validate_compatible(previous: ApiDefinition, current: ApiDefinition) -> None:
    """Enforce the only legal same-major evolution: optional append-only."""
    if previous.api_id != current.api_id:
        raise ApiDefinitionError(
            f"compatibility input API ID {previous.api_id} does not match {current.api_id}"
        )
    if current.major < previous.major:
        raise ApiDefinitionError(
            f"{current.api_id} major version cannot decrease ({previous.major} -> {current.major})"
        )
    if current.major != previous.major:
        return
    if current.minor < previous.minor:
        raise ApiDefinitionError(
            f"{current.api_id} minor version cannot decrease within major {current.major}"
        )

    previous_schemas = {record.id: record for record in previous.schemas}
    current_schemas = {record.id: record for record in current.schemas}
    for record_id, old_record in previous_schemas.items():
        new_record = current_schemas.get(record_id)
        if not new_record or new_record.name != old_record.name:
            raise ApiDefinitionError(f"record {old_record.name} ({record_id}) was removed or renamed")
        old_fields = {field.id: field for field in old_record.fields}
        new_fields = {field.id: field for field in new_record.fields}
        for field_id, old_field in old_fields.items():
            new_field = new_fields.get(field_id)
            if new_field != old_field:
                raise ApiDefinitionError(
                    f"field {old_record.name}.{old_field.name} ({field_id}) changed incompatibly"
                )
        for field_id, new_field in new_fields.items():
            if field_id not in old_fields and not new_field.optional:
                raise ApiDefinitionError(
                    f"new field {new_record.name}.{new_field.name} must be optional in a compatible minor"
                )

    previous_endpoints = {endpoint.name: endpoint for endpoint in previous.endpoints}
    current_endpoints = {endpoint.name: endpoint for endpoint in current.endpoints}
    for name, old_endpoint in previous_endpoints.items():
        if current_endpoints.get(name) != old_endpoint:
            raise ApiDefinitionError(f"endpoint {name} was removed or changed incompatibly")

    if previous.canonical != current.canonical and current.minor == previous.minor:
        raise ApiDefinitionError(
            f"{current.api_id} changed within major {current.major} without increasing its minor version"
        )
    if previous.canonical != current.canonical and previous.hash64 not in current.compatible_hashes:
        raise ApiDefinitionError(
            f"{current.api_id} compatible minor must list predecessor hash "
            f"0x{previous.hash64:016X} in compatible_hashes"
        )


def write_if_changed(path: Path, text: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == text:
        return False
    if check:
        raise ApiDefinitionError(f"generated output is stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, required=True, help=".bmlapi JSON input")
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--header-out-dir", type=Path,
                        help="directory for generated C/C++ descriptor headers (defaults to --out-dir)")
    parser.add_argument("--previous", action="append", type=Path,
                        help="previous .bmlapi to validate as an append-only compatible predecessor")
    parser.add_argument("--check", action="store_true", help="fail instead of rewriting stale output")
    args = parser.parse_args(argv)
    try:
        api_definitions = [parse_api_definition(json.loads(path.read_text(encoding="utf-8"))) for path in args.input]
        previous_apis = [
            parse_api_definition(json.loads(path.read_text(encoding="utf-8")))
            for path in (args.previous or [])
        ]
        header_out_dir = args.header_out_dir or args.out_dir
        seen_api_ids: set[str] = set()
        by_api_id = {api.api_id: api for api in api_definitions}
        for previous in previous_apis:
            current = by_api_id.get(previous.api_id)
            if not current:
                raise ApiDefinitionError(f"no current input supplied for compatibility API ID {previous.api_id}")
            validate_compatible(previous, current)
        for api in api_definitions:
            if api.api_id in seen_api_ids:
                raise ApiDefinitionError(f"duplicate API ID input: {api.api_id}")
            seen_api_ids.add(api.api_id)
            stem = api.api_id.replace(".", "_")
            write_if_changed(header_out_dir / f"{stem}_api.h", emit_header(api), args.check)
            write_if_changed(args.out_dir / f"{stem}.as", emit_stub(api), args.check)
            write_if_changed(args.out_dir / f"{stem}.md", emit_docs(api), args.check)
            write_if_changed(args.out_dir / f"{stem}.json", api.canonical + "\n", args.check)
            write_if_changed(args.out_dir / f"{stem}.test.json", emit_test_metadata(api), args.check)
    except (OSError, json.JSONDecodeError, ApiDefinitionError) as error:
        print(f"interop_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
