#!/usr/bin/env python3
"""Compile old/new generated bindings and verify additive IMC compatibility both ways."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], *, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    if (result.returncode == 0) != expect_success:
        raise AssertionError(
            f"command {'failed' if expect_success else 'unexpectedly succeeded'} "
            f"({result.returncode}): {' '.join(command)}\n{result.stdout}"
        )
    return result


def write_contract(path: Path, *, minor: int,
                   compatible_hashes: list[str] | None = None) -> None:
    fields = [
        {"id": 1, "name": "value", "type": "int", "optional": False},
        {"id": 2, "name": "mode", "type": "enum<mode>", "optional": False},
    ]
    if minor:
        fields.append(
            {"id": 3, "name": "label", "type": "string", "optional": True}
        )
    enum_values = [
        {"name": "off", "value": 0},
        {"name": "on", "value": 1},
    ]
    if minor:
        enum_values.append({"name": "auto", "value": 2})
    contract: dict[str, object] = {
        "api": "test.compatibility",
        "version": {"major": 1, "minor": minor},
        "enums": [{"name": "mode", "values": enum_values}],
        "schemas": [{"id": 1, "name": "sample", "fields": fields}],
        "endpoints": [{"name": "state", "kind": "resource", "output": 1}],
    }
    if compatible_hashes:
        contract["compatible_hashes"] = compatible_hashes
    path.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8")


def cmake_path(path: Path) -> str:
    return path.resolve().as_posix().replace('"', '\\"')


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--generator-platform", default="")
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    generator = source_root / "tools" / "interop_codegen.py"
    with tempfile.TemporaryDirectory(prefix="bml-imc-compat-") as temporary:
        root = Path(temporary)
        old_contract = root / "old.bmlapi"
        new_contract = root / "new.bmlapi"
        old_headers = root / "old"
        new_headers = root / "new"
        write_contract(old_contract, minor=0)
        run([
            sys.executable, str(generator), "--imc-only", "--input", str(old_contract),
            "--out-dir", str(old_headers),
        ])
        old_header = old_headers / "test_compatibility_imc.hpp"
        match = re.search(r"Hash = 0x([0-9A-F]+)ULL", old_header.read_text(encoding="utf-8"))
        if not match:
            raise AssertionError("old generated binding did not expose its descriptor hash")

        write_contract(new_contract, minor=1, compatible_hashes=["0x" + match.group(1)])
        run([
            sys.executable, str(generator), "--imc-only", "--input", str(new_contract),
            "--previous", str(old_contract), "--out-dir", str(new_headers),
        ])
        new_header = new_headers / "test_compatibility_imc.hpp"

        source = root / "compatibility.cpp"
        source.write_text(
            '#include "BML/ImcCpp.hpp"\n'
            '#include <cstdint>\n'
            '#include <vector>\n'
            'namespace OldBinding {\n'
            f'#include "{cmake_path(old_header)}"\n'
            '}\n'
            'namespace NewBinding {\n'
            f'#include "{cmake_path(new_header)}"\n'
            '}\n'
            'namespace OldApi = OldBinding::BML::Imc::Generated::Test::Compatibility;\n'
            'namespace NewApi = NewBinding::BML::Imc::Generated::Test::Compatibility;\n'
            '\n'
            'template <class ApiValue, class Size, class Encode>\n'
            'std::vector<std::uint8_t> EncodeValue(const ApiValue &value, Size size, Encode encode) {\n'
            '    std::vector<std::uint8_t> bytes(size(value));\n'
            '    if (bytes.empty() || encode(value, bytes.data(), bytes.size()) != BML_OK)\n'
            '        return {};\n'
            '    return bytes;\n'
            '}\n'
            '\n'
            'int main() {\n'
            '    OldApi::SampleValue oldValue{};\n'
            '    oldValue.Value = 17; oldValue.Mode = OldApi::Mode::On;\n'
            '    auto oldBytes = EncodeValue(oldValue, OldApi::EncodedSampleSize, OldApi::EncodeSample);\n'
            '    BML_ImcMessage oldMessage = BML_IMC_MESSAGE_INIT;\n'
            '    oldMessage.Data = oldBytes.data(); oldMessage.DataSize = oldBytes.size();\n'
            '    NewApi::SampleValue decodedNew{};\n'
            '    if (NewApi::DecodeSample(oldMessage, decodedNew) != BML_OK ||\n'
            '        decodedNew.Value != 17 || decodedNew.Mode != NewApi::Mode::On ||\n'
            '        !NewApi::IsKnownMode(decodedNew.Mode) || decodedNew.HasLabel)\n'
            '        return 1;\n'
            '\n'
            '    NewApi::SampleValue newValue{};\n'
            '    newValue.Value = 29; newValue.Mode = NewApi::Mode::Auto;\n'
            '    newValue.HasLabel = true; newValue.Label = "added";\n'
            '    auto newBytes = EncodeValue(newValue, NewApi::EncodedSampleSize, NewApi::EncodeSample);\n'
            '    BML_ImcMessage newMessage = BML_IMC_MESSAGE_INIT;\n'
            '    newMessage.Data = newBytes.data(); newMessage.DataSize = newBytes.size();\n'
            '    OldApi::SampleValue decodedOld{};\n'
            '    const int oldStatus = OldApi::DecodeSample(newMessage, decodedOld);\n'
            '    if (oldStatus != BML_OK || decodedOld.Value != 29 ||\n'
            '        static_cast<int>(decodedOld.Mode) != 2 || OldApi::IsKnownMode(decodedOld.Mode))\n'
            '        return oldStatus == BML_ERROR_INTEROP_API_MISMATCH ? 2 : 3;\n'
            '    return 0;\n'
            '}\n',
            encoding="utf-8",
        )
        (root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\n"
            "project(ImcCompatibility LANGUAGES CXX)\n"
            "set(CMAKE_CXX_STANDARD 20)\n"
            "add_executable(compatibility compatibility.cpp)\n"
            f'target_include_directories(compatibility PRIVATE "{cmake_path(source_root / "include")}")\n',
            encoding="utf-8",
        )

        build = root / "build"
        configure = [
            str(args.cmake), "-S", str(root), "-B", str(build),
            "-G", args.generator,
        ]
        if args.generator_platform:
            configure.extend(["-A", args.generator_platform])
        run(configure)
        run([str(args.cmake), "--build", str(build), "--config", "Release"])
        candidates = (
            build / "compatibility.exe",
            build / "Release" / "compatibility.exe",
            build / "compatibility",
            build / "Release" / "compatibility",
        )
        executable = next((path for path in candidates if path.exists()), None)
        if executable is None:
            raise AssertionError("compatibility executable was not produced")
        run([str(executable)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
