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


class CImguiSourceError(RuntimeError):
    pass


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
    output_dir = root / "deps" / "cimgui" / "generator" / "output"
    definitions_path = output_dir / "definitions.json"
    structs_enums_path = output_dir / "structs_and_enums.json"
    missing = [path for path in (definitions_path, structs_enums_path) if not path.exists()]
    if missing:
        relative = ", ".join(str(path.relative_to(root)) for path in missing)
        raise CImguiSourceError(
            "generation requires Dear ImGui metadata files that are not present in this checkout: "
            + relative
            + ". Use 'asgen check' to validate the checked-in bindings."
        )
    definitions = load_json(definitions_path)
    structs_enums = load_json(structs_enums_path)
    return CImguiSource(definitions=definitions, structs_enums=structs_enums, enum_groups=build_enum_groups(structs_enums))
