#!/usr/bin/env python3
"""Audit nearby public IVP free functions against the Ballance API surface.

The member-method audit deliberately has a class-method denominator.  This
companion audit closes the other callable part of the source-level API:
namespace/global FunctionDecl nodes physically owned by nearby headers marked
IVP_EXPORT_PUBLIC and outside their INTERN_START/INTERN_END fences.

The nearby tree is evidence, not ground truth.  Exact Ballance decorated names
and RVAs are reported independently from header definitions so a missing
function can be assigned to a retained DLL body, a compatible reconstruction,
or an intentional retail omission only after review.
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType


def load_member_audit_helpers() -> ModuleType:
    """Load the sibling audit despite its command-oriented hyphenated name."""
    path = Path(__file__).with_name("Audit-IvpPublicInterface.py")
    spec = importlib.util.spec_from_file_location("ivp_public_member_audit", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load audit helpers from {path}")
    module = importlib.util.module_from_spec(spec)
    # dataclasses resolves annotations through the defining module while the
    # helper is imported, so publish it before executing the module body.
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


MEMBER_AUDIT = load_member_audit_helpers()


def normalize_free_signature(value: str) -> str:
    value = MEMBER_AUDIT.normalize_signature(value)
    # On the x86 MS target Clang sometimes prints an explicitly spelled cdecl
    # attribute while omitting the ABI-default spelling from the same nearby
    # declaration. They have the same decorated name and calling convention.
    return re.sub(r"\s*__attribute__\(\(cdecl\)\)\s*$", "", value)


@dataclass(frozen=True)
class FreeFunction:
    namespace: str
    name: str
    signature: str
    mangled: str
    has_body: bool
    retail_invoke: bool
    retail_address_ids: tuple[str, ...]
    header: str

    @property
    def key(self) -> tuple[str, str, str]:
        return self.namespace, self.name, self.signature

    @property
    def qualified_name(self) -> str:
        return (
            f"{self.namespace}::{self.name}" if self.namespace else self.name
        )


@dataclass(frozen=True)
class FreeFunctionEvidence:
    namespace: str
    function: str
    signature: str
    disposition: str
    source: str
    note: str

    @property
    def key(self) -> tuple[str, str, str]:
        return self.namespace, self.function, self.signature


EVIDENCE_DISPOSITIONS = {
    "reconstructed-nearby",
    "reconstructed-adjacent-version",
    "retail-omitted",
}


def load_evidence(
    path: Path,
) -> dict[tuple[str, str, str], FreeFunctionEvidence]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        rows = csv.DictReader(stream, delimiter="\t")
        required = {
            "namespace", "function", "signature", "disposition", "source", "note"
        }
        if rows.fieldnames is None or not required.issubset(rows.fieldnames):
            raise RuntimeError(f"invalid free-function evidence ledger {path}")
        result = {}
        for line, row in enumerate(rows, start=2):
            evidence = FreeFunctionEvidence(
                namespace=row["namespace"].strip(),
                function=row["function"].strip(),
                signature=normalize_free_signature(row["signature"]),
                disposition=row["disposition"].strip(),
                source=row["source"].strip(),
                note=row["note"].strip(),
            )
            if evidence.disposition not in EVIDENCE_DISPOSITIONS:
                raise RuntimeError(
                    f"unknown disposition {evidence.disposition!r} at {path}:{line}"
                )
            if evidence.key in result:
                raise RuntimeError(
                    f"duplicate free-function evidence at {path}:{line}: {evidence.key}"
                )
            result[evidence.key] = evidence
    return result


def validate_evidence(
    reference: dict[tuple[str, str, str], FreeFunction],
    current: dict[tuple[str, str, str], FreeFunction],
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    evidence: dict[tuple[str, str, str], FreeFunctionEvidence],
) -> list[str]:
    problems = []
    unknown = sorted(evidence.keys() - reference.keys())
    if unknown:
        problems.append(f"evidence refers to unknown candidates: {unknown}")
    for key, item in reference.items():
        current_item = current.get(key)
        if item.mangled and item.mangled in retail_symbols:
            if key in evidence:
                problems.append(
                    f"retained retail body has unnecessary reconstruction evidence: "
                    f"{item.qualified_name} {item.signature}"
                )
            route = exact_retail_route(
                item, current_item, retail_symbols, retail_addresses
            )
            if route != "retail-address":
                problems.append(
                    f"retained retail body is not called by exact Address: "
                    f"{item.qualified_name} {item.signature} ({route})"
                )
            continue

        entry = evidence.get(key)
        if entry is None:
            problems.append(
                f"non-retained candidate has no reviewed disposition: "
                f"{item.qualified_name} {item.signature}"
            )
            continue
        if entry.disposition.startswith("reconstructed-"):
            if current_item is None or not current_item.has_body:
                problems.append(
                    f"reviewed reconstruction is missing a header body: "
                    f"{item.qualified_name} {item.signature}"
                )
        elif entry.disposition == "retail-omitted" and current_item is not None:
            problems.append(
                f"retail-omitted candidate is present in current headers: "
                f"{item.qualified_name} {item.signature}"
            )
    return problems


def merge_function(
    result: dict[tuple[str, str, str], FreeFunction],
    function: FreeFunction,
) -> None:
    previous = result.get(function.key)
    if previous is None:
        result[function.key] = function
        return
    mangled_names = {value for value in (previous.mangled, function.mangled) if value}
    if len(mangled_names) > 1:
        raise RuntimeError(
            f"incompatible decorated names for {function.qualified_name} "
            f"{function.signature}: {sorted(mangled_names)}"
        )
    result[function.key] = FreeFunction(
        namespace=function.namespace,
        name=function.name,
        signature=function.signature,
        mangled=next(iter(mangled_names), ""),
        has_body=previous.has_body or function.has_body,
        retail_invoke=previous.retail_invoke or function.retail_invoke,
        retail_address_ids=tuple(sorted(set(
            previous.retail_address_ids + function.retail_address_ids
        ))),
        header=previous.header or function.header,
    )


def extract_free_functions(
    ast: dict,
    declaration_headers: list[Path] | None = None,
    accepted_keys: set[tuple[str, str, str]] | None = None,
    accepted_names: set[tuple[str, str]] | None = None,
) -> dict[tuple[str, str, str], FreeFunction]:
    allowed_headers = None
    if declaration_headers is not None:
        allowed_headers = {
            MEMBER_AUDIT.normalize_path(path): (path, path.read_bytes())
            for path in declaration_headers
        }

    result: dict[tuple[str, str, str], FreeFunction] = {}

    def visit(node: dict, namespaces: tuple[str, ...], in_record: bool) -> None:
        kind = node.get("kind")
        next_namespaces = namespaces
        if kind == "NamespaceDecl" and node.get("name"):
            next_namespaces = namespaces + (node["name"],)
        next_in_record = in_record or kind in {
            "CXXRecordDecl",
            "ClassTemplateSpecializationDecl",
            "ClassTemplatePartialSpecializationDecl",
        }

        if kind == "FunctionDecl" and not in_record and not node.get("isImplicit"):
            owner = None
            if allowed_headers is not None:
                owner = MEMBER_AUDIT.owned_header(node, allowed_headers)
                if owner is None:
                    return
            nested = node.get("inner", [])
            function = FreeFunction(
                    namespace="::".join(namespaces),
                    name=node.get("name", ""),
                    signature=normalize_free_signature(
                        node.get("type", {}).get("qualType", "")
                    ),
                    mangled=node.get("mangledName", ""),
                    has_body=any(
                        child.get("kind") == "CompoundStmt" for child in nested
                    ),
                    retail_invoke=MEMBER_AUDIT.invokes_retail_abi(node),
                    retail_address_ids=MEMBER_AUDIT.retail_abi_address_ids(node),
                    header=owner[0].name if owner is not None else "",
                )
            if (
                (accepted_keys is None or function.key in accepted_keys)
                and (
                    accepted_names is None
                    or (function.namespace, function.name) in accepted_names
                )
            ):
                merge_function(result, function)
            # Function bodies cannot contain namespace-scope API declarations.
            return

        for child in node.get("inner", []):
            visit(child, next_namespaces, next_in_record)

    visit(ast, (), False)
    return result


def same_named_keys(
    functions: dict[tuple[str, str, str], FreeFunction],
) -> set[tuple[str, str]]:
    return {(item.namespace, item.name) for item in functions.values()}


def disposition(
    reference: FreeFunction,
    current: dict[tuple[str, str, str], FreeFunction],
    current_names: set[tuple[str, str]],
    retail_symbols: dict[str, int],
    evidence: dict[tuple[str, str, str], FreeFunctionEvidence],
) -> str:
    entry = evidence.get(reference.key)
    if entry is not None and entry.disposition == "retail-omitted":
        return (
            "retail-omission-violation"
            if reference.key in current
            else "retail-omitted"
        )
    if entry is not None and entry.disposition.startswith("reconstructed-"):
        candidate = current.get(reference.key)
        return (
            entry.disposition
            if candidate is not None and candidate.has_body
            else "reviewed-reconstruction-missing"
        )
    if reference.key in current:
        return "retail-wrapper"
    if (reference.namespace, reference.name) in current_names:
        return "current-signature-diff"
    if reference.mangled and reference.mangled in retail_symbols:
        return "retail-body-missing"
    if reference.has_body:
        return "inline-candidate-missing"
    return "unresolved-missing"


def exact_retail_route(
    reference: FreeFunction,
    current: FreeFunction | None,
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
) -> str:
    if not reference.mangled or reference.mangled not in retail_symbols:
        return "not-in-retail-image"
    if current is None:
        return "missing-current-declaration"
    expected = retail_addresses.get(retail_symbols[reference.mangled], set())
    if expected.intersection(current.retail_address_ids):
        return "retail-address"
    return "missing-retail-route"


def callable_disposition(
    reference: FreeFunction,
    current: FreeFunction | None,
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
) -> str:
    if current is None:
        return "not-current-exact"
    if not current.has_body:
        return "declaration-only"
    if exact_retail_route(
        reference, current, retail_symbols, retail_addresses
    ) == "retail-address":
        return "header-retail-wrapper"
    return "header-reconstruction"


def write_function_rows(
    reference: dict[tuple[str, str, str], FreeFunction],
    current: dict[tuple[str, str, str], FreeFunction],
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    evidence: dict[tuple[str, str, str], FreeFunctionEvidence],
) -> None:
    current_names = same_named_keys(current)
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow([
        "namespace",
        "function",
        "signature",
        "disposition",
        "callable_disposition",
        "nearby_header",
        "nearby_has_body",
        "current_signature",
        "current_has_body",
        "current_retail_invoke",
        "current_retail_address_ids",
        "retail_exact_name",
        "retail_rva",
        "exact_retail_body_route",
        "nearby_mangled_name",
        "review_source",
        "review_note",
    ])
    for key in sorted(reference):
        item = reference[key]
        current_item = current.get(key)
        current_signatures = sorted({
            value.signature
            for value in current.values()
            if value.namespace == item.namespace and value.name == item.name
        })
        retail_rva = (
            retail_symbols.get(item.mangled) if item.mangled else None
        )
        reviewed = evidence.get(item.key)
        writer.writerow([
            item.namespace,
            item.name,
            item.signature,
            disposition(item, current, current_names, retail_symbols, evidence),
            callable_disposition(
                item, current_item, retail_symbols, retail_addresses
            ),
            item.header,
            str(item.has_body).lower(),
            ";".join(current_signatures),
            str(current_item.has_body).lower() if current_item else "",
            str(current_item.retail_invoke).lower() if current_item else "",
            ";".join(current_item.retail_address_ids) if current_item else "",
            str(retail_rva is not None).lower(),
            f"{retail_rva:08X}" if retail_rva is not None else "",
            exact_retail_route(
                item, current_item, retail_symbols, retail_addresses
            ),
            item.mangled,
            reviewed.source if reviewed else "physics_RT.dll",
            reviewed.note if reviewed else "Exact decorated retail body and Address route.",
        ])


def write_summary_rows(
    reference: dict[tuple[str, str, str], FreeFunction],
    current: dict[tuple[str, str, str], FreeFunction],
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    evidence: dict[tuple[str, str, str], FreeFunctionEvidence],
) -> None:
    current_names = same_named_keys(current)
    values = list(reference.values())
    dispositions = [
        disposition(item, current, current_names, retail_symbols, evidence)
        for item in values
    ]
    callability = [
        callable_disposition(
            item, current.get(item.key), retail_symbols, retail_addresses
        )
        for item in values
    ]
    routes = [
        exact_retail_route(
            item, current.get(item.key), retail_symbols, retail_addresses
        )
        for item in values
        if item.mangled and item.mangled in retail_symbols
    ]
    rows = [
        ("candidate_free_functions", len(values), len(values)),
        ("retail_wrapper", dispositions.count("retail-wrapper"), len(values)),
        ("reconstructed_nearby", dispositions.count("reconstructed-nearby"), len(values)),
        ("reconstructed_adjacent_version", dispositions.count("reconstructed-adjacent-version"), len(values)),
        ("retail_omitted", dispositions.count("retail-omitted"), len(values)),
        ("accounted", sum(value in {
            "retail-wrapper", "reconstructed-nearby",
            "reconstructed-adjacent-version", "retail-omitted",
        } for value in dispositions), len(values)),
        ("current_signature_difference", dispositions.count("current-signature-diff"), len(values)),
        ("retail_body_missing", dispositions.count("retail-body-missing"), len(values)),
        ("inline_candidate_missing", dispositions.count("inline-candidate-missing"), len(values)),
        ("unresolved_missing", dispositions.count("unresolved-missing"), len(values)),
        ("current_header_retail_wrapper", callability.count("header-retail-wrapper"), len(values)),
        ("current_header_reconstruction", callability.count("header-reconstruction"), len(values)),
        ("current_declaration_only", callability.count("declaration-only"), len(values)),
        ("retail_exact_named_body", len(routes), len(values)),
        ("current_exact_retail_body_invoke", routes.count("retail-address"), len(routes)),
        ("current_exact_retail_body_missing_route", routes.count("missing-retail-route"), len(routes)),
    ]
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(["metric", "count", "denominator"])
    writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).parents[2])
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--clang", type=Path, default=Path(shutil.which("clang++") or "clang++"))
    parser.add_argument("--evidence-ledger", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--format", choices=("functions-tsv", "summary-tsv"),
        default="functions-tsv",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    contract_check = subprocess.run(
        [sys.executable, str(repo_root / "tools/ivp/sync_retail_contract.py"), "--check"],
        cwd=repo_root,
        check=False,
        capture_output=True,
        text=True,
    )
    if contract_check.returncode != 0:
        detail = contract_check.stderr.strip() or contract_check.stdout.strip()
        raise RuntimeError(f"IVP Retail Contract views are stale: {detail}")

    reference_root = args.reference_root.resolve()
    clang = args.clang.resolve()
    if not clang.is_file():
        raise RuntimeError(f"clang not found: {clang}")
    headers = MEMBER_AUDIT.find_reference_headers(reference_root)
    reference_includes = sorted({header.parent for header in headers})
    for directory_name in (
        "ivp_physics",
        "ivp_utility",
        "ivp_collision",
        "ivp_controller",
        "ivp_compact_builder",
    ):
        path = reference_root / directory_name
        if path.is_dir() and path not in reference_includes:
            reference_includes.append(path)

    reference_ast = MEMBER_AUDIT.clang_ast(
        clang,
        MEMBER_AUDIT.reference_source(headers),
        "c++14",
        reference_includes,
    )
    current_ast = MEMBER_AUDIT.clang_ast(
        clang,
        '#include "BML/IVP/IVP.h"\n',
        "c++17",
        [repo_root / "include"],
        ["_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH"],
    )
    reference = extract_free_functions(reference_ast, headers)
    current = extract_free_functions(
        current_ast,
        accepted_names=same_named_keys(reference),
    )
    retail_symbols = MEMBER_AUDIT.load_retail_manifest(
        repo_root / MEMBER_AUDIT.RETAIL_MANIFEST_PATH
    )
    retail_addresses = MEMBER_AUDIT.load_retail_addresses(
        repo_root / MEMBER_AUDIT.RETAIL_ADDRESS_PATH
    )
    evidence_path = (
        args.evidence_ledger.resolve()
        if args.evidence_ledger
        else repo_root / "tools/ivp/public-free-function-evidence.tsv"
    )
    evidence = load_evidence(evidence_path)
    problems = validate_evidence(
        reference, current, retail_symbols, retail_addresses, evidence
    )
    if args.check and problems:
        raise RuntimeError("free-function closure failed:\n" + "\n".join(problems))

    if args.format == "summary-tsv":
        write_summary_rows(
            reference, current, retail_symbols, retail_addresses, evidence
        )
    else:
        write_function_rows(
            reference, current, retail_symbols, retail_addresses, evidence
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
