#!/usr/bin/env python3
"""Black-box checks for the deterministic IMC API generator."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def api_definition(*, minor: int = 0, extra_field: dict | None = None) -> dict:
    fields = [{"id": 1, "name": "value", "type": "int", "optional": False}]
    if extra_field is not None:
        fields.append(extra_field)
    return {
        "api": "test.codegen",
        "version": {"major": 1, "minor": minor},
        "schemas": [
            {"id": 1, "name": "sample", "fields": fields},
            {"id": 2, "name": "request", "fields": [{"id": 1, "name": "name", "type": "string", "optional": False}]},
        ],
        "endpoints": [
            {"name": "state", "kind": "resource", "output": 1},
            {"name": "lookup", "kind": "query", "input": 2, "output": 1},
            {"name": "entity", "kind": "component", "output": 1},
            {"name": "items", "kind": "collection", "output": 1},
        ],
    }


def write(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def run(generator: Path, *arguments: str, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [sys.executable, str(generator), *arguments],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if (result.returncode == 0) != expect_success:
        raise AssertionError(
            f"generator {'unexpectedly failed' if expect_success else 'unexpectedly succeeded'}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    generator = args.source_root / "tools" / "imc_codegen.py"

    with tempfile.TemporaryDirectory(prefix="bml-imc-codegen-") as temporary:
        root = Path(temporary)
        base = root / "base.bmlapi"
        current = root / "current.bmlapi"
        next_contract = root / "next.bmlapi"
        wrong_wire = root / "wrong-wire.bmlapi"
        incompatible = root / "incompatible.bmlapi"
        write(base, api_definition())
        generated = root / "generated"
        common = ("--out-dir", str(generated))
        run(generator, *common, "--input", str(base),
            "--expected-api-id", "test.codegen")

        expected_id_result = run(
            generator, "--out-dir", str(root / "wrong-api-id"),
            "--input", str(base), "--expected-api-id", "wrong.codegen",
            expect_success=False,
        )
        if (base.name not in expected_id_result.stderr
                or "contract API ID 'test.codegen' does not match expected API ID 'wrong.codegen'"
                not in expected_id_result.stderr):
            raise AssertionError("expected API ID mismatch did not identify the contract and both IDs")

        malformed = root / "malformed.bmlapi"
        malformed.write_text("{\n", encoding="utf-8")
        malformed_result = run(
            generator, "--out-dir", str(root / "malformed"),
            "--input", str(malformed), expect_success=False,
        )
        if malformed.name not in malformed_result.stderr or "line 2 column 1" not in malformed_result.stderr:
            raise AssertionError("malformed JSON diagnostic did not include its input path and location")

        duplicate_property = root / "duplicate-property.bmlapi"
        duplicate_property.write_text(
            """{
  "api": "test.duplicate",
  "version": {"major": 1, "minor": 0},
  "schemas": [{"id": 1, "name": "sample", "fields": [
    {"id": 1, "name": "value", "type": "int", "optional": true, "optional": false}
  ]}],
  "rpcs": [{"name": "get", "response": 1}]
}
""",
            encoding="utf-8",
        )
        duplicate_property_result = run(
            generator, "--out-dir", str(root / "duplicate-property"),
            "--input", str(duplicate_property), expect_success=False,
        )
        if (duplicate_property.name not in duplicate_property_result.stderr
                or "duplicate JSON property 'optional'" not in duplicate_property_result.stderr):
            raise AssertionError("duplicate JSON property was not rejected clearly")

        leading_digit = api_definition()
        leading_digit["api"] = "1test.2codegen"
        leading_digit["enums"] = [{
            "name": "1mode",
            "values": [{"name": "1off", "value": 0}],
        }]
        leading_digit["schemas"][0]["name"] = "1sample"
        leading_digit["schemas"][0]["fields"][0] = {
            "id": 1, "name": "1value", "type": "enum<1mode>", "optional": False,
        }
        leading_digit["endpoints"][0]["name"] = "1state"
        leading_digit_contract = root / "leading-digit.bmlapi"
        leading_digit_output = root / "leading-digit"
        write(leading_digit_contract, leading_digit)
        run(
            generator, "--out-dir", str(leading_digit_output),
            "--input", str(leading_digit_contract),
            "--expected-api-id", "1test.2codegen",
        )
        leading_digit_header = (
            leading_digit_output / "1test_2codegen_imc.hpp"
        ).read_text(encoding="utf-8")
        for snippet in (
            "namespace BML::Imc::Generated::_1test::_2codegen",
            "enum class _1mode", "_1off = 0", "struct _1sampleValue",
            "_1mode _1value{}", "_1stateRoute", "Read_1state",
        ):
            if snippet not in leading_digit_header:
                raise AssertionError(
                    f"leading-digit contract name did not become valid C++: {snippet}"
                )

        punctuation_name = api_definition()
        punctuation_name["schemas"][0]["fields"][0]["name"] = "---"
        punctuation_contract = root / "punctuation-name.bmlapi"
        write(punctuation_contract, punctuation_name)
        punctuation_result = run(
            generator, "--out-dir", str(root / "punctuation-name"),
            "--input", str(punctuation_contract), expect_success=False,
        )
        if (punctuation_contract.name not in punctuation_result.stderr
                or "schemas[0].fields[0].name must contain at least one ASCII letter or digit"
                not in punctuation_result.stderr):
            raise AssertionError("punctuation-only contract name was not rejected clearly")

        empty_api_segment = api_definition()
        empty_api_segment["api"] = "test..codegen"
        empty_api_segment_contract = root / "empty-api-segment.bmlapi"
        write(empty_api_segment_contract, empty_api_segment)
        empty_api_segment_result = run(
            generator, "--out-dir", str(root / "empty-api-segment"),
            "--input", str(empty_api_segment_contract), expect_success=False,
        )
        if (empty_api_segment_contract.name not in empty_api_segment_result.stderr
                or "api must use non-empty dot-separated segments" not in empty_api_segment_result.stderr):
            raise AssertionError("empty API ID segment was not rejected clearly")

        for label, invalid_api_id in (
            ("underscore", "test_codegen"),
            ("hyphen", "test-codegen"),
            ("uppercase", "Test.codegen"),
        ):
            invalid_api = api_definition()
            invalid_api["api"] = invalid_api_id
            invalid_api_contract = root / f"invalid-api-{label}.bmlapi"
            write(invalid_api_contract, invalid_api)
            invalid_api_result = run(
                generator, "--out-dir", str(root / f"invalid-api-{label}"),
                "--input", str(invalid_api_contract), expect_success=False,
            )
            if (invalid_api_contract.name not in invalid_api_result.stderr
                    or "lowercase ASCII letters and digits" not in invalid_api_result.stderr):
                raise AssertionError(
                    f"non-canonical API ID {invalid_api_id!r} was not rejected clearly"
                )

        duplicate_api = api_definition(minor=1)
        duplicate_api_contract = root / "duplicate-api.bmlapi"
        duplicate_api_output = root / "duplicate-api-output"
        duplicate_api_output.mkdir()
        duplicate_api_sentinel = duplicate_api_output / "test_codegen_imc.hpp"
        duplicate_api_sentinel.write_text("sentinel\n", encoding="utf-8")
        write(duplicate_api_contract, duplicate_api)
        duplicate_api_result = run(
            generator, "--out-dir", str(duplicate_api_output),
            "--input", str(base), "--input", str(duplicate_api_contract),
            expect_success=False,
        )
        if "duplicate API ID input: test.codegen" not in duplicate_api_result.stderr:
            raise AssertionError("duplicate API inputs were not rejected before generation")
        if duplicate_api_sentinel.read_text(encoding="utf-8") != "sentinel\n":
            raise AssertionError("failed duplicate-input generation modified an output")

        unknown_property_cases: list[tuple[str, dict, str]] = []

        def add_unknown_property_case(label: str, contract_value: dict,
                                      target: dict, context: str) -> None:
            target["typo_property"] = True
            unknown_property_cases.append((label, contract_value, context))

        unknown_top_level = api_definition()
        add_unknown_property_case(
            "top-level", unknown_top_level, unknown_top_level, "top-level object"
        )
        unknown_version = api_definition()
        add_unknown_property_case(
            "version", unknown_version, unknown_version["version"], "version"
        )
        unknown_enum = api_definition()
        unknown_enum["enums"] = [{
            "name": "mode", "values": [{"name": "off", "value": 0}],
        }]
        add_unknown_property_case(
            "enum", unknown_enum, unknown_enum["enums"][0], "enums[0]"
        )
        unknown_enum_value = api_definition()
        unknown_enum_value["enums"] = [{
            "name": "mode", "values": [{"name": "off", "value": 0}],
        }]
        add_unknown_property_case(
            "enum-value", unknown_enum_value,
            unknown_enum_value["enums"][0]["values"][0], "enums[0].values[0]",
        )
        unknown_schema = api_definition()
        add_unknown_property_case(
            "schema", unknown_schema, unknown_schema["schemas"][0], "schemas[0]"
        )
        unknown_field = api_definition()
        add_unknown_property_case(
            "field", unknown_field, unknown_field["schemas"][0]["fields"][0],
            "schemas[0].fields[0]",
        )
        unknown_endpoint = api_definition()
        add_unknown_property_case(
            "endpoint", unknown_endpoint, unknown_endpoint["endpoints"][0],
            "endpoints[0]",
        )
        unknown_rpc = api_definition()
        unknown_rpc.pop("endpoints")
        unknown_rpc["rpcs"] = [{"name": "state", "response": 1}]
        add_unknown_property_case(
            "rpc", unknown_rpc, unknown_rpc["rpcs"][0], "rpcs[0]"
        )
        unknown_topic = api_definition()
        unknown_topic.pop("endpoints")
        unknown_topic["topics"] = [{"name": "changed", "message": 1}]
        add_unknown_property_case(
            "topic", unknown_topic, unknown_topic["topics"][0], "topics[0]"
        )

        for label, contract_value, context in unknown_property_cases:
            unknown_contract = root / f"unknown-{label}.bmlapi"
            write(unknown_contract, contract_value)
            unknown_result = run(
                generator, "--out-dir", str(root / f"unknown-{label}"),
                "--input", str(unknown_contract), expect_success=False,
            )
            expected = f"{context} has unknown property: 'typo_property'"
            if (unknown_contract.name not in unknown_result.stderr
                    or expected not in unknown_result.stderr):
                raise AssertionError(
                    f"unknown property in {context} was not rejected clearly"
                )

        imc_header = generated / "test_codegen_imc.hpp"
        if not imc_header.exists():
            raise AssertionError("generator did not emit the typed IMC binding")
        imc_header_text = imc_header.read_text(encoding="utf-8")
        if '#include "BML/ImcWire.hpp"' not in imc_header_text:
            raise AssertionError("IMC binding does not use the fixed wire codec")
        for symbol in ("EncodedSampleSize", "EncodeSample", "DecodeSample", "switch (field.Id)",
                       "class Client", "Adopt", "class Provider", "BML_Imc_GetRpcId", "ReadState",
                       "QueryLookup", "StateFuture", "BeginReadState", "LookupFuture",
                       "BeginQueryLookup", "IsStateAvailable", "IsLookupAvailable",
                       "RegisterState", "RegisterLookup"):
            if symbol not in imc_header_text:
                raise AssertionError(f"IMC binding is missing generated codec element: {symbol}")
        if 'test.codegen/v1/rpc/state' not in imc_header_text or 'test.codegen/v1/rpc/lookup' not in imc_header_text:
            raise AssertionError("IMC binding lost deterministic endpoint routes")
        descriptor_match = re.search(r"\bHash = 0x([0-9A-F]+)ULL", imc_header_text)
        wire_match = re.search(r"\bWireHash = 0x([0-9A-F]+)ULL", imc_header_text)
        if not descriptor_match or not wire_match or descriptor_match.group(1) != wire_match.group(1):
            raise AssertionError("base IMC binding did not default its wire hash to the descriptor hash")
        for snippet in (
            "writer.Begin(SampleSchema, WireHash",
            ", WireHash, buffer, request)",
            ", output, WireHash, EncodedSampleSize, EncodeSample)",
        ):
            if snippet not in imc_header_text:
                raise AssertionError(f"IMC wire encoder bypasses the stable wire hash: {snippet}")
        if "RecordBuilder" in imc_header_text or "InteropApi.h" in imc_header_text:
            raise AssertionError("IMC binding leaked the dynamic Interop transport")

        unexpected = sorted(path.name for path in generated.iterdir()
                            if path.name != "test_codegen_imc.hpp")
        if unexpected:
            raise AssertionError(f"IMC generator emitted unexpected artifacts: {unexpected}")
        run(generator, "--check", "--out-dir", str(generated),
            "--input", str(base))

        hash_match = re.search(r"Hash = 0x([0-9A-F]+)ULL", imc_header_text)
        if not hash_match:
            raise AssertionError("generated IMC binding does not expose its structural hash")
        predecessor_hash = "0x" + hash_match.group(1)
        compatible = api_definition(
            minor=1,
            extra_field={"id": 2, "name": "label", "type": "string", "optional": True},
        )
        compatible["compatible_hashes"] = [predecessor_hash]
        write(current, compatible)
        write(incompatible, api_definition(
            minor=1,
            extra_field={"id": 2, "name": "required_label", "type": "string", "optional": False},
        ))

        run(generator, *common, "--input", str(base), "--check")
        run(generator, *common, "--input", str(current), "--previous", str(base))
        current_imc_text = imc_header.read_text(encoding="utf-8")
        current_hash_match = re.search(r"\bHash = 0x([0-9A-F]+)ULL", current_imc_text)
        current_wire_match = re.search(r"\bWireHash = 0x([0-9A-F]+)ULL", current_imc_text)
        if not current_hash_match or not current_wire_match:
            raise AssertionError("compatible IMC binding lost descriptor/wire hash metadata")
        if current_wire_match.group(1) != hash_match.group(1):
            raise AssertionError("compatible IMC binding did not preserve its predecessor wire hash")

        second_minor = api_definition(
            minor=2,
            extra_field={"id": 2, "name": "label", "type": "string", "optional": True},
        )
        current_hash = "0x" + current_hash_match.group(1)
        second_minor["compatible_hashes"] = [predecessor_hash, current_hash]
        write(next_contract, second_minor)
        run(generator, *common, "--input", str(next_contract), "--previous", str(current))
        next_imc_text = imc_header.read_text(encoding="utf-8")
        if f"WireHash = 0x{hash_match.group(1)}ULL" not in next_imc_text:
            raise AssertionError("transitive compatible minor changed its stable IMC wire hash")

        second_minor["compatible_hashes"] = [current_hash, predecessor_hash]
        write(wrong_wire, second_minor)
        wrong_result = run(generator, *common, "--input", str(wrong_wire),
                           "--previous", str(current), expect_success=False)
        if "put it first in compatible_hashes" not in wrong_result.stderr:
            raise AssertionError("wire-hash drift did not produce an actionable diagnostic")
        run(generator, *common, "--input", str(incompatible), "--previous", str(base), expect_success=False)

        wide_types = api_definition()
        wide_types["schemas"][0]["fields"] = [
            {"id": 1, "name": "signed", "type": "int64", "optional": False},
            {"id": 2, "name": "unsigned_value", "type": "uint64", "optional": False},
            {"id": 3, "name": "precise", "type": "double", "optional": False},
            {"id": 4, "name": "payload", "type": "bytes", "optional": False},
            {"id": 5, "name": "signed_values", "type": "array<int64>", "optional": False},
            {"id": 6, "name": "unsigned_values", "type": "array<uint64>", "optional": False},
            {"id": 7, "name": "precise_values", "type": "array<double>", "optional": False},
        ]
        wide_contract = root / "wide-types.bmlapi"
        wide_output = root / "wide-types"
        write(wide_contract, wide_types)
        run(generator, "--out-dir", str(wide_output),
            "--input", str(wide_contract))
        wide_header = (wide_output / "test_codegen_imc.hpp").read_text(encoding="utf-8")
        for snippet in (
            "std::int64_t Signed{}", "std::uint64_t UnsignedValue{}",
            "double Precise{}", "std::vector<std::uint8_t> Payload{}",
            "std::vector<std::int64_t> SignedValues{}",
            "std::vector<std::uint64_t> UnsignedValues{}",
            "std::vector<double> PreciseValues{}", "WriteInt64(", "WriteUInt64(",
            "WriteDouble(", "WriteBytes(", "ReadInt64(", "ReadUInt64(",
            "ReadDouble(", "ReadBytes(", "value.Payload.size()",
        ):
            if snippet not in wide_header:
                raise AssertionError(f"IMC-only wide type binding is missing: {snippet}")
        enum_api = api_definition()
        enum_api["enums"] = [{
            "name": "mode",
            "values": [
                {"name": "off", "value": 0},
                {"name": "on", "value": 1},
            ],
        }]
        enum_api["schemas"][0]["fields"] = [
            {"id": 1, "name": "mode", "type": "enum<mode>", "optional": False},
        ]
        enum_contract = root / "enum.bmlapi"
        enum_output = root / "enum"
        write(enum_contract, enum_api)
        run(generator, "--out-dir", str(enum_output),
            "--input", str(enum_contract))
        enum_header = (enum_output / "test_codegen_imc.hpp").read_text(encoding="utf-8")
        for snippet in (
            "enum class Mode : std::int32_t", "Off = 0", "On = 1",
            "inline constexpr bool IsKnownMode(Mode value)", "Mode Mode{}",
            "WriteInt(SampleField::Mode, static_cast<int>(value.Mode))",
            "int raw{}", "ReadInt(field, raw)",
            "decoded.Mode = static_cast<Mode>(raw)",
        ):
            if snippet not in enum_header:
                raise AssertionError(f"named enum binding is missing: {snippet}")
        enum_hash_match = re.search(r"\bHash = 0x([0-9A-F]+)ULL", enum_header)
        if not enum_hash_match:
            raise AssertionError("named enum binding did not expose its descriptor hash")
        evolved_enum = json.loads(json.dumps(enum_api))
        evolved_enum["version"]["minor"] = 1
        evolved_enum["enums"][0]["values"].append({"name": "auto", "value": 2})
        evolved_enum["compatible_hashes"] = ["0x" + enum_hash_match.group(1)]
        evolved_enum_contract = root / "enum-evolved.bmlapi"
        write(evolved_enum_contract, evolved_enum)
        run(generator, "--out-dir", str(root / "enum-evolved"),
            "--input", str(evolved_enum_contract), "--previous", str(enum_contract))

        renumbered_enum = json.loads(json.dumps(evolved_enum))
        renumbered_enum["enums"][0]["values"][1]["value"] = 7
        renumbered_contract = root / "enum-renumbered.bmlapi"
        write(renumbered_contract, renumbered_enum)
        renumbered_result = run(
            generator, "--out-dir", str(root / "enum-renumbered"),
            "--input", str(renumbered_contract), "--previous", str(enum_contract),
            expect_success=False,
        )
        if "removed, renamed, or renumbered" not in renumbered_result.stderr:
            raise AssertionError("enum renumbering did not produce an actionable diagnostic")

        aliased_enum = json.loads(json.dumps(enum_api))
        aliased_enum["enums"][0]["values"].append({"name": "also_on", "value": 1})
        alias_contract = root / "enum-alias.bmlapi"
        write(alias_contract, aliased_enum)
        alias_result = run(generator, "--out-dir", str(root / "enum-alias"),
                           "--input", str(alias_contract), expect_success=False)
        if "duplicate enum numeric value" not in alias_result.stderr:
            raise AssertionError("duplicate enum numeric values were not rejected clearly")

        colliding_enum = json.loads(json.dumps(enum_api))
        colliding_enum["enums"][0]["values"] = [
            {"name": "foo-bar", "value": 0},
            {"name": "foo_bar", "value": 1},
        ]
        colliding_enum_contract = root / "enum-collision.bmlapi"
        write(colliding_enum_contract, colliding_enum)
        collision_result = run(
            generator, "--out-dir", str(root / "enum-collision"),
            "--input", str(colliding_enum_contract), expect_success=False,
        )
        if "generated identifier collision" not in collision_result.stderr:
            raise AssertionError("enum C++ identifier collisions were not rejected clearly")

        helper_collision = json.loads(json.dumps(enum_api))
        helper_collision["enums"][0]["name"] = "sample_schema"
        helper_collision["schemas"][0]["fields"][0]["type"] = "enum<sample_schema>"
        helper_collision_contract = root / "enum-helper-collision.bmlapi"
        write(helper_collision_contract, helper_collision)
        helper_collision_result = run(
            generator, "--out-dir", str(root / "enum-helper-collision"),
            "--input", str(helper_collision_contract), expect_success=False,
        )
        if "generated identifier collision for SampleSchema" not in helper_collision_result.stderr:
            raise AssertionError("enum/generated-helper collisions were not rejected clearly")

        overflowing_enum = json.loads(json.dumps(enum_api))
        overflowing_enum["enums"][0]["values"][1]["value"] = 1 << 31
        overflowing_enum_contract = root / "enum-overflow.bmlapi"
        write(overflowing_enum_contract, overflowing_enum)
        overflow_result = run(
            generator, "--out-dir", str(root / "enum-overflow"),
            "--input", str(overflowing_enum_contract), expect_success=False,
        )
        if "must be an integer in" not in overflow_result.stderr:
            raise AssertionError("out-of-range enum values were not rejected clearly")

        native_api = api_definition()
        native_api.pop("endpoints")
        native_api["rpcs"] = [
            {"name": "state", "response": 1},
            {"name": "lookup", "request": 2, "response": 1},
        ]
        native_api["topics"] = [{"name": "changed", "message": 1}]
        native_contract = root / "native-imc.bmlapi"
        native_output = root / "native-imc"
        write(native_contract, native_api)
        run(generator, "--out-dir", str(native_output),
            "--input", str(native_contract))
        native_header = (native_output / "test_codegen_imc.hpp").read_text(encoding="utf-8")
        for snippet in (
            "BeginCallState(", "CallState(", "BeginCallLookup(", "CallLookup(",
            "RegisterState(", "RegisterLookup(", "class ChangedSubscription",
            "PublishChanged(", "test.codegen/v1/rpc/state",
            "test.codegen/v1/topic/changed",
        ):
            if snippet not in native_header:
                raise AssertionError(f"IMC-native rpcs/topics binding is missing: {snippet}")
        mixed_api = api_definition()
        mixed_api["rpcs"] = [{"name": "extra", "response": 1}]
        mixed_contract = root / "mixed-endpoints.bmlapi"
        write(mixed_contract, mixed_api)
        mixed_result = run(generator, "--out-dir", str(root / "mixed"),
                           "--input", str(mixed_contract), expect_success=False)
        if "not both" not in mixed_result.stderr:
            raise AssertionError("mixed typed/native endpoint syntax was not rejected clearly")

        overflow = api_definition()
        overflow["schemas"][0]["id"] = 0x1_0000_0000
        write(root / "overflow.bmlapi", overflow)
        run(generator, *common, "--input", str(root / "overflow.bmlapi"), expect_success=False)

        collision = api_definition()
        collision["schemas"][0]["fields"] = [
            {"id": 1, "name": "value", "type": "int", "optional": True},
            {"id": 2, "name": "has_value", "type": "int", "optional": False},
        ]
        write(root / "collision.bmlapi", collision)
        run(generator, *common, "--input", str(root / "collision.bmlapi"), expect_success=False)

        too_many_fields = api_definition()
        too_many_fields["schemas"][0]["fields"] = [
            {"id": index + 1, "name": f"value_{index + 1}", "type": "int",
             "optional": False}
            for index in range(65)
        ]
        write(root / "too-many-fields.bmlapi", too_many_fields)
        result = run(generator, *common, "--input", str(root / "too-many-fields.bmlapi"),
                     expect_success=False)
        if "at most 64 fields" not in result.stderr:
            raise AssertionError("oversized IMC schema did not produce an actionable diagnostic")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
