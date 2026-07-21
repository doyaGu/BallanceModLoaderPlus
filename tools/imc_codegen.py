#!/usr/bin/env python3
"""Generate deterministic typed IMC bindings from .bmlapi JSON.

The loader never reads .bmlapi files at runtime. This tool validates the
authoring representation, enforces append-only compatible evolution, and emits
one typed C++ IMC binding per API.
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


SUPPORTED_FIELD_TYPES = frozenset({
    "bool", "int", "float", "int64", "uint64", "double", "string", "bytes",
    "object", "vec2", "vec3", "mat4",
    "array<bool>", "array<int>", "array<float>", "array<int64>",
    "array<uint64>", "array<double>", "array<string>", "array<object>",
    "array<vec2>", "array<vec3>", "array<mat4>",
})

ENUM_UNDERLYING_RANGES = {
    "int": (-(1 << 31), (1 << 31) - 1),
    "int64": (-(1 << 63), (1 << 63) - 1),
    "uint64": (0, (1 << 64) - 1),
}

ENDPOINT_KINDS = frozenset({
    "resource", "component", "collection", "stream", "query", "command",
    "rpc", "topic",
})
TOPIC_ENDPOINT_KINDS = frozenset({"stream", "topic"})
KEY_RE = re.compile(r"^[A-Za-z0-9_.-]+$")
KEY_ALNUM_RE = re.compile(r"[A-Za-z0-9]")
API_ID_RE = re.compile(r"^[a-z0-9]+(?:\.[a-z0-9]+)*$")
ENUM_FIELD_RE = re.compile(r"^enum<([A-Za-z0-9_.-]+)>$")


class ApiDefinitionError(ValueError):
    pass


def unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for name, value in pairs:
        if name in result:
            raise ApiDefinitionError(f"duplicate JSON property {name!r}")
        result[name] = value
    return result


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
    if not KEY_ALNUM_RE.search(value):
        raise ApiDefinitionError(f"{what} must contain at least one ASCII letter or digit")
    return value


def api_key(value: Any, what: str) -> str:
    if not isinstance(value, str) or not API_ID_RE.fullmatch(value):
        raise ApiDefinitionError(
            f"{what} must use non-empty dot-separated segments containing only "
            "lowercase ASCII letters and digits"
        )
    return value


def reject_unknown_properties(value: dict[str, Any], allowed: tuple[str, ...],
                              what: str) -> None:
    unknown = sorted(set(value).difference(allowed))
    if not unknown:
        return
    label = "property" if len(unknown) == 1 else "properties"
    names = ", ".join(repr(name) for name in unknown)
    expected = ", ".join(allowed)
    raise ApiDefinitionError(
        f"{what} has unknown {label}: {names}; allowed properties: {expected}"
    )


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


def ranged_integer(value: Any, what: str, minimum: int, maximum: int) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not minimum <= value <= maximum:
        raise ApiDefinitionError(f"{what} must be an integer in [{minimum}, {maximum}]")
    return value


def enum_field_name(field_type: Any) -> str | None:
    if not isinstance(field_type, str):
        return None
    match = ENUM_FIELD_RE.fullmatch(field_type)
    return match.group(1) if match else None


def camel(value: str) -> str:
    parts = [part for part in re.split(r"[_\W]+", value) if part]
    result = "".join(part[:1].upper() + part[1:] for part in parts) or "ApiDefinition"
    if result[0].isdigit():
        result = "_" + result
    return result


def validate_generated_identifiers(enums: list[EnumDefinition], schemas: list[Record],
                                   endpoints: list[Endpoint]) -> None:
    imc_top_level: dict[str, str] = {
        name: "generated IMC helper"
        for name in ("ApiId", "Major", "Minor", "Hash", "WireHash", "Schemas", "Endpoints",
                     "IsCompatibleHash", "SchemaMetadata", "EndpointMetadata", "Client",
                     "Provider")
    }

    def add(symbols: dict[str, str], symbol: str, source: str) -> None:
        previous = symbols.get(symbol)
        if previous is not None:
            raise ApiDefinitionError(
                f"generated identifier collision for {symbol}: {previous} and {source}"
            )
        symbols[symbol] = source

    for enum in enums:
        enum_name = camel(enum.name)
        source = f"enum {enum.name}"
        add(imc_top_level, enum_name, source)
        add(imc_top_level, f"IsKnown{enum_name}", source)
        members: dict[str, str] = {}
        for enum_value in enum.values:
            add(members, camel(enum_value.name), f"enum value {enum.name}.{enum_value.name}")

    for record in schemas:
        record_name = camel(record.name)
        source = f"schema {record.name}"
        imc_names = [
            f"{record_name}Field",
            f"{record_name}Schema",
            f"{record_name}Payload",
            f"{record_name}FieldCount",
            f"Encoded{record_name}Size",
            f"Encode{record_name}",
            f"Decode{record_name}",
            value_name(record),
        ]
        for generated_name in imc_names:
            add(imc_top_level, generated_name, source)

        members: dict[str, str] = {}
        for field in record.fields:
            member = camel(field.name)
            add(members, member, f"field {record.name}.{field.name}")
            if field.optional:
                add(members, f"Has{member}", f"optional flag for {record.name}.{field.name}")

    imc_client_symbols: dict[str, str] = {}
    for endpoint in endpoints:
        endpoint_name = camel(endpoint.name)
        source = f"endpoint {endpoint.name}"
        add(imc_top_level, f"{endpoint_name}Route", source)
        if endpoint.kind == "component":
            add(imc_top_level, f"{endpoint_name}RequestPayload", source)
        elif endpoint.kind == "collection":
            add(imc_top_level, f"{endpoint_name}CollectionPayload", source)
        if endpoint.kind in TOPIC_ENDPOINT_KINDS:
            add(imc_top_level, f"{endpoint_name}Subscription", source)
        prefix = {
            "resource": "Read",
            "component": "Read",
            "collection": "Open",
            "stream": "Open",
            "query": "Query",
            "command": "Command",
            "rpc": "Call",
            "topic": "Open",
        }[endpoint.kind]
        generated_method = f"{prefix}{endpoint_name}"
        add(imc_client_symbols, generated_method, source)
        if endpoint.kind not in TOPIC_ENDPOINT_KINDS:
            add(imc_client_symbols, f"Is{endpoint_name}Available", source)


@dataclass(frozen=True)
class EnumValue:
    name: str
    value: int


@dataclass(frozen=True)
class EnumDefinition:
    name: str
    underlying: str
    values: tuple[EnumValue, ...]


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
    enums: tuple[EnumDefinition, ...]
    schemas: tuple[Record, ...]
    endpoints: tuple[Endpoint, ...]
    compatible_hashes: tuple[int, ...]
    canonical: str
    hash64: int

    @property
    def wire_hash(self) -> int:
        """Stable IMC hash written by every compatible minor in this major."""
        return self.compatible_hashes[0] if self.compatible_hashes else self.hash64


def parse_api_definition(raw: Any) -> ApiDefinition:
    if not isinstance(raw, dict):
        raise ApiDefinitionError("top-level value must be an object")
    reject_unknown_properties(
        raw,
        ("api", "version", "enums", "schemas", "endpoints", "rpcs", "topics",
         "compatible_hashes"),
        "top-level object",
    )
    api_id = api_key(raw.get("api"), "api")
    version = raw.get("version")
    if not isinstance(version, dict):
        raise ApiDefinitionError("version must be an object")
    reject_unknown_properties(version, ("major", "minor"), "version")
    major = uint32(version.get("major"), "version.major")
    minor = uint32(version.get("minor", 0), "version.minor", allow_zero=True)

    raw_enums = raw.get("enums", [])
    if not isinstance(raw_enums, list):
        raise ApiDefinitionError("enums must be an array when present")
    enums: list[EnumDefinition] = []
    enum_names: set[str] = set()
    for index, raw_enum in enumerate(raw_enums):
        if not isinstance(raw_enum, dict):
            raise ApiDefinitionError(f"enums[{index}] must be an object")
        reject_unknown_properties(
            raw_enum, ("name", "underlying", "values"), f"enums[{index}]"
        )
        enum_name = key(raw_enum.get("name"), f"enums[{index}].name")
        if enum_name in enum_names:
            raise ApiDefinitionError(f"duplicate enum name: {enum_name}")
        underlying = raw_enum.get("underlying", "int")
        if underlying not in ENUM_UNDERLYING_RANGES:
            supported = ", ".join(ENUM_UNDERLYING_RANGES)
            raise ApiDefinitionError(
                f"enums[{index}].underlying must be one of {supported}"
            )
        raw_values = raw_enum.get("values")
        if not isinstance(raw_values, list) or not raw_values:
            raise ApiDefinitionError(f"enums[{index}].values must be a non-empty array")
        minimum, maximum = ENUM_UNDERLYING_RANGES[underlying]
        values: list[EnumValue] = []
        value_names: set[str] = set()
        numeric_values: set[int] = set()
        for value_index, raw_value in enumerate(raw_values):
            if not isinstance(raw_value, dict):
                raise ApiDefinitionError(f"enums[{index}].values[{value_index}] must be an object")
            reject_unknown_properties(
                raw_value, ("name", "value"),
                f"enums[{index}].values[{value_index}]",
            )
            value_name = key(raw_value.get("name"), f"enums[{index}].values[{value_index}].name")
            value = ranged_integer(
                raw_value.get("value"), f"enums[{index}].values[{value_index}].value",
                minimum, maximum,
            )
            if value_name in value_names:
                raise ApiDefinitionError(f"duplicate enum value name in {enum_name}: {value_name}")
            if value in numeric_values:
                raise ApiDefinitionError(f"duplicate enum numeric value in {enum_name}: {value}")
            value_names.add(value_name)
            numeric_values.add(value)
            values.append(EnumValue(value_name, value))
        enum_names.add(enum_name)
        enums.append(EnumDefinition(
            enum_name, underlying, tuple(sorted(values, key=lambda item: (item.value, item.name)))
        ))

    raw_schemas = raw.get("schemas")
    if not isinstance(raw_schemas, list) or not raw_schemas:
        raise ApiDefinitionError("schemas must be a non-empty array")
    schemas: list[Record] = []
    record_ids: set[int] = set()
    record_names: set[str] = set()
    for index, raw_record in enumerate(raw_schemas):
        if not isinstance(raw_record, dict):
            raise ApiDefinitionError(f"schemas[{index}] must be an object")
        reject_unknown_properties(raw_record, ("id", "name", "fields"), f"schemas[{index}]")
        record_id = uint32(raw_record.get("id"), f"schemas[{index}].id")
        record_name = key(raw_record.get("name"), f"schemas[{index}].name")
        if record_id in record_ids or record_name in record_names:
            raise ApiDefinitionError(f"duplicate record id or name: {record_name}")
        raw_fields = raw_record.get("fields", [])
        if not isinstance(raw_fields, list):
            raise ApiDefinitionError(f"schemas[{index}].fields must be an array")
        if len(raw_fields) > 64:
            raise ApiDefinitionError(
                f"schema {record_name} has {len(raw_fields)} fields; IMC schemas support at most 64 fields"
            )
        fields: list[Field] = []
        field_ids: set[int] = set()
        field_names: set[str] = set()
        for field_index, raw_field in enumerate(raw_fields):
            if not isinstance(raw_field, dict):
                raise ApiDefinitionError(f"schemas[{index}].fields[{field_index}] must be an object")
            reject_unknown_properties(
                raw_field, ("id", "name", "type", "optional"),
                f"schemas[{index}].fields[{field_index}]",
            )
            field_id = uint32(raw_field.get("id"), f"schemas[{index}].fields[{field_index}].id")
            field_name = key(raw_field.get("name"), f"schemas[{index}].fields[{field_index}].name")
            field_type = raw_field.get("type")
            enum_name = enum_field_name(field_type)
            if field_type not in SUPPORTED_FIELD_TYPES and enum_name is None:
                raise ApiDefinitionError(f"unsupported field type {field_type!r} in {record_name}.{field_name}")
            if enum_name is not None and enum_name not in enum_names:
                raise ApiDefinitionError(
                    f"field {record_name}.{field_name} references unknown enum {enum_name!r}"
                )
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
    endpoints: list[Endpoint] = []
    endpoint_names: set[str] = set()

    def add_endpoint(endpoint_name: str, kind: str, input_schema: int,
                     output_schema: int, requires_probe: bool) -> None:
        if input_schema and input_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown input schema {input_schema}")
        if output_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown output schema {output_schema}")
        if endpoint_name in endpoint_names:
            raise ApiDefinitionError(f"duplicate endpoint name {endpoint_name}")
        endpoint_names.add(endpoint_name)
        endpoints.append(Endpoint(endpoint_name, kind, input_schema, output_schema, requires_probe))

    has_typed_endpoints = "endpoints" in raw
    has_imc_endpoints = "rpcs" in raw or "topics" in raw
    if has_typed_endpoints and has_imc_endpoints:
        raise ApiDefinitionError("use either typed endpoints or rpcs/topics, not both")
    if has_typed_endpoints:
        raw_endpoints = raw.get("endpoints")
        if not isinstance(raw_endpoints, list) or not raw_endpoints:
            raise ApiDefinitionError("endpoints must be a non-empty array")
        for index, raw_source in enumerate(raw_endpoints):
            if not isinstance(raw_source, dict):
                raise ApiDefinitionError(f"endpoints[{index}] must be an object")
            reject_unknown_properties(
                raw_source, ("name", "kind", "input", "output", "requires_probe"),
                f"endpoints[{index}]",
            )
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
            add_endpoint(endpoint_name, kind, input_schema, output_schema, requires_probe)
    elif has_imc_endpoints:
        raw_rpcs = raw.get("rpcs", [])
        raw_topics = raw.get("topics", [])
        if not isinstance(raw_rpcs, list):
            raise ApiDefinitionError("rpcs must be an array when present")
        if not isinstance(raw_topics, list):
            raise ApiDefinitionError("topics must be an array when present")
        if not raw_rpcs and not raw_topics:
            raise ApiDefinitionError("rpcs/topics must contain at least one entry")
        for index, raw_rpc in enumerate(raw_rpcs):
            if not isinstance(raw_rpc, dict):
                raise ApiDefinitionError(f"rpcs[{index}] must be an object")
            reject_unknown_properties(
                raw_rpc, ("name", "request", "response"), f"rpcs[{index}]"
            )
            endpoint_name = key(raw_rpc.get("name"), f"rpcs[{index}].name")
            input_schema = uint32(raw_rpc.get("request", 0), f"rpcs[{index}].request", allow_zero=True)
            output_schema = uint32(raw_rpc.get("response"), f"rpcs[{index}].response")
            add_endpoint(endpoint_name, "rpc", input_schema, output_schema, False)
        for index, raw_topic in enumerate(raw_topics):
            if not isinstance(raw_topic, dict):
                raise ApiDefinitionError(f"topics[{index}] must be an object")
            reject_unknown_properties(
                raw_topic, ("name", "message"), f"topics[{index}]"
            )
            endpoint_name = key(raw_topic.get("name"), f"topics[{index}].name")
            message_schema = uint32(raw_topic.get("message"), f"topics[{index}].message")
            add_endpoint(endpoint_name, "topic", 0, message_schema, False)
    else:
        raise ApiDefinitionError("define typed endpoints or rpcs/topics")

    validate_generated_identifiers(enums, schemas, endpoints)

    raw_compatible_hashes = raw.get("compatible_hashes", [])
    if not isinstance(raw_compatible_hashes, list):
        raise ApiDefinitionError("compatible_hashes must be an array when present")
    compatible_hashes = tuple(
        parse_hash64(value, f"compatible_hashes[{index}]")
        for index, value in enumerate(raw_compatible_hashes)
    )
    if len(set(compatible_hashes)) != len(compatible_hashes):
        raise ApiDefinitionError("compatible_hashes must not contain duplicates")

    # Compatibility permits older descriptor hashes to be accepted at runtime.
    # Order is preserved because the first entry is also the stable IMC wire
    # hash for a compatible major-version family.  The list intentionally does
    # not participate in the structural descriptor hash: it changes rollout
    # policy, not API semantics.
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
    # Keep the canonical form (and therefore every existing descriptor hash)
    # byte-for-byte stable for contracts that predate named enums.
    if enums:
        canonical_object["enums"] = [
            {
                "name": enum.name,
                "underlying": enum.underlying,
                "values": [
                    {"name": value.name, "value": value.value}
                    for value in enum.values
                ],
            }
            for enum in sorted(enums, key=lambda item: item.name)
        ]
    canonical = json.dumps(canonical_object, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    descriptor_hash = int.from_bytes(hashlib.sha256(canonical.encode("utf-8")).digest()[:8], "big")
    if descriptor_hash in compatible_hashes:
        raise ApiDefinitionError("compatible_hashes must not repeat the current descriptor hash")
    return ApiDefinition(api_id, major, minor, tuple(sorted(enums, key=lambda item: item.name)),
                    tuple(sorted(schemas, key=lambda item: item.id)),
                    tuple(sorted(endpoints, key=lambda item: item.name)), compatible_hashes, canonical, descriptor_hash)


def value_name(record: Record) -> str:
    """A snapshot value must not collide with its descriptor constant."""
    return f"{camel(record.name)}Value"


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

    previous_enums = {enum.name: enum for enum in previous.enums}
    current_enums = {enum.name: enum for enum in current.enums}
    for enum_name, old_enum in previous_enums.items():
        new_enum = current_enums.get(enum_name)
        if new_enum is None:
            raise ApiDefinitionError(f"enum {enum_name} was removed or renamed")
        if new_enum.underlying != old_enum.underlying:
            raise ApiDefinitionError(
                f"enum {enum_name} underlying type changed incompatibly "
                f"({old_enum.underlying} -> {new_enum.underlying})"
            )
        new_values = {value.name: value.value for value in new_enum.values}
        for old_value in old_enum.values:
            if new_values.get(old_value.name) != old_value.value:
                raise ApiDefinitionError(
                    f"enum value {enum_name}.{old_value.name} was removed, renamed, or renumbered"
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
    if current.wire_hash != previous.wire_hash:
        raise ApiDefinitionError(
            f"{current.api_id} compatible minor must keep IMC wire hash "
            f"0x{previous.wire_hash:016X}; put it first in compatible_hashes and "
            f"retain predecessor hash 0x{previous.hash64:016X} in the list"
        )


IMC_CPP_TYPES = {
    "bool": "bool",
    "int": "int",
    "float": "float",
    "int64": "std::int64_t",
    "uint64": "std::uint64_t",
    "double": "double",
    "string": "std::string",
    "bytes": "std::vector<std::uint8_t>",
    "object": "BML_ObjectRef",
    "vec2": "BML_Vec2",
    "vec3": "BML_Vec3",
    "mat4": "BML_Mat4",
    "array<bool>": "std::vector<bool>",
    "array<int>": "std::vector<int>",
    "array<float>": "std::vector<float>",
    "array<int64>": "std::vector<std::int64_t>",
    "array<uint64>": "std::vector<std::uint64_t>",
    "array<double>": "std::vector<double>",
    "array<string>": "std::vector<std::string>",
    "array<object>": "std::vector<BML_ObjectRef>",
    "array<vec2>": "std::vector<BML_Vec2>",
    "array<vec3>": "std::vector<BML_Vec3>",
    "array<mat4>": "std::vector<BML_Mat4>",
}

IMC_WRITERS = {
    "bool": "WriteBool", "int": "WriteInt", "float": "WriteFloat",
    "int64": "WriteInt64", "uint64": "WriteUInt64", "double": "WriteDouble",
    "string": "WriteString", "bytes": "WriteBytes", "object": "WriteObject", "vec2": "WriteVec2",
    "vec3": "WriteVec3", "mat4": "WriteMat4", "array<bool>": "WriteBoolArray",
    "array<int>": "WriteIntArray", "array<float>": "WriteFloatArray",
    "array<int64>": "WriteInt64Array", "array<uint64>": "WriteUInt64Array",
    "array<double>": "WriteDoubleArray",
    "array<string>": "WriteStringArray", "array<object>": "WriteObjectArray",
    "array<vec2>": "WriteVec2Array", "array<vec3>": "WriteVec3Array",
    "array<mat4>": "WriteMat4Array",
}
IMC_READERS = {
    "bool": "ReadBool", "int": "ReadInt", "float": "ReadFloat",
    "int64": "ReadInt64", "uint64": "ReadUInt64", "double": "ReadDouble",
    "string": "ReadString", "bytes": "ReadBytes", "object": "ReadObject", "vec2": "ReadVec2",
    "vec3": "ReadVec3", "mat4": "ReadMat4", "array<bool>": "ReadBoolArray",
    "array<int>": "ReadIntArray", "array<float>": "ReadFloatArray",
    "array<int64>": "ReadInt64Array", "array<uint64>": "ReadUInt64Array",
    "array<double>": "ReadDoubleArray",
    "array<string>": "ReadStringArray", "array<object>": "ReadObjectArray",
    "array<vec2>": "ReadVec2Array", "array<vec3>": "ReadVec3Array",
    "array<mat4>": "ReadMat4Array",
}
IMC_FIXED_PAYLOAD_SIZES = {
    "bool": 1, "int": 4, "float": 4, "int64": 8, "uint64": 8,
    "double": 8, "object": 12,
    "vec2": 8, "vec3": 12, "mat4": 64,
}
IMC_ARRAY_ELEMENT_SIZES = {
    "array<bool>": 1, "array<int>": 4, "array<float>": 4,
    "array<int64>": 8, "array<uint64>": 8, "array<double>": 8,
    "array<object>": 12, "array<vec2>": 8, "array<vec3>": 12,
    "array<mat4>": 64,
}

ENUM_CPP_UNDERLYING_TYPES = {
    "int": "std::int32_t",
    "int64": "std::int64_t",
    "uint64": "std::uint64_t",
}
ENUM_WIRE_CPP_TYPES = {
    "int": "int",
    "int64": "std::int64_t",
    "uint64": "std::uint64_t",
}


def enum_definition(api: ApiDefinition, field_type: str) -> EnumDefinition | None:
    enum_name = enum_field_name(field_type)
    if enum_name is None:
        return None
    return next(enum for enum in api.enums if enum.name == enum_name)


def imc_field_wire_type(api: ApiDefinition, field_type: str) -> str:
    enum = enum_definition(api, field_type)
    return enum.underlying if enum is not None else field_type


def imc_field_cpp_type(api: ApiDefinition, field_type: str) -> str:
    enum = enum_definition(api, field_type)
    return camel(enum.name) if enum is not None else IMC_CPP_TYPES[field_type]


def enum_cpp_literal(enum: EnumDefinition, value: int) -> str:
    if enum.underlying == "int":
        return "(-2147483647 - 1)" if value == -(1 << 31) else str(value)
    if enum.underlying == "int64":
        if value == -(1 << 63):
            return "(-INT64_C(9223372036854775807) - 1)"
        if value < 0:
            return f"-INT64_C({-value})"
        return f"INT64_C({value})"
    return f"UINT64_C({value})"


def append_imc_codec(lines: list[str], api: ApiDefinition, record: Record) -> None:
    name = camel(record.name)
    value = value_name(record)
    required_mask = sum(1 << index for index, field in enumerate(record.fields) if not field.optional)
    required_count = sum(1 for field in record.fields if not field.optional)
    lines.append(f"inline std::uint32_t {name}FieldCount(const {value} &value) noexcept {{")
    lines.append(f"    std::uint32_t count = {required_count}u;")
    for field in record.fields:
        if field.optional:
            lines.append(f"    if (value.Has{camel(field.name)}) ++count;")
    lines.extend(["    return count;", "}", ""])
    lines.append(f"inline std::size_t Encoded{name}Size(const {value} &value) noexcept {{")
    lines.append("    std::size_t size = ::BML::Imc::Wire::HeaderSize;")
    for field in record.fields:
        member = camel(field.name)
        wire_type = imc_field_wire_type(api, field.type)
        if wire_type == "array<string>":
            add_size = f"::BML::Imc::Wire::AddStringArrayFieldSize(size, value.{member})"
        elif wire_type in IMC_ARRAY_ELEMENT_SIZES:
            add_size = (
                f"::BML::Imc::Wire::AddFixedArrayFieldSize(size, value.{member}.size(), "
                f"{IMC_ARRAY_ELEMENT_SIZES[wire_type]})"
            )
        else:
            payload = (f"value.{member}.size()" if wire_type in {"string", "bytes"}
                       else str(IMC_FIXED_PAYLOAD_SIZES[wire_type]))
            add_size = f"::BML::Imc::Wire::AddFieldSize(size, {payload})"
        condition = f"value.Has{member} && " if field.optional else ""
        lines.append(f"    if ({condition}!{add_size}) return 0;")
    lines.extend(["    return size;", "}", ""])
    lines.append(f"inline int Encode{name}(const {value} &value, void *data, std::size_t size) noexcept {{")
    lines.append(f"    if (size != Encoded{name}Size(value)) return BML_ERROR_INVALID_PARAMETER;")
    lines.append("    ::BML::Imc::Wire::Writer writer(data, size);")
    lines.append(f"    int status = writer.Begin({name}Schema, WireHash, {name}FieldCount(value));")
    for field in record.fields:
        member = camel(field.name)
        enum = enum_definition(api, field.type)
        wire_type = imc_field_wire_type(api, field.type)
        argument = (f"static_cast<{ENUM_WIRE_CPP_TYPES[wire_type]}>(value.{member})"
                    if enum is not None else f"value.{member}")
        call = f"writer.{IMC_WRITERS[wire_type]}({name}Field::{member}, {argument})"
        condition = f"status == BML_OK && value.Has{member}" if field.optional else "status == BML_OK"
        lines.append(f"    if ({condition}) status = {call};")
    lines.extend(["    return status == BML_OK ? writer.Finish() : status;", "}", ""])
    lines.append(f"inline int Decode{name}(const BML_ImcMessage &message, {value} &out) {{")
    lines.append("    if (message.Size < sizeof(BML_ImcMessage) || (message.DataSize && !message.Data)) return BML_ERROR_INVALID_PARAMETER;")
    lines.append("    ::BML::Imc::Wire::Reader reader(message.Data, message.DataSize);")
    lines.append(f"    int status = reader.Begin({name}Schema);")
    lines.append("    if (status != BML_OK) return status;")
    lines.append("    if (!IsCompatibleHash(reader.DescriptorHash())) return BML_ERROR_IMC_API_MISMATCH;")

    lines.extend([f"    {value} decoded{{}};", "    std::uint64_t seen = 0;", "    ::BML::Imc::Wire::FieldView field;"])
    lines.append("    while ((status = reader.Next(field)) == BML_OK) {")
    if record.fields:
        lines.append("        switch (field.Id) {")
        for index, field in enumerate(record.fields):
            member = camel(field.name)
            enum = enum_definition(api, field.type)
            wire_type = imc_field_wire_type(api, field.type)
            if enum is not None:
                lines.append(f"        case {name}Field::{member}: {{")
                lines.append(f"            if (seen & (UINT64_C(1) << {index})) return BML_ERROR_MALFORMED_MESSAGE;")
                lines.append(f"            {ENUM_WIRE_CPP_TYPES[wire_type]} raw{{}};")
                lines.append(f"            status = ::BML::Imc::Wire::Reader::{IMC_READERS[wire_type]}(field, raw);")
                lines.append("            if (status != BML_OK) return status;")
                lines.append(f"            decoded.{member} = static_cast<{camel(enum.name)}>(raw);")
                lines.append(f"            seen |= UINT64_C(1) << {index};")
                if field.optional:
                    lines.append(f"            decoded.Has{member} = true;")
                lines.extend(["            break;", "        }"])
            else:
                lines.append(f"        case {name}Field::{member}:")
                lines.append(f"            if (seen & (UINT64_C(1) << {index})) return BML_ERROR_MALFORMED_MESSAGE;")
                lines.append(f"            status = ::BML::Imc::Wire::Reader::{IMC_READERS[wire_type]}(field, decoded.{member});")
                lines.append("            if (status != BML_OK) return status;")
                lines.append(f"            seen |= UINT64_C(1) << {index};")
                if field.optional:
                    lines.append(f"            decoded.Has{member} = true;")
                lines.append("            break;")
        lines.extend(["        default:", "            break;", "        }"])
    else:
        lines.append("        (void)field;")
    lines.append("    }")
    lines.append("    if (status != BML_ERROR_NOT_FOUND) return status;")
    lines.append("    status = reader.Finish();")
    lines.append("    if (status != BML_OK) return status;")
    lines.append(f"    if ((seen & UINT64_C(0x{required_mask:X})) != UINT64_C(0x{required_mask:X})) return BML_ERROR_MALFORMED_MESSAGE;")
    lines.extend(["    out = std::move(decoded);", "    return BML_OK;", "}", ""])

def append_imc_subscriptions(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    for endpoint in api.endpoints:
        if endpoint.kind not in TOPIC_ENDPOINT_KINDS:
            continue
        name = camel(endpoint.name)
        output = schemas[endpoint.output_schema]
        output_name = camel(output.name)
        output_value = value_name(output)
        lines.extend([
            f"class {name}Subscription {{", "public:",
            f"    using Handler = void (*)(int status, {output_value} *value, const BML_ImcMessage *message, void *userdata);",
            f"    {name}Subscription() = default;", f"    ~{name}Subscription() {{ (void)Close(); }}",
            f"    {name}Subscription(const {name}Subscription &) = delete;",
            f"    {name}Subscription &operator=(const {name}Subscription &) = delete;",
            "    int Open(BML_ImcClient client, BML_ImcTopicId topic, BML_ImcPayloadTypeId payload,",
            "             Handler handler, void *userdata = nullptr, std::uint32_t capacity = 256u,",
            "             BML_ImcBackpressure backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST,",
            "             BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {",
            "        const int closeStatus = Close(); if (IsOpen()) return closeStatus;",
            "        if (!client || topic == BML_IMC_INVALID_ID || payload == BML_IMC_INVALID_ID || !handler || capacity == 0) return BML_ERROR_INVALID_PARAMETER;",
            "        m_Client = client; m_Payload = payload; m_Handler = handler; m_Userdata = userdata;",
            "        BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;",
            "        options.Execution = execution; options.Backpressure = backpressure; options.Capacity = capacity; options.ExpectedPayloadType = payload;",
            "        const int status = BML_Imc_Subscribe(client, topic, &options, &Dispatch, this, &m_Subscription);",
            "        if (status != BML_OK) Reset(); return status;", "    }",
            "    int Close() noexcept {",
            "        if (!m_Subscription) return BML_OK;",
            "        const int status = BML_Imc_Unsubscribe(m_Client, m_Subscription);",
            "        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) Reset();",
            "        return status;", "    }",
            "    bool IsOpen() const noexcept { return m_Subscription != nullptr; }",
            "    int DroppedCount(std::uint64_t &out) const noexcept {",
            "        return m_Subscription ? BML_Imc_GetSubscriptionDroppedCount(m_Client, m_Subscription, &out) : BML_ERROR_INVALID_HANDLE;", "    }",
            "private:",
            "    static void Dispatch(BML_ImcTopicId, const BML_ImcMessage *message, void *userdata) noexcept {",
            f"        auto *self = static_cast<{name}Subscription *>(userdata); if (!self || !self->m_Handler) return;",
            f"        {output_value} value{{}}; int status = BML_ERROR_MALFORMED_MESSAGE;",
            f"        if (message && message->PayloadType == self->m_Payload) status = Decode{output_name}(*message, value);",
            "        else if (message) status = BML_ERROR_TYPE_MISMATCH;",
            "        try { self->m_Handler(status, status == BML_OK ? &value : nullptr, message, self->m_Userdata); } catch (...) {}", "    }",
            "    void Reset() noexcept { m_Client = nullptr; m_Subscription = nullptr; m_Payload = BML_IMC_INVALID_ID; m_Handler = nullptr; m_Userdata = nullptr; }",
            "    BML_ImcClient m_Client = nullptr; BML_ImcSubscription m_Subscription = nullptr;",
            "    BML_ImcPayloadTypeId m_Payload = BML_IMC_INVALID_ID; Handler m_Handler = nullptr; void *m_Userdata = nullptr;",
            "};", "",
        ])

def append_imc_client(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    append_imc_subscriptions(lines, api)
    lines.extend([
        "class Client {", "public:", "    Client() = default;", "    ~Client() { (void)Close(); }",
        "    Client(const Client &) = delete;", "    Client &operator=(const Client &) = delete;", "",
    ])
    for endpoint in api.endpoints:
        if endpoint.kind in TOPIC_ENDPOINT_KINDS:
            continue
        name = camel(endpoint.name)
        output_value = value_name(schemas[endpoint.output_schema])
        if endpoint.kind == "collection":
            output_value = f"std::vector<{output_value}>"
        lines.append(f"    using {name}Future = ::BML::Imc::RpcFuture<{output_value}>;")
    lines.extend([
        "",
        "    int Open(const char *ownerId = nullptr) noexcept {",
        "        const int closeStatus = Close(); if (m_Client) return closeStatus;",
        "        BML_ImcClient client = nullptr;",
        "        const int status = BML_Imc_OpenClient(ownerId, &client);",
        "        return status == BML_OK ? Adopt(client) : status;", "    }",
        "    int Adopt(BML_ImcClient client) noexcept {",
        "        const int closeStatus = Close(); if (m_Client) return closeStatus;",
        "        if (!client) return BML_ERROR_INVALID_PARAMETER;",
        "        m_Client = client;", "        int status = BML_OK;",
    ])
    for record in api.schemas:
        name = camel(record.name)
        lines.append(f"        if (status == BML_OK) status = BML_Imc_GetPayloadTypeId(m_Client, {name}Payload, &m_{name}Payload);")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind == "component":
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetPayloadTypeId(m_Client, {name}RequestPayload, &m_{name}RequestPayload);")
        elif endpoint.kind == "collection":
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetPayloadTypeId(m_Client, {name}CollectionPayload, &m_{name}CollectionPayload);")
        if endpoint.kind in TOPIC_ENDPOINT_KINDS:
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetTopicId(m_Client, {name}Route, &m_{name}Topic);")
        else:
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetRpcId(m_Client, {name}Route, &m_{name}Rpc);")
    lines.extend([
        "        if (status != BML_OK) (void)Close();", "        return status;", "    }",
        "    int Close() noexcept {", "        if (!m_Client) return BML_OK;",
        "        const int status = BML_Imc_CloseClient(m_Client);",
        "        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) { m_Client = nullptr; ResetIds(); }",
        "        return status;", "    }",
        "    BML_ImcClient Handle() const noexcept { return m_Client; }",
        "    int EnsureOpen(const char *ownerId = nullptr) noexcept { return m_Client ? BML_OK : Open(ownerId); }",
    ])
    for record in api.schemas:
        name = camel(record.name)
        lines.append(f"    BML_ImcPayloadTypeId {name}PayloadType() const noexcept {{ return m_{name}Payload; }}")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind in TOPIC_ENDPOINT_KINDS:
            lines.append(f"    BML_ImcTopicId {name}TopicId() const noexcept {{ return m_{name}Topic; }}")
        else:
            lines.append(f"    BML_ImcRpcId {name}RpcId() const noexcept {{ return m_{name}Rpc; }}")
        if endpoint.kind == "component":
            lines.append(f"    BML_ImcPayloadTypeId {name}RequestPayloadType() const noexcept {{ return m_{name}RequestPayload; }}")
        elif endpoint.kind == "collection":
            lines.append(f"    BML_ImcPayloadTypeId {name}CollectionPayloadType() const noexcept {{ return m_{name}CollectionPayload; }}")
        if endpoint.kind not in TOPIC_ENDPOINT_KINDS:
            lines.extend([
                f"    int Is{name}Available(bool &out) const noexcept {{",
                "        int available = 0;",
                f"        const int status = BML_Imc_IsRpcAvailable(m_Client, m_{name}Rpc, &available);",
                "        if (status == BML_OK) out = available != 0;",
                "        return status;", "    }",
            ])
    lines.append("")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        output = schemas[endpoint.output_schema]
        output_name = camel(output.name)
        output_value = value_name(output)
        if endpoint.kind == "rpc":
            if endpoint.input_schema:
                input_record = schemas[endpoint.input_schema]
                input_name = camel(input_record.name)
                input_value = value_name(input_record)
                lines.extend([
                    f"    int BeginCall{name}(const {input_value} &input, {name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                    "        ::BML::Imc::MessageBuffer buffer; BML_ImcMessage request{};",
                    f"        int status = ::BML::Imc::EncodeMessage(input, m_{input_name}Payload, buffer, request, Encoded{input_name}Size, Encode{input_name});",
                    f"        return status == BML_OK ? ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, &request, m_{output_name}Payload, out, Decode{output_name}, timeoutMs) : status;", "    }",
                    f"    int Call{name}(const {input_value} &input, {output_value} &out, std::uint32_t timeoutMs = 5000u) {{",
                    f"        {name}Future future; int status = BeginCall{name}(input, future, timeoutMs);",
                    "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
                ])
            else:
                lines.extend([
                    f"    int BeginCall{name}({name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                    f"        return ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, nullptr, m_{output_name}Payload, out, Decode{output_name}, timeoutMs);", "    }",
                    f"    int Call{name}({output_value} &out, std::uint32_t timeoutMs = 5000u) {{",
                    f"        {name}Future future; int status = BeginCall{name}(future, timeoutMs);",
                    "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
                ])
        elif endpoint.kind == "resource":
            lines.extend([
                f"    int BeginRead{name}({name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                f"        return ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, nullptr, m_{output_name}Payload, out, Decode{output_name}, timeoutMs);", "    }",
                f"    int Read{name}({output_value} &out, std::uint32_t timeoutMs = 5000u) {{",
                f"        {name}Future future; int status = BeginRead{name}(future, timeoutMs);",
                "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
            ])
        elif endpoint.kind in {"query", "command"}:
            input_record = schemas[endpoint.input_schema]
            input_name = camel(input_record.name)
            input_value = value_name(input_record)
            verb = "Query" if endpoint.kind == "query" else "Command"
            lines.extend([
                f"    int Begin{verb}{name}(const {input_value} &input, {name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                "        ::BML::Imc::MessageBuffer buffer; BML_ImcMessage request{};",
                f"        int status = ::BML::Imc::EncodeMessage(input, m_{input_name}Payload, buffer, request, Encoded{input_name}Size, Encode{input_name});",
                f"        return status == BML_OK ? ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, &request, m_{output_name}Payload, out, Decode{output_name}, timeoutMs) : status;", "    }",
                f"    int {verb}{name}(const {input_value} &input, {output_value} &out, std::uint32_t timeoutMs = 5000u) {{",
                f"        {name}Future future; int status = Begin{verb}{name}(input, future, timeoutMs);",
                "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
            ])
        elif endpoint.kind == "component":
            lines.extend([
                f"    int BeginRead{name}(const BML_ObjectRef &object, {name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                "        ::BML::Imc::MessageBuffer buffer; BML_ImcMessage request{};",
                f"        int status = ::BML::Imc::EncodeObjectRequest(object, m_{name}RequestPayload, WireHash, buffer, request);",
                f"        return status == BML_OK ? ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, &request, m_{output_name}Payload, out, Decode{output_name}, timeoutMs) : status;", "    }",
                f"    int Read{name}(const BML_ObjectRef &object, {output_value} &out, std::uint32_t timeoutMs = 5000u) {{",
                f"        {name}Future future; int status = BeginRead{name}(object, future, timeoutMs);",
                "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
            ])
        elif endpoint.kind == "collection":
            lines.extend([
                f"    int BeginRead{name}({name}Future &out, std::uint32_t timeoutMs = 5000u) {{",
                f"        auto decode = [](const BML_ImcMessage &message, std::vector<{output_value}> &values) {{",
                f"            std::uint64_t hash = 0; int status = ::BML::Imc::DecodeCollection(message, values, hash, Decode{output_name});",
                "            return status == BML_OK && !IsCompatibleHash(hash) ? BML_ERROR_IMC_API_MISMATCH : status;", "        };",
                f"        return ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, nullptr, m_{name}CollectionPayload, out, decode, timeoutMs);", "    }",
                f"    int Read{name}(std::vector<{output_value}> &out, std::uint32_t timeoutMs = 5000u) {{",
                f"        {name}Future future; int status = BeginRead{name}(future, timeoutMs);",
                "        return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;", "    }",
            ])
        elif endpoint.kind in TOPIC_ENDPOINT_KINDS:
            lines.extend([
                f"    int Publish{name}(const {output_value} &value, std::size_t *outDelivered = nullptr) noexcept {{",
                f"        return ::BML::Imc::Publish(m_Client, m_{name}Topic, m_{output_name}Payload, value, Encoded{output_name}Size, Encode{output_name}, outDelivered);", "    }",
                f"    int Get{name}SubscriberCount(std::size_t &outCount) const noexcept {{",
                f"        return BML_Imc_GetTopicSubscriberCount(m_Client, m_{name}Topic, &outCount);", "    }",
                f"    int Subscribe{name}({name}Subscription &out, {name}Subscription::Handler handler, void *userdata = nullptr, std::uint32_t capacity = 256u,",
                "                     BML_ImcBackpressure backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST, BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {",
                f"        return out.Open(m_Client, m_{name}Topic, m_{output_name}Payload, handler, userdata, capacity, backpressure, execution);", "    }",
            ])
    lines.extend(["private:", "    void ResetIds() noexcept {"])
    for record in api.schemas:
        lines.append(f"        m_{camel(record.name)}Payload = BML_IMC_INVALID_ID;")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        lines.append(f"        m_{name}{'Topic' if endpoint.kind in TOPIC_ENDPOINT_KINDS else 'Rpc'} = BML_IMC_INVALID_ID;")
        if endpoint.kind == "component":
            lines.append(f"        m_{name}RequestPayload = BML_IMC_INVALID_ID;")
        elif endpoint.kind == "collection":
            lines.append(f"        m_{name}CollectionPayload = BML_IMC_INVALID_ID;")
    lines.extend(["    }", "    BML_ImcClient m_Client = nullptr;"])
    for record in api.schemas:
        lines.append(f"    BML_ImcPayloadTypeId m_{camel(record.name)}Payload = BML_IMC_INVALID_ID;")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind in TOPIC_ENDPOINT_KINDS:
            lines.append(f"    BML_ImcTopicId m_{name}Topic = BML_IMC_INVALID_ID;")
        else:
            lines.append(f"    BML_ImcRpcId m_{name}Rpc = BML_IMC_INVALID_ID;")
        if endpoint.kind == "component":
            lines.append(f"    BML_ImcPayloadTypeId m_{name}RequestPayload = BML_IMC_INVALID_ID;")
        elif endpoint.kind == "collection":
            lines.append(f"    BML_ImcPayloadTypeId m_{name}CollectionPayload = BML_IMC_INVALID_ID;")
    lines.extend(["};", ""])


def append_imc_provider(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    rpc_endpoints = [endpoint for endpoint in api.endpoints if endpoint.kind not in TOPIC_ENDPOINT_KINDS]
    if not rpc_endpoints:
        return
    lines.extend([
        "class Provider {", "public:", "    Provider() = default;", "    ~Provider() { (void)Close(); }",
        "    Provider(const Provider &) = delete;", "    Provider &operator=(const Provider &) = delete;",
        "    int Open(const char *ownerId = nullptr) noexcept { const int status = Close(); return m_Transport.Handle() ? status : m_Transport.Open(ownerId); }",
        "    int Close() noexcept { const int status = m_Transport.Close(); if (!m_Transport.Handle()) ResetSlots(); return status; }",
        "    Client &Transport() noexcept { return m_Transport; }",
        "    const Client &Transport() const noexcept { return m_Transport; }", "",
    ])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        output_value = value_name(schemas[endpoint.output_schema])
        if endpoint.kind == "resource" or (endpoint.kind == "rpc" and not endpoint.input_schema):
            signature = f"int (*)({output_value} &, void *)"
        elif endpoint.kind in {"query", "command", "rpc"}:
            signature = f"int (*)(const {value_name(schemas[endpoint.input_schema])} &, {output_value} &, void *)"
        elif endpoint.kind == "component":
            signature = f"int (*)(const BML_ObjectRef &, {output_value} &, void *)"
        else:
            signature = f"int (*)(std::vector<{output_value}> &, void *)"
        lines.append(f"    using {name}Handler = {signature};")
        lines.extend([
            f"    int Register{name}({name}Handler handler, void *userdata = nullptr, BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {{",
            "        if (!m_Transport.Handle() || !handler) return BML_ERROR_INVALID_PARAMETER;",
            f"        if (m_{name}.Registered) return BML_ERROR_ALREADY_EXISTS;",
            f"        m_{name} = {{this, handler, userdata, false}};",
            "        BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT; options.Execution = execution;",
            f"        const int status = BML_Imc_RegisterRpc(m_Transport.Handle(), m_Transport.{name}RpcId(), &options, &{name}Thunk, &m_{name});",
            f"        if (status == BML_OK) m_{name}.Registered = true; else m_{name} = {{}};", "        return status;", "    }",
            f"    int Unregister{name}() noexcept {{", f"        if (!m_{name}.Registered) return BML_ERROR_NOT_FOUND;",
            f"        const int status = BML_Imc_UnregisterRpc(m_Transport.Handle(), m_Transport.{name}RpcId());",
            f"        if (status == BML_OK) m_{name} = {{}};", "        return status;", "    }", "",
        ])
    lines.append("private:")
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        output = schemas[endpoint.output_schema]
        output_name = camel(output.name)
        output_value = value_name(output)
        lines.append(f"    struct {name}Slot {{ Provider *Owner = nullptr; {name}Handler Function = nullptr; void *Userdata = nullptr; bool Registered = false; }};")
        lines.extend([
            f"    static int {name}Thunk(BML_ImcRpcId, const BML_ImcMessage *request, BML_ImcResponse *response, void *userdata) noexcept {{",
            f"        auto *slot = static_cast<{name}Slot *>(userdata);",
            "        if (!slot || !slot->Owner || !slot->Function) return BML_ERROR_INVALID_PARAMETER;", "        try {",
        ])
        if endpoint.kind == "resource" or (endpoint.kind == "rpc" and not endpoint.input_schema):
            lines.extend([
                "            if (request && (request->Size < sizeof(BML_ImcMessage) || request->DataSize != 0)) return BML_ERROR_MALFORMED_MESSAGE;",
                f"            {output_value} output{{}};", "            const int status = slot->Function(output, slot->Userdata);",
            ])
        elif endpoint.kind in {"query", "command", "rpc"}:
            input_record = schemas[endpoint.input_schema]
            input_name = camel(input_record.name)
            input_value = value_name(input_record)
            lines.extend([
                "            if (!request || request->Size < sizeof(BML_ImcMessage)) return BML_ERROR_MALFORMED_MESSAGE;",
                f"            if (request->PayloadType != slot->Owner->m_Transport.{input_name}PayloadType()) return BML_ERROR_TYPE_MISMATCH;",
                f"            {input_value} input{{}}; int status = Decode{input_name}(*request, input);",
                "            if (status != BML_OK) return status;", f"            {output_value} output{{}}; status = slot->Function(input, output, slot->Userdata);",
            ])
        elif endpoint.kind == "component":
            lines.extend([
                "            if (!request) return BML_ERROR_MALFORMED_MESSAGE;",
                "            BML_ObjectRef object{}; std::uint64_t hash = 0;",
                f"            int status = ::BML::Imc::DecodeObjectRequest(*request, slot->Owner->m_Transport.{name}RequestPayloadType(), object, hash);",
                "            if (status != BML_OK) return status;",
                "            if (!IsCompatibleHash(hash)) return BML_ERROR_IMC_API_MISMATCH;",
                f"            {output_value} output{{}}; status = slot->Function(object, output, slot->Userdata);",
            ])
        else:
            lines.extend([
                "            if (request && (request->Size < sizeof(BML_ImcMessage) || request->DataSize != 0)) return BML_ERROR_MALFORMED_MESSAGE;",
                f"            std::vector<{output_value}> output;", "            const int status = slot->Function(output, slot->Userdata);",
            ])
        lines.append("            if (status != BML_OK) return status;")
        if endpoint.kind == "collection":
            lines.append(f"            return ::BML::Imc::WriteCollectionResponse(response, slot->Owner->m_Transport.{name}CollectionPayloadType(), output, WireHash, Encoded{output_name}Size, Encode{output_name});")
        else:
            lines.append(f"            return ::BML::Imc::WriteResponse(response, slot->Owner->m_Transport.{output_name}PayloadType(), output, Encoded{output_name}Size, Encode{output_name});")
        lines.extend(["        } catch (...) { return BML_ERROR_IMC_TARGET_EXECUTION_FAILED; }", "    }"])
    lines.extend(["    void ResetSlots() noexcept {"])
    for endpoint in rpc_endpoints:
        lines.append(f"        m_{camel(endpoint.name)} = {{}};")
    lines.extend(["    }", "    Client m_Transport;"])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        lines.append(f"    {name}Slot m_{name}{{}};")
    lines.extend(["};", ""])

def emit_imc_header(api: ApiDefinition) -> str:
    """Emit typed values and stable IMC routes without a dynamic record API."""
    namespace = "::".join(camel(part) for part in api.api_id.split("."))
    compatible_hashes = " || ".join(["value == Hash"] + [
        f"value == 0x{value:016X}ULL" for value in api.compatible_hashes
    ])
    lines = [
        "// Generated by tools/imc_codegen.py. Do not edit by hand.",
        "#pragma once",
        "",
        '#include "BML/ImcCpp.hpp"',
        '#include "BML/ImcWire.hpp"',
        "#include <cstdint>",
        "#include <string>",
        "#include <utility>",
        "#include <vector>",
        "",
        f"namespace BML::Imc::Generated::{namespace} {{",
        f'inline constexpr const char ApiId[] = "{api.api_id}";',
        f"inline constexpr unsigned int Major = {api.major};",
        f"inline constexpr unsigned int Minor = {api.minor};",
        f"inline constexpr std::uint64_t Hash = 0x{api.hash64:016X}ULL;",
        f"inline constexpr std::uint64_t WireHash = 0x{api.wire_hash:016X}ULL;",
        f"inline bool IsCompatibleHash(std::uint64_t value) noexcept {{ return {compatible_hashes}; }}",
        "",
        "struct SchemaMetadata { std::uint32_t Id; const char *Name; const char *Payload; };",
        "struct EndpointMetadata { const char *Name; const char *Route; bool Topic; std::uint32_t Input; std::uint32_t Output; };",
        "",
    ]
    for enum in api.enums:
        enum_name = camel(enum.name)
        lines.append(f"enum class {enum_name} : {ENUM_CPP_UNDERLYING_TYPES[enum.underlying]} {{")
        for enum_value in enum.values:
            lines.append(
                f"    {camel(enum_value.name)} = {enum_cpp_literal(enum, enum_value.value)},"
            )
        lines.extend(["};", f"inline constexpr bool IsKnown{enum_name}({enum_name} value) noexcept {{", "    switch (value) {"])
        for enum_value in enum.values:
            lines.append(f"    case {enum_name}::{camel(enum_value.name)}:")
        lines.extend(["        return true;", "    default:", "        return false;", "    }", "}", ""])

    for record in api.schemas:
        record_name = camel(record.name)
        lines.append(f"inline constexpr std::uint32_t {record_name}Schema = {record.id}u;")
        lines.append(
            f'inline constexpr const char {record_name}Payload[] = '
            f'"{api.api_id}/v{api.major}/payload/{record.name}";'
        )
        lines.append(f"namespace {record_name}Field {{")
        for field in record.fields:
            lines.append(f"inline constexpr std::uint32_t {camel(field.name)} = {field.id}u;")
        lines.append("}")
        lines.append(f"struct {value_name(record)} {{")
        for field in record.fields:
            member = camel(field.name)
            if field.optional:
                lines.append(f"    bool Has{member} = false;")
            lines.append(f"    {imc_field_cpp_type(api, field.type)} {member}{{}};")
        lines.extend(["};", ""])
        append_imc_codec(lines, api, record)

    lines.append("inline constexpr SchemaMetadata Schemas[] = {")
    for record in api.schemas:
        record_name = camel(record.name)
        lines.append(f'    {{{record.id}u, "{record.name}", {record_name}Payload}},')
    lines.extend(["};", ""])

    for endpoint in api.endpoints:
        endpoint_name = camel(endpoint.name)
        route_kind = "topic" if endpoint.kind in TOPIC_ENDPOINT_KINDS else "rpc"
        lines.append(
            f'inline constexpr const char {endpoint_name}Route[] = '
            f'"{api.api_id}/v{api.major}/{route_kind}/{endpoint.name}";'
        )
        if endpoint.kind == "component":
            lines.append(
                f'inline constexpr const char {endpoint_name}RequestPayload[] = '
                f'"{api.api_id}/v{api.major}/payload/{endpoint.name}.request";'
            )
        elif endpoint.kind == "collection":
            lines.append(
                f'inline constexpr const char {endpoint_name}CollectionPayload[] = '
                f'"{api.api_id}/v{api.major}/payload/{endpoint.name}.collection";'
            )
    lines.append("")
    append_imc_client(lines, api)
    append_imc_provider(lines, api)
    lines.append("inline constexpr EndpointMetadata Endpoints[] = {")
    for endpoint in api.endpoints:
        endpoint_name = camel(endpoint.name)
        topic = "true" if endpoint.kind in TOPIC_ENDPOINT_KINDS else "false"
        lines.append(
            f'    {{"{endpoint.name}", {endpoint_name}Route, {topic}, '
            f'{endpoint.input_schema}u, {endpoint.output_schema}u}},'
        )
    lines.extend(["};", "", f"}} // namespace BML::Imc::Generated::{namespace}", ""])
    return "\n".join(lines)


def write_if_changed(path: Path, text: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == text:
        return False
    if check:
        raise ApiDefinitionError(f"generated output is stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def load_api_definition(path: Path, role: str) -> ApiDefinition:
    try:
        raw = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_json_object,
        )
        return parse_api_definition(raw)
    except (OSError, json.JSONDecodeError, ApiDefinitionError) as error:
        raise ApiDefinitionError(f"{role} {path}: {error}") from error


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, required=True, help=".bmlapi JSON input")
    parser.add_argument("--out-dir", type=Path, required=True,
                        help="directory for generated *_imc.hpp headers")
    parser.add_argument("--previous", action="append", type=Path,
                        help="previous .bmlapi to validate as an append-only compatible predecessor")
    parser.add_argument("--expected-api-id",
                        help="require a single input to declare this API ID (used by CMake output tracking)")
    parser.add_argument("--check", action="store_true", help="fail instead of rewriting stale output")
    args = parser.parse_args(argv)
    try:
        api_definitions = [load_api_definition(path, "input") for path in args.input]
        previous_apis = [
            load_api_definition(path, "previous input")
            for path in (args.previous or [])
        ]
        if args.expected_api_id is not None:
            expected_api_id = api_key(args.expected_api_id, "--expected-api-id")
            if len(api_definitions) != 1:
                raise ApiDefinitionError("--expected-api-id requires exactly one --input")
            actual_api_id = api_definitions[0].api_id
            if actual_api_id != expected_api_id:
                raise ApiDefinitionError(
                    f"input {args.input[0]}: contract API ID {actual_api_id!r} does not match "
                    f"expected API ID {expected_api_id!r}"
                )
        seen_api_ids: set[str] = set()
        seen_output_paths: dict[str, str] = {}
        planned_apis: list[tuple[ApiDefinition, str]] = []
        for api in api_definitions:
            if api.api_id in seen_api_ids:
                raise ApiDefinitionError(f"duplicate API ID input: {api.api_id}")
            seen_api_ids.add(api.api_id)
            stem = api.api_id.replace(".", "_")
            output_paths = [args.out_dir / f"{stem}_imc.hpp"]
            for output_path in output_paths:
                path_key = str(output_path.resolve()).casefold()
                previous_api_id = seen_output_paths.get(path_key)
                if previous_api_id is not None:
                    raise ApiDefinitionError(
                        f"generated output path collision: API IDs {previous_api_id!r} "
                        f"and {api.api_id!r} both map to {output_path}"
                    )
                seen_output_paths[path_key] = api.api_id
            planned_apis.append((api, stem))

        by_api_id = {api.api_id: api for api in api_definitions}
        for previous in previous_apis:
            current = by_api_id.get(previous.api_id)
            if not current:
                raise ApiDefinitionError(f"no current input supplied for API ID {previous.api_id}")
            validate_compatible(previous, current)
        for api, stem in planned_apis:
            write_if_changed(args.out_dir / f"{stem}_imc.hpp", emit_imc_header(api), args.check)
    except (OSError, json.JSONDecodeError, ApiDefinitionError) as error:
        print(f"imc_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
