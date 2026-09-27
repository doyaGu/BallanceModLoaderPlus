#include "Api/BehaviorApi.h"

#include <cstdint>
#include <intrin.h>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "Api/Behavior/Codec.h"
#include "Behavior/Blocks/HookBlock.h"
#include "Loader/ModContext.h"

namespace BML::Api::Behavior {
namespace {

using BML::Behavior::Internal::Phase;
using BML::Behavior::Internal::PlanCallbackState;
namespace HookBlock = BML::Behavior::Internal::HookBlock;

// Every entry checks its own pointers first. Enter then takes the
// ModContext lease, refuses every call off the game thread, runs body and
// writes the Status body leaves behind, including when body throws.
template <typename Body>
int Enter(BML_BehaviorStatus *status, Body &&body) noexcept {
    Status result;
    int code;
    try {
        ModContextLease context;
        if (!context)
            code = BML_ERROR_FROZEN;
        else if (!context->IsMainThread())
            code = BML_ERROR_WRONG_THREAD;
        else
            code = body(*context, result);
    } catch (const std::bad_alloc &) {
        code = BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        code = BML_ERROR_FAIL;
    }
    WriteStatus(status, result);
    return code;
}

bool ReadSessionOwner(BML_BehaviorSession session, ModContext &context,
                      SessionOwner &out, Status &status) {
    status = context.BehaviorSessions().ReadOwner(SessionId(session), out);
    return static_cast<bool>(status);
}

int OpenRunResult(const BML::Behavior::Internal::OpenRun &opened,
                  BML_BehaviorRun *outRun, BML_BehaviorRunInfo *info,
                  Status &result) {
    result = opened.Result;
    if (!opened)
        return ResultCode(result);
    *outRun = RunHandle(opened.Id);
    WriteRunInfo(info, opened.Info);
    return BML_OK;
}

int BML_BEHAVIOR_CALL OpenSession(BML_BehaviorString requestedOwner,
                                  BML_BehaviorSession *outSession,
                                  BML_BehaviorStatus *status) {
    const void *caller = _ReturnAddress();
    if (!PrepareStatus(status) || !outSession)
        return BML_ERROR_INVALID_PARAMETER;
    *outSession = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        std::string requested;
        if (!ReadString(requestedOwner, requested))
            return BML_ERROR_INVALID_PARAMETER;
        if (!context.AreModsLoaded())
            return BML_ERROR_FROZEN;
        const std::string owner = context.GetNativeModOwnerId(
            caller, requested.empty() ? nullptr : requested.c_str());
        if (owner.empty() || (!requested.empty() && owner != requested))
            return BML_ERROR_ACCESS_DENIED;
        std::uintptr_t id = 0;
        result = context.BehaviorSessions().OpenSession(owner, id);
        if (!result)
            return ResultCode(result);
        *outSession = SessionHandle(id);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseSession(BML_BehaviorSession session) {
    if (!session)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &) {
        const std::uintptr_t id = SessionId(session);
        context.BehaviorSessions().CloseSession(id);
        context.BehaviorScripts().CloseSession(id);
        return BML_OK;
    });
}

enum class OpenKind { Call, Start, Spawn };

int OpenRunEntry(OpenKind kind, BML_BehaviorSession session,
                 BML_ObjectRef ownerReference,
                 const BML_BehaviorBlock *block,
                 const BML_BehaviorFramePolicy *frames,
                 const BML_BehaviorSelector *input,
                 BML_BehaviorRun *outRun,
                 BML_BehaviorRunInfo *info,
                 BML_BehaviorStatus *status) noexcept {
    if (!ValidOutputs(info, status) || !session || !block || !frames || !outRun ||
        (kind != OpenKind::Spawn && !input))
        return BML_ERROR_INVALID_PARAMETER;
    *outRun = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        BlockSpec spec;
        FrameRetention retention;
        if (!ReadBlock(*block, context, spec, result) ||
            !ReadFrames(*frames, retention, result))
            return BML_ERROR_INVALID_PARAMETER;
        CKBeObject *owner = ReadOwner(ownerReference, context, result);
        if (ownerReference.Domain && !owner)
            return BML_ERROR_OBJECT_INVALID;
        Slot in;
        if (kind != OpenKind::Spawn &&
            !ReadSelector(*input, SlotKind::Input, CKGUID(), in, result))
            return BML_ERROR_INVALID_PARAMETER;

        Sessions &sessions = context.BehaviorSessions();
        const std::uintptr_t id = SessionId(session);
        const auto opened = kind == OpenKind::Call
            ? sessions.Call(id, owner, spec, in, retention)
            : kind == OpenKind::Start
                ? sessions.Start(id, owner, spec, in, retention)
                : sessions.Spawn(id, owner, spec, retention);
        return OpenRunResult(opened, outRun, info, result);
    });
}

