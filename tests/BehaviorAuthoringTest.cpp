#include "BML/Behavior.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace BML::Behavior;

struct CapturedStep {
    std::uint32_t Kind = 0;
    std::uint32_t Result = 0;
    std::uint32_t Flags = 0;
    std::uint32_t Target = 0;
    std::uint32_t Node = 0;
    std::uint32_t SlotKind = 0;
    std::int32_t Delay = 0;
    std::string Name;
    BML_BehaviorGuid Prototype{};
    BML_BehaviorGuid Type{};
    BML_BehaviorPortRef Source{};
    BML_BehaviorPortRef Sink{};
    std::string SourceSlot;
    std::string SinkSlot;
    BML_BehaviorValue Value{};
    bool HasHook = false;
    std::vector<std::pair<std::uint32_t, std::string>> Ordering;
};

struct FakeState {
    std::string Owner;
    std::string Input;
    std::string SettingName;
    std::string Text;
    std::uint32_t SelectorKind = 0;
    std::uint32_t FrameKind = 0;
    std::uint32_t FrameLimit = 0;
    std::uint64_t Generation = 0;
    std::uint64_t ProviderGeneration = 73;
    std::uint64_t LiveGeneration = 9;
    int LayoutCalls = 0;
    int OpenRuns = 0;
    int SessionCloses = 0;
    int RunCloses = 0;
    int RunCode = BML_OK;
    bool FramesAvailable = true;
    bool LayoutUnavailable = false;
    bool MalformedLayout = false;
    bool NullRun = false;
    std::uint32_t GraphView = 0;
    int WatchCloses = 0;
    BML_BehaviorWatchSpec WatchSpec{};
    BML_BehaviorWatchFunction WatchFunction{};
    std::vector<BML_ObjectRef> RunOwners;
    std::vector<const BML_BehaviorBlock *> Blocks;
    int PlanSubmits = 0;
    int PlanReads = 0;
    int PlanCloses = 0;
    int PlanSubmitCode = BML_OK;
    std::uint32_t PlanTargets = 0;
    std::string PlanName;
    std::string PlanScript;
    std::vector<CapturedStep> PlanSteps;
    std::vector<BML_BehaviorHookFunction> PlanHooks;
    int PatchApplies = 0;
    int PatchReads = 0;
    int PatchCloses = 0;
    int PatchApplyCode = BML_OK;
    std::string PatchName;
    BML_ObjectRef PatchGraph{};
    std::vector<CapturedStep> PatchSteps;
    std::vector<BML_BehaviorHookFunction> PatchHooks;
    std::uint32_t SetKind = 0;
    std::uint64_t SetGeneration = 0;
    std::string SetSlot;
    std::int32_t SetValue = 0;
    std::uint32_t BindRelation = 0;
    BML_ObjectRef BindNode{};
    std::string BindSource;
    std::vector<std::string> ConfiguredSettings;
};

FakeState g_State;

template <class T>
void Init(T *value) {
    if (!value)
        return;
    *value = {};
    value->StructSize = sizeof(*value);
}

void Success(BML_BehaviorStatus *status) {
    Init(status);
}

