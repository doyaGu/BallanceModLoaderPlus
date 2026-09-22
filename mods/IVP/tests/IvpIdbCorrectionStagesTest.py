"""Check the guarded IDA correction order without importing IDA."""

import ast
from pathlib import Path


STAGES = (
    "check_and_annotate_retail_image",
    "apply_type_definitions",
    "apply_layout_corrections",
    "annotate_types",
    "apply_vtable_layouts",
    "apply_names_and_vtable_notes",
    "annotate_ownership_and_lifetimes",
)


def call_name(statement: ast.stmt) -> str | None:
    if not isinstance(statement, ast.Expr) or not isinstance(statement.value, ast.Call):
        return None
    function = statement.value.func
    if isinstance(function, ast.Name):
        return function.id
    if isinstance(function, ast.Attribute):
        return function.attr
    return None


def main() -> None:
    source = (
        Path(__file__).resolve().parents[1]
        / "tools/ivp/Apply-BallanceIvpIdbCorrections.py"
    )
    module = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    functions = {
        node.name: node
        for node in module.body
        if isinstance(node, ast.FunctionDef)
    }
    missing = set(STAGES) - functions.keys()
    if missing:
        raise AssertionError(f"Missing correction stages: {sorted(missing)}")

    entry = functions["main"]
    calls = tuple(call_name(statement) for statement in entry.body[: len(STAGES)])
    if calls != STAGES:
        raise AssertionError(f"Correction stage order changed: {calls}")
    if any(
        isinstance(node, ast.Call)
        and isinstance(node.func, ast.Name)
        and node.func.id in STAGES
        for statement in entry.body[len(STAGES) :]
        for node in ast.walk(statement)
    ):
        raise AssertionError("A correction stage is called more than once")
    if call_name(functions[STAGES[0]].body[0]) != "auto_wait":
        raise AssertionError("IDA auto-analysis must finish before retail checks")
    if call_name(entry.body[-1]) != "qexit":
        raise AssertionError("The IDA transaction must finish through qexit")


if __name__ == "__main__":
    main()
