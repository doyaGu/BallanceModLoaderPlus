#!/usr/bin/env python3
"""Guard the public SDK against legacy Interop API regressions."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def read(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root

    removed_paths = (
        "include/BML/InteropApi.h",
        "include/BML/InteropClient.h",
        "include/BML/InteropMath.h",
        "include/BML/InteropTypes.h",
        "include/BML/Core.h",
        "tools/interop_codegen.py",
        "interop",
    )
    for relative in removed_paths:
        if (root / relative).exists():
            raise AssertionError(f"removed compatibility surface returned: {relative}")

    strict_c_headers = (
        "include/BML/ImcTypes.h",
        "include/BML/EventKinds.h",
        "include/BML/Imc.h",
    )
    forbidden_cxx = re.compile(r"(^|[^A-Za-z0-9_])(namespace|class|template|std::)([^A-Za-z0-9_]|$)")
    for relative in strict_c_headers:
        text = read(root, relative)
        filtered = "\n".join(
            line for line in text.splitlines()
            if not line.lstrip().startswith(("/*", "*", "//"))
        )
        if forbidden_cxx.search(filtered):
            raise AssertionError(f"{relative} is no longer a C-only IMC header")

    imc_api = read(root, "include/BML/Imc.h")
    exported = re.findall(r"BML_EXPORT\s+(?:int|void)\s+([A-Za-z0-9_]+)", imc_api)
    if not exported or any(not name.startswith("BML_Imc_") for name in exported):
        raise AssertionError("Imc.h exposes something other than BML_Imc_ C functions")

    public_headers = list((root / "include/BML").glob("*.h"))
    public_headers += list((root / "include/BML").glob("*.hpp"))
    public_headers += list((root / "include/BML/Generated").glob("*.hpp"))
    forbidden_legacy = ("BML::Interop", "BML_Interop", "BML_INTEROP", "BML_ERROR_INTEROP", "BML::Core")
    for header in public_headers:
        text = header.read_text(encoding="utf-8")
        for token in forbidden_legacy:
            if token in text:
                raise AssertionError(f"{header.relative_to(root)} exposes removed token {token}")

    legacy_generated = list((root / "include/BML/Generated").glob("*_api.h"))
    if legacy_generated:
        raise AssertionError(f"legacy generated headers remain: {legacy_generated}")

    header_only = (
        "include/BML/ImcCpp.hpp",
        "include/BML/ImcWire.hpp",
        "include/BML/ImcMath.h",
        "include/BML/Runtime.h",
        "include/BML/Scene.h",
        "include/BML/Gameplay.h",
        "include/BML/Events.h",
        "include/BML/UI.h",
        "include/BML/Speedrun.h",
    )
    for relative in header_only:
        if "BML_EXPORT" in read(root, relative):
            raise AssertionError(f"{relative} introduces an exported C++ IMC helper")

    for header in (root / "include/BML/Generated").glob("*_imc.hpp"):
        generated = header.read_text(encoding="utf-8")
        if "BML_EXPORT" in generated:
            raise AssertionError(f"{header} introduces an exported generated C++ helper")
        if "WireHash" in generated or "IsCompatibleHash" in generated:
            raise AssertionError(f"{header} reintroduces payload hash negotiation")

    field_number = re.compile(
        r"^\s*(?:optional\s+)?(?:bool|int|float|int64|uint64|double|string|bytes|"
        r"object|vec2|vec3|mat4|array<[^>]+>|enum<[^>]+>)\s+[^\s=]+\s*=",
        re.MULTILINE,
    )
    interface_dir = root / "imc/interfaces"
    legacy_interfaces = list(interface_dir.glob("*.bmlapi"))
    legacy_interfaces += list(interface_dir.glob("*.bmlabi"))
    if legacy_interfaces:
        raise AssertionError(f"legacy interface filenames remain: {legacy_interfaces}")

    for interface in interface_dir.glob("*.imc"):
        source = interface.read_text(encoding="utf-8")
        if re.search(r"^\s*(?:schema|wire|accept)\b", source, re.MULTILINE):
            raise AssertionError(f"{interface} uses legacy transport declarations")
        if field_number.search(source):
            raise AssertionError(f"{interface} exposes handwritten field numbers")

        lock_path = interface.with_suffix(".imc.lock")
        if not lock_path.is_file():
            raise AssertionError(f"{interface} has no adjacent interface lock")
        snapshot = json.loads(lock_path.read_text(encoding="utf-8"))
        if snapshot.get("format") != 1:
            raise AssertionError(f"{lock_path} has an unsupported lock format")
        for record in snapshot.get("schemas", []):
            if "id" in record:
                raise AssertionError(f"{lock_path} freezes an unused numeric record ID")
            field_ids = [field["id"] for field in record.get("fields", [])]
            if len(field_ids) != len(set(field_ids)):
                raise AssertionError(
                    f"{lock_path} contains duplicate field IDs in {record['name']}"
                )

    wire = read(root, "include/BML/ImcWire.hpp")
    if "DescriptorHash" in wire or "SchemaId" in wire:
        raise AssertionError("ImcWire.hpp duplicates payload identity inside the payload")

    implementation = read(root, "src/ImcApi.cpp")
    open_client = implementation.find("BML_EXPORT int BML_Imc_OpenClient")
    capture = implementation.find("callerReturnAddress = _ReturnAddress()", open_client)
    guard = implementation.find("return GuardImc", open_client)
    resolve_match = re.search(
        r"GetNativeImcOwnerId\(\s*callerReturnAddress\s*,\s*ownerId\s*\)",
        implementation[open_client:],
    )
    resolve = open_client + resolve_match.start() if resolve_match else -1
    if not (open_client < capture < guard < resolve):
        raise AssertionError(
            "BML_Imc_OpenClient must capture the external return address before GuardImc"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
