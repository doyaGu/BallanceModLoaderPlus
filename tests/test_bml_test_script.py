"""Tests for tools/bml_test.py resolve subcommand."""
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tomllib

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "tools" / "bml_test.py"


def run_resolve(*args: str) -> dict:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "resolve", *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return json.loads(result.stdout)


def discover_source_modules(exclude_test: bool = True) -> set[str]:
    """Scan modules/ for directories with mod.toml, excluding test modules."""
    modules = set()
    for toml_path in (REPO_ROOT / "modules").glob("*/mod.toml"):
        manifest = tomllib.loads(toml_path.read_text(encoding="utf-8"))
        if exclude_test and manifest.get("package", {}).get("category") == "test":
            continue
        modules.add(toml_path.parent.name)
    return modules


def test_resolve_all_builtin():
    """resolve --all-builtin output matches modules/ directory contents."""
    payload = run_resolve("--all-builtin")
    actual = discover_source_modules(exclude_test=True)
    assert set(payload["builtin_modules"]) == actual


def test_resolve_dependency_closure():
    """Each module in resolved list has a corresponding mod.toml."""
    payload = run_resolve("--all-builtin")
    for mod in payload["resolved_modules"]:
        assert (REPO_ROOT / "modules" / mod / "mod.toml").exists()


def test_resolve_no_duplicates():
    """No duplicate entries in resolved module list."""
    payload = run_resolve("--all-builtin")
    modules = payload["resolved_modules"]
    assert len(modules) == len(set(modules))


def test_resolve_dependency_graph_complete():
    """Every dependency ID in the graph maps to a resolved module."""
    payload = run_resolve("--all-builtin")
    all_ids: set[str] = set()
    for mod in payload["resolved_modules"]:
        toml_path = REPO_ROOT / "modules" / mod / "mod.toml"
        manifest = tomllib.loads(toml_path.read_text(encoding="utf-8"))
        all_ids.add(manifest["package"]["id"])
    for mod_name, deps in payload["dependency_graph"].items():
        for dep_id in deps:
            assert dep_id in all_ids, f"{mod_name}: unresolved dependency {dep_id}"


def test_resolve_default_is_all_builtin():
    """resolve with no flags defaults to --all-builtin."""
    explicit = run_resolve("--all-builtin")
    default = run_resolve()
    assert explicit["builtin_modules"] == default["builtin_modules"]
    assert set(explicit["resolved_modules"]) == set(default["resolved_modules"])


def test_resolve_single_module():
    """resolve --modules BML_Input returns only that module (no deps)."""
    payload = run_resolve("--modules", "BML_Input")
    assert "BML_Input" in payload["resolved_modules"]
    assert payload["resolved_modules"] == ["BML_Input"]
