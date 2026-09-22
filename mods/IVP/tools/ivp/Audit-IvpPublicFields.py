#!/usr/bin/env python3
"""Compare nearby public IVP fields with reconstructed Ballance declarations.

This is a source-surface inventory, not a claim that the nearby layout is the
retail layout.  It reports field spelling/type/order separately so differences
can be resolved with retained field accesses and object-size evidence before
being accepted as Ballance-specific variants.
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, replace
from pathlib import Path
from types import ModuleType


def load_member_audit() -> ModuleType:
    path = Path(__file__).with_name("Audit-IvpPublicInterface.py")
    spec = importlib.util.spec_from_file_location("ivp_public_field_helpers", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load audit helpers from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


AUDIT = load_member_audit()


@dataclass(frozen=True)
class Field:
    owner: str
    name: str
    type: str
    order: int
    bit_width: int | None
    header: str
    access: str
    source_position: tuple[int, int, int]

    @property
    def key(self) -> tuple[str, str]:
        return self.owner, self.name


@dataclass(frozen=True)
class FieldEvidence:
    owner: str
    field: str
    comparison: str
    classification: str
    source: str
    note: str

    @property
    def key(self) -> tuple[str, str]:
        return self.owner, self.field


FIELD_EVIDENCE_CLASSIFICATIONS = {
    "retail-field-absent",
    "compiler-safe-storage-variant",
    "retail-compatible-storage-view",
}


# Clang preserves typedef spelling in ``qualType``.  The public surface uses
# both the historical IVP names and fixed-width spellings, although all of
# these aliases have the same x86 ABI.  Canonicalise only aliases whose width
# is locked by Types.h; enums and pointer pointees deliberately remain distinct.
FIELD_TYPE_ALIASES = (
    (r"\bstd::int32_t\b", "int"),
    (r"\bIVP_INT32\b", "int"),
    (r"\bIVP_Time_CODE\b", "int"),
    (r"\bstd::uint32_t\b", "unsigned int"),
    (r"\bIVP_UINT32\b", "unsigned int"),
    (r"\bstd::int16_t\b", "short"),
    (r"\bstd::uint16_t\b", "unsigned short"),
    (r"\bstd::int64_t\b", "long long"),
    (r"\bstd::uint64_t\b", "unsigned long long"),
    (r"\bIVP_FLOAT\b", "float"),
    (r"\bIVP_DOUBLE\b", "double"),
)


def normalize_field_type(value: str) -> str:
    value = AUDIT.normalize_signature(value)
    for pattern, replacement in FIELD_TYPE_ALIASES:
        value = re.sub(pattern, replacement, value)
    # Anonymous record spellings include a source path and line number.  That
    # provenance differs between the nearby and reconstructed headers and is
    # not a C++ type/ABI distinction.  Preserve the record kind so an unnamed
    # union is never equated with an unnamed struct or enum.
    value = re.sub(
        r"\b(unnamed(?: union| struct| class| enum)?|anonymous(?: union| struct| class| enum)?)"
        r" at [^)]+",
        r"\1",
        value,
    )
    return value


def constant_value(node: dict) -> int | None:
    stack = [node]
    while stack:
        value = stack.pop()
        raw = value.get("value")
        if isinstance(raw, str):
            try:
                return int(raw, 0)
            except ValueError:
                pass
        stack.extend(value.get("inner", []))
    return None


def extract_public_fields(
    ast: dict,
    declaration_headers: list[Path] | None = None,
    accepted_owners: set[str] | None = None,
    public_only: bool = True,
) -> dict[tuple[str, str], Field]:
    allowed_headers = None
    if declaration_headers is not None:
        allowed_headers = {
            AUDIT.normalize_path(path): (path, path.read_bytes())
            for path in declaration_headers
        }

    # Anonymous-union promotion is represented by a type-less
    # ``IndirectFieldDecl`` on the outer class plus the real ``FieldDecl`` in
    # the nested record.  Index the latter so public union-backed storage is
    # not reported as missing merely because we suppress host-side automatic
    # construction/destruction with an anonymous union.
    indirect_types: dict[tuple[int | None, int | None, str], tuple[str, int | None]] = {}
    scan = [ast]
    while scan:
        value = scan.pop()
        scan.extend(value.get("inner", []))
        if value.get("kind") != "FieldDecl" or not value.get("name"):
            continue
        location = AUDIT.spelling_location(value.get("loc", {}))
        key = (location.get("line"), location.get("col"), value["name"])
        indirect_types[key] = (
            normalize_field_type(value.get("type", {}).get("qualType", "")),
            constant_value(value) if value.get("isBitfield") else None,
        )

    result: dict[tuple[str, str], Field] = {}
    stack = [ast]
    while stack:
        node = stack.pop()
        stack.extend(node.get("inner", []))
        if node.get("kind") != "CXXRecordDecl" or not node.get(
            "completeDefinition"
        ):
            continue
        owner = node.get("name", "")
        if not AUDIT.is_public_owner_name(owner):
            continue
        if accepted_owners is not None and owner not in accepted_owners:
            continue
        owner_header = None
        if allowed_headers is not None:
            owner_header = AUDIT.owned_header(node, allowed_headers)
            if owner_header is None:
                continue

        access = "private" if node.get("tagUsed") == "class" else "public"
        order = 0
        anonymous_order: int | None = None
        anonymous_access = access
        for child in node.get("inner", []):
            if child.get("kind") == "AccessSpecDecl":
                access = child.get("access", access)
                continue
            if child.get("kind") == "IndirectFieldDecl" and child.get("name"):
                if anonymous_order is None:
                    continue
                location = AUDIT.spelling_location(child.get("loc", {}))
                key = (location.get("line"), location.get("col"), child["name"])
                indirect = indirect_types.get(key)
                if indirect is None:
                    continue
                if public_only and anonymous_access != "public":
                    continue
                field = Field(
                    owner=owner,
                    name=child["name"],
                    type=indirect[0],
                    order=anonymous_order,
                    bit_width=indirect[1],
                    header=owner_header[0].name if owner_header else "",
                    access=anonymous_access,
                    source_position=(
                        int(location.get("offset") or 0),
                        int(location.get("line") or 0),
                        int(location.get("col") or 0),
                    ),
                )
                result[field.key] = field
                continue
            if child.get("kind") != "FieldDecl":
                continue
            if owner_header is not None and AUDIT.location_is_internal(
                AUDIT.spelling_location(child.get("loc", {})), owner_header[1]
            ):
                continue
            field_order = order
            order += 1
            anonymous_order = field_order if not child.get("name") else None
            anonymous_access = access
            if (public_only and access != "public") or not child.get("name"):
                continue
            field = Field(
                owner=owner,
                name=child["name"],
                type=normalize_field_type(
                    child.get("type", {}).get("qualType", "")
                ),
                order=field_order,
                bit_width=(constant_value(child) if child.get("isBitfield") else None),
                header=owner_header[0].name if owner_header else "",
                access=access,
                source_position=(lambda location: (
                    int(location.get("offset") or 0),
                    int(location.get("line") or 0),
                    int(location.get("col") or 0),
                ))(AUDIT.spelling_location(child.get("loc", {}))),
            )
            previous = result.get(field.key)
            if previous is not None and previous != field:
                raise RuntimeError(
                    f"incompatible public field declarations for "
                    f"{owner}::{field.name}: {previous} / {field}"
                )
            result[field.key] = field
    return result


def rank_common_field_order(
    reference: dict[tuple[str, str], Field],
    current: dict[tuple[str, str], Field],
) -> tuple[dict[tuple[str, str], Field], dict[tuple[str, str], Field]]:
    """Rank only fields present in both surfaces by declaration order.

    A field deliberately absent in the retail ABI must be reported once as
    missing; it must not make every later field look reordered.  Likewise,
    raw storage aliases and explicit padding in the reconstructed type are not
    part of the nearby public denominator.  Source positions also give fields
    promoted from an anonymous union their proper nested declaration order.
    """

    common = set(reference).intersection(current)
    ranked_reference = dict(reference)
    ranked_current = dict(current)
    owners = {owner for owner, _ in common}
    for owner in owners:
        owner_keys = [key for key in common if key[0] == owner]
        for fields, destination in (
            (reference, ranked_reference), (current, ranked_current)
        ):
            ordered = sorted(
                owner_keys,
                key=lambda key: (fields[key].source_position, key[1]),
            )
            for rank, key in enumerate(ordered):
                destination[key] = replace(fields[key], order=rank)
    return ranked_reference, ranked_current


def field_disposition(reference: Field, current: Field | None) -> str:
    if current is None:
        return "missing"
    if current.access != "public":
        return "access-difference"
    if current.type != reference.type:
        return "type-difference"
    if current.bit_width != reference.bit_width:
        return "bit-width-difference"
    if current.order != reference.order:
        return "order-difference"
    return "exact"


def load_field_evidence(path: Path) -> dict[tuple[str, str], FieldEvidence]:
    if not path.is_file():
        raise RuntimeError(f"public field evidence ledger not found: {path}")
    result: dict[tuple[str, str], FieldEvidence] = {}
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        expected = {
            "class", "field", "comparison", "classification", "source", "note"
        }
        if set(reader.fieldnames or ()) != expected:
            raise RuntimeError(
                f"unexpected public field evidence columns: {reader.fieldnames}"
            )
        for row in reader:
            evidence = FieldEvidence(
                owner=row["class"], field=row["field"],
                comparison=row["comparison"],
                classification=row["classification"],
                source=row["source"], note=row["note"],
            )
            if evidence.classification not in FIELD_EVIDENCE_CLASSIFICATIONS:
                raise RuntimeError(
                    f"unknown field evidence classification for "
                    f"{evidence.owner}::{evidence.field}: "
                    f"{evidence.classification}"
                )
            if evidence.key in result:
                raise RuntimeError(
                    f"duplicate public field evidence for "
                    f"{evidence.owner}::{evidence.field}"
                )
            result[evidence.key] = evidence
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).parents[2])
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--clang", type=Path, default=Path(shutil.which("clang++") or "clang++"))
    parser.add_argument(
        "--evidence", type=Path,
        default=Path("tools/ivp/public-field-evidence.tsv"),
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--format", choices=("fields-tsv", "summary-tsv"),
        default="fields-tsv",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    evidence_path = args.evidence
    if not evidence_path.is_absolute():
        evidence_path = repo_root / evidence_path
    evidence = load_field_evidence(evidence_path)
    contract_check = subprocess.run(
        [sys.executable, str(repo_root / "tools/ivp/sync_retail_contract.py"), "--check"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if contract_check.returncode != 0:
        detail = contract_check.stderr.strip() or contract_check.stdout.strip()
        raise RuntimeError(f"IVP Retail Contract views are stale: {detail}")

    reference_root = args.reference_root.resolve()
    clang = args.clang.resolve()
    if not clang.is_file():
        raise RuntimeError(f"clang not found: {clang}")
    headers = AUDIT.find_reference_headers(reference_root)
    include_directories = sorted({path.parent for path in headers})
    for directory_name in (
        "ivp_physics", "ivp_utility", "ivp_collision",
        "ivp_controller", "ivp_compact_builder",
    ):
        path = reference_root / directory_name
        if path.is_dir() and path not in include_directories:
            include_directories.append(path)

    reference_ast = AUDIT.clang_ast(
        clang, AUDIT.reference_source(headers), "c++14", include_directories
    )
    reference = extract_public_fields(reference_ast, headers)
    owners = {field.owner for field in reference.values()}
    current_ast = AUDIT.clang_ast(
        clang, '#include "BML/IVP/IVP.h"\n', "c++17",
        [repo_root / "include"],
        ["_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH"],
    )
    current = extract_public_fields(
        current_ast, accepted_owners=owners, public_only=False
    )
    reference, current = rank_common_field_order(reference, current)

    dispositions = {
        key: field_disposition(field, current.get(key))
        for key, field in reference.items()
    }
    differences = {
        key: disposition
        for key, disposition in dispositions.items()
        if disposition != "exact"
    }
    unresolved = sorted(set(differences).difference(evidence))
    stale = sorted(set(evidence).difference(differences))
    mismatched = sorted(
        key for key in set(differences).intersection(evidence)
        if evidence[key].comparison != differences[key]
    )
    if args.check and (unresolved or stale or mismatched):
        details = []
        if unresolved:
            details.append(
                "unresolved=" + ",".join(f"{a}::{b}" for a, b in unresolved)
            )
        if stale:
            details.append(
                "stale=" + ",".join(f"{a}::{b}" for a, b in stale)
            )
        if mismatched:
            details.append(
                "comparison-mismatch=" +
                ",".join(f"{a}::{b}" for a, b in mismatched)
            )
        raise RuntimeError("public field evidence is not closed: " + "; ".join(details))
    if args.format == "summary-tsv":
        writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
        writer.writerow(["metric", "count", "denominator"])
        writer.writerow(["candidate_public_fields", len(reference), len(reference)])
        for value in (
            "exact", "access-difference", "type-difference", "bit-width-difference",
            "order-difference", "missing",
        ):
            writer.writerow([
                value.replace("-", "_"),
                sum(item == value for item in dispositions.values()),
                len(reference),
            ])
        writer.writerow([
            "accounted_differences",
            len(set(differences).intersection(evidence)) - len(mismatched),
            len(differences),
        ])
        writer.writerow(["unresolved_differences", len(unresolved), len(differences)])
        writer.writerow(["stale_evidence", len(stale), len(evidence)])
    else:
        writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "class", "field", "nearby_type", "current_type",
            "nearby_order", "current_order", "nearby_bit_width",
            "current_bit_width", "current_access", "disposition", "nearby_header",
            "classification", "evidence_source", "evidence_note",
        ])
        for key in sorted(reference):
            field = reference[key]
            current_field = current.get(key)
            field_evidence = evidence.get(key)
            writer.writerow([
                field.owner, field.name, field.type,
                current_field.type if current_field else "",
                field.order,
                current_field.order if current_field else "",
                field.bit_width if field.bit_width is not None else "",
                (
                    current_field.bit_width
                    if current_field is not None
                    and current_field.bit_width is not None
                    else ""
                ),
                current_field.access if current_field else "",
                dispositions[key], field.header,
                field_evidence.classification if field_evidence else "",
                field_evidence.source if field_evidence else "",
                field_evidence.note if field_evidence else "",
            ])
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
