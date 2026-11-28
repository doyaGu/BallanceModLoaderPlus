import argparse
import re
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"Behavior dependency check failed: {message}")


def includes(path: Path) -> set[str]:
    return set(re.findall(
        r'^\s*#include\s*[<"]([^>"]+)[>"]',
        path.read_text(encoding="utf-8"),
        flags=re.MULTILINE,
    ))


def check_adapter_dependencies(root: Path, source: Path) -> None:
    paths = [source / "Hooks/RenderHookWin32.cpp", *(source / "Engine").glob("*")]
    for path in paths:
        if path.suffix not in {".h", ".cpp"}:
            continue
        for include in includes(path):
            if include.startswith(("Api/", "Behavior/", "Loader/", "Logging/")):
                fail(f"Engine adapter {path.relative_to(root)} reaches a higher layer: {include}")
        if "BML_GetModContext" in path.read_text(encoding="utf-8"):
            fail(f"Engine adapter {path.relative_to(root)} acquires the ambient Loader context")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    args = parser.parse_args()

    root = Path(args.source_root).resolve()
    source = root / "src"
    behavior = source / "Behavior"
    check_adapter_dependencies(root, source)
    block_names = {
        "ObjectLoad",
        "Physicalize",
        "PhysicsForce",
        "PhysicsImpulse",
        "PhysicsWakeUp",
        "SendMessage",
        "Text2D",
    }

    obsolete = ("IMod.h", "IBML.h", "IConfig.h", "ICommand.h", "ILogger.h",
                "IMessageReceiver.h", "InputHook.h", "ExecuteBB.h", "ModInterface.hpp",
                "RefCount.h")
    for name in obsolete:
        if (root / "include" / "BML" / name).exists():
            fail(f"obsolete public native header remains: {name}")
    if (source / "Api" / "ExecuteBB.cpp").exists():
        fail("the obsolete ExecuteBB export facade remains")
    # Public authoring headers must remain usable without the Loader source tree.
    # Resolve includes as well as checking names, so relative paths cannot bypass
    # this contract. CK/ImGui adapters still use their explicit SDK dependencies.
    private_root = source.resolve()
    for header in (root / "include" / "BML").rglob("*"):
        if header.suffix not in {".h", ".hpp"}:
            continue
        for include in includes(header):
            candidates = (header.parent / include, root / "include" / include,
                          source / include)
            if any(path.is_file() and path.resolve().is_relative_to(private_root)
                   for path in candidates):
                fail(f"public header {header.relative_to(root)} includes private implementation: {include}")
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
    for name in ("HookBlock", "PhysicsForce", "Text2DView"):
        for suffix in (".h", ".cpp"):
            if not (behavior / "Blocks" / f"{name}{suffix}").is_file():
                fail(f"{name} private Building Block module is missing {suffix}")
            if (behavior / f"{name}{suffix}").exists():
                fail(f"{name} private Building Block module remains at Behavior root")
    if (behavior / "Blocks.h").exists():
        fail("the obsolete private Building Block umbrella remains")
    for legacy in ("Specs.h", "Specs.cpp", "Forces.h", "Forces.cpp"):
        if (behavior / legacy).exists():
            fail(f"mechanism-based Behavior bucket still exists: Behavior/{legacy}")

    forbidden_calls = re.compile(
        r"ExecuteBB::(?:Create|ObjectLoad|Physicalize|Unphysicalize|"
        r"SetPhysics|UnsetPhysics|PhysicsImpulse|PhysicsWakeUp|Init|GetFont)"
    )
    for path in source.rglob("*.cpp"):
        content = path.read_text(encoding="utf-8")
        if '#include "BML/ExecuteBB.h"' in content:
            fail(f"loader implementation includes the removed ExecuteBB interface: {path.relative_to(root)}")
        match = forbidden_calls.search(content)
        if match:
            fail(
                f"loader implementation calls the removed ExecuteBB facade in "
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
        "Behavior/Blocks/HookBlock.h",
        "Behavior/Blocks/PhysicsForce.h",
        "Behavior/Blocks/Text2DView.h",
    }
    leaks = set().union(*(includes(path) for path in runtime_files)) & block_headers
    if leaks:
        fail(
            "Behavior Runtime depends on concrete Building Block modules: "
            + ", ".join(sorted(leaks))
        )


if __name__ == "__main__":
    main()
