#!/usr/bin/env python3
"""Generate deterministic typed IMC bindings from compact .imc interfaces.

The loader never reads .imc or .imc.lock files at runtime. Authors edit the
small declaration-oriented .imc file; this tool owns the adjacent .imc.lock
snapshot that assigns permanent wire identities and records compatibility.
"""

from __future__ import annotations

import argparse
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

KEY_RE = re.compile(r"^[A-Za-z0-9_.-]+$")
KEY_ALNUM_RE = re.compile(r"[A-Za-z0-9]")
API_ID_RE = re.compile(r"^[a-z0-9]+(?:\.[a-z0-9]+)*$")
ENUM_FIELD_RE = re.compile(r"^enum<([A-Za-z0-9_.-]+)>$")


class ApiDefinitionError(ValueError):
    pass


@dataclass(frozen=True)
class IdlToken:
    value: str
    line: int
    column: int


def tokenize_imc(text: str) -> list[IdlToken]:
    """Tokenize the deliberately small, dependency-free .imc language."""
    tokens: list[IdlToken] = []
    index = 0
    line = 1
    column = 1
    length = len(text)

    def advance() -> str:
        nonlocal index, line, column
        character = text[index]
        index += 1
        if character == "\n":
            line += 1
            column = 1
        else:
            column += 1
        return character

    while index < length:
        character = text[index]
        if character.isspace() or (index == 0 and character == "\ufeff"):
            advance()
            continue
        if character == "#" or text.startswith("//", index):
            while index < length and text[index] != "\n":
                advance()
            continue

        token_line, token_column = line, column
        if text.startswith("->", index):
            advance()
            advance()
            tokens.append(IdlToken("->", token_line, token_column))
            continue
        if character in "{}():=,;":
            tokens.append(IdlToken(advance(), token_line, token_column))
            continue

        start = index
        while index < length:
            character = text[index]
            if (character.isspace() or character in "{}():=,;#"
                    or text.startswith("//", index) or text.startswith("->", index)):
                break
            advance()
        if start == index:
            raise ApiDefinitionError(
                f"line {line} column {column}: unexpected character {text[index]!r}"
            )
        tokens.append(IdlToken(text[start:index], token_line, token_column))

    tokens.append(IdlToken("<eof>", line, column))
    return tokens


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
        for name in ("ApiId", "Major", "Minor", "Client", "Provider")
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
        source = f"record {record.name}"
        imc_names = [
            f"{record_name}Field",
            f"{record_name}Payload",
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
    provider_handler_symbols: dict[str, str] = {
        name: "generated Provider handler table"
        for name in ("Handlers", "Userdata", "Execution")
    }
    for endpoint in endpoints:
        endpoint_name = camel(endpoint.name)
        source = f"endpoint {endpoint.name}"
        add(imc_top_level, f"{endpoint_name}Route", source)
        if endpoint.kind == "topic":
            add(imc_top_level, f"{endpoint_name}Subscription", source)
        prefix = "Call" if endpoint.kind == "rpc" else "Open"
        generated_method = f"{prefix}{endpoint_name}"
        add(imc_client_symbols, generated_method, source)
        if endpoint.kind == "rpc":
            add(imc_client_symbols, f"Is{endpoint_name}Available", source)
            add(provider_handler_symbols, endpoint_name, source)


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


@dataclass(frozen=True)
class ApiDefinition:
    api_id: str
    major: int
    minor: int
    enums: tuple[EnumDefinition, ...]
    schemas: tuple[Record, ...]
    endpoints: tuple[Endpoint, ...]


def parse_api_definition(raw: Any) -> ApiDefinition:
    if not isinstance(raw, dict):
        raise ApiDefinitionError("top-level value must be an object")
    reject_unknown_properties(
        raw,
        ("api", "version", "enums", "schemas", "rpcs", "topics"),
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

    raw_schemas = raw.get("schemas", [])
    if not isinstance(raw_schemas, list):
        raise ApiDefinitionError("schemas must be an array")
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
                     output_schema: int) -> None:
        if input_schema and input_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown input schema {input_schema}")
        if output_schema and output_schema not in known_schema_ids:
            raise ApiDefinitionError(f"endpoint {endpoint_name} references unknown output schema {output_schema}")
        if endpoint_name in endpoint_names:
            raise ApiDefinitionError(f"duplicate endpoint name {endpoint_name}")
        endpoint_names.add(endpoint_name)
        endpoints.append(Endpoint(endpoint_name, kind, input_schema, output_schema))

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
        output_schema = uint32(raw_rpc.get("response", 0), f"rpcs[{index}].response", allow_zero=True)
        add_endpoint(endpoint_name, "rpc", input_schema, output_schema)
    for index, raw_topic in enumerate(raw_topics):
        if not isinstance(raw_topic, dict):
            raise ApiDefinitionError(f"topics[{index}] must be an object")
        reject_unknown_properties(
            raw_topic, ("name", "message"), f"topics[{index}]"
        )
        endpoint_name = key(raw_topic.get("name"), f"topics[{index}].name")
        message_schema = uint32(raw_topic.get("message"), f"topics[{index}].message")
        add_endpoint(endpoint_name, "topic", 0, message_schema)

    validate_generated_identifiers(enums, schemas, endpoints)

    return ApiDefinition(api_id, major, minor, tuple(sorted(enums, key=lambda item: item.name)),
                    tuple(sorted(schemas, key=lambda item: item.id)),
                    tuple(sorted(endpoints, key=lambda item: item.name)))


class ImcParser:
    """Recursive-descent parser for the small declaration-oriented IDL."""

    def __init__(self, text: str):
        self._tokens = tokenize_imc(text)
        self._index = 0

    def _current(self) -> IdlToken:
        return self._tokens[self._index]

    def _error(self, message: str, token: IdlToken | None = None) -> ApiDefinitionError:
        location = token or self._current()
        return ApiDefinitionError(
            f"line {location.line} column {location.column}: {message}"
        )

    def _accept(self, value: str) -> bool:
        if self._current().value != value:
            return False
        self._index += 1
        return True

    def _expect(self, value: str) -> IdlToken:
        token = self._current()
        if token.value != value:
            raise self._error(f"expected {value!r}, found {token.value!r}", token)
        self._index += 1
        return token

    def _atom(self, what: str) -> IdlToken:
        token = self._current()
        if token.value in {"<eof>", "{", "}", "(", ")", ":", "=", ",", ";", "->"}:
            raise self._error(f"expected {what}, found {token.value!r}", token)
        self._index += 1
        return token

    def _skip_separators(self) -> None:
        while self._current().value in {",", ";"}:
            self._index += 1

    def _integer(self, what: str) -> int:
        token = self._atom(what)
        try:
            return int(token.value, 10)
        except ValueError as error:
            raise self._error(f"{what} must be a decimal integer", token) from error

    def parse(self) -> dict[str, Any]:
        if self._current().value == "{":
            raise self._error(
                "JSON is not an .imc interface; start with 'api <id> <major>.<minor>'"
            )
        self._expect("api")
        api_id = self._atom("API ID").value
        version_token = self._atom("version")
        version_match = re.fullmatch(r"([0-9]+)\.([0-9]+)", version_token.value)
        if not version_match:
            raise self._error("version must use <major>.<minor>", version_token)
        self._skip_separators()

        enums: list[dict[str, Any]] = []
        schemas: list[dict[str, Any]] = []
        rpcs: list[dict[str, Any]] = []
        topics: list[dict[str, Any]] = []

        while self._current().value != "<eof>":
            declaration = self._atom("declaration").value
            if declaration == "enum":
                enums.append(self._parse_enum())
            elif declaration == "record":
                schemas.append(self._parse_record())
            elif declaration == "schema":
                raise self._error(
                    "schema wire IDs are no longer authored here; use 'record <name> { ... }'",
                    self._tokens[self._index - 1],
                )
            elif declaration == "rpc":
                rpc, inline = self._parse_rpc()
                rpcs.append(rpc)
                schemas.extend(inline)
            elif declaration == "topic":
                topic, inline = self._parse_topic()
                topics.append(topic)
                schemas.extend(inline)
            else:
                raise self._error(
                    f"unknown declaration {declaration!r}; expected enum, record, rpc, or topic",
                    self._tokens[self._index - 1],
                )
            self._skip_separators()

        return {
            "api": api_id,
            "version": {
                "major": int(version_match.group(1)),
                "minor": int(version_match.group(2)),
            },
            "enums": enums,
            "schemas": schemas,
            "rpcs": rpcs,
            "topics": topics,
        }

    def _parse_enum(self) -> dict[str, Any]:
        name = self._atom("enum name").value
        underlying = "int"
        if self._accept(":"):
            underlying = self._atom("enum underlying type").value
        self._expect("{")
        values = []
        self._skip_separators()
        while not self._accept("}"):
            if self._current().value == "<eof>":
                raise self._error(f"unterminated enum {name!r}; expected '}}'")
            value_name = self._atom("enum value name").value
            self._expect("=")
            values.append({"name": value_name, "value": self._integer("enum value")})
            self._skip_separators()
        return {"name": name, "underlying": underlying, "values": values}

    def _parse_record(self) -> dict[str, Any]:
        name = self._atom("record name").value
        self._expect("{")
        fields = self._parse_fields("record", name, "}")
        return {"name": name, "fields": fields}

    def _parse_fields(self, owner_kind: str, owner_name: str,
                      terminator: str) -> list[dict[str, Any]]:
        fields: list[dict[str, Any]] = []
        self._skip_separators()
        while not self._accept(terminator):
            if self._current().value == "<eof>":
                raise self._error(
                    f"unterminated {owner_kind} {owner_name!r}; expected {terminator!r}"
                )
            optional = self._accept("optional")
            field_type = self._atom("field type").value
            field_name = self._atom("field name").value
            fields.append({
                "name": field_name,
                "type": field_type,
                "optional": optional,
            })
            self._skip_separators()
        return fields

    def _parse_payload(self, owner_kind: str, owner_name: str,
                       synthesized_name: str, *, allow_empty: bool) -> tuple[str | None, dict[str, Any] | None]:
        self._expect("(")
        if self._accept(")"):
            if allow_empty:
                return None, None
            raise self._error(f"{owner_kind} {owner_name!r} requires a payload")
        first = self._current()
        next_value = self._tokens[self._index + 1].value
        if first.value != "optional" and next_value == ")":
            reference = self._atom("payload record").value
            self._expect(")")
            return reference, None
        fields = self._parse_fields(owner_kind, owner_name, ")")
        return synthesized_name, {"name": synthesized_name, "fields": fields}

    def _parse_rpc(self) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        name = self._atom("RPC name").value
        request, request_record = self._parse_payload(
            "RPC", name, f"{name}_request", allow_empty=True
        )
        response: str | None = None
        response_record: dict[str, Any] | None = None
        if self._accept("->"):
            if self._current().value == "(":
                response, response_record = self._parse_payload(
                    "RPC response", name, f"{name}_reply", allow_empty=False
                )
            else:
                response = self._atom("response record").value
        rpc: dict[str, Any] = {"name": name}
        if request is not None:
            rpc["request"] = request
        if response is not None:
            rpc["response"] = response
        return rpc, [record for record in (request_record, response_record) if record is not None]

    def _parse_topic(self) -> tuple[dict[str, Any], list[dict[str, Any]]]:
        name = self._atom("Topic name").value
        message, message_record = self._parse_payload(
            "Topic", name, f"{name}_event", allow_empty=False
        )
        assert message is not None
        return {"name": name, "message": message}, ([] if message_record is None else [message_record])


def parse_imc(text: str) -> dict[str, Any]:
    return ImcParser(text).parse()


def value_name(record: Record) -> str:
    """A generated value must not collide with its payload constant."""
    return f"{camel(record.name)}Value"


LOCK_FORMAT = 1


def _unique_by_name(values: list[dict[str, Any]], what: str) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for value in values:
        name = key(value.get("name"), f"{what} name")
        if name in result:
            raise ApiDefinitionError(f"duplicate {what} name: {name}")
        result[name] = value
    return result


def _canonical_source(source: dict[str, Any]) -> dict[str, Any]:
    """Remove declaration-order noise before comparing interface definitions."""
    return {
        "api": source["api"],
        "version": source["version"],
        "enums": [
            {
                **enum,
                "values": sorted(
                    enum.get("values", []),
                    key=lambda value: (value["value"], value["name"]),
                ),
            }
            for enum in sorted(source.get("enums", []), key=lambda enum: enum["name"])
        ],
        "schemas": [
            {
                **record,
                "fields": sorted(
                    record.get("fields", []), key=lambda field: field["name"]
                ),
            }
            for record in sorted(
                source.get("schemas", []), key=lambda record: record["name"]
            )
        ],
        "rpcs": sorted(source.get("rpcs", []), key=lambda endpoint: endpoint["name"]),
        "topics": sorted(
            source.get("topics", []), key=lambda endpoint: endpoint["name"]
        ),
    }


def _source_signature(source: dict[str, Any]) -> str:
    return json.dumps(
        _canonical_source(source),
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def _active_source_from_lock(snapshot: dict[str, Any]) -> dict[str, Any]:
    return {
        "api": snapshot["api"],
        "version": snapshot["version"],
        "enums": snapshot.get("enums", []),
        "schemas": [
            {
                "name": record["name"],
                "fields": [
                    {name: field[name] for name in ("name", "type", "optional")}
                    for field in record.get("fields", []) if not field.get("reserved", False)
                ],
            }
            for record in snapshot.get("schemas", [])
        ],
        "rpcs": snapshot.get("rpcs", []),
        "topics": snapshot.get("topics", []),
    }


def _validate_enum_evolution(previous: list[dict[str, Any]], current: list[dict[str, Any]]) -> None:
    old_enums = _unique_by_name(previous, "enum")
    new_enums = _unique_by_name(current, "enum")
    for enum_name, old_enum in old_enums.items():
        new_enum = new_enums.get(enum_name)
        if new_enum is None:
            raise ApiDefinitionError(f"enum {enum_name} was removed or renamed")
        if new_enum.get("underlying", "int") != old_enum.get("underlying", "int"):
            raise ApiDefinitionError(f"enum {enum_name} underlying type changed incompatibly")
        old_values = {value["name"]: value["value"] for value in old_enum["values"]}
        new_values = {value["name"]: value["value"] for value in new_enum["values"]}
        for value_name, value in old_values.items():
            if new_values.get(value_name) != value:
                raise ApiDefinitionError(
                    f"enum value {enum_name}.{value_name} was removed, renamed, or renumbered"
                )


def build_interface_lock(source: dict[str, Any], previous: dict[str, Any] | None) -> dict[str, Any]:
    """Assign permanent IDs and validate evolution against the interface lock."""
    api_id = api_key(source.get("api"), "api")
    version = source.get("version")
    if not isinstance(version, dict):
        raise ApiDefinitionError("version must be an object")
    major = uint32(version.get("major"), "version.major")
    minor = uint32(version.get("minor", 0), "version.minor", allow_zero=True)
    source_records = source.get("schemas", [])
    source_rpcs = source.get("rpcs", [])
    source_topics = source.get("topics", [])
    source_enums = source.get("enums", [])
    _unique_by_name(source_records, "record")
    _unique_by_name(source_rpcs + source_topics, "endpoint")

    if previous is not None:
        reject_unknown_properties(
            previous,
            ("format", "api", "version", "enums", "schemas", "rpcs", "topics"),
            "interface lock",
        )
        if previous.get("format") != LOCK_FORMAT:
            raise ApiDefinitionError(
                f"interface lock format must be {LOCK_FORMAT}, found {previous.get('format')!r}"
            )
        previous_api = api_key(previous.get("api"), "interface lock api")
        if previous_api != api_id:
            raise ApiDefinitionError(
                f"interface lock API ID {previous_api!r} does not match {api_id!r}"
            )
        previous_version = previous.get("version", {})
        previous_major = uint32(previous_version.get("major"), "interface lock version.major")
        previous_minor = uint32(
            previous_version.get("minor", 0), "interface lock version.minor", allow_zero=True
        )
        if major < previous_major:
            raise ApiDefinitionError(
                f"{api_id} major version cannot decrease ({previous_major} -> {major})"
            )
        if major > previous_major:
            previous = None
        elif minor < previous_minor:
            raise ApiDefinitionError(
                f"{api_id} minor version cannot decrease within major {major}"
            )

    if previous is None:
        schemas = []
        for record in source_records:
            schemas.append({
                "name": record["name"],
                "fields": [
                    {"id": field_id, **field, "reserved": False}
                    for field_id, field in enumerate(record.get("fields", []), 1)
                ],
            })
    else:
        _validate_enum_evolution(previous.get("enums", []), source_enums)
        old_records = _unique_by_name(previous.get("schemas", []), "ABI record")
        new_record_names = {record["name"] for record in source_records}
        missing_records = sorted(set(old_records).difference(new_record_names))
        if missing_records:
            raise ApiDefinitionError(
                f"record {missing_records[0]} was removed or renamed; bump the API major version"
            )
        schemas = []
        for source_record in source_records:
            record_name = source_record["name"]
            old_record = old_records.get(record_name)
            if old_record is None:
                fields = [
                    {"id": field_id, **field, "reserved": False}
                    for field_id, field in enumerate(source_record.get("fields", []), 1)
                ]
                schemas.append({"name": record_name, "fields": fields})
                continue

            old_fields = _unique_by_name(old_record.get("fields", []), f"field in {record_name}")
            source_fields = _unique_by_name(
                source_record.get("fields", []), f"field in {record_name}"
            )
            next_field_id = max((uint32(field.get("id"), f"field {record_name} ID")
                                 for field in old_record.get("fields", [])), default=0) + 1
            fields = []
            for source_field in source_record.get("fields", []):
                field_name = source_field["name"]
                old_field = old_fields.get(field_name)
                if old_field is not None:
                    if old_field.get("reserved", False):
                        raise ApiDefinitionError(
                            f"field {record_name}.{field_name} is reserved and cannot be reused"
                        )
                    for property_name in ("type", "optional"):
                        if source_field.get(property_name) != old_field.get(property_name):
                            raise ApiDefinitionError(
                                f"field {record_name}.{field_name} changed {property_name} incompatibly"
                            )
                    fields.append({
                        "id": uint32(old_field.get("id"), f"field {record_name}.{field_name} ID"),
                        **source_field,
                        "reserved": False,
                    })
                else:
                    if not source_field.get("optional", False):
                        raise ApiDefinitionError(
                            f"new field {record_name}.{field_name} must be optional in major {major}"
                        )
                    fields.append({"id": next_field_id, **source_field, "reserved": False})
                    next_field_id += 1
            for old_field in old_record.get("fields", []):
                if old_field["name"] in source_fields:
                    continue
                if not old_field.get("reserved", False) and not old_field.get("optional", False):
                    raise ApiDefinitionError(
                        f"required field {record_name}.{old_field['name']} cannot be removed"
                    )
                fields.append({**old_field, "reserved": True})
            if len(fields) > 64:
                raise ApiDefinitionError(
                    f"record {record_name} has exhausted its 64 permanent field IDs"
                )
            schemas.append({
                "name": record_name,
                "fields": sorted(fields, key=lambda field: field["id"]),
            })

        for kind in ("rpcs", "topics"):
            old_endpoints = _unique_by_name(previous.get(kind, []), kind[:-1])
            new_endpoints = _unique_by_name(source.get(kind, []), kind[:-1])
            for endpoint_name, old_endpoint in old_endpoints.items():
                if new_endpoints.get(endpoint_name) != old_endpoint:
                    raise ApiDefinitionError(
                        f"{kind[:-1]} {endpoint_name} was removed or changed incompatibly"
                    )

    snapshot = {
        "format": LOCK_FORMAT,
        "api": api_id,
        "version": {"major": major, "minor": minor},
        "enums": [
            {
                **enum,
                "values": sorted(
                    enum.get("values", []),
                    key=lambda value: (value["value"], value["name"]),
                ),
            }
            for enum in sorted(source_enums, key=lambda enum: enum["name"])
        ],
        "schemas": sorted(schemas, key=lambda record: record["name"]),
        "rpcs": sorted(source_rpcs, key=lambda endpoint: endpoint["name"]),
        "topics": sorted(source_topics, key=lambda endpoint: endpoint["name"]),
    }
    if previous is not None:
        old_source = _active_source_from_lock(previous)
        new_source = _active_source_from_lock(snapshot)
        if (_source_signature(old_source) != _source_signature(new_source)
                and minor == previous["version"].get("minor", 0)):
            raise ApiDefinitionError(
                f"{api_id} changed within major {major} without increasing its minor version"
            )
    return snapshot


def resolved_definition(snapshot: dict[str, Any]) -> ApiDefinition:
    schema_ids = {
        record["name"]: record_id
        for record_id, record in enumerate(snapshot["schemas"], 1)
    }

    def schema_id(name: str | None, role: str) -> int:
        if name is None:
            return 0
        result = schema_ids.get(name)
        if result is None:
            raise ApiDefinitionError(f"unknown {role} record {name!r}")
        return result

    raw = {
        "api": snapshot["api"],
        "version": snapshot["version"],
        "enums": snapshot.get("enums", []),
        "schemas": [
            {
                "id": record_id,
                "name": record["name"],
                "fields": [
                    {name: field[name] for name in ("id", "name", "type", "optional")}
                    for field in record.get("fields", []) if not field.get("reserved", False)
                ],
            }
            for record_id, record in enumerate(snapshot["schemas"], 1)
        ],
        "rpcs": [
            {
                "name": rpc["name"],
                "request": schema_id(rpc.get("request"), "request"),
                "response": schema_id(rpc.get("response"), "response"),
            }
            for rpc in snapshot.get("rpcs", [])
        ],
        "topics": [
            {"name": topic["name"], "message": schema_id(topic.get("message"), "message")}
            for topic in snapshot.get("topics", [])
        ],
    }
    return parse_api_definition(raw)


def emit_interface_lock(snapshot: dict[str, Any]) -> str:
    return json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n"


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
IMC_LENGTH_DELIMITED_PAYLOAD_SIZES = {
    "object": 12,
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
    lines.append(f"inline std::size_t Encoded{name}Size(const {value} &value) noexcept {{")
    lines.append("    std::size_t size = 0;")
    for field in record.fields:
        member = camel(field.name)
        field_id = f"{name}Field::{member}"
        wire_type = imc_field_wire_type(api, field.type)
        if wire_type == "array<string>":
            add_size = f"::BML::Imc::Wire::AddStringArrayFieldSize(size, {field_id}, value.{member})"
        elif wire_type in IMC_ARRAY_ELEMENT_SIZES:
            add_size = (
                f"::BML::Imc::Wire::AddFixedArrayFieldSize(size, {field_id}, value.{member}.size(), "
                f"{IMC_ARRAY_ELEMENT_SIZES[wire_type]})"
            )
        elif wire_type == "bool":
            add_size = f"::BML::Imc::Wire::AddBoolFieldSize(size, {field_id})"
        elif wire_type in {"int", "float"}:
            add_size = f"::BML::Imc::Wire::AddFixed32FieldSize(size, {field_id})"
        elif wire_type in {"int64", "uint64", "double"}:
            add_size = f"::BML::Imc::Wire::AddFixed64FieldSize(size, {field_id})"
        else:
            payload = (f"value.{member}.size()" if wire_type in {"string", "bytes"}
                       else str(IMC_LENGTH_DELIMITED_PAYLOAD_SIZES[wire_type]))
            add_size = f"::BML::Imc::Wire::AddLengthDelimitedFieldSize(size, {field_id}, {payload})"
        condition = f"value.Has{member} && " if field.optional else ""
        lines.append(f"    if ({condition}!{add_size}) return 0;")
    lines.extend(["    return size;", "}", ""])
    lines.append(f"[[nodiscard]] inline int Encode{name}(const {value} &value, void *data, std::size_t size) noexcept {{")
    lines.append(f"    if (size != Encoded{name}Size(value)) return BML_ERROR_INVALID_PARAMETER;")
    lines.append("    ::BML::Imc::Wire::Writer writer(data, size);")
    lines.append("    int status = writer.Begin();")
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
    lines.append(f"[[nodiscard]] inline int Decode{name}(const BML_ImcMessage &message, {value} &out) {{")
    lines.append("    if (message.Size < sizeof(BML_ImcMessage) || (message.DataSize && !message.Data)) return BML_ERROR_INVALID_PARAMETER;")
    lines.append("    ::BML::Imc::Wire::Reader reader(message.Data, message.DataSize);")
    lines.append("    int status = reader.Begin();")
    lines.append("    if (status != BML_OK) return status;")

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
        if endpoint.kind != "topic":
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
            "    [[nodiscard]] int Open(BML_ImcClient client, BML_ImcTopicId topic, BML_ImcPayloadTypeId payload,",
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
            "    [[nodiscard]] int Close() noexcept {",
            "        if (!m_Subscription) return BML_OK;",
            "        const int status = BML_Imc_Unsubscribe(m_Client, m_Subscription);",
            "        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) Reset();",
            "        return status;", "    }",
            "    bool IsOpen() const noexcept { return m_Subscription != nullptr; }",
            "    [[nodiscard]] int DroppedCount(std::uint64_t &out) const noexcept {",
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
        if endpoint.kind == "topic":
            continue
        name = camel(endpoint.name)
        if endpoint.output_schema:
            output_value = value_name(schemas[endpoint.output_schema])
            lines.append(f"    using {name}Future = ::BML::Imc::RpcFuture<{output_value}>;")
        else:
            lines.append(f"    using {name}Future = ::BML::Imc::RpcFuture<void>;")
    lines.extend([
        "",
        "    [[nodiscard]] int Open(const char *ownerId = nullptr) noexcept {",
        "        const int closeStatus = Close(); if (m_Client) return closeStatus;",
        "        BML_ImcClient client = nullptr;",
        "        const int status = BML_Imc_OpenClient(ownerId, &client);",
        "        return status == BML_OK ? Adopt(client) : status;", "    }",
        "    [[nodiscard]] int Adopt(BML_ImcClient client) noexcept {",
        "        const int closeStatus = Close(); if (m_Client) return closeStatus;",
        "        if (!client) return BML_ERROR_INVALID_PARAMETER;",
        "        m_Client = client;", "        int status = BML_OK;",
    ])
    for record in api.schemas:
        name = camel(record.name)
        lines.append(f"        if (status == BML_OK) status = BML_Imc_GetPayloadTypeId(m_Client, {name}Payload, &m_{name}Payload);")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind == "topic":
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetTopicId(m_Client, {name}Route, &m_{name}Topic);")
        else:
            lines.append(f"        if (status == BML_OK) status = BML_Imc_GetRpcId(m_Client, {name}Route, &m_{name}Rpc);")
    lines.extend([
        "        if (status != BML_OK) (void)Close();", "        return status;", "    }",
        "    [[nodiscard]] int Close() noexcept {", "        if (!m_Client) return BML_OK;",
        "        const int status = BML_Imc_CloseClient(m_Client);",
        "        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) { m_Client = nullptr; ResetIds(); }",
        "        return status;", "    }",
        "    BML_ImcClient Handle() const noexcept { return m_Client; }",
        "    bool IsOpen() const noexcept { return m_Client != nullptr; }",
        "    [[nodiscard]] int EnsureOpen(const char *ownerId = nullptr) noexcept { return m_Client ? BML_OK : Open(ownerId); }",
    ])
    for record in api.schemas:
        name = camel(record.name)
        lines.append(f"    BML_ImcPayloadTypeId {name}PayloadType() const noexcept {{ return m_{name}Payload; }}")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind == "topic":
            lines.append(f"    BML_ImcTopicId {name}TopicId() const noexcept {{ return m_{name}Topic; }}")
        else:
            lines.append(f"    BML_ImcRpcId {name}RpcId() const noexcept {{ return m_{name}Rpc; }}")
        if endpoint.kind == "rpc":
            lines.extend([
                f"    [[nodiscard]] int Is{name}Available(bool &out) const noexcept {{",
                "        int available = 0;",
                f"        const int status = BML_Imc_IsRpcAvailable(m_Client, m_{name}Rpc, &available);",
                "        if (status == BML_OK) out = available != 0;",
                "        return status;", "    }",
            ])
    lines.append("")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind == "rpc":
            output = schemas[endpoint.output_schema] if endpoint.output_schema else None
            output_name = camel(output.name) if output else ""
            output_value = value_name(output) if output else ""
            if endpoint.input_schema:
                input_record = schemas[endpoint.input_schema]
                input_name = camel(input_record.name)
                input_value = value_name(input_record)
                lines.extend([
                    f"    [[nodiscard]] int BeginCall{name}(const {input_value} &input, {name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                    "        ::BML::Imc::MessageBuffer buffer; BML_ImcMessage request{};",
                    f"        int status = ::BML::Imc::EncodeMessage(input, m_{input_name}Payload, buffer, request, Encoded{input_name}Size, Encode{input_name});",
                ])
                if output:
                    lines.append(f"        return status == BML_OK ? ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, &request, m_{output_name}Payload, out, Decode{output_name}, timeoutMs) : status;")
                    call_signature = f"const {input_value} &input, {output_value} &out, std::uint32_t timeoutMs = 5000u"
                    await_expression = "future.AwaitResult(out, timeoutMs)"
                else:
                    lines.append(f"        return status == BML_OK ? ::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, &request, out, timeoutMs) : status;")
                    call_signature = f"const {input_value} &input, std::uint32_t timeoutMs = 5000u"
                    await_expression = "future.AwaitResult(timeoutMs)"
                lines.extend([
                    "    }",
                    f"    [[nodiscard]] int Call{name}({call_signature}) {{",
                    f"        {name}Future future; int status = BeginCall{name}(input, future, timeoutMs);",
                    f"        return status == BML_OK ? {await_expression} : status;", "    }",
                ])
            else:
                if output:
                    begin_expression = f"::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, nullptr, m_{output_name}Payload, out, Decode{output_name}, timeoutMs)"
                    call_signature = f"{output_value} &out, std::uint32_t timeoutMs = 5000u"
                    await_expression = "future.AwaitResult(out, timeoutMs)"
                else:
                    begin_expression = f"::BML::Imc::BeginRpc(m_Client, m_{name}Rpc, nullptr, out, timeoutMs)"
                    call_signature = "std::uint32_t timeoutMs = 5000u"
                    await_expression = "future.AwaitResult(timeoutMs)"
                lines.extend([
                    f"    [[nodiscard]] int BeginCall{name}({name}Future &out, std::uint32_t timeoutMs = 5000u) noexcept {{",
                    f"        return {begin_expression};", "    }",
                    f"    [[nodiscard]] int Call{name}({call_signature}) {{",
                    f"        {name}Future future; int status = BeginCall{name}(future, timeoutMs);",
                    f"        return status == BML_OK ? {await_expression} : status;", "    }",
                ])
        elif endpoint.kind == "topic":
            output = schemas[endpoint.output_schema]
            output_name = camel(output.name)
            output_value = value_name(output)
            lines.extend([
                f"    [[nodiscard]] int Publish{name}(const {output_value} &value, std::size_t *outDelivered = nullptr) noexcept {{",
                f"        return ::BML::Imc::Publish(m_Client, m_{name}Topic, m_{output_name}Payload, value, Encoded{output_name}Size, Encode{output_name}, outDelivered);", "    }",
                f"    [[nodiscard]] int Get{name}SubscriberCount(std::size_t &outCount) const noexcept {{",
                f"        return BML_Imc_GetTopicSubscriberCount(m_Client, m_{name}Topic, &outCount);", "    }",
                f"    [[nodiscard]] int Subscribe{name}({name}Subscription &out, {name}Subscription::Handler handler, void *userdata = nullptr, std::uint32_t capacity = 256u,",
                "                     BML_ImcBackpressure backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST, BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {",
                f"        return out.Open(m_Client, m_{name}Topic, m_{output_name}Payload, handler, userdata, capacity, backpressure, execution);", "    }",
            ])
    lines.extend(["private:", "    void ResetIds() noexcept {"])
    for record in api.schemas:
        lines.append(f"        m_{camel(record.name)}Payload = BML_IMC_INVALID_ID;")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        lines.append(f"        m_{name}{'Topic' if endpoint.kind == 'topic' else 'Rpc'} = BML_IMC_INVALID_ID;")
    lines.extend(["    }", "    BML_ImcClient m_Client = nullptr;"])
    for record in api.schemas:
        lines.append(f"    BML_ImcPayloadTypeId m_{camel(record.name)}Payload = BML_IMC_INVALID_ID;")
    for endpoint in api.endpoints:
        name = camel(endpoint.name)
        if endpoint.kind == "topic":
            lines.append(f"    BML_ImcTopicId m_{name}Topic = BML_IMC_INVALID_ID;")
        else:
            lines.append(f"    BML_ImcRpcId m_{name}Rpc = BML_IMC_INVALID_ID;")
    lines.extend(["};", ""])


def append_imc_provider(lines: list[str], api: ApiDefinition) -> None:
    schemas = {record.id: record for record in api.schemas}
    rpc_endpoints = [endpoint for endpoint in api.endpoints if endpoint.kind == "rpc"]
    if not rpc_endpoints:
        return
    lines.extend([
        "class Provider {", "public:", "    Provider() = default;", "    ~Provider() { (void)Close(); }",
        "    Provider(const Provider &) = delete;", "    Provider &operator=(const Provider &) = delete;",
    ])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        output_value = value_name(schemas[endpoint.output_schema]) if endpoint.output_schema else ""
        input_value = value_name(schemas[endpoint.input_schema]) if endpoint.input_schema else ""
        if input_value and output_value:
            signature = f"int (*)(const {input_value} &, {output_value} &, void *)"
        elif input_value:
            signature = f"int (*)(const {input_value} &, void *)"
        elif output_value:
            signature = f"int (*)({output_value} &, void *)"
        else:
            signature = "int (*)(void *)"
        lines.append(f"    using {name}Handler = {signature};")
    lines.extend(["", "    struct Handlers {", "        void *Userdata = nullptr;",
                  "        BML_ImcExecution Execution = BML_IMC_EXECUTION_GAME_THREAD;"])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        lines.append(f"        {name}Handler {name} = nullptr;")
    lines.extend([
        "    };", "",
        "    [[nodiscard]] int Open(const char *ownerId = nullptr) noexcept { const int status = Close(); return m_Transport.IsOpen() ? status : m_Transport.Open(ownerId); }",
        "    [[nodiscard]] int Close() noexcept { const int status = m_Transport.Close(); if (!m_Transport.IsOpen()) ResetSlots(); return status; }",
        "    bool IsOpen() const noexcept { return m_Transport.IsOpen(); }",
        "    Client &Transport() noexcept { return m_Transport; }",
        "    const Client &Transport() const noexcept { return m_Transport; }", "",
        "    [[nodiscard]] int Start(const Handlers &handlers, const char *ownerId = nullptr) noexcept {",
        "        if (IsOpen()) return BML_ERROR_ALREADY_EXISTS;",
        f"        if (!({' || '.join(f'handlers.{camel(endpoint.name)}' for endpoint in rpc_endpoints)})) return BML_ERROR_INVALID_PARAMETER;",
        "        if (handlers.Execution != BML_IMC_EXECUTION_GAME_THREAD && handlers.Execution != BML_IMC_EXECUTION_CALLER_THREAD) return BML_ERROR_INVALID_PARAMETER;",
        "        int status = m_Transport.Open(ownerId);",
    ])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        lines.append(f"        if (status == BML_OK && handlers.{name}) status = Register{name}(handlers.{name}, handlers.Userdata, handlers.Execution);")
    lines.extend([
        "        if (status == BML_OK) return BML_OK;",
        "        const int cleanupStatus = Close();",
        "        return cleanupStatus == BML_OK || cleanupStatus == BML_ERROR_INVALID_HANDLE ? status : cleanupStatus;",
        "    }", "",
    ])
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        lines.extend([
            f"    [[nodiscard]] int Register{name}({name}Handler handler, void *userdata = nullptr, BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {{",
            "        if (!m_Transport.Handle() || !handler) return BML_ERROR_INVALID_PARAMETER;",
            f"        if (m_{name}.Registered) return BML_ERROR_ALREADY_EXISTS;",
            f"        m_{name} = {{this, handler, userdata, false}};",
            "        BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT; options.Execution = execution;",
            f"        const int status = BML_Imc_RegisterRpc(m_Transport.Handle(), m_Transport.{name}RpcId(), &options, &{name}Thunk, &m_{name});",
            f"        if (status == BML_OK) m_{name}.Registered = true; else m_{name} = {{}};", "        return status;", "    }",
            f"    [[nodiscard]] int Unregister{name}() noexcept {{", f"        if (!m_{name}.Registered) return BML_ERROR_NOT_FOUND;",
            f"        const int status = BML_Imc_UnregisterRpc(m_Transport.Handle(), m_Transport.{name}RpcId());",
            f"        if (status == BML_OK) m_{name} = {{}};", "        return status;", "    }", "",
        ])
    lines.append("private:")
    for endpoint in rpc_endpoints:
        name = camel(endpoint.name)
        output = schemas[endpoint.output_schema] if endpoint.output_schema else None
        output_name = camel(output.name) if output else ""
        output_value = value_name(output) if output else ""
        lines.append(f"    struct {name}Slot {{ Provider *Owner = nullptr; {name}Handler Function = nullptr; void *Userdata = nullptr; bool Registered = false; }};")
        lines.extend([
            f"    [[nodiscard]] static int {name}Thunk(BML_ImcRpcId, const BML_ImcMessage *request, BML_ImcResponse *{'response' if output else ''}, void *userdata) noexcept {{",
            f"        auto *slot = static_cast<{name}Slot *>(userdata);",
            "        if (!slot || !slot->Owner || !slot->Function) return BML_ERROR_INVALID_PARAMETER;",
            "        const auto function = slot->Function; void *handlerUserdata = slot->Userdata;",
        ])
        if endpoint.input_schema or output:
            lines.append("        auto *owner = slot->Owner;")
        lines.append("        try {")
        if output:
            lines.append(f"            const BML_ImcPayloadTypeId responsePayload = owner->m_Transport.{output_name}PayloadType();")
        if endpoint.input_schema:
            input_record = schemas[endpoint.input_schema]
            input_name = camel(input_record.name)
            input_value = value_name(input_record)
            lines.extend([
                "            if (!request || request->Size < sizeof(BML_ImcMessage)) return BML_ERROR_MALFORMED_MESSAGE;",
                f"            if (request->PayloadType != owner->m_Transport.{input_name}PayloadType()) return BML_ERROR_TYPE_MISMATCH;",
                f"            {input_value} input{{}}; int status = Decode{input_name}(*request, input);",
                "            if (status != BML_OK) return status;",
            ])
            if output:
                lines.append(f"            {output_value} output{{}}; status = function(input, output, handlerUserdata);")
            else:
                lines.append("            status = function(input, handlerUserdata);")
        else:
            lines.extend([
                "            if (request && (request->Size < sizeof(BML_ImcMessage) || request->DataSize != 0)) return BML_ERROR_MALFORMED_MESSAGE;",
            ])
            if output:
                lines.append(f"            {output_value} output{{}}; const int status = function(output, handlerUserdata);")
            else:
                lines.append("            const int status = function(handlerUserdata);")
        lines.append("            if (status != BML_OK) return status;")
        if output:
            lines.append(f"            return ::BML::Imc::WriteResponse(response, responsePayload, output, Encoded{output_name}Size, Encode{output_name});")
        else:
            lines.append("            return BML_OK;")
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

    for endpoint in api.endpoints:
        endpoint_name = camel(endpoint.name)
        route_kind = endpoint.kind
        lines.append(
            f'inline constexpr const char {endpoint_name}Route[] = '
            f'"{api.api_id}/v{api.major}/{route_kind}/{endpoint.name}";'
        )
    lines.append("")
    append_imc_client(lines, api)
    append_imc_provider(lines, api)
    lines.extend([f"}} // namespace BML::Imc::Generated::{namespace}", ""])
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


def load_imc_source(path: Path, role: str) -> dict[str, Any]:
    try:
        return parse_imc(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ApiDefinitionError) as error:
        raise ApiDefinitionError(f"{role} {path}: {error}") from error


def load_interface_lock(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise ApiDefinitionError(
            f"interface lock {path} does not exist; run with --update-lock once"
        ) from error
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ApiDefinitionError(f"interface lock {path}: {error}") from error
    if not isinstance(value, dict):
        raise ApiDefinitionError(f"interface lock {path}: top-level value must be an object")
    return value


def update_lock_instruction(command: str | None) -> str:
    if command:
        return f"run {command}"
    return "run with --update-lock once"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, required=True,
                        help=".imc interface input")
    parser.add_argument("--out-dir", type=Path, required=True,
                        help="directory for generated *_imc.hpp headers")
    parser.add_argument(
        "--update-lock", action="store_true",
        help="create or update each adjacent .imc.lock snapshot before generation",
    )
    parser.add_argument(
        "--update-lock-command",
        help="command shown when an interface lock is missing or stale",
    )
    parser.add_argument("--expected-api-id",
                        help="require a single input to declare this API ID (used by CMake output tracking)")
    parser.add_argument("--check", action="store_true", help="fail instead of rewriting stale output")
    args = parser.parse_args(argv)
    try:
        if args.update_lock and args.check:
            raise ApiDefinitionError("--update-lock and --check cannot be used together")
        api_definitions: list[ApiDefinition] = []
        lock_outputs: list[tuple[Path, str]] = []
        for input_path in args.input:
            if input_path.suffix != ".imc":
                raise ApiDefinitionError(f"input must use the .imc extension: {input_path}")
            source = load_imc_source(input_path, "input")
            lock_path = input_path.with_suffix(".imc.lock")
            previous = None
            if lock_path.exists():
                previous = load_interface_lock(lock_path)
            elif not args.update_lock:
                raise ApiDefinitionError(
                    f"interface lock {lock_path} does not exist; "
                    f"{update_lock_instruction(args.update_lock_command)}"
                )
            snapshot = build_interface_lock(source, previous)
            snapshot_text = emit_interface_lock(snapshot)
            if not args.update_lock and (
                previous is None or emit_interface_lock(previous) != snapshot_text
            ):
                raise ApiDefinitionError(
                    f"interface lock is stale: {lock_path}; review the change and "
                    f"{update_lock_instruction(args.update_lock_command)}"
                )
            api_definitions.append(resolved_definition(snapshot))
            lock_outputs.append((lock_path, snapshot_text))
        if args.expected_api_id is not None:
            expected_api_id = api_key(args.expected_api_id, "--expected-api-id")
            if len(api_definitions) != 1:
                raise ApiDefinitionError("--expected-api-id requires exactly one --input")
            actual_api_id = api_definitions[0].api_id
            if actual_api_id != expected_api_id:
                raise ApiDefinitionError(
                    f"input {args.input[0]}: interface API ID {actual_api_id!r} does not match "
                    f"expected API ID {expected_api_id!r}"
                )
        seen_api_ids: set[str] = set()
        seen_output_paths: dict[str, str] = {}
        planned_outputs: list[tuple[Path, str]] = []
        for api in api_definitions:
            if api.api_id in seen_api_ids:
                raise ApiDefinitionError(f"duplicate API ID input: {api.api_id}")
            seen_api_ids.add(api.api_id)
            stem = api.api_id.replace(".", "_")
            output_path = args.out_dir / f"{stem}_imc.hpp"
            path_key = str(output_path.resolve()).casefold()
            previous_api_id = seen_output_paths.get(path_key)
            if previous_api_id is not None:
                raise ApiDefinitionError(
                    f"generated output path collision: API IDs {previous_api_id!r} "
                    f"and {api.api_id!r} both map to {output_path}"
                )
            seen_output_paths[path_key] = api.api_id
            planned_outputs.append((output_path, emit_imc_header(api)))

        # No author-owned interface state changes until every input and generated
        # output has validated successfully.
        if args.update_lock:
            for lock_path, snapshot_text in lock_outputs:
                write_if_changed(lock_path, snapshot_text, False)
        for output_path, output_text in planned_outputs:
            write_if_changed(output_path, output_text, args.check)
    except (OSError, UnicodeError, ApiDefinitionError) as error:
        print(f"imc_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
