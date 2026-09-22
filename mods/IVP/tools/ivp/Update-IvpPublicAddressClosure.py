#!/usr/bin/env python3
"""Add missing public Address::* targets to the curated retail contract.

The updater never invents an RVA: addresses come from retail-contract.json,
header ownership comes from the checked-in public wrappers, and names come from
the corrected-IDB manifest when one exists.  Multiple public Address ids that
alias one RVA intentionally produce one dependency target.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
ROOT = TOOLS_DIR.parent.parent
CONTRACT_PATH = TOOLS_DIR / "retail-contract.json"
ADDRESS_PATTERN = re.compile(r"\bAddress::([A-Za-z_][A-Za-z0-9_]*)\b")


def header_category(path: str) -> str:
    stem = Path(path).stem
    words = re.sub(r"(?<!^)(?=[A-Z])", "-", stem).lower()
    return f"public-{words}"


def load_manifest(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        name, raw_rva = line.split("\t", 1)
        rva = f"{int(raw_rva, 16):08X}"
        if rva in result:
            raise ValueError(f"Multiple corrected-IDB names at RVA {rva}")
        result[rva] = name
    return result


def scan_headers() -> dict[str, list[str]]:
    locations: dict[str, list[str]] = defaultdict(list)
    for header in sorted((ROOT / "include/BML/IVP").rglob("*.h")):
        relative = header.relative_to(ROOT).as_posix()
        for address_id in ADDRESS_PATTERN.findall(
            header.read_text(encoding="utf-8")
        ):
            locations[address_id].append(relative)
    return locations


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "build-dev/physics_RT-symbols.tsv",
    )
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
    address_by_id = {entry["id"]: entry for entry in contract["addresses"]}
    locations = scan_headers()
    unknown = sorted(locations.keys() - address_by_id.keys())
    if unknown:
        raise ValueError(f"Header Address ids absent from contract: {unknown}")

    manifest = load_manifest(args.manifest.resolve(strict=True))
    direct_rvas = {
        row["rva"] for row in contract["direct_dependencies"]
        if row["disposition"] == "typed-wrapper"
    }
    ids_by_rva: dict[str, list[str]] = defaultdict(list)
    for address_id in locations:
        ids_by_rva[address_by_id[address_id]["rva"]].append(address_id)

    additions = []
    for rva in sorted(ids_by_rva.keys() - direct_rvas):
        address_id = sorted(ids_by_rva[rva])[0]
        public_header = sorted(set(locations[address_id]))[0]
        symbol = manifest.get(rva, f"unnamed retail body ({address_id})")
        additions.append({
            "rva": rva,
            "symbol": symbol,
            "category": header_category(public_header),
            "disposition": "typed-wrapper",
            "public_header": public_header,
            "probe": f"Address::{address_id}",
            "address_id": address_id,
        })

    print(f"PUBLIC_ADDRESS_IDS\t{len(locations)}")
    print(f"PUBLIC_ADDRESS_RVAS\t{len(ids_by_rva)}")
    print(f"MISSING_DIRECT_RVAS\t{len(additions)}")
    if not args.write:
        return

    contract["direct_dependencies"].extend(additions)
    CONTRACT_PATH.write_text(
        json.dumps(contract, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"ADDED\t{len(additions)}")


if __name__ == "__main__":
    main()
