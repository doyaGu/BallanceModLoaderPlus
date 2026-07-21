#!/usr/bin/env python3
"""Guard the public Interop headers against a C++ DLL ABI regression."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def read(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root

    strict_c_headers = (
        "include/BML/InteropTypes.h",
        "include/BML/EventKinds.h",
        "include/BML/InteropApi.h",
    )
    forbidden_cxx_tokens = re.compile(r"(^|[^A-Za-z0-9_])(namespace|class|template|std::)([^A-Za-z0-9_]|$)")
    for relative in strict_c_headers:
        text = read(root, relative)
        filtered = "\n".join(
            line for line in text.splitlines()
            if not line.lstrip().startswith(("/*", "*", "//"))
        )
        if forbidden_cxx_tokens.search(filtered):
            raise AssertionError(f"{relative} is no longer a C-only Interop header")

    api = read(root, "include/BML/InteropApi.h")
    exported = re.findall(r"BML_EXPORT\s+(?:int|void|BML_InteropRecordBuilder\s*\*)\s+([A-Za-z0-9_]+)", api)
    if not exported or any(not name.startswith("BML_Interop_") for name in exported):
        raise AssertionError("InteropApi.h exposes something other than BML_Interop C functions")

    header_only = (
        "include/BML/InteropClient.h",
        "include/BML/InteropMath.h",
        "include/BML/Runtime.h",
        "include/BML/Scene.h",
        "include/BML/Gameplay.h",
        "include/BML/Events.h",
        "include/BML/UI.h",
        "include/BML/Core.h",
    )
    for relative in header_only:
        if "BML_EXPORT" in read(root, relative):
            raise AssertionError(f"{relative} introduces an exported C++ Interop helper")

    generated = root / "include/BML/Generated"
    for header in generated.glob("*_api.h"):
        text = header.read_text(encoding="utf-8")
        if "BML_EXPORT" in text:
            raise AssertionError(f"{header} introduces an exported generated C++ helper")
        if "#ifdef __cplusplus" not in text or "#else" not in text:
            raise AssertionError(f"{header} does not provide separate C and C++ descriptor forms")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
