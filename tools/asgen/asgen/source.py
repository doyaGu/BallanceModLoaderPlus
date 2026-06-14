from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class CImguiSource:
    definitions: dict[str, Any]
    structs_enums: dict[str, Any]
    enum_groups: dict[str, tuple[str, list[dict[str, Any]]]]


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def build_enum_groups(structs_enums: dict[str, Any]) -> dict[str, tuple[str, list[dict[str, Any]]]]:
    groups: dict[str, tuple[str, list[dict[str, Any]]]] = {}
    for raw_name, values in structs_enums.get("enums", {}).items():
        type_name = raw_name[:-1] if raw_name.endswith("_") else raw_name
        groups[type_name] = (raw_name, values)
    return groups


def load_cimgui_source(root: Path) -> CImguiSource:
    definitions = load_json(root / "deps" / "cimgui" / "generator" / "output" / "definitions.json")
    structs_enums = load_json(root / "deps" / "cimgui" / "generator" / "output" / "structs_and_enums.json")
    return CImguiSource(definitions=definitions, structs_enums=structs_enums, enum_groups=build_enum_groups(structs_enums))
