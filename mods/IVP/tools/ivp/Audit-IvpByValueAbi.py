#!/usr/bin/env python3
"""Audit public IVP value passing at the Ballance x86 DLL boundary.

The neighboring tree supplies declaration spelling, not binary authority.
``Audit-IvpPublicInterface.py`` has already matched those declarations against
the reconstructed headers and the exact-retail evidence ledger.  This script
reuses that result to isolate the ABI-sensitive cases: record values, record
returns (hidden-sret candidates), enums, and the IVP float aliases.
"""

from __future__ import annotations

import argparse
import csv
import io
import re
import subprocess
import sys
from pathlib import Path


EXPECTED_RETAIL_RECORD_VALUES = {
    ("IVP_Controller", "reset_time"),
    ("IVP_Core", "calc_at_matrix"),
    ("IVP_Core", "calc_movement_state"),
    ("IVP_PerformanceCounter_Simple", "reset_and_print_performance_counters"),
    ("IVP_Real_Object", "calc_at_matrix"),
}


def split_function_type(signature: str) -> list[str]:
    opening = signature.find("(")
    if opening < 0:
        return [signature]
    result = [signature[:opening].strip()]
    current: list[str] = []
    depth = 0
    for character in signature[opening + 1 :]:
        if character == ")" and depth == 0:
            value = "".join(current).strip()
            if value:
                result.append(value)
            return result
        if character == "," and depth == 0:
            result.append("".join(current).strip())
            current.clear()
            continue
        current.append(character)
        if character in "<([":
            depth += 1
        elif character in ">)]":
            depth -= 1
    raise RuntimeError(f"unterminated function type: {signature}")


def top_level_ivp_value(type_spelling: str) -> str | None:
    spelling = re.sub(
        r"\b(?:const|volatile|class|struct|enum)\b", " ", type_spelling
    ).strip()
    if "*" in spelling or "&" in spelling:
        return None
    match = re.match(r"(IVP_[A-Za-z0-9_]+)", spelling)
    return match.group(1) if match else None


def declared_types(headers: list[Path], kind: str) -> set[str]:
    pattern = re.compile(
        rf"\b(?:{kind})\s+(IVP_[A-Za-z0-9_]+)"
    )
    result: set[str] = set()
    for header in headers:
        result.update(pattern.findall(header.read_text(encoding="utf-8")))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).parents[2])
    parser.add_argument("--reference-root", type=Path, required=True)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    audit = repo_root / "tools/ivp/Audit-IvpPublicInterface.py"
    completed = subprocess.run(
        [
            sys.executable,
            str(audit),
            "--repo-root",
            str(repo_root),
            "--reference-root",
            str(args.reference_root.resolve()),
            "--format",
            "methods-tsv",
        ],
        cwd=repo_root,
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        return completed.returncode

    rows = list(csv.DictReader(io.StringIO(completed.stdout), delimiter="\t"))
    headers = sorted((repo_root / "include/BML/IVP").rglob("*.h"))
    records = declared_types(headers, r"class|struct|union")
    enums = declared_types(headers, r"enum(?:\s+class)?")

    record_methods: list[dict[str, str]] = []
    record_returns: list[dict[str, str]] = []
    enum_methods: list[dict[str, str]] = []
    for row in rows:
        parts = split_function_type(row["signature"])
        values = [top_level_ivp_value(part) for part in parts]
        if any(value in records for value in values):
            record_methods.append(row)
            if values[0] in records:
                record_returns.append(row)
        if any(value in enums for value in values):
            enum_methods.append(row)

    retail_record_methods = [
        row for row in record_methods if row["current_retail_invoke"] == "true"
    ]
    retail_record_returns = [
        row for row in record_returns if row["current_retail_invoke"] == "true"
    ]

    problems: list[str] = []
    for row in record_methods + enum_methods:
        # A reference-only class such as the later Mopp surface manager is
        # accounted for by the public-interface unavailable ledger. It has no
        # current declaration and therefore creates no current value ABI.
        if row["callable_disposition"] == "not-current-exact":
            continue
        if row["disposition"] not in {
            "current-exact",
            "retail-confirmed-signature-variant",
        }:
            problems.append(
                f"signature mismatch: {row['class']}::{row['method']} "
                f"{row['signature']} ({row['disposition']})"
            )
    actual_retail = {
        (row["class"], row["method"]) for row in retail_record_methods
    }
    if actual_retail != EXPECTED_RETAIL_RECORD_VALUES:
        problems.append(
            "retail record-value boundary changed: "
            f"expected {sorted(EXPECTED_RETAIL_RECORD_VALUES)}, "
            f"got {sorted(actual_retail)}"
        )
    for row in retail_record_methods:
        if row["current_exact_retail_body_route"] != "retail-address":
            problems.append(
                f"record value lacks exact retail route: "
                f"{row['class']}::{row['method']}"
            )
    if retail_record_returns:
        problems.append(
            "record return unexpectedly crosses the DLL boundary: "
            + ", ".join(
                f"{row['class']}::{row['method']}"
                for row in retail_record_returns
            )
        )

    enum_pattern = re.compile(
        r"\benum(?:\s+class)?\s+(IVP_[A-Za-z0-9_]+)\s*"
        r"(?::\s*([^\{]+?))?\s*\{"
    )
    enum_definitions: dict[str, str] = {}
    for header in headers:
        contents = header.read_text(encoding="utf-8")
        for name, underlying in enum_pattern.findall(contents):
            enum_definitions[name] = underlying.strip()
    non_int32_enums = {
        name: underlying
        for name, underlying in enum_definitions.items()
        if underlying != "std::int32_t"
    }
    if non_int32_enums:
        problems.append(
            "public IVP enums without explicit int32 ABI: "
            + ", ".join(
                f"{name}={underlying or '<implicit>'}"
                for name, underlying in sorted(non_int32_enums.items())
            )
        )

    types_header = (repo_root / "include/BML/IVP/Types.h").read_text(
        encoding="utf-8"
    )
    for name, scalar in (("IVP_FLOAT", "float"), ("IVP_DOUBLE", "double")):
        if not re.search(
            rf"\busing\s+{name}\s*=\s*{scalar}\s*;", types_header
        ):
            problems.append(f"{name} is not explicitly {scalar}")

    metrics = (
        ("PUBLIC_VALUE_RECORD_METHODS", len(record_methods)),
        ("RETAIL_VALUE_RECORD_METHODS", len(retail_record_methods)),
        ("PUBLIC_RECORD_RETURNS", len(record_returns)),
        ("RETAIL_RECORD_RETURNS", len(retail_record_returns)),
        ("PUBLIC_ENUM_VALUE_METHODS", len(enum_methods)),
        ("CURRENT_FIXED_32BIT_ENUMS", len(enum_definitions)),
        ("PROBLEMS", len(problems)),
    )
    for name, value in metrics:
        print(f"{name}\t{value}")
    for problem in problems:
        print(f"PROBLEM\t{problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
