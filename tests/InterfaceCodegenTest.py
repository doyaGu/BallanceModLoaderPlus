#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SOURCE_ROOT: Path


class InterfaceCodegenTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.definition = self.root / "value.bml-interface"
        self.output = self.root / "generated" / "ValueInterface.h"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_codegen(self, *extra: str, expect: int = 0) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            str(SOURCE_ROOT / "tools" / "interface_codegen.py"),
            "--input", str(self.definition),
            "--output", str(self.output),
            "--provider-id", "test.value-provider",
            "--provider-version", "1.2.3",
            "--namespace", "TestValue",
            *extra,
        ]
        result = subprocess.run(command, text=True, capture_output=True, check=False)
        self.assertEqual(expect, result.returncode, result.stdout + result.stderr)
        return result

    def write(self, version: str = "1.0", extra: str = "") -> None:
        self.definition.write_text(
            f"interface test.value-provider.value {version}\n\n"
            "fn read_value(int input) -> int value\n" + extra,
            encoding="utf-8",
        )

    def test_generates_header_and_stable_lock(self) -> None:
        self.write()
        self.run_codegen("--update-lock")
        header = self.output.read_text(encoding="utf-8")
        self.assertIn("typedef struct TestValueInterface", header)
        self.assertIn("int(BML_CDECL *ReadValue)(int input, int *outValue);", header)
        self.assertIn("BML_DECLARE_INTERFACE_TRAITS(TestValueTraits", header)
        self.run_codegen("--check")

    def test_append_requires_minor_bump(self) -> None:
        self.write()
        self.run_codegen("--update-lock")
        self.write(extra="fn enabled() -> bool value\n")
        result = self.run_codegen("--update-lock", expect=1)
        self.assertIn("higher minor version", result.stderr)
        self.write(version="1.1", extra="fn enabled() -> bool value\n")
        self.run_codegen("--update-lock")

    def test_existing_function_cannot_change_without_new_major(self) -> None:
        self.write()
        self.run_codegen("--update-lock")
        self.definition.write_text(
            "interface test.value-provider.value 1.1\n\n"
            "fn read_value(float input) -> int value\n",
            encoding="utf-8",
        )
        result = self.run_codegen("--update-lock", expect=1)
        self.assertIn("bump the major version", result.stderr)
        self.definition.write_text(
            "interface test.value-provider.value 2.0\n\n"
            "fn read_value(float input) -> int value\n",
            encoding="utf-8",
        )
        self.run_codegen("--update-lock")

    def test_reports_actionable_type_error(self) -> None:
        self.definition.write_text(
            "interface test.value-provider.value 1.0\n\n"
            "fn read_value(vector input) -> int value\n",
            encoding="utf-8",
        )
        result = self.run_codegen("--update-lock", expect=1)
        self.assertIn("unsupported type 'vector'", result.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args, remaining = parser.parse_known_args()
    global SOURCE_ROOT
    SOURCE_ROOT = args.source_root.resolve()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(InterfaceCodegenTest)
    return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