int BML_BEHAVIOR_CALL Call(BML_BehaviorSession session, BML_ObjectRef owner,
                           const BML_BehaviorBlock *block,
                           const BML_BehaviorFramePolicy *frames,
                           const BML_BehaviorSelector *input,
                           BML_BehaviorRun *outRun,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    return OpenRunEntry(OpenKind::Call, session, owner, block, frames, input,
                        outRun, info, status);
}

int BML_BEHAVIOR_CALL Start(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            const BML_BehaviorFramePolicy *frames,
                            const BML_BehaviorSelector *input,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return OpenRunEntry(OpenKind::Start, session, owner, block, frames, input,
                        outRun, info, status);
}

int BML_BEHAVIOR_CALL Spawn(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            const BML_BehaviorFramePolicy *frames,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return OpenRunEntry(OpenKind::Spawn, session, owner, block, frames, nullptr,
                        outRun, info, status);
}

int BML_BEHAVIOR_CALL AttachBlock(BML_BehaviorSession session,
                                  BML_ObjectRef graph,
                                  const BML_BehaviorBlock *block,
                                  const BML_BehaviorFramePolicy *frames,
                                  BML_BehaviorRun *outRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !session || !block || !frames ||
        !outRun)
        return BML_ERROR_INVALID_PARAMETER;
    *outRun = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        BlockSpec spec;
        FrameRetention retention;
        if (!ReadBlock(*block, context, spec, result) ||
            !ReadFrames(*frames, retention, result))
            return BML_ERROR_INVALID_PARAMETER;
        CKBehavior *parent = ReadBehavior(graph, context, result);
        if (!parent)
            return ResultCode(result);
        return OpenRunResult(
            context.BehaviorSessions().Attach(
                SessionId(session), parent, spec, retention),
            outRun, info, result);
    });
}

int BML_BEHAVIOR_CALL AttachHook(BML_BehaviorSession session,
                                 BML_ObjectRef graph,
                                 const BML_BehaviorHookFunction *hook,
                                 const BML_BehaviorHookBlock *block,
                                 BML_BehaviorRun *outRun,
                                 BML_BehaviorRunInfo *info,
                                 BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !session || !hook || !block ||
        !outRun)
        return BML_ERROR_INVALID_PARAMETER;
    *outRun = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        if (!HasStructSize(block) || block->Inputs == 0 ||
            block->Inputs > BML_BEHAVIOR_HOOK_BLOCK_MAX_PORTS ||
            block->Outputs > BML_BEHAVIOR_HOOK_BLOCK_MAX_PORTS ||
            (block->Flags & ~std::uint32_t{
                 BML_BEHAVIOR_HOOK_BLOCK_MANUAL_OUTPUTS}) != 0) {
            result = InvalidValue(
                "A Hook Block needs 1 to 256 Ins, at most 256 Outs and "
                "known flags.");
            return BML_ERROR_INVALID_PARAMETER;
        }
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        CKBehavior *parent = ReadBehavior(graph, context, result);
        if (!parent)
            return ResultCode(result);
        HookBlock::Hook callback;
        result = ReadHook(hook, callback);
        if (!result)
            return ResultCode(result);
        std::shared_ptr<HookBlock::Binding> binding = callback.Bind();
        if (!binding) {
            result = {Error::CallbackFailed, CK_OK, CKBR_BEHAVIORERROR,
                      "The Hook callback could not be admitted."};
            return ResultCode(result);
        }
        // Closing the Session closes the callback before the Run close
        // reaches the Block.
        binding->AdmitThrough(owner.Admission);
        const BlockSpec spec = HookBlock::Make(
            std::move(binding), static_cast<int>(block->Inputs),
            static_cast<int>(block->Outputs),
            (block->Flags & BML_BEHAVIOR_HOOK_BLOCK_MANUAL_OUTPUTS) == 0);
        return OpenRunResult(
            context.BehaviorSessions().Attach(SessionId(session), parent, spec),
            outRun, info, result);
    });
}