int BML_BEHAVIOR_CALL OpenSession(BML_BehaviorString owner,
                                  BML_BehaviorSession *session,
                                  BML_BehaviorStatus *status) {
    g_State.Owner.assign(owner.Data, owner.Length);
    *session = reinterpret_cast<BML_BehaviorSession>(1);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseSession(BML_BehaviorSession) {
    ++g_State.SessionCloses;
    return BML_OK;
}

int OpenRun(BML_ObjectRef owner,
            const BML_BehaviorBlock *block,
            const BML_BehaviorSelector *input,
            BML_BehaviorRun *run,
            BML_BehaviorRunInfo *info,
            BML_BehaviorStatus *status,
            std::uint32_t kind) {
    ++g_State.OpenRuns;
    g_State.RunOwners.push_back(owner);
    g_State.Blocks.push_back(block);
    g_State.Generation = block->PrototypeGeneration;
    g_State.FrameKind = block->Frames.Kind;
    g_State.FrameLimit = block->Frames.Limit;
    if (input) {
        g_State.SelectorKind = input->Kind;
        g_State.Input.assign(input->Name.Data, input->Name.Length);
    }
    if (block->SettingStageCount && block->SettingStages[0].SettingCount) {
        const BML_BehaviorBinding &binding =
            block->SettingStages[0].Settings[0];
        g_State.SettingName.assign(binding.Slot.Name.Data,
                                   binding.Slot.Name.Length);
        g_State.Text.assign(binding.Value.Data.Utf8.Data,
                            binding.Value.Data.Utf8.Length);
    }
    *run = g_State.NullRun ? nullptr : reinterpret_cast<BML_BehaviorRun>(2);
    Init(info);
    info->Kind = kind;
    info->State = kind == BML_BEHAVIOR_RUN_CALL
        ? BML_BEHAVIOR_RUN_PENDING
        : BML_BEHAVIOR_RUN_READY;
    Init(&info->Status);
    Success(status);
    return g_State.RunCode;
}

int BML_BEHAVIOR_CALL CallRun(BML_BehaviorSession, BML_ObjectRef owner,
                              const BML_BehaviorBlock *block,
                              const BML_BehaviorSelector *input,
                              BML_BehaviorRun *run,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    return OpenRun(owner, block, input, run, info, status,
                   BML_BEHAVIOR_RUN_CALL);
}

int BML_BEHAVIOR_CALL StartRun(BML_BehaviorSession, BML_ObjectRef owner,
                               const BML_BehaviorBlock *block,
                               const BML_BehaviorSelector *input,
                               BML_BehaviorRun *run,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    return OpenRun(owner, block, input, run, info, status,
                   BML_BEHAVIOR_RUN_TASK);
}

int BML_BEHAVIOR_CALL SpawnRun(BML_BehaviorSession, BML_ObjectRef owner,
                               const BML_BehaviorBlock *block,
                               BML_BehaviorRun *run,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    return OpenRun(owner, block, nullptr, run, info, status,
                   BML_BEHAVIOR_RUN_INSTANCE);
}

int BML_BEHAVIOR_CALL ContinueRun(BML_BehaviorRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status) {
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_TASK;
    info->State = BML_BEHAVIOR_RUN_PENDING;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL PulseRun(BML_BehaviorRun,
                               const BML_BehaviorSelector *input,
                               std::uint32_t *admission,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    g_State.Input.assign(input->Name.Data, input->Name.Length);
    *admission = BML_BEHAVIOR_ADMISSION_EXECUTED;
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_INSTANCE;
    info->State = BML_BEHAVIOR_RUN_READY;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadRun(BML_BehaviorRun,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    Init(info);
    info->Kind = BML_BEHAVIOR_RUN_TASK;
    info->State = BML_BEHAVIOR_RUN_PENDING;
    Init(&info->Status);
    Success(status);
    return BML_OK;
}

std::vector<std::uint8_t> FramePayload() {
    std::vector<std::uint8_t> payload(73, 0);
    BML_BehaviorOutRecord out{};
    out.StructSize = sizeof(out);
    out.NameOffset = 20;
    out.NameLength = 4;
    std::memcpy(payload.data(), &out, sizeof(out));
    std::memcpy(payload.data() + 20, "Done", 4);

    BML_BehaviorPoutRecord pout{};
    pout.StructSize = sizeof(pout);
    pout.Type = {CKPGUID_INT.d1, CKPGUID_INT.d2};
    pout.Kind = BML_BEHAVIOR_VALUE_INT32;
    pout.NameOffset = 64;
    pout.NameLength = 5;
    pout.ValueOffset = 69;
    pout.ValueSize = 4;
    std::memcpy(payload.data() + 24, &pout, sizeof(pout));
    std::memcpy(payload.data() + 64, "Value", 5);
    payload[69] = 42;
    return payload;
}

int BML_BEHAVIOR_CALL TakeFrames(BML_BehaviorRun,
                                 BML_BehaviorRunFrame *frames,
                                 std::uint32_t frameCapacity,
                                 std::uint32_t,
                                 void *payload,
                                 std::uint32_t payloadCapacity,
                                 std::uint32_t *frameCount,
                                 std::uint32_t *payloadSize,
                                 BML_BehaviorStatus *status) {
    Success(status);
    if (!g_State.FramesAvailable) {
        *frameCount = 0;
        *payloadSize = 0;
        return BML_OK;
    }
    const std::vector<std::uint8_t> bytes = FramePayload();
    *frameCount = 1;
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    if (frameCapacity < 1 || payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *frames = {};
    frames->StructSize = sizeof(*frames);
    frames->Sequence = 7;
    frames->Frame = 91;
    frames->OutOffset = 0;
    frames->OutCount = 1;
    frames->PoutOffset = 24;
    frames->PoutCount = 1;
    std::memcpy(payload, bytes.data(), bytes.size());
    g_State.FramesAvailable = false;
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseRun(BML_BehaviorRun) {
    ++g_State.RunCloses;
    return BML_OK;
}

int BML_BEHAVIOR_CALL FindPrototypes(
    BML_BehaviorSession, const BML_BehaviorPrototypeQuery *,
    BML_BehaviorPrototypeInfo *, std::uint32_t, std::uint32_t, void *,
    std::uint32_t, std::uint32_t *, std::uint32_t *, BML_BehaviorStatus *) {
    return BML_ERROR_NOT_IMPLEMENTED;
}

struct FakeSlot {
    std::uint32_t Kind;
    std::int32_t Index;
    std::int32_t Occurrence;
    CKGUID Type;
    std::uint32_t ValueKind;
    std::string_view Name;
    bool Supported = true;
};

std::vector<std::uint8_t> DeclaredLayout(
    const BML_BehaviorPrototypeRef &prototype,
    BML_BehaviorLayout &layout) {
    const FakeSlot slots[] = {
        {BML_BEHAVIOR_SLOT_IN, 0, 0, CKGUID(), 0, "Run", false},
        {BML_BEHAVIOR_SLOT_SETTING, 0, 0, CKPGUID_STRING,
         BML_BEHAVIOR_VALUE_UTF8, "Caption"},
        {BML_BEHAVIOR_SLOT_SETTING, 1, 0, CKPGUID_BOOL,
         BML_BEHAVIOR_VALUE_BOOL, "Retry"},
        {BML_BEHAVIOR_SLOT_SETTING, 2, 0, CKPGUID_BOOL,
         BML_BEHAVIOR_VALUE_BOOL, "Extended Layout"},
        {BML_BEHAVIOR_SLOT_SETTING, 4, 1, CKPGUID_BOOL,
         BML_BEHAVIOR_VALUE_BOOL, "Duplicate"},
        {BML_BEHAVIOR_SLOT_SETTING, 3, 0, CKPGUID_INT,
         BML_BEHAVIOR_VALUE_INT32, "Duplicate"},
        {BML_BEHAVIOR_SLOT_SETTING, 5, 0, CKGUID(91, 92), 0,
         "Opaque", false},
        {BML_BEHAVIOR_SLOT_PIN, 0, 0, CKPGUID_INT,
         BML_BEHAVIOR_VALUE_INT32, "Value"},
        {BML_BEHAVIOR_SLOT_LOCAL, 0, 0, CKPGUID_INT,
         BML_BEHAVIOR_VALUE_INT32, "State"},
    };

    std::vector<std::uint8_t> payload(sizeof(slots) / sizeof(slots[0]) *
                                      sizeof(BML_BehaviorSlotRecord));
    for (std::size_t index = 0; index < std::size(slots); ++index) {
        const FakeSlot &source = slots[index];
        BML_BehaviorSlotRecord slot{};
        slot.StructSize = sizeof(slot);
        slot.Kind = source.Kind;
        slot.Index = source.Index;
        slot.Occurrence = source.Occurrence;
        slot.Type = {source.Type.d1, source.Type.d2};
        slot.ValueKind = source.ValueKind;
        if (source.Supported)
            slot.Flags |= BML_BEHAVIOR_SLOT_VALUE_SUPPORTED;
        slot.Name.Offset = static_cast<std::uint32_t>(payload.size());
        slot.Name.Length = static_cast<std::uint32_t>(source.Name.size());
        payload.insert(payload.end(), source.Name.begin(), source.Name.end());
        slot.TypeName.Offset = static_cast<std::uint32_t>(payload.size());
        std::memcpy(payload.data() + index * sizeof(slot), &slot, sizeof(slot));
    }

    layout = {};
    layout.StructSize = sizeof(layout);
    layout.Origin = BML_BEHAVIOR_LAYOUT_DECLARED;
    layout.Prototype = prototype;
    if (!layout.Prototype.Generation)
        layout.Prototype.Generation = g_State.ProviderGeneration;
    layout.LayoutGeneration = 1;
    layout.Kind = BML_BEHAVIOR_KIND_FUNCTION;
    layout.SlotOffset = 0;
    layout.SlotCount = g_State.MalformedLayout
        ? (std::numeric_limits<std::uint32_t>::max)()
        : static_cast<std::uint32_t>(std::size(slots));
    return payload;
}

int BML_BEHAVIOR_CALL ReadDeclaredLayout(
    BML_BehaviorSession, const BML_BehaviorPrototypeRef *prototype,
    BML_BehaviorLayout *layout, void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    ++g_State.LayoutCalls;
    if (g_State.LayoutUnavailable) {
        Success(status);
        return BML_ERROR_UNAVAILABLE;
    }
    const std::vector<std::uint8_t> bytes = DeclaredLayout(*prototype, *layout);
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    Success(status);
    if (payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    if (!bytes.empty())
        std::memcpy(payload, bytes.data(), bytes.size());
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadLiveLayout(
    BML_BehaviorRun, BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *payloadSize,
    BML_BehaviorStatus *status) {
    BML_BehaviorPrototypeRef prototype{};
    prototype.StructSize = sizeof(prototype);
    prototype.Prototype = {1, 2};
    prototype.Generation = g_State.ProviderGeneration;
    const std::vector<std::uint8_t> bytes = DeclaredLayout(prototype, *layout);
    layout->Origin = BML_BEHAVIOR_LAYOUT_LIVE;
    layout->LayoutGeneration = g_State.LiveGeneration;
    layout->Flags = BML_BEHAVIOR_LAYOUT_MATERIALIZED_NOW;
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    Success(status);
    if (payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    if (!bytes.empty())
        std::memcpy(payload, bytes.data(), bytes.size());
    return BML_OK;
}

std::vector<std::uint8_t> GraphPayload(BML_ObjectRef root,
                                       BML_BehaviorGraph &graph) {
    const std::uint32_t nodeOffset = 0;
    const std::uint32_t linkOffset = sizeof(BML_BehaviorGraphNode);
    const std::uint32_t portOffset = linkOffset +
        sizeof(BML_BehaviorGraphLink);
    std::vector<std::uint8_t> payload(
        portOffset + sizeof(BML_BehaviorGraphPort));

    BML_BehaviorGraphNode node{};
    node.StructSize = sizeof(node);
    node.Id = 101;
    node.Object = root;
    node.Prototype = {21, 22};
    node.Priority = 7;
    node.Active = 1;
    node.PortOffset = portOffset;
    node.PortCount = 1;
    node.Name.Offset = static_cast<std::uint32_t>(payload.size());
    node.Name.Length = 4;
    payload.insert(payload.end(), {'R', 'o', 'o', 't'});

    BML_BehaviorGraphPort port{};
    port.StructSize = sizeof(port);
    port.Node = node.Id;
    port.Kind = BML_BEHAVIOR_SLOT_IN;
    port.Index = 0;
    port.Active = 1;
    port.Name.Offset = static_cast<std::uint32_t>(payload.size());
    port.Name.Length = 3;
    payload.insert(payload.end(), {'R', 'u', 'n'});

    BML_BehaviorGraphLink link{};
    link.StructSize = sizeof(link);
    link.Id = 201;
    link.Object = {31, 32, 33};
    link.SourceNode = node.Id;
    link.SourceKind = BML_BEHAVIOR_SLOT_OUT;
    link.SourceIndex = 0;
    link.TargetNode = node.Id;
    link.TargetKind = BML_BEHAVIOR_SLOT_IN;
    link.TargetIndex = 0;
    link.InitialDelay = 2;
    link.RemainingDelay = 1;
    link.Pending = BML_BEHAVIOR_UNKNOWN;

    std::memcpy(payload.data() + nodeOffset, &node, sizeof(node));
    std::memcpy(payload.data() + linkOffset, &link, sizeof(link));
    std::memcpy(payload.data() + portOffset, &port, sizeof(port));

    graph = {};
    graph.StructSize = sizeof(graph);
    graph.View = g_State.GraphView;
    graph.Root = root;
    graph.Generation = 5;
    graph.Fingerprint = 0x713;
    graph.NodeOffset = nodeOffset;
    graph.NodeCount = 1;
    graph.LinkOffset = linkOffset;
    graph.LinkCount = 1;
    return payload;
}

int BML_BEHAVIOR_CALL InspectGraph(
    BML_BehaviorSession, BML_ObjectRef root, std::uint32_t view,
    BML_BehaviorGraph *graph, void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    g_State.GraphView = view;
    BML_BehaviorGraph wire{};
    const std::vector<std::uint8_t> bytes = GraphPayload(root, wire);
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    Success(status);
    if (payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *graph = wire;
    if (!bytes.empty())
        std::memcpy(payload, bytes.data(), bytes.size());
    return BML_OK;
}

int BML_BEHAVIOR_CALL InspectRun(
    BML_BehaviorRun, std::uint32_t view, BML_BehaviorGraph *graph,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    return InspectGraph(nullptr, {71, 72, 73}, view, graph, payload,
                        payloadCapacity, payloadSize, status);
}

int BML_BEHAVIOR_CALL SetRun(
    BML_BehaviorRun, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValue *value, std::uint64_t *generation,
    BML_BehaviorStatus *status) {
    Init(status);
    if (slot->LayoutGeneration &&
        slot->LayoutGeneration != g_State.LiveGeneration) {
        status->Error = BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
        return BML_ERROR_FAIL;
    }
    g_State.SetKind = slot->Kind;
    g_State.SetGeneration = slot->LayoutGeneration;
    g_State.SetSlot.assign(slot->Slot.Name.Data ? slot->Slot.Name.Data : "",
                           slot->Slot.Name.Length);
    if (value->Kind == BML_BEHAVIOR_VALUE_INT32)
        g_State.SetValue = value->Data.Int32;
    *generation = g_State.LiveGeneration;
    return BML_OK;
}

int BML_BEHAVIOR_CALL BindRun(
    BML_BehaviorRun, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValueRef *source, std::uint32_t relation,
    std::uint64_t *generation, BML_BehaviorStatus *status) {
    Init(status);
    if (slot->LayoutGeneration != g_State.LiveGeneration) {
        status->Error = BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
        return BML_ERROR_FAIL;
    }
    g_State.BindRelation = relation;
    g_State.BindNode = source->Node;
    g_State.BindSource.assign(
        source->Slot.Name.Data ? source->Slot.Name.Data : "",
        source->Slot.Name.Length);
    *generation = g_State.LiveGeneration;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ConfigureRun(
    BML_BehaviorRun, const BML_BehaviorSettingStage *stages,
    std::uint32_t stageCount, std::uint64_t *generation,
    BML_BehaviorStatus *status) {
    Init(status);
    g_State.ConfiguredSettings.clear();
    for (std::uint32_t stageIndex = 0; stageIndex < stageCount;
         ++stageIndex) {
        for (std::uint32_t index = 0;
             index < stages[stageIndex].SettingCount; ++index) {
            const BML_BehaviorBinding &binding =
                stages[stageIndex].Settings[index];
            g_State.ConfiguredSettings.emplace_back(
                binding.Slot.Name.Data ? binding.Slot.Name.Data : "",
                binding.Slot.Name.Length);
        }
    }
    *generation = ++g_State.LiveGeneration;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadNodeLayout(
    BML_BehaviorSession, BML_ObjectRef, BML_BehaviorLayout *, void *,
    std::uint32_t, std::uint32_t *, BML_BehaviorStatus *) {
    return BML_ERROR_NOT_IMPLEMENTED;
}

int BML_BEHAVIOR_CALL ReadGraphValue(
    BML_BehaviorSession, BML_ObjectRef, std::uint32_t,
    const BML_BehaviorSelector *slot, std::uint32_t,
    BML_BehaviorGraphValue *value, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *payloadSize,
    BML_BehaviorStatus *status) {
    Success(status);
    const std::string_view name(
        slot->Name.Data ? slot->Name.Data : "", slot->Name.Length);
    *value = {};
    value->StructSize = sizeof(*value);
    value->Type = {CKPGUID_INT.d1, CKPGUID_INT.d2};
    if (name == "Computed") {
        value->State = BML_BEHAVIOR_VALUE_INDETERMINATE;
        value->Relation = BML_BEHAVIOR_VALUE_OPERATION;
        *payloadSize = 0;
        return BML_OK;
    }
    value->State = BML_BEHAVIOR_VALUE_AVAILABLE;
    value->Relation = BML_BEHAVIOR_VALUE_STORED;
    value->Kind = BML_BEHAVIOR_VALUE_INT32;
    value->ValueOffset = 0;
    value->ValueSize = 4;
    *payloadSize = 4;
    if (payloadCapacity < 4)
        return BML_ERROR_BUFFER_TOO_SMALL;
    const std::uint8_t bytes[] = {42, 0, 0, 0};
    std::memcpy(payload, bytes, sizeof(bytes));
    return BML_OK;
}

int BML_BEHAVIOR_CALL OpenWatch(
    BML_BehaviorSession, const BML_BehaviorWatchSpec *spec,
    const BML_BehaviorWatchFunction *callback,
    BML_BehaviorWatch *watch, BML_BehaviorStatus *status) {
    Success(status);
    if (spec->Kind == BML_BEHAVIOR_WATCH_EXACT_VALUE)
        return BML_ERROR_UNAVAILABLE;
    g_State.WatchSpec = *spec;
    g_State.WatchFunction = *callback;
    if (callback->Retain)
        callback->Retain(callback->State);
    *watch = reinterpret_cast<BML_BehaviorWatch>(3);
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseWatch(BML_BehaviorWatch) {
    ++g_State.WatchCloses;
    if (g_State.WatchFunction.Release)
        g_State.WatchFunction.Release(g_State.WatchFunction.State);
    g_State.WatchFunction = {};
    return BML_OK;
}

std::string Copy(BML_BehaviorString text) {
    return text.Data ? std::string(text.Data, text.Length) : std::string();
}

void CaptureSteps(const BML_BehaviorEditStep *steps, std::uint32_t count,
                  std::vector<CapturedStep> &capturedSteps,
                  std::vector<BML_BehaviorHookFunction> &hooks) {
    capturedSteps.clear();
    hooks.clear();
    for (std::uint32_t index = 0; index < count; ++index) {
        const BML_BehaviorEditStep &step = steps[index];
        CapturedStep captured;
        captured.Kind = step.Kind;
        captured.Result = step.Result;
        captured.Flags = step.Flags;
        captured.Target = step.Target;
        captured.Node = step.Node;
        captured.SlotKind = step.SlotKind;
        captured.Delay = step.Delay;
        captured.Name = Copy(step.Name);
        captured.Prototype = step.Prototype;
        captured.Type = step.Type;
        captured.Source = step.Source;
        captured.Sink = step.Sink;
        captured.SourceSlot = Copy(step.Source.Slot.Name);
        captured.SinkSlot = Copy(step.Sink.Slot.Name);
        captured.Value = step.Value;
        captured.HasHook = step.Hook != nullptr;
        for (std::uint32_t entry = 0; entry < step.OrderCount; ++entry) {
            captured.Ordering.push_back(
                {step.Ordering[entry].Kind,
                 Copy(step.Ordering[entry].Owner) + "/" +
                     Copy(step.Ordering[entry].Name)});
        }
        if (step.Hook)
            hooks.push_back(*step.Hook);
        capturedSteps.push_back(std::move(captured));
    }
}

int BML_BEHAVIOR_CALL SubmitPlan(
    BML_BehaviorSession, const BML_BehaviorPlanSpec *spec,
    BML_BehaviorPlan *plan, BML_BehaviorPlanInfo *info,
    BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PlanSubmits;
    g_State.PlanName = Copy(spec->Name);
    g_State.PlanScript = Copy(spec->Script);
    g_State.PlanTargets = spec->Targets;
    CaptureSteps(spec->Steps, spec->StepCount, g_State.PlanSteps,
                 g_State.PlanHooks);
    if (g_State.PlanSubmitCode != BML_OK) {
        g_State.PlanHooks.clear();
        return g_State.PlanSubmitCode;
    }
    // The Loader takes its own reference to every callback it accepted.
    for (const BML_BehaviorHookFunction &hook : g_State.PlanHooks) {
        if (hook.Retain)
            hook.Retain(hook.State);
    }
    if (info) {
        Init(info);
        info->State = BML_BEHAVIOR_PLAN_RECONCILING;
    }
    *plan = reinterpret_cast<BML_BehaviorPlan>(9);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadPlan(BML_BehaviorSession, BML_BehaviorPlan plan,
                               BML_BehaviorPlanInfo *info,
                               BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PlanReads;
    if (!plan)
        return BML_ERROR_INVALID_HANDLE;
    Init(info);
    info->State = BML_BEHAVIOR_PLAN_ACTIVE;
    info->World = 12;
    info->Matches = 1;
    info->Installations = 1;
    Init(&info->Diagnostic);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ClosePlan(BML_BehaviorSession, BML_BehaviorPlan) {
    ++g_State.PlanCloses;
    for (const BML_BehaviorHookFunction &hook : g_State.PlanHooks) {
        if (hook.Release)
            hook.Release(hook.State);
    }
    g_State.PlanHooks.clear();
    return BML_OK;
}

int BML_BEHAVIOR_CALL ApplyPatch(
    BML_BehaviorSession, const BML_BehaviorPatchSpec *spec,
    BML_BehaviorPatch *patch, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PatchApplies;
    g_State.PatchName = Copy(spec->Name);
    g_State.PatchGraph = spec->Graph;
    CaptureSteps(spec->Steps, spec->StepCount, g_State.PatchSteps,
                 g_State.PatchHooks);
    if (g_State.PatchApplyCode != BML_OK) {
        g_State.PatchHooks.clear();
        return g_State.PatchApplyCode;
    }
    for (const BML_BehaviorHookFunction &hook : g_State.PatchHooks) {
        if (hook.Retain)
            hook.Retain(hook.State);
    }
    if (info) {
        Init(info);
        info->State = BML_BEHAVIOR_PATCH_ACTIVE;
        Init(&info->Diagnostic);
    }
    *patch = reinterpret_cast<BML_BehaviorPatch>(10);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadPatch(
    BML_BehaviorSession, BML_BehaviorPatch patch,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PatchReads;
    if (!patch)
        return BML_ERROR_INVALID_HANDLE;
    Init(info);
    info->State = BML_BEHAVIOR_PATCH_ACTIVE;
    Init(&info->Diagnostic);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ClosePatch(BML_BehaviorSession, BML_BehaviorPatch) {
    ++g_State.PatchCloses;
    for (const BML_BehaviorHookFunction &hook : g_State.PatchHooks) {
        if (hook.Release)
            hook.Release(hook.State);
    }
    g_State.PatchHooks.clear();
    return BML_OK;
}

BML_BehaviorInterface g_Interface = {
    BML_IFACE_HEADER(BML_BehaviorInterface, BML_BEHAVIOR_INTERFACE_ID,
                     BML_BEHAVIOR_INTERFACE_MAJOR,
                     BML_BEHAVIOR_INTERFACE_MINOR),
    &OpenSession,
    &CloseSession,
    &CallRun,
    &StartRun,
    &SpawnRun,
    &ContinueRun,
    &PulseRun,
    &ReadRun,
    &TakeFrames,
    &CloseRun,
    &FindPrototypes,
    &ReadDeclaredLayout,
    &ReadLiveLayout,
    &InspectGraph,
    &ReadNodeLayout,
    &ReadGraphValue,
    &OpenWatch,
    &CloseWatch,
    &SubmitPlan,
    &ReadPlan,
    &ClosePlan,
    &InspectRun,
    &SetRun,
    &BindRun,
    &ConfigureRun,
    &ApplyPatch,
    &ReadPatch,
    &ClosePatch,
};

} // namespace

extern "C" int BML_GetInterface(const char *id, std::uint16_t major,
                                const void **out) {
    if (!id || !out)
        return BML_ERROR_INVALID_PARAMETER;
    if (std::string_view(id) != BML_BEHAVIOR_INTERFACE_ID)
        return BML_ERROR_NOT_FOUND;
    if (major != BML_BEHAVIOR_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &g_Interface;
    return BML_OK;
}

template <class T>
concept ConfigurableBlock = requires(T &value) {
    value.Setting("Value", std::int32_t{1});
    value.Frames(signals(1));
};

static_assert(ConfigurableBlock<Builder>);
static_assert(!ConfigurableBlock<Block>);
static_assert(std::same_as<
              decltype(std::declval<Builder &>().Frames(signals(1))),
              Builder &>);
static_assert(std::same_as<
              decltype(std::declval<Builder &&>().Frames(signals(1))),
              Builder &&>);

TEST(BehaviorAuthoring, OwnsBlockTextAndUsesDomainSelectors) {
    g_State = {};
    auto opened = Session::Open("test.mod");
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    std::string settingName = "Caption";
    std::string text = "owned text";
    auto compiled = session.Use(Prototype(CKGUID(11, 22), 37))
        .Settings(setting(settingName, text))
        .Frames(eachFrame(8))
        .Compile();
    ASSERT_TRUE(compiled) << compiled.Code();
    Block block = std::move(compiled).Value();
    settingName.assign("changed");
    text.assign("changed");

    auto called = block.Call();
    ASSERT_TRUE(called) << called.Code();
    EXPECT_EQ(g_State.Owner, "test.mod");
    EXPECT_EQ(g_State.SelectorKind, BML_BEHAVIOR_SELECTOR_ONLY);
    EXPECT_EQ(g_State.SettingName, "Caption");
    EXPECT_EQ(g_State.Text, "owned text");
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_EACH_FRAME);
    EXPECT_EQ(g_State.FrameLimit, 8u);
    EXPECT_EQ(g_State.Generation, 37u);
    EXPECT_EQ(g_State.LayoutCalls, 2);
}

TEST(BehaviorAuthoring, TakesOwnedFramesAndContinuesTheSameRun) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto called = session.Use(CKGUID(1, 2)).Call("Run");
    ASSERT_TRUE(called);
    Call call = std::move(called).Value();

    auto taken = call.Take();
    ASSERT_TRUE(taken);
    ASSERT_EQ(taken.Value().size(), 1u);
    const Frame &frame = taken.Value().front();
    EXPECT_EQ(frame.Sequence, 7u);
    EXPECT_EQ(frame.GameFrame, 91u);
    EXPECT_EQ(frame.Continuation, BML_BEHAVIOR_CONTINUATION_NONE);
    EXPECT_TRUE(frame.HasOut("Done"));
    const Pout *pout = frame.FindPout("Value");
    ASSERT_NE(pout, nullptr);
    ASSERT_NE(pout->Get<std::int32_t>(), nullptr);
    EXPECT_EQ(*pout->Get<std::int32_t>(), 42);

    auto continued = std::move(call).Continue();
    ASSERT_TRUE(continued);
    Task task = std::move(continued).Value();
    auto info = task.Read();
    ASSERT_TRUE(info);
    EXPECT_EQ(info.Value().Kind, RunKind::Task);
    EXPECT_EQ(info.Value().State, RunState::Pending);
}

TEST(BehaviorAuthoring, PulseAndRaiiCloseUseTheRunHandle) {
    g_State = {};
    {
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();
        auto spawned = session.Use(CKGUID(3, 4)).Spawn();
        ASSERT_TRUE(spawned);
        Instance instance = std::move(spawned).Value();
        auto admission = instance.Pulse("Create");
        ASSERT_TRUE(admission);
        EXPECT_EQ(admission.Value(), Admission::Executed);
        EXPECT_EQ(g_State.Input, "Create");
    }
    EXPECT_EQ(g_State.RunCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, ReadsLayoutAndGraphFromTheOwnedBehavior) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto spawned = session.Use(CKGUID(1, 2)).Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();

    auto layout = instance.Layout();
    ASSERT_TRUE(layout) << layout.Detail().Message;
    EXPECT_EQ(layout->Origin, LayoutOrigin::Live);
    EXPECT_EQ(layout->Kind, BehaviorKind::Function);
    EXPECT_EQ(layout->Generation, 9u);
    EXPECT_TRUE(layout->MaterializedNow);
    const Slot *input = layout->Find(SlotKind::In, "Run");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->Index, 0);

    auto inspected = instance.Inspect();
    ASSERT_TRUE(inspected) << inspected.Detail().Message;
    EXPECT_EQ(inspected->Root().Domain, 71u);
    EXPECT_EQ(inspected->Mode(), View::Logical);
    EXPECT_NE(inspected->Find("Root"), nullptr);
}

TEST(BehaviorAuthoring, EditsTheLiveRunThroughLayoutSlots) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto spawned = session.Use(CKGUID(1, 2)).Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();

    auto layout = instance.Layout();
    ASSERT_TRUE(layout);
    const Slot *pinSlot = layout->Find(SlotKind::Pin, "Value");
    ASSERT_NE(pinSlot, nullptr);
    EXPECT_EQ(pinSlot->Generation, 9u);

    auto set = instance.Set(*pinSlot, std::int32_t{713});
    ASSERT_TRUE(set);
    EXPECT_EQ(set.Value(), 9u);
    EXPECT_EQ(g_State.SetKind, BML_BEHAVIOR_SLOT_PIN);
    EXPECT_EQ(g_State.SetGeneration, 9u);
    EXPECT_EQ(g_State.SetValue, 713);

    auto inspected = instance.Inspect();
    ASSERT_TRUE(inspected);
    auto bound = instance.Bind(
        *pinSlot, pout(inspected->Root(), "Value"), Relation::Direct);
    ASSERT_TRUE(bound);
    EXPECT_EQ(g_State.BindRelation, BML_BEHAVIOR_VALUE_DIRECT);
    EXPECT_EQ(g_State.BindNode.Domain, inspected->Root().Domain);
    EXPECT_EQ(g_State.BindSource, "Value");

    auto configured = instance.Configure({setting("Retry", true)});
    ASSERT_TRUE(configured);
    EXPECT_EQ(configured.Value(), 10u);
    ASSERT_EQ(g_State.ConfiguredSettings.size(), 1u);
    EXPECT_EQ(g_State.ConfiguredSettings[0], "Retry");

    auto stale = instance.Set(*pinSlot, std::int32_t{1});
    EXPECT_FALSE(stale);
    EXPECT_EQ(stale.Detail().Error,
              BML_BEHAVIOR_ERROR_LAYOUT_CHANGED);

    auto current = instance.Layout();
    ASSERT_TRUE(current);
    const Slot *currentPin = current->Find(SlotKind::Pin, "Value");
    ASSERT_NE(currentPin, nullptr);
    EXPECT_EQ(currentPin->Generation, 10u);
}

TEST(BehaviorAuthoring, ClosingSessionInvalidatesExistingBlocks) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto compiled = session.Use(CKGUID(5, 6)).Compile();
    ASSERT_TRUE(compiled);
    Block block = std::move(compiled).Value();
    session.Close();

    auto called = block.Call();
    EXPECT_FALSE(called);
    EXPECT_EQ(called.Code(), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, PinsProviderAndReusesOneBlockAcrossOwners) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto compiled = session.Use(CKGUID(7, 8)).Compile();
    ASSERT_TRUE(compiled) << compiled.Detail().Message;
    Block block = std::move(compiled).Value();

    const BML_ObjectRef firstOwner{1, 2, 3};
    const BML_ObjectRef secondOwner{4, 5, 6};
    auto called = block.Call(firstOwner, unique("Run"));
    auto started = block.Start(secondOwner, unique("Run"));
    auto spawned = block.Spawn(firstOwner);
    ASSERT_TRUE(called);
    ASSERT_TRUE(started);
    ASSERT_TRUE(spawned);
    ASSERT_EQ(g_State.Blocks.size(), 3u);
    EXPECT_EQ(g_State.Blocks[0], g_State.Blocks[1]);
    EXPECT_EQ(g_State.Blocks[1], g_State.Blocks[2]);
    ASSERT_EQ(g_State.RunOwners.size(), 3u);
    EXPECT_EQ(g_State.RunOwners[0].Domain, firstOwner.Domain);
    EXPECT_EQ(g_State.RunOwners[1].Domain, secondOwner.Domain);
    EXPECT_EQ(g_State.Generation, g_State.ProviderGeneration);
    EXPECT_EQ(g_State.LayoutCalls, 2);
}

TEST(BehaviorAuthoring, CompileChecksTheDeclaredLayout) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto missing = session.Use(CKGUID(9, 10))
        .Setting("Missing", std::int32_t{1})
        .Compile();
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.Detail().Error, BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND);

    auto ambiguous = session.Use(CKGUID(9, 10))
        .Setting("Duplicate", std::int32_t{1})
        .Compile();
    EXPECT_FALSE(ambiguous);
    EXPECT_EQ(ambiguous.Detail().Error, BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS);

    auto wrongKind = session.Use(CKGUID(9, 10))
        .Setting("Caption", std::int32_t{1})
        .Compile();
    EXPECT_FALSE(wrongKind);
    EXPECT_EQ(wrongKind.Detail().Error, BML_BEHAVIOR_ERROR_TYPE_MISMATCH);

    auto unsupported = session.Use(CKGUID(9, 10))
        .Setting("Opaque", std::int32_t{1})
        .Compile();
    EXPECT_FALSE(unsupported);
    EXPECT_EQ(unsupported.Detail().Error,
              BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED);

    auto missingCall = session.Use(CKGUID(9, 10))
        .Setting("Missing", std::int32_t{1})
        .Call("Run");
    EXPECT_FALSE(missingCall);
    EXPECT_EQ(missingCall.Detail().Error,
              BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND);

    auto selected = session.Use(CKGUID(9, 10))
        .Setting(named("Duplicate", 1), true)
        .Compile();
    EXPECT_TRUE(selected) << selected.Detail().Message;
    EXPECT_EQ(g_State.OpenRuns, 0);
}

TEST(BehaviorAuthoring, CompileLeavesDynamicLayoutToTheNativeLifecycle) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto compiled = session.Use(CKGUID(13, 14))
        .Setting("Extended Layout", true)
        .NextStage()
        .Setting("Created Later", std::int32_t{4})
        .Pin("Dynamic Value", std::int32_t{713})
        .Compile();
    EXPECT_TRUE(compiled) << compiled.Detail().Message;

    auto dynamicPin = session.Use(CKGUID(13, 14))
        .Pin("Dynamic Value", std::int32_t{713})
        .Compile();
    EXPECT_TRUE(dynamicPin) << dynamicPin.Detail().Message;
}

TEST(BehaviorAuthoring, FallsBackWhenPrototypeDiscoveryIsUnavailable) {
    g_State = {};
    g_State.LayoutUnavailable = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto called = session.Use(CKGUID(15, 16))
        .Setting("Runtime Setting", std::int32_t{9})
        .Call("Run");
    EXPECT_TRUE(called) << called.Detail().Message;
    EXPECT_EQ(g_State.Generation, 0u);
    EXPECT_EQ(g_State.LayoutCalls, 1);
    EXPECT_EQ(g_State.OpenRuns, 1);
}

TEST(BehaviorAuthoring, RejectsMalformedDeclaredLayoutBeforeAllocation) {
    g_State = {};
    g_State.MalformedLayout = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto compiled = session.Use(CKGUID(17, 18)).Compile();
    EXPECT_FALSE(compiled);
    EXPECT_EQ(compiled.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(compiled.Detail().Error,
              BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE);
}

TEST(BehaviorAuthoring, ContainsMalformedRunResults) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto compiled = session.Use(CKGUID(19, 20)).Compile();
    ASSERT_TRUE(compiled);
    Block block = std::move(compiled).Value();

    g_State.RunCode = BML_ERROR_FAIL;
    auto failed = block.Call("Run");
    EXPECT_FALSE(failed);
    EXPECT_EQ(failed.Code(), BML_ERROR_FAIL);
    EXPECT_EQ(g_State.RunCloses, 1);

    g_State.RunCode = BML_OK;
    g_State.NullRun = true;
    auto malformed = block.Spawn();
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(g_State.RunCloses, 1);
}

TEST(BehaviorAuthoring, ReadsLogicalAndLiveGraphsWithoutNativePointers) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    const BML_ObjectRef root{41, 42, 43};

    auto inspected = session.Inspect(root);
    ASSERT_TRUE(inspected) << inspected.Detail().Message;
    Graph graph = std::move(inspected).Value();
    EXPECT_EQ(graph.Mode(), View::Logical);
    EXPECT_EQ(graph.Root().Domain, root.Domain);
    EXPECT_EQ(graph.Generation(), 5u);
    EXPECT_EQ(graph.Fingerprint(), 0x713u);
    ASSERT_EQ(graph.Nodes().size(), 1u);
    const Node *node = graph.Find("Root");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->Prototype, Guid(CKGUID(21, 22)));
    EXPECT_TRUE(node->Active);
    ASSERT_EQ(node->Ports.size(), 1u);
    EXPECT_EQ(node->Ports[0].Name, "Run");
    EXPECT_TRUE(node->Ports[0].Active);
    ASSERT_EQ(graph.Links().size(), 1u);
    EXPECT_EQ(graph.Links()[0].Source.Node, 101u);
    EXPECT_EQ(graph.Links()[0].Source.Kind, BML_BEHAVIOR_SLOT_OUT);
    EXPECT_EQ(graph.Links()[0].Target.Kind, BML_BEHAVIOR_SLOT_IN);
    EXPECT_EQ(graph.Links()[0].InitialDelay, 2);
    EXPECT_EQ(graph.Links()[0].RemainingDelay, 1);
    EXPECT_EQ(graph.Links()[0].Pending, TruthValue::Unknown);

    auto live = graph.Live();
    ASSERT_TRUE(live);
    EXPECT_EQ(live.Value().Mode(), View::Live);
    EXPECT_EQ(g_State.GraphView, BML_BEHAVIOR_GRAPH_LIVE);
}

TEST(BehaviorAuthoring, ReadsStoredAndOperationBackedValuesNonForcing) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();
    const BML_ObjectRef node = graph.Nodes().front().Object;

    auto stored = graph.Read(pin(node, "Value"));
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored.Value().State, ObservationState::Available);
    EXPECT_EQ(stored.Value().Source, Relation::Stored);
    ASSERT_TRUE(stored.Value().Kind.has_value());
    EXPECT_EQ(*stored.Value().Kind, ValueKind::Int32);
    ASSERT_NE(std::get_if<std::int32_t>(&stored.Value().Data), nullptr);
    EXPECT_EQ(*std::get_if<std::int32_t>(&stored.Value().Data), 42);

    auto computed = graph.Read(pin(node, "Computed"));
    ASSERT_TRUE(computed);
    EXPECT_EQ(computed.Value().State, ObservationState::Indeterminate);
    EXPECT_EQ(computed.Value().Source, Relation::Operation);
    EXPECT_FALSE(computed.Value().Kind.has_value());
}

TEST(BehaviorAuthoring, OwnsWatchCallbackAndReportsDomainChanges) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();
    std::vector<Change> changes;

    {
        auto watched = graph.Watch(
            graphChanged, [&](const Change &change) {
                changes.push_back(change);
            });
        ASSERT_TRUE(watched) << watched.Detail().Message;
        EXPECT_EQ(g_State.WatchSpec.Kind, BML_BEHAVIOR_WATCH_GRAPH);
        EXPECT_EQ(g_State.WatchSpec.View, BML_BEHAVIOR_GRAPH_LOGICAL);

        BML_BehaviorWatchEvent event{};
        Init(&event);
        event.Kind = BML_BEHAVIOR_WATCH_GRAPH;
        event.Sequence = 3;
        event.Frame = 17;
        event.Before = 0x11;
        event.After = 0x22;
        Init(&event.PreviousValue);
        Init(&event.PreviousValue.Value);
        event.PreviousValue.State = BML_BEHAVIOR_VALUE_UNSUPPORTED;
        event.PreviousValue.Relation = BML_BEHAVIOR_VALUE_STORED;
        Init(&event.CurrentValue);
        Init(&event.CurrentValue.Value);
        event.CurrentValue.State = BML_BEHAVIOR_VALUE_UNSUPPORTED;
        event.CurrentValue.Relation = BML_BEHAVIOR_VALUE_STORED;
        ASSERT_NE(g_State.WatchFunction.Invoke, nullptr);
        g_State.WatchFunction.Invoke(g_State.WatchFunction.State, &event);
        ASSERT_EQ(changes.size(), 1u);
        EXPECT_EQ(changes[0].Kind, ChangeKind::Graph);
        EXPECT_EQ(changes[0].Sequence, 3u);
        EXPECT_EQ(changes[0].GameFrame, 17u);
        EXPECT_EQ(changes[0].Before, 0x11u);
        EXPECT_EQ(changes[0].After, 0x22u);
    }
    EXPECT_EQ(g_State.WatchCloses, 1);
    EXPECT_EQ(g_State.WatchFunction.Invoke, nullptr);
}

TEST(BehaviorAuthoring, RejectsExactWatchWhenTheProviderCannotObserveIt) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();

    auto watched = graph.Watch(
        exact(pin(graph.Nodes().front().Object, "Value")),
        [](const Change &) {});
    EXPECT_FALSE(watched);
    EXPECT_EQ(watched.Code(), BML_ERROR_UNAVAILABLE);
    EXPECT_EQ(g_State.WatchCloses, 0);
    EXPECT_EQ(g_State.WatchFunction.Invoke, nullptr);
}

TEST(BehaviorAuthoring, SubmitsAPatchProgramAsADurablePlan) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto alive = std::make_shared<int>(0);
    std::vector<float> deltas;
    {
        PatchBuilder draft = session.Plan("extra-life");
        draft.OnSingle("Gameplay_Events");
        const auto counter = draft.Require("Counter_Active", CKGUID(1, 2));
        const auto added = draft.Add(CKGUID(3, 4));
        const auto amount = draft.AppendPin(added, "Amount", CKPGUID_FLOAT);
        const auto link =
            draft.Between(counter.Out(0), draft.Graph().In("Reset"), 2);
        draft.Splice(link, added,
                     {before("Other", "hud"), after("Third", "sound")})
            .Bind(amount, 3.5f)
            .Bind(added.Pin("Other"), counter.Pout("Value"))
            .Share(added.Pin(1), counter.Pout(0))
            .Push(counter.Pout(2), added.Pin(2))
            .Flow(added.Out(0), counter.In(0), 1)
            .FlowCycle(added.Out(1), counter.In(0))
            .After(counter.Out(1), [alive, &deltas](const HookEvent &event) {
                deltas.push_back(event.DeltaTime);
                return HookResult::AgainNextFrame;
            });
        EXPECT_EQ(alive.use_count(), 2);

        auto submitted = draft.Submit();
        ASSERT_TRUE(submitted) << submitted.Detail().Message;
        Plan plan = std::move(submitted).Value();
        EXPECT_EQ(g_State.PlanSubmits, 1);
        EXPECT_EQ(g_State.PlanName, "extra-life");
        EXPECT_EQ(g_State.PlanScript, "Gameplay_Events");
        EXPECT_EQ(g_State.PlanTargets,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_TARGETS_ONE));
        // The Loader took a reference of its own to the one record that owns
        // the callback, so the callback itself was not copied.
        EXPECT_EQ(alive.use_count(), 2);

        ASSERT_EQ(g_State.PlanSteps.size(), 13u);
        const CapturedStep &require = g_State.PlanSteps[0];
        EXPECT_EQ(require.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_REQUIRE_NODE));
        EXPECT_EQ(require.Name, "Counter_Active");
        EXPECT_EQ(require.Prototype.Data1, 1u);
        EXPECT_EQ(require.Result, BML_BEHAVIOR_EDIT_GRAPH + 1u);

        const CapturedStep &append = g_State.PlanSteps[2];
        EXPECT_EQ(append.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_APPEND_SLOT));
        EXPECT_EQ(append.Target, g_State.PlanSteps[1].Result);
        EXPECT_EQ(append.SlotKind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_PIN));
        EXPECT_EQ(append.Name, "Amount");
        EXPECT_EQ(append.Type.Data1, CKPGUID_FLOAT.d1);

        const CapturedStep &between = g_State.PlanSteps[3];
        EXPECT_EQ(between.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_REQUIRE_LINK));
        EXPECT_EQ(between.Flags & BML_BEHAVIOR_EDIT_HAS_DELAY,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_HAS_DELAY));
        EXPECT_EQ(between.Delay, 2);
        EXPECT_EQ(between.Source.Handle, require.Result);
        EXPECT_EQ(between.Source.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_OUT));
        EXPECT_EQ(between.Source.Slot.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_SELECTOR_INDEX));
        EXPECT_EQ(between.Sink.Handle, BML_BEHAVIOR_EDIT_GRAPH);
        EXPECT_EQ(between.SinkSlot, "Reset");

        const CapturedStep &splice = g_State.PlanSteps[4];
        EXPECT_EQ(splice.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SPLICE));
        EXPECT_EQ(splice.Target, between.Result);
        EXPECT_EQ(splice.Node, g_State.PlanSteps[1].Result);
        ASSERT_EQ(splice.Ordering.size(), 2u);
        EXPECT_EQ(splice.Ordering[0].first,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_ORDER_BEFORE));
        EXPECT_EQ(splice.Ordering[0].second, "Other/hud");
        EXPECT_EQ(splice.Ordering[1].first,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_ORDER_AFTER));
        EXPECT_EQ(splice.Ordering[1].second, "Third/sound");

        // An appended slot is addressed by handle, because it has no
        // author-visible index until the edit compiles.
        const CapturedStep &bound = g_State.PlanSteps[5];
        EXPECT_EQ(bound.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BIND_VALUE));
        EXPECT_EQ(bound.Sink.Handle, append.Result);
        EXPECT_EQ(bound.Sink.Kind, 0u);
        EXPECT_EQ(bound.Value.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_VALUE_FLOAT32));
        EXPECT_FLOAT_EQ(bound.Value.Data.Float32, 3.5f);
        EXPECT_EQ(bound.Result, 0u);

        EXPECT_EQ(g_State.PlanSteps[6].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BIND_PORT));
        EXPECT_EQ(g_State.PlanSteps[7].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SHARE));
        EXPECT_EQ(g_State.PlanSteps[8].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_PUSH));
        EXPECT_EQ(g_State.PlanSteps[9].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_FLOW));
        EXPECT_EQ(g_State.PlanSteps[9].Delay, 1);
        EXPECT_EQ(g_State.PlanSteps[10].Flags & BML_BEHAVIOR_EDIT_CONFIRM_CYCLE,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_CONFIRM_CYCLE));

        // Hooking a port follows the chain leaving it, then hooks the end of
        // that chain.
        const CapturedStep &follow = g_State.PlanSteps[11];
        EXPECT_EQ(follow.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_FOLLOW));
        EXPECT_EQ(follow.Source.Handle, require.Result);
        EXPECT_EQ(follow.Source.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_OUT));
        const CapturedStep &hooked = g_State.PlanSteps[12];
        EXPECT_EQ(hooked.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_AFTER));
        EXPECT_EQ(hooked.Target, follow.Result);
        EXPECT_TRUE(hooked.HasHook);

        auto read = plan.Read();
        ASSERT_TRUE(read);
        EXPECT_EQ(read.Value().State, PlanState::Active);
        EXPECT_EQ(read.Value().World, 12u);
        EXPECT_TRUE(read.Value().Installed());

        ASSERT_EQ(g_State.PlanHooks.size(), 1u);
        BML_BehaviorHookContext context{};
        Init(&context);
        context.DeltaTime = 16.5f;
        context.Block = {1, 2, 3};
        ASSERT_NE(g_State.PlanHooks[0].Invoke, nullptr);
        EXPECT_EQ(g_State.PlanHooks[0].Invoke(g_State.PlanHooks[0].State,
                                              &context),
                  BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME);
        ASSERT_EQ(deltas.size(), 1u);
        EXPECT_FLOAT_EQ(deltas[0], 16.5f);

        plan.Close();
        EXPECT_EQ(g_State.PlanCloses, 1);
        plan.Close();
        EXPECT_EQ(g_State.PlanCloses, 1);
        // The builder still owns the reference the Loader dropped.
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, AppliesTheSameEditLanguageToOneLiveGraph) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();

    auto alive = std::make_shared<int>(0);
    {
        auto edit = graph.Patch("one-graph");
        const auto existing = edit.Require("Counter_Active", CKGUID(1, 2));
        const auto added = edit.Add(CKGUID(3, 4));
        const auto amount = edit.AppendPin(added, "Amount", CKPGUID_FLOAT);
        const auto link = edit.Between(existing.Out(), edit.Graph().In());
        edit.Bind(amount, 2.5f)
            .Splice(link, added)
            .Tap(added.Out(), [alive] {});

        auto applied = edit.Apply();
        ASSERT_TRUE(applied) << applied.Detail().Message;
        GraphPatch patch = std::move(applied).Value();
        EXPECT_EQ(g_State.PatchApplies, 1);
        EXPECT_EQ(g_State.PatchName, "one-graph");
        EXPECT_EQ(g_State.PatchGraph.Domain, 41u);
        EXPECT_EQ(g_State.PatchGraph.Slot, 42u);
        EXPECT_EQ(g_State.PatchGraph.Generation, 43u);
        ASSERT_EQ(g_State.PatchSteps.size(), 7u);
        EXPECT_EQ(g_State.PatchSteps[0].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_REQUIRE_NODE));
        EXPECT_EQ(g_State.PatchSteps[1].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_ADD_BLOCK));
        EXPECT_EQ(g_State.PatchSteps[2].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_APPEND_SLOT));
        EXPECT_EQ(g_State.PatchSteps[4].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BIND_VALUE));
        EXPECT_EQ(g_State.PatchSteps[5].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SPLICE));
        EXPECT_EQ(g_State.PatchSteps[6].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_TAP));

        auto read = patch.Read();
        ASSERT_TRUE(read);
        EXPECT_EQ(read.Value().State, GraphPatchState::Active);
        EXPECT_TRUE(read.Value().Installed());
        EXPECT_EQ(read.Value().Conflicts, 0u);

        patch.Close();
        EXPECT_EQ(g_State.PatchCloses, 1);
        patch.Close();
        EXPECT_EQ(g_State.PatchCloses, 1);
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, RejectsPlanWithoutAScriptBeforeCallingTheLoader) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    PatchBuilder draft = session.Plan("nameless");
    auto submitted = draft.Submit();
    EXPECT_FALSE(submitted);
    EXPECT_EQ(submitted.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.PlanSubmits, 0);
    EXPECT_EQ(g_State.PlanCloses, 0);
}

TEST(BehaviorAuthoring, KeepsHookStateWhenTheLoaderRejectsThePlan) {
    g_State = {};
    g_State.PlanSubmitCode = BML_ERROR_INVALID_PARAMETER;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto alive = std::make_shared<int>(0);
    {
        PatchBuilder draft = session.Plan("rejected");
        draft.On("Gameplay_Events")
            .Tap(draft.Graph().Out(0), [alive] {});
        auto submitted = draft.Submit();
        EXPECT_FALSE(submitted);
        EXPECT_EQ(submitted.Code(), BML_ERROR_INVALID_PARAMETER);
        EXPECT_EQ(g_State.PlanSubmits, 1);
        EXPECT_EQ(g_State.PlanCloses, 0);
        // A rejected Plan retained nothing, so only the builder still owns it.
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, ClosingSessionLeavesPlanHandlesInert) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto submitted = session.Plan("inert").On("Gameplay_Events").Submit();
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();
    ASSERT_TRUE(plan);

    session.Close();
    EXPECT_FALSE(plan);
    EXPECT_FALSE(plan.Read());
    plan.Close();
    EXPECT_EQ(g_State.PlanCloses, 0);
}
