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
    virtools = source / "Virtools"
    adapter_path = virtools / "ExecuteBB.cpp"
    runtime_path = virtools / "BehaviorRuntime.cpp"
    runtime_header = virtools / "BehaviorRuntime.h"
    presets_path = virtools / "BallanceBehaviorPresets.cpp"
    legacy_state_path = virtools / "LegacyExecuteBBAdapter.cpp"
    force_sessions_path = virtools / "PhysicsForceSessions.cpp"
    mod_context_path = source / "Loader" / "ModContext.cpp"
    mod_manager_path = source / "Loader" / "ModManager.cpp"

    for path in (adapter_path, runtime_path, runtime_header, presets_path, legacy_state_path,
                 force_sessions_path, mod_context_path, mod_manager_path):
        if not path.is_file():
            fail(f"missing module implementation: {path.relative_to(root)}")

    for removed in (virtools / "VirtoolsActions.cpp", virtools / "BehaviorGraphRecipes.cpp"):
        if removed.exists():
            fail(f"obsolete shallow module still exists: {removed.relative_to(root)}")

    direct_call = re.compile(
        r"ExecuteBB::(?:Create|ObjectLoad|Physicalize|Unphysicalize|"
        r"SetPhysics|UnsetPhysics|PhysicsImpulse|PhysicsWakeUp|Init|GetFont)"
    )
    for path in source.rglob("*.cpp"):
        if path == adapter_path:
            continue
        content = path.read_text(encoding="utf-8")
        if '#include "BML/ExecuteBB.h"' in content:
            fail(f"internal implementation includes the legacy adapter: {path.relative_to(root)}")
        match = direct_call.search(content)
        if match:
            fail(f"internal implementation calls the legacy adapter in {path.relative_to(root)}: {match.group(0)}")

    adapter = adapter_path.read_text(encoding="utf-8")
    for leaked_detail in (
        "GetInputParameter(", "GetLocalParameter(", "ActivateInput(", "->Execute(",
        "CKM_BEHAVIORSETTINGSEDITED", "CKBR_ACTIVATENEXTFRAME",
    ):
        if leaked_detail in adapter:
            fail(f"legacy adapter contains Behavior Runtime implementation detail: {leaked_detail}")
    if "GetLegacyExecuteBB()" not in adapter or "Virtools::Presets::" not in adapter:
        fail("legacy adapter does not delegate through its state and declarative presets")

    runtime_header_text = runtime_header.read_text(encoding="utf-8")
    runtime = runtime_path.read_text(encoding="utf-8")
    required_runtime_semantics = (
        "CKGetPrototypeFromGuid", "GetManagerNeededCount", "IsLocalParameterSetting",
        "CKM_BEHAVIORCREATE", "CKM_BEHAVIORATTACH", "CKM_BEHAVIORSETTINGSEDITED",
        "CKM_BEHAVIORDETACH", "CKM_BEHAVIORDELETE", "CKBR_ACTIVATENEXTFRAME",
        "m_BehaviorContext", "m_CurrentBehavior", "Frames = 2",
        "SetDirectSource", "ShareSourceWith", "BehaviorSlotHandle",
        "LayoutGeneration", "BehaviorError::StaleLayout", "OwnedSources",
        "CreateCKParameterLocal", "ReleaseRequested", "RequestRelease",
        "Raw value size does not match",
    )
    for semantic in required_runtime_semantics:
        if semantic not in runtime and semantic not in runtime_header_text:
            fail(f"Behavior Runtime does not own required Virtools semantic: {semantic}")

    queue_destroy = runtime.split("void BehaviorRuntime::QueueDestroy", 1)[1].split(
        "void BehaviorRuntime::QueueSourceDestroy", 1
    )[0]
    if "CKM_BEHAVIORRESET" in queue_destroy:
        fail("normal instance release still sends Virtools RESET before DELETE")
    if "storage->CreateLocalParameter" in runtime or "behavior->CreateLocalParameter" in runtime:
        fail("runtime-owned input sources still pollute reflected behavior/graph locals")

    force_sessions = force_sessions_path.read_text(encoding="utf-8")
    for semantic in (
        "CloseAfterEpoch", "m_PhysicsEpoch + 1", "ProcessFrame()",
        "SlotSelector::At(BehaviorSlotKind::Input, 1)", "CancellationArmed",
        "m_Runtime.Reconfigure(", "Generation", "m_Identities.Resolve(session.Target)",
    ):
        if semantic not in force_sessions:
            fail(f"Physics Force session does not own deferred Shutdown semantic: {semantic}")

    mod_context = mod_context_path.read_text(encoding="utf-8")
    world_reset = mod_context.split("void ModContext::ResetVirtoolsWorld()", 1)[1].split("\n}", 1)[0]
    reset_order = (
        world_reset.find("m_PhysicsForceSessions.Reset()"),
        world_reset.find("m_BehaviorRuntime.ResetWorld()"),
        world_reset.find("m_ObjectIdentities.ResetWorld()"),
    )
    if min(reset_order) < 0 or reset_order != tuple(sorted(reset_order)):
        fail("world teardown must close force sessions and runtime before invalidating identities")

    object_deletion = mod_context.split(
        "void ModContext::VirtoolsObjectsToBeDeleted", 1
    )[1].split("\n}", 1)[0]
    deletion_order = (
        object_deletion.find("m_PhysicsForceSessions.ObjectsToBeDeleted"),
        object_deletion.find("m_BehaviorRuntime.ObjectsToBeDeleted"),
        object_deletion.find("m_ObjectIdentities.Invalidate"),
    )
    if min(deletion_order) < 0 or deletion_order != tuple(sorted(deletion_order)):
        fail("object deletion must close sessions and runtime state before invalidating identities")

    frame_process = mod_context.split("void ModContext::ProcessVirtoolsFrame()", 1)[1].split("\n}", 1)[0]
    force_post = frame_process.find("m_PhysicsForceSessions.ProcessFrame();")
    runtime_post = frame_process.find("m_BehaviorRuntime.ProcessFrame();")
    legacy_post = frame_process.find("m_LegacyExecuteBB.ProcessFrame();")
    if min(force_post, runtime_post, legacy_post) < 0 or not force_post < runtime_post < legacy_post:
        fail("physics callback drain, force shutdown, and runtime teardown are ordered incorrectly")

    mod_manager = mod_manager_path.read_text(encoding="utf-8")
    physics_post = mod_manager.find("PhysicsPostProcess();")
    virtools_post = mod_manager.find("ProcessVirtoolsFrame();")
    if min(physics_post, virtools_post) < 0 or physics_post >= virtools_post:
        fail("Virtools frame processing must run after the native physics callback drain")

    presets = presets_path.read_text(encoding="utf-8")
    for implementation_detail in (
        "CreateObject(", "InitFromGuid(", "CallCallbackFunction(", "->Execute(",
        "AddSubBehavior(", "DestroyObject(",
    ):
        if implementation_detail in presets:
            fail(f"declarative presets perform runtime work: {implementation_detail}")

    internal_legacy_uses = "\n".join(
        path.read_text(encoding="utf-8")
        for path in source.rglob("*.cpp")
        if path != adapter_path
    )
    if "GetLegacyExecuteBB" in internal_legacy_uses:
        fail("loader implementation bypasses Behavior Runtime through the legacy state adapter")


if __name__ == "__main__":
    main()
