#!/usr/bin/env python3
"""Generate and verify the checked-in views of Ballance's IVP retail contract.

retail-contract.json is curated evidence, not an IDA export.  IDA types and
nearby IVP sources may support a record, but neither is promoted to ground truth
by this generator.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable


TOOLS_DIR = Path(__file__).resolve().parent
ROOT = TOOLS_DIR.parent.parent
CATALOG_PATH = TOOLS_DIR / "retail-contract.json"

GENERATED_PATHS = {
    "addresses": ROOT / "include/BML/IVP/detail/AddressEntries.inc",
    "data_addresses": ROOT / "include/BML/IVP/detail/DataAddresses.inc",
    "runtime_image": ROOT / "src/IVP/generated/IvpRetailImage.inc",
    "direct_dependencies": TOOLS_DIR / "physics-rt-api-coverage.tsv",
    "indirect_callsites": TOOLS_DIR / "physics-rt-indirect-api-coverage.tsv",
    "public_evidence": TOOLS_DIR / "public-api-evidence.tsv",
}

DIRECT_FIELDS = (
    "rva", "symbol", "category", "disposition", "public_header", "probe",
    "address_id",
)
INDIRECT_FIELDS = (
    "callsite_rva", "owner", "contract", "slot", "public_header", "probe",
)
EVIDENCE_FIELDS = ("owner", "method", "mangled", "evidence", "source", "note")

EXPECTED_ADDRESS_COUNT = 685
EXPECTED_DATA_ADDRESS_COUNT = 5
EXPECTED_DIRECT_DEPENDENCY_COUNT = 681
EXPECTED_INDIRECT_CALLSITE_COUNT = 14
EXPECTED_PUBLIC_EVIDENCE_COUNT = 1071
EXPECTED_INSTRUCTION_ANCHOR_COUNT = 5
EXPECTED_NONPUBLIC_ADDRESS_IDS = {
    "EnvironmentSimulatePsi",
    "MindistHullLimitExceeded",
    "MindistHullManagerReset",
    "VHashStoreRehash",
}


def canonical_rva(value: str) -> str:
    number = int(value, 16)
    if number < 0 or number > 0xFFFFFFFF:
        raise ValueError(f"RVA outside uint32: {value!r}")
    return f"{number:08X}"


def load_catalog() -> dict:
    with CATALOG_PATH.open("r", encoding="utf-8") as stream:
        catalog = json.load(stream)
    if catalog.get("schema_version") != 1:
        raise ValueError("Unsupported retail-contract.json schema_version")
    return catalog


def validate(catalog: dict) -> None:
    image = catalog["image"]
    if image["architecture"] != "x86":
        raise ValueError("The Ballance retail contract must remain x86")
    if len(image["sha256"]) != 64:
        raise ValueError("image.sha256 must contain 64 hexadecimal digits")
    int(image["sha256"], 16)
    canonical_rva(image["timestamp"])
    canonical_rva(image["image_size"])
    canonical_rva(image["manager_get_physics_object_rva"])

    anchors = image["instruction_anchors"]
    if len(anchors) != EXPECTED_INSTRUCTION_ANCHOR_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_INSTRUCTION_ANCHOR_COUNT} instruction anchors, "
            f"got {len(anchors)}"
        )
    for anchor in anchors:
        canonical_rva(anchor["rva"])
        raw = bytes.fromhex(anchor["bytes"])
        if not raw or len(raw) > 16:
            raise ValueError(f"Invalid instruction anchor at {anchor['rva']}")
    for name, value in image["runtime_layout"].items():
        try:
            canonical_rva(value)
        except (TypeError, ValueError) as error:
            raise ValueError(f"Invalid runtime layout offset {name}={value!r}") from error

    addresses = catalog["addresses"]
    if len(addresses) != EXPECTED_ADDRESS_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_ADDRESS_COUNT} Address records, got {len(addresses)}"
        )
    ids = [entry["id"] for entry in addresses]
    if len(ids) != len(set(ids)):
        duplicate = [key for key, count in Counter(ids).items() if count > 1]
        raise ValueError(f"Duplicate Address ids: {duplicate}")
    address_rvas: dict[str, list[str]] = defaultdict(list)
    for entry in addresses:
        if not entry["id"].isidentifier():
            raise ValueError(f"Invalid Address id: {entry['id']!r}")
        entry["rva"] = canonical_rva(entry["rva"])
        address_rvas[entry["rva"]].append(entry["id"])
    allowed_aliases = {
        canonical_rva(alias["rva"]): sorted(alias["ids"])
        for alias in catalog.get("address_aliases", [])
    }
    actual_aliases = {
        rva: sorted(names)
        for rva, names in address_rvas.items()
        if len(names) > 1
    }
    if actual_aliases != allowed_aliases:
        raise ValueError(
            f"Address RVA aliases differ: actual={actual_aliases}, "
            f"allowed={allowed_aliases}"
        )

    data_addresses = catalog.get("data_addresses", [])
    if len(data_addresses) != EXPECTED_DATA_ADDRESS_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_DATA_ADDRESS_COUNT} data Address records, "
            f"got {len(data_addresses)}"
        )
    data_ids = set()
    for entry in data_addresses:
        if not entry["id"].isidentifier() or entry["id"] in data_ids:
            raise ValueError(f"Invalid or duplicate data Address id: {entry['id']!r}")
        data_ids.add(entry["id"])
        entry["rva"] = canonical_rva(entry["rva"])

    direct = catalog["direct_dependencies"]
    if len(direct) != EXPECTED_DIRECT_DEPENDENCY_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_DIRECT_DEPENDENCY_COUNT} direct dependency "
            f"records, got {len(direct)}"
        )
    direct_rvas = [canonical_rva(row["rva"]) for row in direct]
    if len(direct_rvas) != len(set(direct_rvas)):
        raise ValueError("Direct dependency RVAs must be unique")
    allowed_dispositions = {"typed-wrapper", "reconstructed-inline", "adapter-internal"}
    address_by_id = {entry["id"]: entry["rva"] for entry in addresses}
    for row, rva in zip(direct, direct_rvas):
        row["rva"] = rva
        if row["disposition"] not in allowed_dispositions:
            raise ValueError(f"Invalid direct disposition at {rva}")
        address_id = row.get("address_id", "")
        if row["disposition"] == "typed-wrapper":
            if address_by_id.get(address_id) != rva:
                raise ValueError(
                    f"Direct dependency {rva} disagrees with Address::{address_id}"
                )
        elif address_id:
            raise ValueError(f"Non-wrapper direct dependency {rva} has Address id")

    public_address_ids: set[str] = set()
    address_pattern = re.compile(r"\bAddress::([A-Za-z_][A-Za-z0-9_]*)\b")
    for header in (ROOT / "include/BML/IVP").rglob("*.h"):
        public_address_ids.update(
            address_pattern.findall(header.read_text(encoding="utf-8"))
        )
    unresolved_ids = public_address_ids - address_by_id.keys()
    if unresolved_ids:
        raise ValueError(
            f"Public headers use unknown Address ids: {sorted(unresolved_ids)}"
        )
    nonpublic_ids = address_by_id.keys() - public_address_ids
    if nonpublic_ids != EXPECTED_NONPUBLIC_ADDRESS_IDS:
        raise ValueError(
            "Known non-public Address ids differ: "
            f"actual={sorted(nonpublic_ids)}, "
            f"expected={sorted(EXPECTED_NONPUBLIC_ADDRESS_IDS)}"
        )
    public_rvas = {address_by_id[address_id] for address_id in public_address_ids}
    typed_rows = [row for row in direct if row["disposition"] == "typed-wrapper"]
    typed_rvas = {row["rva"] for row in typed_rows}
    if typed_rvas != public_rvas:
        missing = sorted(public_rvas - typed_rvas)
        stale = sorted(typed_rvas - public_rvas)
        raise ValueError(
            "Direct dependency reverse closure differs from public headers: "
            f"missing={missing}, stale={stale}"
        )
    stale_ids = sorted(
        row["address_id"] for row in typed_rows
        if row["address_id"] not in public_address_ids
    )
    if stale_ids:
        raise ValueError(
            f"Typed dependency Address ids are not used publicly: {stale_ids}"
        )

    indirect = catalog["indirect_callsites"]
    if len(indirect) != EXPECTED_INDIRECT_CALLSITE_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_INDIRECT_CALLSITE_COUNT} indirect callsites, "
            f"got {len(indirect)}"
        )
    indirect_rvas = [canonical_rva(row["callsite_rva"]) for row in indirect]
    if len(indirect_rvas) != len(set(indirect_rvas)):
        raise ValueError("Indirect callsite RVAs must be unique")
    for row, rva in zip(indirect, indirect_rvas):
        row["callsite_rva"] = rva

    if len(catalog["public_evidence"]) != EXPECTED_PUBLIC_EVIDENCE_COUNT:
        raise ValueError(
            f"Expected {EXPECTED_PUBLIC_EVIDENCE_COUNT} public evidence records, "
            f"got {len(catalog['public_evidence'])}"
        )

    for relative_path in catalog["artifacts"].values():
        if not (ROOT / relative_path).is_file():
            raise ValueError(f"Retail-contract artifact is missing: {relative_path}")


def comment_lines(note: str) -> Iterable[str]:
    for paragraph in note.splitlines():
        paragraph = paragraph.strip()
        if paragraph:
            yield f"// {paragraph}"


def render_addresses(catalog: dict) -> str:
    lines = [
        "// Generated by tools/ivp/sync_retail_contract.py.",
        "// Source: tools/ivp/retail-contract.json (curated retail evidence).",
    ]
    previous_section = None
    for entry in catalog["addresses"]:
        section = entry.get("section", "")
        if previous_section is not None and section != previous_section:
            lines.append("")
        previous_section = section
        note = entry.get("note", "")
        if note:
            lines.extend(comment_lines(note))
        lines.append(f"{entry['id']} = 0x{entry['rva']}u,")
    return "\n".join(lines) + "\n"


def render_runtime_image(catalog: dict) -> str:
    image = catalog["image"]
    lines = [
        "// Generated by tools/ivp/sync_retail_contract.py.",
        "// Source: tools/ivp/retail-contract.json (curated retail evidence).",
        f"constexpr DWORD kImageTimestamp = 0x{image['timestamp']}u;",
        f"constexpr DWORD kImageSize = 0x{image['image_size']}u;",
        f'constexpr char kImageSha256[] = "{image["sha256"].upper()}";',
        "constexpr uint32_t kRvaGetPhysicsObject =",
        f"    0x{image['manager_get_physics_object_rva']}u;",
        "",
    ]
    for name, value in image["runtime_layout"].items():
        cpp_name = "k" + "".join(part.title() for part in name.split("_"))
        lines.append(f"constexpr size_t {cpp_name} = 0x{value}u;")
    lines.extend([
        "",
        "struct RetailInstructionAnchor {",
        "    uint32_t Rva;",
        "    std::array<unsigned char, 16> Bytes;",
        "    size_t Size;",
        "};",
        "",
        "constexpr RetailInstructionAnchor kInstructionAnchors[] = {",
    ])
    for anchor in image["instruction_anchors"]:
        raw = bytes.fromhex(anchor["bytes"])
        values = ", ".join(f"0x{value:02X}" for value in raw)
        lines.append(
            f"    {{0x{anchor['rva']}u, {{{values}}}, {len(raw)}}},"
        )
    lines.extend(["};", ""])
    return "\n".join(lines)


def render_data_addresses(catalog: dict) -> str:
    lines = [
        "// Generated by tools/ivp/sync_retail_contract.py.",
        "// Source: tools/ivp/retail-contract.json (curated retail evidence).",
    ]
    for entry in catalog.get("data_addresses", []):
        if entry.get("note"):
            lines.extend(comment_lines(entry["note"]))
        lines.append(
            f"inline constexpr std::uint32_t {entry['id']} = 0x{entry['rva']}u;"
        )
    return "\n".join(lines) + "\n"


def render_tsv(rows: list[dict], fields: tuple[str, ...]) -> str:
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(
        buffer, fieldnames=fields, delimiter="\t", lineterminator="\n",
        extrasaction="raise",
    )
    writer.writeheader()
    for row in rows:
        writer.writerow({field: row.get(field, "") for field in fields})
    # Keep the final empty field explicit without a trailing tab in the file.
    return buffer.getvalue().replace("\t\n", '\t""\n')


def render_all(catalog: dict) -> dict[str, str]:
    return {
        "addresses": render_addresses(catalog),
        "data_addresses": render_data_addresses(catalog),
        "runtime_image": render_runtime_image(catalog),
        "direct_dependencies": render_tsv(catalog["direct_dependencies"], DIRECT_FIELDS),
        "indirect_callsites": render_tsv(catalog["indirect_callsites"], INDIRECT_FIELDS),
        "public_evidence": render_tsv(catalog["public_evidence"], EVIDENCE_FIELDS),
    }


def write_outputs(outputs: dict[str, str]) -> None:
    for name, text in outputs.items():
        path = GENERATED_PATHS[name]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="")


def check_outputs(outputs: dict[str, str]) -> None:
    stale = []
    for name, expected in outputs.items():
        path = GENERATED_PATHS[name]
        actual = path.read_text(encoding="utf-8") if path.exists() else None
        if actual != expected:
            stale.append(str(path.relative_to(ROOT)))
    if stale:
        joined = "\n  ".join(stale)
        raise SystemExit(
            "Retail-contract generated views are stale:\n  " + joined +
            "\nRun: python tools/ivp/sync_retail_contract.py --write"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    catalog = load_catalog()
    validate(catalog)
    outputs = render_all(catalog)
    if args.write:
        write_outputs(outputs)
    else:
        check_outputs(outputs)

    dispositions = Counter(
        row["disposition"] for row in catalog["direct_dependencies"]
    )
    print(json.dumps({
        "addresses": len(catalog["addresses"]),
        "data_addresses": len(catalog.get("data_addresses", [])),
        "direct_dependencies": len(catalog["direct_dependencies"]),
        "typed_wrappers": dispositions["typed-wrapper"],
        "adapter_internal": dispositions["adapter-internal"],
        "indirect_callsites": len(catalog["indirect_callsites"]),
        "public_evidence": len(catalog["public_evidence"]),
        "instruction_anchors": len(catalog["image"]["instruction_anchors"]),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
