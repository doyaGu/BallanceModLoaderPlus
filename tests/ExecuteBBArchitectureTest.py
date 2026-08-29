import argparse
import re
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"ExecuteBB architecture check failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    args = parser.parse_args()

    root = Path(args.source_root).resolve()
    source = root / "src"
    adapter_path = source / "Virtools" / "ExecuteBB.cpp"
    actions_path = source / "Virtools" / "VirtoolsActions.cpp"
    recipes_path = source / "Virtools" / "BehaviorGraphRecipes.cpp"

    for path in (adapter_path, actions_path, recipes_path):
        if not path.is_file():
            fail(f"missing module implementation: {path.relative_to(root)}")

    direct_call = re.compile(
        r"ExecuteBB::(?:Create|ObjectLoad|Physicalize|Unphysicalize|"
        r"SetPhysics|UnsetPhysics|PhysicsImpulse|PhysicsWakeUp|Init|GetFont)"
    )
    for path in source.rglob("*.cpp"):
        if path == adapter_path:
            continue
        text = path.read_text(encoding="utf-8")
        if '#include "BML/ExecuteBB.h"' in text:
            fail(f"internal implementation includes the legacy adapter: {path.relative_to(root)}")
        match = direct_call.search(text)
        if match:
            fail(f"internal implementation calls the legacy adapter in {path.relative_to(root)}: {match.group(0)}")

    adapter = adapter_path.read_text(encoding="utf-8")
    for leaked_detail in ("GetInputParameter(", "GetLocalParameter(", "ActivateInput(", "->Execute("):
        if leaked_detail in adapter:
            fail(f"legacy adapter contains Building Block implementation detail: {leaked_detail}")
    if "GetVirtoolsActions()" not in adapter or "BehaviorGraphRecipes::" not in adapter:
        fail("legacy adapter does not delegate to both deep modules")


if __name__ == "__main__":
    main()
