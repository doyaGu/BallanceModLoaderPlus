#include "Api/BehaviorTestApi.h"

#include <cstring>

#include "Behavior/Patches.h"
#include "Behavior/Blocks/Text2D.h"
#include "Loader/ModContext.h"

namespace BML::Api {
namespace {

constexpr int BottomLeftAlignment = 9;

struct HookTrace {
    std::uint32_t Retains = 0;
    std::uint32_t Releases = 0;
    std::uint32_t Taps = 0;
    std::uint32_t Afters = 0;
};

HookTrace g_Hooks;

void RetainHook(void *state) {
    if (state)
        ++static_cast<HookTrace *>(state)->Retains;
}

void ReleaseHook(void *state) {
    if (state)
        ++static_cast<HookTrace *>(state)->Releases;
}

int CountHook(const CKBehaviorContext *, void *argument) {
    if (argument)
        ++*static_cast<std::uint32_t *>(argument);
    return CKBR_OK;
}

std::uintptr_t SessionId(BML_BehaviorSession session) {
    return reinterpret_cast<std::uintptr_t>(session);
}

int Result(const Behavior::Status &status) {
    if (status)
        return BML_OK;
    switch (status.Code) {
    case Behavior::Error::WrongThread: return BML_ERROR_WRONG_THREAD;
    case Behavior::Error::OwnerInvalid: return BML_ERROR_ACCESS_DENIED;
    case Behavior::Error::Unavailable: return BML_ERROR_UNAVAILABLE;
    default: return BML_ERROR_FAIL;
    }
}

bool Ready(ModContext *context) {
    return context && context->AreModsLoaded() && context->IsMainThread();
}

bool Owner(ModContext &context, BML_BehaviorSession session,
           Behavior::SessionOwner &out) {
    return session &&
           context.BehaviorSessions().ReadOwner(SessionId(session), out);
}

int BML_BEHAVIOR_CALL InstallSplice(
    BML_BehaviorSession session, void *rawGraph, void *rawLink,
    BML_BehaviorGuid prototype, const char *name,
    std::uintptr_t *out) {
    if (!out || !rawGraph || !rawLink || !name || !*name)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;

    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;

        Behavior::Edit edit;
        Behavior::Patches &patches = context->BehaviorPatches();
        Behavior::Status status = patches.Begin(
            owner, static_cast<CKBehavior *>(rawGraph), name, edit);
        Behavior::Link anchor;
        if (status)
            status = patches.Use(
                edit, static_cast<CKBehaviorLink *>(rawLink), anchor);
        Behavior::Node node;
        if (status)
            status = patches.Add(
                edit, Behavior::Spec(CKGUID(prototype.Data1,
                                             prototype.Data2)), node);
        if (status)
            edit.Splice(anchor, node);
        if (status)
            status = patches.Apply(owner, edit, *out);
        return Result(status);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL InstallTextSplice(
    BML_BehaviorSession session, void *rawGraph, void *rawLink,
    void *rawTarget, const char *text, const char *name,
    std::uintptr_t *out) {
    if (!out || !rawGraph || !rawLink || !rawTarget || !text || !name ||
        !*name)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;

    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;

        Behavior::Text2D::Options options;
        options.Target = static_cast<CK2dEntity *>(rawTarget);
        options.FontIndex = context->GetGameFonts().Resolve(GameFont::Normal);
        options.Text = text;
        options.Alignment = BottomLeftAlignment;

        Behavior::Edit edit;
        Behavior::Patches &patches = context->BehaviorPatches();
        Behavior::Status status = patches.Begin(
            owner, static_cast<CKBehavior *>(rawGraph), name, edit);
        Behavior::Link anchor;
        if (status)
            status = patches.Use(
                edit, static_cast<CKBehaviorLink *>(rawLink), anchor);
        Behavior::Node node;
        if (status)
            status = patches.Add(edit, Behavior::Text2D::Make(options), node);
        if (status)
            edit.Splice(anchor, node);
        if (status)
            status = patches.Apply(owner, edit, *out);
        return Result(status);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ReadPatch(BML_BehaviorSession session,
                                std::uintptr_t patch,
                                std::uint32_t *state) {
    if (!patch || !state)
        return BML_ERROR_INVALID_PARAMETER;
    *state = 0;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        Behavior::PatchInfo info;
        const Behavior::Status status =
            context->BehaviorPatches().Read(owner, patch, info);
        if (!status)
            return Result(status);
        *state = static_cast<std::uint32_t>(info.State) + 1;
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ClosePatch(BML_BehaviorSession session,
                                 std::uintptr_t patch) {
    if (!patch)
        return BML_ERROR_INVALID_PARAMETER;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        return Result(context->BehaviorPatches().Close(owner, patch));
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ResetPatches(BML_BehaviorSession session) {
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        context->BehaviorPatches().ResetWorld();
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL RetirePatches(BML_BehaviorSession session) {
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        Behavior::Status status =
            context->BehaviorPlans().RetireOwner(owner.Id);
        if (status)
            status = context->BehaviorPatches().RetireOwner(owner.Id);
        return Result(status);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ObserveScript(BML_BehaviorSession session,
                                    void *rawScript) {
    if (!rawScript)
        return BML_ERROR_INVALID_PARAMETER;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        context->BehaviorScriptLoaded(static_cast<CKBehavior *>(rawScript));
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL SubmitEdit(
    BML_BehaviorSession session, const char *script,
    const char *sourceNode, const char *sinkNode,
    BML_BehaviorGuid prototype, const char *name,
    std::uintptr_t *out) {
    if (!script || !*script || !sourceNode || !*sourceNode ||
        !sinkNode || !*sinkNode || !name || !*name || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        Behavior::GraphEdit edit;
        const Behavior::Node source = edit.RequireOne({sourceNode});
        const Behavior::Node sink = edit.RequireOne({sinkNode});
        const Behavior::Link link = edit.RequireOne(source.Out(), sink.In());
        g_Hooks = {};
        edit.Tap(
            source.Out(),
            Behavior::HookBlock::Hook(
                Behavior::PlanCallbackState::Retained(
                    &g_Hooks, RetainHook, ReleaseHook),
                CountHook, &g_Hooks.Taps));
        edit.After(
            edit.Follow(source.Out()),
            Behavior::HookBlock::Hook(
                Behavior::PlanCallbackState::Retained(
                    &g_Hooks, RetainHook, ReleaseHook),
                CountHook, &g_Hooks.Afters));
        const Behavior::Node block = edit.Add(
            CKGUID(prototype.Data1, prototype.Data2));
        (void) edit.AppendIn(block, "Again");
        (void) edit.AppendOut(block, "Finished");
        const Behavior::Port literal = edit.AppendPin(
            block, "Literal", CKPGUID_INT);
        const Behavior::Port direct = edit.AppendPin(
            block, "Direct", CKPGUID_INT);
        const Behavior::Port shared = edit.AppendPin(
            block, "Shared", CKPGUID_INT);
        const Behavior::Port value = edit.AppendPout(
            block, "Value", CKPGUID_INT);
        const Behavior::Port destination = edit.AppendPout(
            block, "Destination", CKPGUID_INT);
        const int number = 41;
        edit.Bind(literal, Behavior::Value::From(CKPGUID_INT, number));
        edit.Bind(direct, value);
        edit.Share(shared, block.Pin("Source"));
        edit.Push(value, destination);
        edit.Splice(link, block);
        Behavior::PlanId plan = 0;
        const Behavior::Status status = context->BehaviorPatches().Submit(
            context->BehaviorPlans(), owner, {script}, name,
            std::move(edit), plan);
        if (status)
            *out = static_cast<std::uintptr_t>(plan);
        return Result(status);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ReadPlan(
    BML_BehaviorSession session, std::uintptr_t plan,
    std::uint32_t *state, std::uint32_t *matches,
    std::uint32_t *installations, std::uint64_t *world) {
    if (!plan || !state || !matches || !installations || !world)
        return BML_ERROR_INVALID_PARAMETER;
    *state = 0;
    *matches = 0;
    *installations = 0;
    *world = 0;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        Behavior::PlanInfo info;
        const Behavior::Status status = context->BehaviorPlans().Read(
            owner.Id, owner.Generation, plan, info);
        if (!status)
            return Result(status);
        *state = static_cast<std::uint32_t>(info.State) + 1;
        *matches = static_cast<std::uint32_t>(info.Matches);
        *installations = static_cast<std::uint32_t>(info.Installations);
        *world = info.World;
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ClosePlan(BML_BehaviorSession session,
                                std::uintptr_t plan) {
    if (!plan)
        return BML_ERROR_INVALID_PARAMETER;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        return Result(context->BehaviorPlans().Close(
            owner.Id, owner.Generation, plan));
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ResetPlans(BML_BehaviorSession session) {
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    try {
        Behavior::SessionOwner owner;
        if (!Owner(*context, session, owner))
            return BML_ERROR_ACCESS_DENIED;
        Behavior::Status status = context->BehaviorPlans().ResetWorld();
        context->BehaviorPatches().ResetWorld();
        return Result(status);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_BEHAVIOR_CALL ReadHooks(
    BML_BehaviorSession session, std::uint32_t *retains,
    std::uint32_t *releases, std::uint32_t *taps,
    std::uint32_t *afters) {
    if (!retains || !releases || !taps || !afters)
        return BML_ERROR_INVALID_PARAMETER;
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    Behavior::SessionOwner owner;
    if (!Owner(*context, session, owner))
        return BML_ERROR_ACCESS_DENIED;
    *retains = g_Hooks.Retains;
    *releases = g_Hooks.Releases;
    *taps = g_Hooks.Taps;
    *afters = g_Hooks.Afters;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReferenceObject(
    BML_BehaviorSession session, void *rawObject,
    BML_ObjectRef *reference) {
    if (!rawObject || !reference)
        return BML_ERROR_INVALID_PARAMETER;
    *reference = {};
    ModContext *context = BML_GetModContext();
    if (!Ready(context))
        return context && !context->IsMainThread()
            ? BML_ERROR_WRONG_THREAD : BML_ERROR_FAIL;
    Behavior::SessionOwner owner;
    if (!Owner(*context, session, owner))
        return BML_ERROR_ACCESS_DENIED;
    *reference = context->ObjectRefs().Issue(
        static_cast<CKObject *>(rawObject));
    return reference->Domain ? BML_OK : BML_ERROR_FAIL;
}

const BML_BehaviorTestInterface kInterface = {
    BML_IFACE_HEADER(BML_BehaviorTestInterface,
                     BML_BEHAVIOR_TEST_INTERFACE_ID,
                     BML_BEHAVIOR_TEST_INTERFACE_MAJOR,
                     BML_BEHAVIOR_TEST_INTERFACE_MINOR),
    &InstallSplice,
    &InstallTextSplice,
    &ReadPatch,
    &ClosePatch,
    &ResetPatches,
    &RetirePatches,
    &ObserveScript,
    &SubmitEdit,
    &ReadPlan,
    &ClosePlan,
    &ResetPlans,
    &ReadHooks,
    &ReferenceObject,
};

} // namespace

const BML_BehaviorTestInterface &BehaviorTestInterface() {
    return kInterface;
}

} // namespace BML::Api
