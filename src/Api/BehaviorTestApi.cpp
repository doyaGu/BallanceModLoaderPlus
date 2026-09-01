#include "Api/BehaviorTestApi.h"

#include <cstring>

#include "Behavior/Patches.h"
#include "Loader/ModContext.h"

namespace BML::Api {
namespace {

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
        return Result(context->BehaviorPatches().RetireOwner(owner.Id));
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

const BML_BehaviorTestInterface kInterface = {
    BML_IFACE_HEADER(BML_BehaviorTestInterface,
                     BML_BEHAVIOR_TEST_INTERFACE_ID,
                     BML_BEHAVIOR_TEST_INTERFACE_MAJOR,
                     BML_BEHAVIOR_TEST_INTERFACE_MINOR),
    &InstallSplice,
    &ReadPatch,
    &ClosePatch,
    &ResetPatches,
    &RetirePatches,
};

} // namespace

const BML_BehaviorTestInterface &BehaviorTestInterface() {
    return kInterface;
}

} // namespace BML::Api
