import argparse
import re
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"ExecuteBB dependency check failed: {message}")


def includes(path: Path) -> set[str]:
    return set(re.findall(
        r'^\s*#include\s+"([^"]+)"',
        path.read_text(encoding="utf-8"),
        flags=re.MULTILINE,
    ))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    args = parser.parse_args()

    root = Path(args.source_root).resolve()
    source = root / "src"
    behavior = source / "Behavior"
    facade = source / "Api" / "ExecuteBB.cpp"
    block_names = {
        "ObjectLoad",
        "Physicalize",
        "PhysicsForce",
        "PhysicsImpulse",
        "PhysicsWakeUp",
        "SendMessage",
        "Text2D",
    }

    if not facade.is_file():
        fail("ExecuteBB API facade is missing")
    public_blocks = root / "include" / "BML" / "Behavior" / "Blocks"
    for name in block_names:
        header = public_blocks / f"{name}.hpp"
        if not header.is_file():
            fail(f"{name} public Building Block adapter is missing")
        content = header.read_text(encoding="utf-8")
        if "BML_BEHAVIOR_INTERNAL" in content:
            fail(f"{name} public adapter has a private compile mode")
        private_includes = {
            include for include in includes(header)
            if include.startswith("Behavior/")
        }
        if private_includes:
            fail(
                f"{name} public adapter includes private Behavior headers: "
                + ", ".join(sorted(private_includes))
            )
        for suffix in (".h", ".cpp"):
            if (behavior / "Blocks" / f"{name}{suffix}").exists():
                fail(
                    f"{name} still has a duplicate private Building Block "
                    f"implementation ({suffix})"
                )
    for name in ("HookBlock", "PhysicsForce", "Text2DView"):
        for suffix in (".h", ".cpp"):
            if not (behavior / f"{name}{suffix}").is_file():
                fail(f"{name} Behavior runtime module is missing {suffix}")
    if (behavior / "Blocks.h").exists():
        fail("the obsolete private Building Block umbrella remains")
    private_blocks = behavior / "Blocks"
    if private_blocks.exists() and any(private_blocks.iterdir()):
        fail("private named Building Block adapters remain")
    for legacy in ("Specs.h", "Specs.cpp", "Forces.h", "Forces.cpp"):
        if (behavior / legacy).exists():
            fail(f"mechanism-based Behavior bucket still exists: Behavior/{legacy}")

    required = {
        "BML/ExecuteBB.h",
        "Loader/ModContext.h",
        "UI/GameFontCatalog.h",
        "Api/ExecuteBBAdapter.h",
        "Behavior/HookBlock.h",
        "Behavior/Block.h",
        "BML/Behavior/Blocks.hpp",
    }
    missing = required - includes(facade)
    if missing:
        fail(
            "ExecuteBB facade no longer crosses the declared module seams: "
            + ", ".join(sorted(missing))
        )

    forbidden_calls = re.compile(
        r"ExecuteBB::(?:Create|ObjectLoad|Physicalize|Unphysicalize|"
        r"SetPhysics|UnsetPhysics|PhysicsImpulse|PhysicsWakeUp|Init|GetFont)"
    )
    for path in source.rglob("*.cpp"):
        if path == facade:
            continue
        content = path.read_text(encoding="utf-8")
        if '#include "BML/ExecuteBB.h"' in content:
            fail(f"loader implementation includes the public ExecuteBB interface: {path.relative_to(root)}")
        match = forbidden_calls.search(content)
        if match:
            fail(
                f"loader implementation calls the public ExecuteBB facade in "
                f"{path.relative_to(root)}: {match.group(0)}"
            )
    for path in behavior.rglob("*"):
        if path.suffix not in {".h", ".cpp"}:
            continue
        leaks = {
            include for include in includes(path)
            if include.startswith("Loader/") or include.startswith("Api/")
        }
        if leaks:
            fail(
                f"Behavior module depends outward in {path.relative_to(root)}: "
                + ", ".join(sorted(leaks))
            )
        content = path.read_text(encoding="utf-8")
        if "BML_ObjectRef" in content or '"BML/Types.h"' in content:
            fail(
                f"Behavior module leaks the public object-reference seam in "
                f"{path.relative_to(root)}"
            )

    runtime_files = (behavior / "Runtime.h", behavior / "Runtime.cpp")
    block_headers = {
        "BML/Behavior/Blocks.hpp",
        "Behavior/HookBlock.h",
        "Behavior/PhysicsForce.h",
        "Behavior/Text2DView.h",
    }
    leaks = set().union(*(includes(path) for path in runtime_files)) & block_headers
    if leaks:
        fail(
            "Behavior Runtime depends on concrete Building Block modules: "
            + ", ".join(sorted(leaks))
        )


if __name__ == "__main__":
    main()
