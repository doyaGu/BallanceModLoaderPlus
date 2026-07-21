#!/usr/bin/env python3
"""Black-box checks for the deterministic Interop API generator."""

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
    generator = args.source_root / "tools" / "interop_codegen.py"

    with tempfile.TemporaryDirectory(prefix="bml-interop-codegen-") as temporary:
        root = Path(temporary)
        base = root / "base.bmlapi"
        current = root / "current.bmlapi"
        incompatible = root / "incompatible.bmlapi"
        write(base, api_definition())
        generated = root / "generated"
        headers = root / "headers"
        common = ("--out-dir", str(generated), "--header-out-dir", str(headers))
        run(generator, *common, "--input", str(base))

        header = headers / "test_codegen_api.h"
        script = generated / "test_codegen.as"
        metadata = generated / "test_codegen.test.json"
        if not header.exists() or not script.exists() or not metadata.exists():
            raise AssertionError("generator did not emit every API binding artifact")
        header_text = header.read_text(encoding="utf-8")
        script_text = script.read_text(encoding="utf-8")
        if "#ifdef __cplusplus" not in header_text or "inline int RegisterProvider" not in header_text:
            raise AssertionError("native binding is not an explicitly header-only C++ convenience")
        if "#else" not in header_text or "#define BML_INTEROP_TEST_CODEGEN_DESCRIPTOR" not in header_text:
            raise AssertionError("native binding does not emit a C descriptor branch")
        if "CreateApi" not in script_text or "api.AddField" not in script_text:
            raise AssertionError("AngelScript binding is missing its executable provider API builder")
        if "class SampleValue" not in script_text or "ReadState" not in script_text or "QueryLookup" not in script_text:
            raise AssertionError("AngelScript binding does not emit a typed consumer facade")
        if "struct SampleValue" not in header_text or "DecodeSample" not in header_text or "QueryLookup" not in header_text:
            raise AssertionError("native binding does not emit header-only typed consumer helpers")
        native_query = re.search(r"inline int QueryLookup\(.*?\n\}", header_text, re.DOTALL)
        script_query = re.search(r"int QueryLookup\(.*?\n\}", script_text, re.DOTALL)
        if not native_query or "int status = Require();" not in native_query.group(0):
            raise AssertionError("native query helper bypasses API compatibility validation")
        if not script_query or "int status = Require();" not in script_query.group(0):
            raise AssertionError("script query helper bypasses API compatibility validation")
        if json.loads(metadata.read_text(encoding="utf-8"))["api"] != "test.codegen":
            raise AssertionError("generated test metadata lost API identity")

        hash_match = re.search(r"Hash = 0x([0-9A-F]+)ULL", header_text)
        if not hash_match:
            raise AssertionError("generated native descriptor does not expose its structural hash")
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
        run(generator, *common, "--input", str(incompatible), "--previous", str(base), expect_success=False)

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

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
