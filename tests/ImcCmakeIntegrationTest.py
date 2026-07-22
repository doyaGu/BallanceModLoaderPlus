#!/usr/bin/env python3
"""Configure and build a tiny consumer using the installed-style IMC helper."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def quote_cmake(path: Path) -> str:
    return path.resolve().as_posix().replace('"', '\\"')


def render_imc(value: dict) -> str:
    version = value["version"]
    lines = [f"api {value['api']} {version['major']}.{version.get('minor', 0)}", ""]
    for enum in value.get("enums", []):
        underlying = enum.get("underlying", "int")
        suffix = "" if underlying == "int" else f" : {underlying}"
        lines.append(f"enum {enum['name']}{suffix} {{")
        lines.extend(
            f"    {enum_value['name']} = {enum_value['value']}"
            for enum_value in enum["values"]
        )
        lines.extend(["}", ""])
    schema_names = {schema["id"]: schema["name"] for schema in value["schemas"]}
    for schema in value["schemas"]:
        lines.append(f"record {schema['name']} {{")
        for field in schema["fields"]:
            optional = "optional " if field.get("optional", False) else ""
            lines.append(f"    {optional}{field['type']} {field['name']}")
        lines.extend(["}", ""])
    for rpc in value.get("rpcs", []):
        request = schema_names[rpc["request"]] if rpc.get("request") else ""
        response = f" -> {schema_names[rpc['response']]}" if rpc.get("response") else ""
        lines.append(f"rpc {rpc['name']}({request}){response}")
    for topic in value.get("topics", []):
        lines.append(f"topic {topic['name']}({schema_names[topic['message']]})")
    return "\n".join(lines).rstrip() + "\n"


def run(command: list[str], *, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    if (result.returncode == 0) != expect_success:
        raise AssertionError(
            f"command {'failed' if expect_success else 'unexpectedly succeeded'} "
            f"({result.returncode}): {' '.join(command)}\n{result.stdout}"
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--generator-platform", default="")
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    with tempfile.TemporaryDirectory(prefix="bml-imc-cmake-") as temporary:
        root = Path(temporary)
        source = root / "source"
        build = root / "build"
        source.mkdir()
        interface = {
            "api": "example.echo",
            "version": {"major": 1, "minor": 0},
            "enums": [
                {"name": "mode", "values": [
                    {"name": "off", "value": 0},
                    {"name": "on", "value": 1}
                ]},
                {"name": "signed_mode", "underlying": "int64", "values": [
                    {"name": "minimum", "value": -(1 << 63)},
                    {"name": "maximum", "value": (1 << 63) - 1}
                ]},
                {"name": "unsigned_mode", "underlying": "uint64", "values": [
                    {"name": "zero", "value": 0},
                    {"name": "maximum", "value": (1 << 64) - 1}
                ]}
            ],
            "schemas": [
                {"id": 1, "name": "request", "fields": [
                    {"id": 1, "name": "text", "type": "string", "optional": False},
                    {"id": 2, "name": "signed", "type": "int64", "optional": False},
                    {"id": 3, "name": "unsigned_value", "type": "uint64", "optional": False},
                    {"id": 4, "name": "precise", "type": "double", "optional": False},
                    {"id": 5, "name": "payload", "type": "bytes", "optional": False},
                    {"id": 6, "name": "samples", "type": "array<double>", "optional": False},
                    {"id": 7, "name": "mode", "type": "enum<mode>", "optional": False},
                    {"id": 8, "name": "signed_mode", "type": "enum<signed_mode>", "optional": False},
                    {"id": 9, "name": "unsigned_mode", "type": "enum<unsigned_mode>", "optional": False}
                ]},
                {"id": 2, "name": "reply", "fields": [
                    {"id": 1, "name": "text", "type": "string", "optional": False}
                ]},
            ],
            "rpcs": [
                {"name": "echo", "request": 1, "response": 2},
                {"name": "status", "response": 2}
            ],
            "topics": [
                {"name": "changed", "message": 2}
            ],
        }
        interface_path = source / "echo-interface.imc"
        interface_path.write_text(
            render_imc(interface), encoding="utf-8"
        )
        numeric_interface = {
            "api": "0",
            "version": {"major": 1, "minor": 0},
            "schemas": [
                {"id": 1, "name": "1reply", "fields": [
                    {"id": 1, "name": "1text", "type": "string", "optional": False}
                ]}
            ],
            "rpcs": [{"name": "1status", "response": 1}],
        }
        numeric_interface_path = source / "numeric-interface.imc"
        numeric_interface_path.write_text(
            render_imc(numeric_interface), encoding="utf-8"
        )
        ping_interface_path = source / "ping-interface.imc"
        ping_interface_path.write_text(
            "api example.ping 1.0\n\nrpc ping()\n", encoding="utf-8"
        )
        run([
            sys.executable, str(source_root / "tools" / "imc_codegen.py"),
            "--update-lock", "--out-dir", str(root / "bootstrap"),
            "--input", str(interface_path),
            "--input", str(numeric_interface_path),
            "--input", str(ping_interface_path),
        ])
        (source / "consumer.cpp").write_text(
            '#include "example_echo_imc.hpp"\n'
            'int consume(BML::Imc::Generated::Example::Echo::Client &client) {\n'
            '    using Api = BML::Imc::Generated::Example::Echo::Client;\n'
            '    BML::Imc::Generated::Example::Echo::RequestValue request;\n'
            '    request.Text = "wide";\n'
            '    request.Signed = -9223372036854775807LL;\n'
            '    request.UnsignedValue = 18446744073709551615ULL;\n'
            '    request.Precise = 1.0 / 3.0;\n'
            '    request.Payload = {0, 1, 255};\n'
            '    request.Samples = {1.25, -2.5};\n'
            '    request.Mode = BML::Imc::Generated::Example::Echo::Mode::On;\n'
            '    request.SignedMode = BML::Imc::Generated::Example::Echo::SignedMode::Minimum;\n'
            '    request.UnsignedMode = BML::Imc::Generated::Example::Echo::UnsignedMode::Maximum;\n'
            '    if (!BML::Imc::Generated::Example::Echo::IsKnownMode(request.Mode)) return 1;\n'
            '    Api::EchoFuture future;\n'
            '    Api::StatusFuture statusFuture;\n'
            '    BML::Imc::Generated::Example::Echo::ChangedSubscription subscription;\n'
            '    (void)statusFuture; (void)subscription;\n'
            '    return client.BeginCallEcho(request, future, 0);\n'
            '}\n',
            encoding="utf-8",
        )
        (source / "numeric_consumer.cpp").write_text(
            '#include "0_imc.hpp"\n'
            'namespace Numeric = BML::Imc::Generated::_0;\n'
            'int consume_numeric(Numeric::Client &client) {\n'
            '    bool available = false;\n'
            '    Numeric::_1replyValue reply;\n'
            '    int status = client.Is_1statusAvailable(available);\n'
            '    if (status == BML_OK && available) status = client.Call_1status(reply, 0);\n'
            '    return status;\n'
            '}\n',
            encoding="utf-8",
        )
        (source / "ping_consumer.cpp").write_text(
            '#include "example_ping_imc.hpp"\n'
            'namespace Ping = BML::Imc::Generated::Example::Ping;\n'
            'int consume_ping(Ping::Client &client) { return client.CallPing(0); }\n',
            encoding="utf-8",
        )
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(ImcConsumer LANGUAGES CXX)\n"
            f'set(BML_IMC_CODEGEN "{quote_cmake(source_root / "tools" / "imc_codegen.py")}")\n'
            f'include("{quote_cmake(source_root / "cmake" / "BMLImc.cmake")}")\n'
            "add_library(consumer STATIC consumer.cpp)\n"
            f'target_include_directories(consumer PRIVATE "{quote_cmake(source_root / "include")}")\n'
            "bml_target_imc_api(consumer INPUT echo-interface.imc API_ID example.echo)\n"
            "add_library(numeric_consumer STATIC numeric_consumer.cpp)\n"
            f'target_include_directories(numeric_consumer PRIVATE "{quote_cmake(source_root / "include")}")\n'
            "bml_target_imc_api(numeric_consumer INPUT numeric-interface.imc API_ID 0)\n"
            "add_library(ping_consumer STATIC ping_consumer.cpp)\n"
            f'target_include_directories(ping_consumer PRIVATE "{quote_cmake(source_root / "include")}")\n'
            "bml_target_imc_api(ping_consumer INPUT ping-interface.imc API_ID example.ping)\n",
            encoding="utf-8",
        )

        configure = [
            str(args.cmake), "-S", str(source), "-B", str(build),
            "-G", args.generator, f"-DPython3_EXECUTABLE={sys.executable}",
        ]
        if args.generator_platform:
            configure.extend(["-A", args.generator_platform])
        run(configure)
        run([str(args.cmake), "--build", str(build), "--config", "Release"])
        generated = build / "bml-imc" / "example_echo_imc.hpp"
        if not generated.exists():
            raise AssertionError(f"CMake helper did not create {generated}")
        unexpected = list((build / "bml-imc").glob("example_echo_api.h"))
        if unexpected:
            raise AssertionError("CMake helper generated the legacy compatibility header")
        numeric_generated = build / "bml-imc" / "0_imc.hpp"
        if not numeric_generated.exists():
            raise AssertionError(
                "CMake helper did not compile a interface with leading-digit names"
            )
        ping_generated = build / "bml-imc" / "example_ping_imc.hpp"
        if not ping_generated.exists():
            raise AssertionError("CMake helper did not compile a schema-less RPC interface")

        missing_source = root / "missing-abi-source"
        missing_build = root / "missing-abi-build"
        missing_source.mkdir()
        (missing_source / "consumer.cpp").write_text("int value = 0;\n", encoding="utf-8")
        missing_interface = missing_source / "missing.imc"
        missing_interface.write_text("api example.missing 1.0\n\nrpc ping()\n", encoding="utf-8")
        (missing_source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(ImcMissingPrevious LANGUAGES CXX)\n"
            f'set(BML_IMC_CODEGEN "{quote_cmake(source_root / "tools" / "imc_codegen.py")}")\n'
            f'include("{quote_cmake(source_root / "cmake" / "BMLImc.cmake")}")\n'
            "add_library(consumer STATIC consumer.cpp)\n"
            "bml_target_imc_api(consumer INPUT missing.imc API_ID example.missing)\n",
            encoding="utf-8",
        )
        missing_configure = [
            str(args.cmake), "-S", str(missing_source), "-B", str(missing_build),
            "-G", args.generator, f"-DPython3_EXECUTABLE={sys.executable}",
        ]
        if args.generator_platform:
            missing_configure.extend(["-A", args.generator_platform])
        missing_result = run(missing_configure, expect_success=False)
        if "interface lock does not exist" not in missing_result.stdout:
            raise AssertionError("missing interface lock did not fail during CMake configuration")

        invalid_id_source = root / "invalid-id-source"
        invalid_id_build = root / "invalid-id-build"
        invalid_id_source.mkdir()
        (invalid_id_source / "consumer.cpp").write_text("int value = 0;\n", encoding="utf-8")
        (invalid_id_source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(ImcInvalidApiId LANGUAGES CXX)\n"
            f'set(BML_IMC_CODEGEN "{quote_cmake(source_root / "tools" / "imc_codegen.py")}")\n'
            f'include("{quote_cmake(source_root / "cmake" / "BMLImc.cmake")}")\n'
            "add_library(consumer STATIC consumer.cpp)\n"
            f'bml_target_imc_api(consumer INPUT "{quote_cmake(interface_path)}" '
            "API_ID example_echo)\n",
            encoding="utf-8",
        )
        invalid_id_configure = [
            str(args.cmake), "-S", str(invalid_id_source), "-B", str(invalid_id_build),
            "-G", args.generator, f"-DPython3_EXECUTABLE={sys.executable}",
        ]
        if args.generator_platform:
            invalid_id_configure.extend(["-A", args.generator_platform])
        invalid_id_result = run(invalid_id_configure, expect_success=False)
        if ("invalid API_ID 'example_echo'" not in invalid_id_result.stdout
                or "expected non-empty" not in invalid_id_result.stdout
                or "dot-separated segments" not in invalid_id_result.stdout):
            raise AssertionError(
                "invalid CMake API_ID did not fail during configuration with the expected "
                f"diagnostic:\n{invalid_id_result.stdout}"
            )

        interface["api"] = "example.mismatch"
        interface_path.write_text(render_imc(interface), encoding="utf-8")
        interface_path.with_suffix(".imc.lock").unlink()
        run([
            sys.executable, str(source_root / "tools" / "imc_codegen.py"),
            "--update-lock", "--out-dir", str(root / "mismatch-bootstrap"),
            "--input", str(interface_path),
        ])
        mismatch = run([str(args.cmake), "--build", str(build), "--config", "Release"],
                       expect_success=False)
        expected_diagnostic = (
            "interface API ID 'example.mismatch' does not match expected API ID 'example.echo'"
        )
        if expected_diagnostic not in mismatch.stdout or interface_path.name not in mismatch.stdout:
            raise AssertionError("CMake API_ID mismatch did not produce an actionable diagnostic")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
