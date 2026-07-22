#!/usr/bin/env python3
"""Black-box checks for IMC source, lock snapshot, and C++ generation."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


BASE_INTERFACE = """api test.codegen 1.0

record sample {
    int value
}

record request {
    string name
}

rpc state() -> sample
rpc lookup(request) -> sample
rpc notify(request)
rpc flush()
topic changed(sample)
"""


def run(generator: Path, *arguments: str,
        expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [sys.executable, str(generator), *arguments],
        text=True,
        encoding="utf-8",
        errors="replace",
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


def read_abi(interface: Path) -> dict:
    return json.loads(interface.with_suffix(".imc.lock").read_text(encoding="utf-8"))


def generate(generator: Path, interface: Path, output: Path, *, update: bool = False,
             check: bool = False, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    arguments = ["--out-dir", str(output), "--input", str(interface)]
    if update:
        arguments.append("--update-lock")
    if check:
        arguments.append("--check")
    return run(generator, *arguments, expect_success=expect_success)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    generator = args.source_root / "tools" / "imc_codegen.py"

    with tempfile.TemporaryDirectory(prefix="bml-imc-codegen-") as temporary:
        root = Path(temporary)
        interface = root / "test.codegen.imc"
        output = root / "generated"
        interface.write_text(BASE_INTERFACE, encoding="utf-8")

        missing = generate(generator, interface, output, expect_success=False)
        if "does not exist; run with --update-lock once" not in missing.stderr:
            raise AssertionError("missing interface lock did not produce an actionable diagnostic")

        old_extension = root / "old-name.bmlapi"
        old_extension.write_text(BASE_INTERFACE, encoding="utf-8")
        old_extension_result = generate(
            generator, old_extension, output, update=True, expect_success=False
        )
        if "input must use the .imc extension" not in old_extension_result.stderr:
            raise AssertionError("legacy interface extension was not rejected")

        generate(generator, interface, output, update=True)
        abi_path = interface.with_suffix(".imc.lock")
        header_path = output / "test_codegen_imc.hpp"
        if not abi_path.exists() or not header_path.exists():
            raise AssertionError("initial generation did not create interface lock and binding")

        abi = read_abi(interface)
        if abi["format"] != 1 or abi["api"] != "test.codegen":
            raise AssertionError("interface lock identity is incomplete")
        sample = next(record for record in abi["schemas"] if record["name"] == "sample")
        if "id" in sample or sample["fields"][0]["id"] != 1:
            raise AssertionError("interface lock did not assign dense initial wire IDs")

        header = header_path.read_text(encoding="utf-8")
        for snippet in (
            "inline constexpr std::uint32_t Value = 1u;",
            "EncodedSampleSize", "EncodeSample", "DecodeSample",
            "class Client", "class Provider", "CallState", "CallLookup",
            "CallNotify", "CallFlush", "RpcFuture<void>",
            "RegisterState", "RegisterLookup", "class ChangedSubscription",
            "writer.Begin();", "reader.Begin();", "WriteResponse(",
        ):
            if snippet not in header:
                raise AssertionError(f"generated binding is missing {snippet!r}")
        for obsolete in (
            "WireHash", "IsCompatibleHash", "DescriptorHash", "HeaderSize",
            "SchemaMetadata", "EndpointMetadata",
            "inline constexpr std::uint64_t Hash",
        ):
            if obsolete in header:
                raise AssertionError(f"generated binding leaked obsolete wire metadata: {obsolete}")

        generate(generator, interface, output, check=True)
        header_path.write_text("stale\n", encoding="utf-8")
        stale_header = generate(generator, interface, output, check=True, expect_success=False)
        if "generated output is stale" not in stale_header.stderr:
            raise AssertionError("--check did not reject a stale generated header")
        generate(generator, interface, output)

        wrong_id = run(
            generator, "--out-dir", str(output), "--input", str(interface),
            "--expected-api-id", "wrong.codegen", expect_success=False,
        )
        if "interface API ID 'test.codegen' does not match expected API ID 'wrong.codegen'" not in wrong_id.stderr:
            raise AssertionError("expected API ID mismatch was not diagnosed")

        legacy = root / "legacy.imc"
        legacy.write_text(
            "api test.legacy 1.0\nschema value = 1 { int number = 1 }\nrpc get() -> value\n",
            encoding="utf-8",
        )
        legacy_result = generate(generator, legacy, root / "legacy", update=True,
                                 expect_success=False)
        if "schema wire IDs are no longer authored here" not in legacy_result.stderr:
            raise AssertionError("legacy numbered schema syntax was not rejected clearly")

        manual_wire = root / "manual-wire.imc"
        manual_wire.write_text(
            "api test.manual 1.0\nwire 0x1234\nrpc ping()\n", encoding="utf-8"
        )
        manual_wire_result = generate(
            generator, manual_wire, root / "manual-wire", update=True,
            expect_success=False,
        )
        if "unknown declaration 'wire'" not in manual_wire_result.stderr:
            raise AssertionError("manual wire hash syntax was not rejected")

        json_interface = root / "json.imc"
        json_interface.write_text('{"api":"test.json"}', encoding="utf-8")
        json_result = generate(generator, json_interface, root / "json", update=True,
                               expect_success=False)
        if "JSON is not an .imc interface" not in json_result.stderr:
            raise AssertionError("JSON input was not rejected with the migration hint")

        comments = root / "comments.imc"
        comments.write_text(
            "# comments and optional separators\n"
            "api test.comments 1.0;\n"
            "record value { int number; } // payload\n"
            "rpc get() -> value;\n",
            encoding="utf-8",
        )
        generate(generator, comments, root / "comments", update=True)

        inline = root / "inline.imc"
        inline.write_text(
            "api test.inline 1.0\n"
            "rpc echo(string text, optional int limit) -> (string text)\n"
            "topic changed(string text, int sequence)\n",
            encoding="utf-8",
        )
        inline_output = root / "inline"
        generate(generator, inline, inline_output, update=True)
        inline_abi = read_abi(inline)
        if [record["name"] for record in inline_abi["schemas"]] != [
            "changed_event", "echo_reply", "echo_request"
        ]:
            raise AssertionError("inline payloads did not synthesize stable named records")
        inline_header = (inline_output / "test_inline_imc.hpp").read_text(encoding="utf-8")
        for snippet in ("EchoRequestValue", "EchoReplyValue", "ChangedEventValue"):
            if snippet not in inline_header:
                raise AssertionError(f"inline payload binding is missing {snippet}")

        invalid_update = root / "invalid-update.imc"
        invalid_fields = "\n".join(
            f"    int field_{index}" for index in range(1, 66)
        )
        invalid_update.write_text(
            f"api test.invalid 1.0\nrecord oversized {{\n{invalid_fields}\n}}\n",
            encoding="utf-8",
        )
        generate(
            generator, invalid_update, root / "invalid-update", update=True,
            expect_success=False,
        )
        if invalid_update.with_suffix(".imc.lock").exists():
            raise AssertionError("failed --update-lock left a partial interface lock")

        # Reordering authoring declarations must not change the machine-owned IDs.
        before_reorder = abi_path.read_text(encoding="utf-8")
        interface.write_text(
            BASE_INTERFACE.replace("    int value\n", "    int value\n")
            .replace("rpc state() -> sample\nrpc lookup(request) -> sample",
                     "rpc lookup(request) -> sample\nrpc state() -> sample"),
            encoding="utf-8",
        )
        generate(generator, interface, output, update=True)
        if abi_path.read_text(encoding="utf-8") == before_reorder:
            pass
        else:
            reordered = read_abi(interface)
            reordered_sample = next(r for r in reordered["schemas"] if r["name"] == "sample")
            if reordered_sample["fields"][0]["id"] != 1:
                raise AssertionError("source reordering changed stable ABI IDs")

        # Additive evolution is staged: normal builds reject stale ABI, update assigns an ID.
        compatible = BASE_INTERFACE.replace("1.0", "1.1").replace(
            "    int value\n", "    int value\n    optional string label\n"
        )
        interface.write_text(compatible, encoding="utf-8")
        stale_abi = generate(generator, interface, output, expect_success=False)
        if "interface lock is stale" not in stale_abi.stderr:
            raise AssertionError("source change bypassed the explicit lock update step")
        generate(generator, interface, output, update=True)
        compatible_abi = read_abi(interface)
        compatible_sample = next(r for r in compatible_abi["schemas"] if r["name"] == "sample")
        label = next(field for field in compatible_sample["fields"] if field["name"] == "label")
        if label["id"] != 2 or not label["optional"]:
            raise AssertionError("new optional field did not receive the next permanent ID")

        required = root / "required.imc"
        required.write_text(
            compatible.replace("optional string label", "string label"), encoding="utf-8"
        )
        required.with_suffix(".imc.lock").write_text(
            abi_path.read_text(encoding="utf-8"), encoding="utf-8"
        )
        required_result = generate(generator, required, root / "required-out", update=True,
                                   expect_success=False)
        if "changed optional incompatibly" not in required_result.stderr:
            raise AssertionError("requiredness change was not rejected")

        no_minor = root / "no-minor.imc"
        no_minor.write_text(BASE_INTERFACE.replace(
            "    int value\n", "    int value\n    optional string label\n"
        ), encoding="utf-8")
        no_minor.with_suffix(".imc.lock").write_text(before_reorder, encoding="utf-8")
        no_minor_result = generate(generator, no_minor, root / "no-minor-out", update=True,
                                   expect_success=False)
        if "without increasing its minor version" not in no_minor_result.stderr:
            raise AssertionError("same-minor ABI change was not rejected")

        # Removing an optional field keeps a permanent tombstone; reuse is rejected.
        interface.write_text(BASE_INTERFACE.replace("1.0", "1.2"), encoding="utf-8")
        generate(generator, interface, output, update=True)
        removed_abi = read_abi(interface)
        removed_sample = next(r for r in removed_abi["schemas"] if r["name"] == "sample")
        tombstone = next(field for field in removed_sample["fields"] if field["name"] == "label")
        if tombstone["id"] != 2 or not tombstone["reserved"]:
            raise AssertionError("removed optional field did not become a permanent tombstone")
        interface.write_text(compatible.replace("1.1", "1.3"), encoding="utf-8")
        reuse = generate(generator, interface, output, update=True, expect_success=False)
        if "is reserved and cannot be reused" not in reuse.stderr:
            raise AssertionError("reserved field name was reusable")

        # A major bump deliberately starts a new ABI identity space.
        major = root / "major.imc"
        major.write_text(
            "api test.codegen 2.0\nrecord sample { string value }\nrpc state() -> sample\n",
            encoding="utf-8",
        )
        major.with_suffix(".imc.lock").write_text(
            abi_path.read_text(encoding="utf-8"), encoding="utf-8"
        )
        generate(generator, major, root / "major-out", update=True)
        major_abi = read_abi(major)
        if major_abi["version"] != {"major": 2, "minor": 0}:
            raise AssertionError("major bump did not reset the interface lock")

        malformed = root / "malformed.imc"
        malformed.write_text("api test.malformed 1.0\nrecord value {\n", encoding="utf-8")
        malformed_result = generate(generator, malformed, root / "malformed", update=True,
                                    expect_success=False)
        if malformed.name not in malformed_result.stderr or "line 3 column 1" not in malformed_result.stderr:
            raise AssertionError("malformed IDL diagnostic lost its path or location")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