int BML_BEHAVIOR_CALL Continue(BML_BehaviorRun run,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !run)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Sessions &sessions = context.BehaviorSessions();
        result = sessions.Continue(RunId(run)).Detail;
        RunInfo current;
        if (sessions.ReadRun(RunId(run), current))
            WriteRunInfo(info, current);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Pulse(BML_BehaviorRun run,
                           const BML_BehaviorSelector *input,
                           std::uint32_t *admission,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !run || !input || !admission)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Slot in;
        if (!ReadSelector(*input, SlotKind::Input, CKGUID(), in, result))
            return BML_ERROR_INVALID_PARAMETER;
        Sessions &sessions = context.BehaviorSessions();
        RunResult pulsed = sessions.Pulse(RunId(run), in);
        result = std::move(pulsed.Detail);
        if (pulsed.Admission == AdmissionState::Failed)
            return ResultCode(result);
        *admission = pulsed.Admission == AdmissionState::Queued
            ? BML_BEHAVIOR_ADMISSION_QUEUED
            : BML_BEHAVIOR_ADMISSION_EXECUTED;
        RunInfo current;
        if (sessions.ReadRun(RunId(run), current))
            WriteRunInfo(info, current);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadRun(BML_BehaviorRun run,
                             BML_BehaviorRunInfo *info,
                             BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !run || !info)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        RunInfo current;
        result = context.BehaviorSessions().ReadRun(RunId(run), current);
        if (!result)
            return ResultCode(result);
        WriteRunInfo(info, current);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL TakeFrames(
    BML_BehaviorRun run, BML_BehaviorRunFrame *headers,
    std::uint32_t headerCapacity, std::uint32_t headerStride,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outHeaderCount, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !run || !outHeaderCount || !outPayloadSize ||
        (headerCapacity && (!headers || headerStride < sizeof(*headers))) ||
        (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &) {
        std::shared_ptr<FrameStore> store =
            context.BehaviorSessions().Frames(RunId(run));
        if (!store)
            return BML_ERROR_INVALID_HANDLE;
        return WriteFrames(*store, headers, headerCapacity, headerStride,
                           payload, payloadCapacity, outHeaderCount,
                           outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL CloseRun(BML_BehaviorRun run) {
    if (!run)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &) {
        context.BehaviorSessions().CloseRun(RunId(run));
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL FindPrototypes(
    BML_BehaviorSession session, const BML_BehaviorPrototypeQuery *query,
    BML_BehaviorPrototypeInfo *prototypes, std::uint32_t prototypeCapacity,
    std::uint32_t prototypeStride, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPrototypeCount,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !query ||
        !outPrototypeCount || !outPayloadSize ||
        (prototypeCapacity &&
         (!prototypes || prototypeStride < sizeof(*prototypes))) ||
        (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        PrototypeQuery requested;
        if (!ReadPrototypeQuery(*query, requested, result))
            return BML_ERROR_INVALID_PARAMETER;
        std::vector<PrototypeInfo> found;
        result = context.BehaviorSessions().FindPrototypes(
            SessionId(session), requested, found);
        if (!result)
            return ResultCode(result);
        return WritePrototypes(found, prototypes, prototypeCapacity,
                               prototypeStride, payload, payloadCapacity,
                               outPrototypeCount, outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadDeclaredLayout(
    BML_BehaviorSession session, const BML_BehaviorPrototypeRef *prototype,
    BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(prototype) ||
        !HasStructSize(layout) || !outPayloadSize ||
        (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Layout source;
        result = context.BehaviorSessions().ReadDeclaredLayout(
            SessionId(session),
            PrototypeRef{Guid(prototype->Prototype), prototype->Generation},
            source);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ValidateBlock(BML_BehaviorSession session,
                                    const BML_BehaviorBlock *block,
                                    BML_BehaviorPrototypeRef *outPrototype,
                                    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !block ||
        !HasStructSize(outPrototype))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        // The identity checks come before ReadBlock resolves any object, so
        // a Block without a Prototype reports that rather than its Target.
        if (HasStructSize(block) && !Guid(block->Prototype).IsValid()) {
            result = {Error::PrototypeNotFound, CKERR_INVALIDPARAMETER,
                      CKBR_PARAMETERERROR, "A Block requires a Prototype GUID."};
            result.Details.Stage = Phase::PrototypeResolution;
            return BML_ERROR_INVALID_PARAMETER;
        }
        if (HasStructSize(block) &&
            block->Target.StructSize >= sizeof(block->Target) &&
            block->Target.Kind != BML_BEHAVIOR_TARGET_OWNER &&
            !Guid(block->Target.Type).IsValid()) {
            result = {Error::TargetInvalid, CKERR_INVALIDPARAMETER,
                      CKBR_PARAMETERERROR,
                      "An explicit Target requires a parameter type."};
            result.Details.Stage = Phase::TargetBinding;
            result.Details.Prototype = Guid(block->Prototype);
            return BML_ERROR_INVALID_PARAMETER;
        }
        BlockSpec spec;
        if (!ReadBlock(*block, context, spec, result))
            return BML_ERROR_INVALID_PARAMETER;
        Layout declared;
        result = context.BehaviorSessions().ReadDeclaredLayout(
            SessionId(session),
            PrototypeRef{spec.Prototype(), spec.PrototypeGeneration()},
            declared);
        if (!result)
            return ResultCode(result);
        result = CheckDeclared(declared, spec, context.GetParameterManager());
        if (!result)
            return ResultCode(result);
        outPrototype->Prototype = Guid(declared.Prototype);
        outPrototype->Generation = declared.ProviderGeneration;
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadLiveLayout(
    BML_BehaviorRun run, BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !run || !HasStructSize(layout) ||
        !outPayloadSize || (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Layout source;
        result = context.BehaviorSessions().ReadLiveLayout(RunId(run), source);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

bool ReadView(std::uint32_t view, GraphView &out) noexcept {
    if (view == BML_BEHAVIOR_GRAPH_LOGICAL)
        out = GraphView::Logical;
    else if (view == BML_BEHAVIOR_GRAPH_LIVE)
        out = GraphView::Live;
    else
        return false;
    return true;
}

int BML_BEHAVIOR_CALL Inspect(
    BML_BehaviorSession session, BML_ObjectRef root, std::uint32_t view,
    BML_BehaviorGraph *graph, void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    GraphView graphView;
    if (!PrepareStatus(status) || !session || !HasStructSize(graph) ||
        !outPayloadSize || (payloadCapacity && !payload) ||
        !ReadView(view, graphView))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        CKBehavior *native = ReadBehavior(root, context, result);
        if (!native)
            return ResultCode(result);
        GraphModel source;
        result = context.BehaviorSessions().ReadGraph(
            SessionId(session), native, graphView, source);
        if (!result)
            return ResultCode(result);
        return WriteGraphResult(source, graph, payload, payloadCapacity,
                                outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL InspectRun(
    BML_BehaviorRun run, std::uint32_t view, BML_BehaviorGraph *graph,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    GraphView graphView;
    if (!PrepareStatus(status) || !run || !HasStructSize(graph) ||
        !outPayloadSize || (payloadCapacity && !payload) ||
        !ReadView(view, graphView))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        GraphModel source;
        result = context.BehaviorSessions().ReadGraph(
            RunId(run), graphView, source);
        if (!result)
            return ResultCode(result);
        return WriteGraphResult(source, graph, payload, payloadCapacity,
                                outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadNodeLayout(
    BML_BehaviorSession session, BML_ObjectRef node,
    BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(layout) ||
        !outPayloadSize || (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        CKBehavior *native = ReadBehavior(node, context, result);
        if (!native)
            return ResultCode(result);
        Layout source;
        result = context.BehaviorSessions().ReadNodeLayout(
            SessionId(session), native, source);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadGraphValue(
    BML_BehaviorSession session, BML_ObjectRef node, std::uint32_t slotKind,
    std::uint64_t layoutGeneration,
    const BML_BehaviorSelector *slot, std::uint32_t read,
    BML_BehaviorGraphValue *value, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !slot ||
        !HasStructSize(value) || !outPayloadSize ||
        (payloadCapacity && !payload) ||
        read != BML_BEHAVIOR_READ_NON_FORCING)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SlotKind kind;
        if (!ReadDataSlotKind(slotKind, kind))
            return BML_ERROR_INVALID_PARAMETER;
        if (slot->Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
            layoutGeneration == 0) {
            result = InvalidValue(
                "An indexed graph port requires its Layout generation.");
            return BML_ERROR_INVALID_PARAMETER;
        }
        Slot selector;
        if (!ReadSelector(*slot, kind, CKGUID(), selector, result))
            return BML_ERROR_INVALID_PARAMETER;
        CKBehavior *native = ReadBehavior(node, context, result);
        if (!native)
            return ResultCode(result);
        GraphValue source;
        result = context.BehaviorSessions().ReadGraphValue(
            SessionId(session), native, layoutGeneration, selector,
            ReadMode::NonForcing, source);
        if (!result)
            return ResultCode(result);
        return WriteGraphValueResult(source, value, payload,
                                     payloadCapacity, outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL OpenWatch(
    BML_BehaviorSession session, const BML_BehaviorWatchSpec *source,
    const BML_BehaviorWatchFunction *callback,
    BML_BehaviorWatch *outWatch, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(source) ||
        !HasStructSize(callback) || !outWatch || !callback->Invoke ||
        (!!callback->Retain != !!callback->Release))
        return BML_ERROR_INVALID_PARAMETER;
    *outWatch = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        WatchSpec spec;
        CKBehavior *root = nullptr;
        CKBehavior *node = nullptr;
        const int read =
            ReadWatchSpec(*source, context, spec, root, node, result);
        if (read != BML_OK)
            return read;
        PlanCallbackState state = callback->Retain
            ? PlanCallbackState::Retained(
                  callback->State, callback->Retain, callback->Release)
            : PlanCallbackState::Static(callback->State);
        std::uintptr_t id = 0;
        result = context.BehaviorSessions().OpenWatch(
            SessionId(session), root, node, std::move(spec), std::move(state),
            WatchThunk(context, *callback), id);
        if (!result)
            return ResultCode(result);
        *outWatch = WatchHandle(id);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseWatch(BML_BehaviorWatch watch) {
    if (!watch)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &) {
        context.BehaviorSessions().CloseWatch(WatchId(watch));
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadWatch(BML_BehaviorWatch watch,
                                BML_BehaviorWatchInfo *info,
                                BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !watch || !HasStructSize(info))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        BML::Behavior::Internal::WatchInfo current;
        result = context.BehaviorSessions().ReadWatch(WatchId(watch), current);
        if (!result)
            return ResultCode(result);
        WriteWatchInfo(info, current);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL Set(
    BML_BehaviorRun run, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValue *value, std::uint64_t *outLayoutGeneration,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !run || !HasStructSize(slot) ||
        !HasStructSize(value) || !outLayoutGeneration)
        return BML_ERROR_INVALID_PARAMETER;
    *outLayoutGeneration = 0;
    return Enter(status, [&](ModContext &context, Status &result) {
        Slot target;
        Parameter::Binding binding;
        if (!ReadLiveSlot(*slot, target, result) ||
            !ReadValue(*value, context, binding, result))
            return ResultCode(result);
        result = context.BehaviorSessions().Set(
            RunId(run), slot->LayoutGeneration, target, binding,
            *outLayoutGeneration);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Bind(
    BML_BehaviorRun run, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValueRef *source, std::uint32_t relation,
    std::uint64_t *outLayoutGeneration, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !run || !HasStructSize(slot) ||
        !HasStructSize(source) || !outLayoutGeneration)
        return BML_ERROR_INVALID_PARAMETER;
    *outLayoutGeneration = 0;
    return Enter(status, [&](ModContext &context, Status &result) {
        Slot targetSlot;
        Slot sourceSlot;
        SlotKind sourceKind;
        if (!ReadLiveSlot(*slot, targetSlot, result) ||
            !ReadSlotKind(source->Kind, sourceKind) ||
            (source->Slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
             source->LayoutGeneration == 0) ||
            !ReadSelector(source->Slot, sourceKind, CKGUID(),
                          sourceSlot, result)) {
            if (result)
                result = InvalidValue("A Behavior Bind source is invalid.");
            return ResultCode(result);
        }
        Parameter::BindingKind nativeRelation;
        if (relation == BML_BEHAVIOR_VALUE_DIRECT) {
            nativeRelation = Parameter::BindingKind::Direct;
        } else if (relation == BML_BEHAVIOR_VALUE_SHARED) {
            nativeRelation = Parameter::BindingKind::Shared;
        } else {
            result = InvalidValue(
                "Behavior Bind accepts direct or shared source semantics.");
            return ResultCode(result);
        }
        CKBehavior *native = ReadBehavior(source->Node, context, result);
        if (!native)
            return ResultCode(result);
        result = context.BehaviorSessions().Bind(
            RunId(run), slot->LayoutGeneration, targetSlot, native,
            source->LayoutGeneration, sourceSlot, nativeRelation,
            *outLayoutGeneration);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Configure(
    BML_BehaviorRun run, const BML_BehaviorSettingStage *stages,
    std::uint32_t stageCount, std::uint64_t *outLayoutGeneration,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !run || !stages || stageCount == 0 ||
        !outLayoutGeneration)
        return BML_ERROR_INVALID_PARAMETER;
    *outLayoutGeneration = 0;
    return Enter(status, [&](ModContext &context, Status &result) {
        BlockSpec settings;
        for (std::uint32_t index = 0; index < stageCount; ++index) {
            const BML_BehaviorSettingStage &stage = stages[index];
            if (!HasStructSize(&stage)) {
                result = InvalidValue(
                    "A Behavior Setting stage has an unsupported StructSize.");
                return ResultCode(result);
            }
            if (index)
                settings.NextSettingStage();
            if (!ReadBindings(stage.Settings, stage.SettingCount,
                              SlotKind::Setting, context, settings, result))
                return ResultCode(result);
        }
        result = context.BehaviorSessions().Configure(
            RunId(run), settings, *outLayoutGeneration);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL SubmitPlan(
    BML_BehaviorSession session, const BML_BehaviorPlanSpec *spec,
    BML_BehaviorPlan *outPlan, BML_BehaviorPlanInfo *info,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(spec) ||
        !outPlan || (info && !HasStructSize(info)) || !spec->EditCount ||
        !spec->Edits)
        return BML_ERROR_INVALID_PARAMETER;
    *outPlan = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        std::string name;
        if (!ReadString(spec->Name, name) || name.empty())
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        std::vector<Installations::Rule> rules;
        result = ReadScriptEdits(spec->Edits, spec->EditCount, context, rules);
        if (!result)
            return ResultCode(result);

        Installations &installations = context.BehaviorInstallations();
        PlanId id = 0;
        result = installations.Submit(owner, std::move(name),
                                      std::move(rules), id);
        if (!result)
            return ResultCode(result);
        *outPlan = PlanHandleOf(id);
        PlanInfo read;
        if (info && installations.ReadPlan(owner, id, read))
            WritePlanInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPlan(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    BML_BehaviorPlanInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || !HasStructSize(info))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        PlanInfo read;
        result = context.BehaviorInstallations().ReadPlan(
            owner, PlanIdOf(plan), read);
        if (!result)
            return ResultCode(result);
        WritePlanInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPlanFailures(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    BML_BehaviorFailures *failures, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan ||
        !HasStructSize(failures))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        PlanInfo read;
        result = context.BehaviorInstallations().ReadPlan(
            owner, PlanIdOf(plan), read);
        if (!result)
            return ResultCode(result);
        WriteFailures(failures, read.ApplyFailure, read.RestoreFailure);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPlanInstances(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    BML_BehaviorPlanInstance *instances,
    std::uint32_t instanceCapacity, std::uint32_t instanceStride,
    std::uint32_t *outInstanceCount, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || !outInstanceCount ||
        (instanceCapacity &&
         (!instances || instanceStride < sizeof(*instances))))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        std::vector<Installations::PlanInstance> found;
        result = context.BehaviorInstallations().ReadPlanInstances(
            owner, PlanIdOf(plan), found);
        if (!result)
            return ResultCode(result);
        return WritePlanInstances(found, instances, instanceCapacity,
                                  instanceStride, outInstanceCount);
    });
}

int BML_BEHAVIOR_CALL ClosePlan(BML_BehaviorSession session,
                                BML_BehaviorPlan plan) {
    if (!session || !plan)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        return ResultCode(context.BehaviorInstallations().ClosePlan(
            owner, PlanIdOf(plan)));
    });
}

int BML_BEHAVIOR_CALL SetPlanActive(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    std::uint32_t active, BML_BehaviorPlanInfo *info,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || active > 1 ||
        (info && !HasStructSize(info)))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        Installations &installations = context.BehaviorInstallations();
        result = installations.SetPlanActive(owner, PlanIdOf(plan),
                                             active != 0);
        PlanInfo read;
        if (info && installations.ReadPlan(owner, PlanIdOf(plan), read))
            WritePlanInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ReplacePlan(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    const BML_BehaviorScriptEdit *edits, std::uint32_t editCount,
    BML_BehaviorPlanInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan ||
        (info && !HasStructSize(info)))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        Installations &installations = context.BehaviorInstallations();
        std::vector<Installations::Rule> rules;
        result = ReadScriptEdits(edits, editCount, context, rules);
        if (result)
            result = installations.ReplacePlan(owner, PlanIdOf(plan),
                                               std::move(rules));
        PlanInfo read;
        if (info && installations.ReadPlan(owner, PlanIdOf(plan), read))
            WritePlanInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ApplyPatch(
    BML_BehaviorSession session, const BML_BehaviorPatchSpec *spec,
    BML_BehaviorPatch *outPatch, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(spec) ||
        !outPatch || (info && !HasStructSize(info)) || !spec->EditCount ||
        !spec->Edits)
        return BML_ERROR_INVALID_PARAMETER;
    *outPatch = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        std::string name;
        if (!ReadString(spec->Name, name) || name.empty())
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        std::vector<Installations::Target> targets;
        result = ReadGraphEdits(spec->Edits, spec->EditCount, context,
                                targets);
        if (!result)
            return ResultCode(result);

        Installations &installations = context.BehaviorInstallations();
        PatchId id = 0;
        result = installations.Apply(owner, std::move(name),
                                     std::move(targets), id);
        if (id)
            *outPatch = PatchHandleOf(id);
        if (!result)
            return ResultCode(result);
        PatchInfo read;
        if (info && installations.Read(owner, id, read))
            WritePatchInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPatch(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch || !HasStructSize(info))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        PatchInfo read;
        result = context.BehaviorInstallations().Read(
            owner, PatchIdOf(patch), read);
        if (!result)
            return ResultCode(result);
        WritePatchInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPatchFailures(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    BML_BehaviorFailures *failures, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch ||
        !HasStructSize(failures))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        PatchInfo read;
        result = context.BehaviorInstallations().Read(
            owner, PatchIdOf(patch), read);
        if (!result)
            return ResultCode(result);
        WriteFailures(failures, read.ApplyFailure, read.RestoreFailure);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ClosePatch(BML_BehaviorSession session,
                                 BML_BehaviorPatch patch) {
    if (!session || !patch)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        return ResultCode(context.BehaviorInstallations().Close(
            owner, PatchIdOf(patch)));
    });
}

int BML_BEHAVIOR_CALL SetPatchActive(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    std::uint32_t active, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch || active > 1 ||
        (info && !HasStructSize(info)))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        Installations &installations = context.BehaviorInstallations();
        result = installations.SetActive(owner, PatchIdOf(patch), active != 0);
        PatchInfo read;
        if (info && installations.Read(owner, PatchIdOf(patch), read))
            WritePatchInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ReplacePatch(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    const BML_BehaviorGraphEdit *edits, std::uint32_t editCount,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch ||
        (info && !HasStructSize(info)))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        Installations &installations = context.BehaviorInstallations();
        std::vector<Installations::Target> targets;
        result = ReadGraphEdits(edits, editCount, context, targets);
        if (result)
            result = installations.Replace(owner, PatchIdOf(patch),
                                           std::move(targets));
        PatchInfo read;
        if (info && installations.Read(owner, PatchIdOf(patch), read))
            WritePatchInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Reference(BML_BehaviorSession session,
                                std::uint32_t object,
                                BML_ObjectRef *outReference,
                                BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !outReference)
        return BML_ERROR_INVALID_PARAMETER;
    *outReference = BML_ObjectRef{};
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        CKContext *ck = context.GetCKContext();
        CKObject *found = ck && object
            ? ck->GetObject(static_cast<CK_ID>(object)) : nullptr;
        if (!found || found->IsToBeDeleted())
            return BML_ERROR_NOT_FOUND;
        const BML_ObjectRef issued = context.ObjectRefs().Issue(found);
        if (!issued.Domain)
            return BML_ERROR_FAIL;
        *outReference = issued;
        return BML_OK;
    });
}

BML_ObjectRef PublicRef(
    const BML::Behavior::Internal::ObjectRef &from) noexcept {
    return {from.Domain, from.Slot, from.Generation};
}

int BML_BEHAVIOR_CALL ResolvePatchNode(BML_BehaviorSession session,
                                      BML_BehaviorPatch patch,
                                      const BML_BehaviorNodeRef *node,
                                      BML_ObjectRef *outNode,
                                      BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch || !node || !outNode)
        return BML_ERROR_INVALID_PARAMETER;
    *outNode = BML_ObjectRef{};
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::SymbolRef selected;
        if (!ReadNodeRef(*node, selected, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        BML::Behavior::Internal::ObjectRef resolved;
        result = context.BehaviorInstallations().ResolveNode(
            owner, PatchIdOf(patch), selected, resolved);
        if (!result)
            return ResultCode(result);
        *outNode = PublicRef(resolved);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ResolvePlanInstanceNode(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    const BML_BehaviorPlanInstance *instance,
    const BML_BehaviorNodeRef *node, BML_ObjectRef *outNode,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || !instance ||
        !node || !outNode)
        return BML_ERROR_INVALID_PARAMETER;
    *outNode = BML_ObjectRef{};
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::PlanInstance selected;
        Installations::SymbolRef selectedNode;
        if (!ReadPlanInstance(*instance, selected, result) ||
            !ReadNodeRef(*node, selectedNode, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        BML::Behavior::Internal::ObjectRef resolved;
        result = context.BehaviorInstallations().ResolvePlanNode(
            owner, PlanIdOf(plan), selected, selectedNode, resolved);
        if (!result)
            return ResultCode(result);
        *outNode = PublicRef(resolved);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPatchValue(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    const BML_BehaviorPortRef *port, BML_BehaviorGraphValue *value,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch || !port ||
        !HasStructSize(value) || !outPayloadSize ||
        (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::PortQuery selected;
        if (!ReadPortQuery(*port, selected, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        GraphValue read;
        result = context.BehaviorInstallations().ReadValue(
            owner, PatchIdOf(patch), selected, read);
        if (!result)
            return ResultCode(result);
        return WriteGraphValueResult(read, value, payload, payloadCapacity,
                                     outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL WritePatchValue(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    const BML_BehaviorPortRef *port, const BML_BehaviorValue *value,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !patch || !port || !value)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::PortQuery selected;
        Parameter::Binding binding;
        if (!ReadPortQuery(*port, selected, result) ||
            !ReadValue(*value, context, binding, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        result = context.BehaviorInstallations().WriteValue(
            owner, PatchIdOf(patch), selected, binding);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ReadPlanInstanceValue(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    const BML_BehaviorPlanInstance *instance,
    const BML_BehaviorPortRef *port, BML_BehaviorGraphValue *value,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || !instance ||
        !port || !HasStructSize(value) || !outPayloadSize ||
        (payloadCapacity && !payload))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::PlanInstance selected;
        Installations::PortQuery selectedPort;
        if (!ReadPlanInstance(*instance, selected, result) ||
            !ReadPortQuery(*port, selectedPort, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        GraphValue read;
        result = context.BehaviorInstallations().ReadPlanValue(
            owner, PlanIdOf(plan), selected, selectedPort, read);
        if (!result)
            return ResultCode(result);
        return WriteGraphValueResult(read, value, payload, payloadCapacity,
                                     outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL WritePlanInstanceValue(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    const BML_BehaviorPlanInstance *instance,
    const BML_BehaviorPortRef *port, const BML_BehaviorValue *value,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !plan || !instance ||
        !port || !value)
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        Installations::PlanInstance selected;
        Installations::PortQuery selectedPort;
        Parameter::Binding binding;
        if (!ReadPlanInstance(*instance, selected, result) ||
            !ReadPortQuery(*port, selectedPort, result) ||
            !ReadValue(*value, context, binding, result))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        result = context.BehaviorInstallations().WritePlanValue(
            owner, PlanIdOf(plan), selected, selectedPort, binding);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL CreateScript(
    BML_BehaviorSession session, const BML_BehaviorScriptSpec *spec,
    BML_BehaviorScript *outScript, BML_BehaviorScriptInfo *info,
    BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !HasStructSize(spec) ||
        !outScript || (info && !HasStructSize(info)) || !spec->Owner.Domain)
        return BML_ERROR_INVALID_PARAMETER;
    *outScript = nullptr;
    return Enter(status, [&](ModContext &context, Status &result) {
        std::string name;
        if (!ReadNativeString(spec->Name, name))
            return BML_ERROR_INVALID_PARAMETER;
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        Program body;
        result = DecodeProgram(spec->Program, context, body);
        if (!result)
            return ResultCode(result);
        CKBeObject *nativeOwner = ReadOwner(spec->Owner, context, result);
        if (!nativeOwner)
            return BML_ERROR_OBJECT_INVALID;
        ScriptResult opened = context.BehaviorScripts().Create(
            owner, SessionId(session), nativeOwner, std::move(name),
            spec->Priority, std::move(body));
        result = opened.Result;
        if (!opened)
            return ResultCode(result);
        *outScript = ScriptHandle(opened.Id);
        WriteScriptInfo(info, opened.Info);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadScript(
    BML_BehaviorSession session, BML_BehaviorScript script,
    BML_BehaviorScriptInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !script ||
        !HasStructSize(info))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        ScriptInfo read;
        result = context.BehaviorScripts().Read(
            owner, ScriptIdOf(script), read);
        if (!result)
            return ResultCode(result);
        WriteScriptInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL SetScriptActive(
    BML_BehaviorSession session, BML_BehaviorScript script,
    std::uint32_t active, std::uint32_t reset,
    BML_BehaviorScriptInfo *info, BML_BehaviorStatus *status) {
    if (!PrepareStatus(status) || !session || !script ||
        (info && !HasStructSize(info)) || active > 1 || reset > 1 ||
        (!active && reset))
        return BML_ERROR_INVALID_PARAMETER;
    return Enter(status, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        ScriptInfo read;
        result = context.BehaviorScripts().SetActive(
            owner, ScriptIdOf(script), active != 0, reset != 0, read);
        if (!result)
            return ResultCode(result);
        WriteScriptInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseScript(BML_BehaviorSession session,
                                  BML_BehaviorScript script) {
    if (!session || !script)
        return BML_ERROR_INVALID_HANDLE;
    return Enter(nullptr, [&](ModContext &context, Status &result) {
        SessionOwner owner;
        if (!ReadSessionOwner(session, context, owner, result))
            return ResultCode(result);
        return ResultCode(context.BehaviorScripts().Close(
            owner, ScriptIdOf(script)));
    });
}

const BML_BehaviorInterface kBehaviorInterface = {
    BML_IFACE_HEADER(BML_BehaviorInterface, BML_BEHAVIOR_INTERFACE_ID,
                     BML_BEHAVIOR_INTERFACE_MAJOR,
                     BML_BEHAVIOR_INTERFACE_MINOR),
    &OpenSession,
    &CloseSession,
    &Call,
    &Start,
    &Spawn,
    &Continue,
    &Pulse,
    &ReadRun,
    &TakeFrames,
    &CloseRun,
    &FindPrototypes,
    &ReadDeclaredLayout,
    &ReadLiveLayout,
    &Inspect,
    &ReadNodeLayout,
    &ReadGraphValue,
    &OpenWatch,
    &CloseWatch,
    &SubmitPlan,
    &ReadPlan,
    &ClosePlan,
    &InspectRun,
    &Set,
    &Bind,
    &Configure,
    &ApplyPatch,
    &ReadPatch,
    &ClosePatch,
    &ReadWatch,
    &Reference,
    &ResolvePatchNode,
    &AttachBlock,
    &CreateScript,
    &ReadScript,
    &SetScriptActive,
    &CloseScript,
    &SetPatchActive,
    &ReplacePatch,
    &SetPlanActive,
    &ReplacePlan,
    &ReadPatchFailures,
    &ReadPlanFailures,
    &ReadPlanInstances,
    &ResolvePlanInstanceNode,
    &ReadPatchValue,
    &WritePatchValue,
    &ReadPlanInstanceValue,
    &WritePlanInstanceValue,
    &ValidateBlock,
    &AttachHook,
};

} // namespace
} // namespace BML::Api::Behavior

namespace BML::Api {

const BML_BehaviorInterface &BehaviorInterface() noexcept {
    return Behavior::kBehaviorInterface;
}

int OpenBehaviorSessionFor(std::string_view ownerId,
                           BML_BehaviorSession *outSession,
                           BML_BehaviorStatus *status) noexcept {
    if (!Behavior::PrepareStatus(status) || !outSession || ownerId.empty())
        return BML_ERROR_INVALID_PARAMETER;
    *outSession = nullptr;
    return Behavior::Enter(status, [&](ModContext &context,
                                       Behavior::Status &result) {
        std::uintptr_t id = 0;
        result = context.BehaviorSessions().OpenSession(std::string(ownerId), id);
        if (!result)
            return Behavior::ResultCode(result);
        *outSession = Behavior::SessionHandle(id);
        return BML_OK;
    });
}

} // namespace BML::Api
