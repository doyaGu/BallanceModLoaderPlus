#include "BML/Behavior.hpp"
#include "BML/Behavior/Blocks/Text2D.hpp"

#include "BML/Guids/Interface.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
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
    BML_BehaviorPrototypeRef Prototype{};
    BML_BehaviorGuid Type{};
    BML_BehaviorPortRef Source{};
    BML_BehaviorPortRef Sink{};
    std::string SourceSlot;
    std::string SinkSlot;
    BML_BehaviorValue Value{};
    BML_ObjectRef Object{};
    bool HasHook = false;
    std::vector<std::pair<std::uint32_t, std::string>> Ordering;
};

struct FakeState {
    std::string Owner;
    std::string Input;
    std::string SettingName;
    std::uint32_t SettingSelectorKind = 0;
    std::int32_t SettingIndex = 0;
    std::string Text;
    std::uint32_t SelectorKind = 0;
    std::uint32_t FrameKind = 0;
    std::uint32_t FrameLimit = 0;
    std::uint32_t FrameFlags = 0;
    std::uint64_t Generation = 0;
    std::uint64_t ProviderGeneration = 73;
    std::uint64_t LiveGeneration = 9;
    int LayoutCalls = 0;
    int OpenRuns = 0;
    int SessionOpenCode = BML_OK;
    int SessionCloses = 0;
    int RunCloses = 0;
    int RunCloseCode = BML_OK;
    int RunCode = BML_OK;
    int FrameTakes = 0;
    bool FramesAvailable = true;
    bool MalformedFrames = false;
    bool DuplicateFrameNames = false;
    bool RejectStaleGeneration = false;
    bool LayoutUnavailable = false;
    bool MalformedLayout = false;
    bool WrongNodeLayoutPrototype = false;
    bool DeclaredTargetable = true;
    bool NullRun = false;
    bool NullSession = false;
    bool WrongRunKind = false;
    bool WrongRunLayoutPrototype = false;
    bool WrongContinuationKind = false;
    bool WrongPulseKind = false;
    bool WrongFollowupPrototype = false;
    bool InvalidAdmission = false;
    std::uint32_t LastRunKind = BML_BEHAVIOR_RUN_INSTANCE;
    BML_BehaviorPrototypeRef LastRunPrototype{};
    bool DuplicateGraphNames = false;
    bool WrongGraphRoot = false;
    bool ReturnsAnotherGraph = false;
    bool MissingGraphEndpoint = false;
    bool DuplicateGraphNode = false;
    bool DuplicateGraphPort = false;
    bool DuplicateGraphPortNames = false;
    bool DuplicateGraphLink = false;
    bool DuplicateGraphLinkObject = false;
    bool GraphLinkUsesNodeId = false;
    bool GraphLinkUsesNodeObject = false;
    bool InvalidGraphObject = false;
    bool InvalidGraphLinkKind = false;
    bool InvalidGraphPortFlag = false;
    bool InvalidGraphOccurrence = false;
    bool NullInterface = false;
    std::uint32_t PrototypeMatch = 0;
    std::string PrototypeName;
    std::vector<BML_BehaviorGuid> RequiredManagers;
    std::uint32_t GraphView = 0;
    std::uint64_t GraphFingerprint = 0x713;
    int WatchCloses = 0;
    int WatchOpenCode = BML_OK;
    int WatchCloseCode = BML_OK;
    int WatchReads = 0;
    std::uint32_t WatchState = BML_BEHAVIOR_WATCH_ACTIVE;
    BML_BehaviorWatchSpec WatchSpec{};
    BML_BehaviorWatchFunction WatchFunction{};
    std::vector<BML_ObjectRef> RunOwners;
    std::vector<BML_BehaviorBlock> Blocks;
    int PlanSubmits = 0;
    int PlanReads = 0;
    int PlanCloses = 0;
    int PlanSubmitCode = BML_OK;
    bool ReturnPlanHandleOnError = false;
    int PlanCloseCode = BML_OK;
    std::uint32_t PlanState = BML_BEHAVIOR_PLAN_ACTIVE;
    std::uint32_t PlanTargets = 0;
    std::string PlanName;
    std::string PlanScript;
    std::vector<CapturedStep> PlanSteps;
    std::vector<BML_BehaviorHookFunction> PlanHooks;
    int PatchApplies = 0;
    int PatchReads = 0;
    int PatchCloses = 0;
    int PatchApplyCode = BML_OK;
    bool ReturnPatchHandleOnError = false;
    int PatchCloseCode = BML_OK;
    std::uint32_t PatchState = BML_BEHAVIOR_PATCH_ACTIVE;
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
    int References = 0;
    std::uint32_t ReferencedObject = 0;
    int ReferenceCode = BML_OK;
    bool MalformedReference = false;
    int NodeResolves = 0;
    std::uint32_t ResolvedHandle = 0;
    int ResolveNodeCode = BML_OK;
    bool MalformedResolvedNode = false;
    bool MalformedObservedObject = false;
    bool MalformedInfoDiagnostic = false;
    bool MalformedPatchReserved = false;
    bool MalformedCallStatus = false;
    int Attaches = 0;
    BML_ObjectRef AttachGraph{};
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
    if (status && g_State.MalformedCallStatus)
        status->StructSize = 0;
}

void DescribeLastRun(BML_BehaviorRunInfo *info, std::uint32_t kind,
                     std::uint32_t state) {
    Init(info);
    info->Kind = kind;
    info->State = state;
    Init(&info->Prototype);
    info->Prototype = g_State.LastRunPrototype;
    if (g_State.WrongFollowupPrototype)
        ++info->Prototype.Prototype.Data1;
    Init(&info->Status);
}

int BML_BEHAVIOR_CALL OpenSession(BML_BehaviorString owner,
                                  BML_BehaviorSession *session,
                                  BML_BehaviorStatus *status) {
    g_State.Owner.assign(owner.Data, owner.Length);
    *session = g_State.NullSession
        ? nullptr : reinterpret_cast<BML_BehaviorSession>(1);
    Success(status);
    return g_State.SessionOpenCode;
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
    if (block)
        g_State.Blocks.push_back(*block);
    g_State.Generation = block->PrototypeGeneration;
    g_State.FrameKind = block->Frames.Kind;
    g_State.FrameLimit = block->Frames.Limit;
    g_State.FrameFlags = block->Frames.Flags;
    if (input) {
        g_State.SelectorKind = input->Kind;
        g_State.Input.assign(input->Name.Data, input->Name.Length);
    }
    if (block->SettingStageCount && block->SettingStages[0].SettingCount) {
        const BML_BehaviorBinding &binding =
            block->SettingStages[0].Settings[0];
        g_State.SettingSelectorKind = binding.Slot.Kind;
        g_State.SettingIndex = binding.Slot.Index;
        g_State.SettingName.assign(binding.Slot.Name.Data,
                                   binding.Slot.Name.Length);
        g_State.Text.assign(binding.Value.Data.Utf8.Data,
                            binding.Value.Data.Utf8.Length);
    }
    if (g_State.RejectStaleGeneration &&
        block->PrototypeGeneration != g_State.ProviderGeneration) {
        Init(status);
        status->Error = BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED;
        status->Phase = BML_BEHAVIOR_PHASE_PROTOTYPE;
        *run = nullptr;
        return BML_ERROR_INVALID_HANDLE;
    }
    *run = g_State.NullRun ? nullptr : reinterpret_cast<BML_BehaviorRun>(2);
    Init(info);
    info->Kind = g_State.WrongRunKind
        ? static_cast<std::uint32_t>(BML_BEHAVIOR_RUN_TASK) : kind;
    info->State = kind == BML_BEHAVIOR_RUN_CALL
        ? BML_BEHAVIOR_RUN_PENDING
        : BML_BEHAVIOR_RUN_READY;
    Init(&info->Prototype);
    info->Prototype.Prototype = block->Prototype;
    info->Prototype.Generation = block->PrototypeGeneration
        ? block->PrototypeGeneration : g_State.ProviderGeneration;
    g_State.LastRunKind = info->Kind;
    g_State.LastRunPrototype = info->Prototype;
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
    const std::uint32_t kind = g_State.WrongContinuationKind
        ? BML_BEHAVIOR_RUN_INSTANCE : BML_BEHAVIOR_RUN_TASK;
    DescribeLastRun(info, kind, BML_BEHAVIOR_RUN_PENDING);
    if (!g_State.WrongContinuationKind)
        g_State.LastRunKind = BML_BEHAVIOR_RUN_TASK;
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL PulseRun(BML_BehaviorRun,
                               const BML_BehaviorSelector *input,
                               std::uint32_t *admission,
                               BML_BehaviorRunInfo *info,
                               BML_BehaviorStatus *status) {
    g_State.Input.assign(input->Name.Data, input->Name.Length);
    *admission = g_State.InvalidAdmission
        ? 0xffffffffu : BML_BEHAVIOR_ADMISSION_EXECUTED;
    const std::uint32_t kind = g_State.WrongPulseKind
        ? BML_BEHAVIOR_RUN_CALL : g_State.LastRunKind;
    DescribeLastRun(info, kind, BML_BEHAVIOR_RUN_READY);
    Success(status);
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadRun(BML_BehaviorRun,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    DescribeLastRun(info, g_State.LastRunKind,
                    BML_BEHAVIOR_RUN_PENDING);
    Success(status);
    return BML_OK;
}

std::vector<std::uint8_t> FramePayload() {
    if (g_State.DuplicateFrameNames) {
        std::vector<std::uint8_t> payload(148, 0);
        for (std::uint32_t index = 0; index < 2; ++index) {
            BML_BehaviorOutRecord out{};
            out.StructSize = sizeof(out);
            out.Index = static_cast<std::int32_t>(index);
            out.Occurrence = static_cast<std::int32_t>(index);
            out.NameOffset = 40 + index * 4;
            out.NameLength = 4;
            std::memcpy(payload.data() + index * sizeof(out), &out,
                        sizeof(out));
            std::memcpy(payload.data() + out.NameOffset, "Done", 4);

            BML_BehaviorPoutRecord pout{};
            pout.StructSize = sizeof(pout);
            pout.Index = static_cast<std::int32_t>(index);
            pout.Occurrence = static_cast<std::int32_t>(index);
            pout.Type = {CKPGUID_INT.d1, CKPGUID_INT.d2};
            pout.Kind = BML_BEHAVIOR_VALUE_INT32;
            pout.NameOffset = 128 + index * 5;
            pout.NameLength = 5;
            pout.ValueOffset = 140 + index * 4;
            pout.ValueSize = 4;
            std::memcpy(payload.data() + 48 + index * sizeof(pout), &pout,
                        sizeof(pout));
            std::memcpy(payload.data() + pout.NameOffset, "Value", 5);
            payload[pout.ValueOffset] = static_cast<std::uint8_t>(42 + 42 * index);
        }
        return payload;
    }
    std::vector<std::uint8_t> payload(76, 0);
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
    pout.ValueOffset = 72;
    pout.ValueSize = 4;
    std::memcpy(payload.data() + 24, &pout, sizeof(pout));
    std::memcpy(payload.data() + 64, "Value", 5);
    payload[72] = 42;
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
    ++g_State.FrameTakes;
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
    frames->OutCount = g_State.DuplicateFrameNames ? 2 : 1;
    frames->PoutOffset = g_State.DuplicateFrameNames ? 48 : 24;
    frames->PoutCount = g_State.DuplicateFrameNames ? 2 : 1;
    if (g_State.MalformedFrames)
        frames->PoutOffset = std::numeric_limits<std::uint32_t>::max();
    std::memcpy(payload, bytes.data(), bytes.size());
    g_State.FramesAvailable = false;
    return BML_OK;
}

int BML_BEHAVIOR_CALL CloseRun(BML_BehaviorRun) {
    ++g_State.RunCloses;
    return g_State.RunCloseCode;
}

int BML_BEHAVIOR_CALL FindPrototypes(
    BML_BehaviorSession, const BML_BehaviorPrototypeQuery *query,
    BML_BehaviorPrototypeInfo *prototypes, std::uint32_t prototypeCapacity,
    std::uint32_t prototypeStride, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *prototypeCount,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    Success(status);
    g_State.PrototypeMatch = query->Match;
    g_State.PrototypeName.assign(query->Name.Data ? query->Name.Data : "",
                                 query->Name.Length);
    g_State.RequiredManagers.clear();
    if (query->RequiredManagerCount) {
        g_State.RequiredManagers.assign(
            query->RequiredManagers,
            query->RequiredManagers + query->RequiredManagerCount);
    }

    BML_BehaviorManagerInfo manager{};
    manager.StructSize = sizeof(manager);
    manager.Guid = {91, 92};
    manager.Available = 1;
    std::vector<std::uint8_t> bytes(sizeof(manager));
    std::memcpy(bytes.data(), &manager, sizeof(manager));
    auto text = [&](std::string_view value) {
        BML_BehaviorText result{static_cast<std::uint32_t>(bytes.size()),
                                static_cast<std::uint32_t>(value.size())};
        bytes.insert(bytes.end(), value.begin(), value.end());
        return result;
    };

    BML_BehaviorPrototypeInfo info{};
    info.StructSize = sizeof(info);
    info.Ref.StructSize = sizeof(info.Ref);
    info.Ref.Prototype = {17, 18};
    info.Ref.Generation = g_State.ProviderGeneration;
    info.Provider = {31, 32};
    info.Version = 7;
    info.CompatibleClass = 99;
    info.ManagerOffset = 0;
    info.ManagerCount = 1;
    info.Name = text("Fixture");
    info.Category = text("Tests/Behavior");
    info.ProviderName = text("Fixture Provider");
    info.Author = text("BML");
    info.Description = text("Discovery fixture");

    *prototypeCount = 1;
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    if (!prototypes || !payload || prototypeCapacity < 1 ||
        prototypeStride < sizeof(info) || payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *prototypes = info;
    std::memcpy(payload, bytes.data(), bytes.size());
    return BML_OK;
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

// The retail 2D Text prototype, spelled the way its Building Block declares
// it, so a Blocks-header definition is validated against a real layout.
const FakeSlot kText2dSlots[] = {
    {BML_BEHAVIOR_SLOT_SETTING, 0, 0, CKPGUID_TEXTPROPERTIES,
     BML_BEHAVIOR_VALUE_INT32, "Text Properties"},
    {BML_BEHAVIOR_SLOT_PIN, 0, 0, CKPGUID_FONT,
     BML_BEHAVIOR_VALUE_INT32, "Font"},
    {BML_BEHAVIOR_SLOT_PIN, 1, 0, CKPGUID_STRING,
     BML_BEHAVIOR_VALUE_UTF8, "Text"},
    {BML_BEHAVIOR_SLOT_PIN, 2, 0, CKPGUID_ALIGNMENT,
     BML_BEHAVIOR_VALUE_INT32, "Alignment"},
    {BML_BEHAVIOR_SLOT_PIN, 3, 0, CKPGUID_RECT,
     BML_BEHAVIOR_VALUE_RECT, "Margins"},
    {BML_BEHAVIOR_SLOT_PIN, 4, 0, CKPGUID_2DVECTOR,
     BML_BEHAVIOR_VALUE_VEC2, "Offset"},
    {BML_BEHAVIOR_SLOT_PIN, 5, 0, CKPGUID_2DVECTOR,
     BML_BEHAVIOR_VALUE_VEC2, "Paragraph Indentation"},
    {BML_BEHAVIOR_SLOT_PIN, 6, 0, CKPGUID_MATERIAL,
     BML_BEHAVIOR_VALUE_OBJECT, "Background Material"},
    {BML_BEHAVIOR_SLOT_PIN, 7, 0, CKPGUID_PERCENTAGE,
     BML_BEHAVIOR_VALUE_FLOAT32, "Caret Size"},
    {BML_BEHAVIOR_SLOT_PIN, 8, 0, CKPGUID_MATERIAL,
     BML_BEHAVIOR_VALUE_OBJECT, "Caret Material"},
};

std::vector<std::uint8_t> DeclaredLayout(
    const BML_BehaviorPrototypeRef &prototype,
    BML_BehaviorLayout &layout) {
    const FakeSlot genericSlots[] = {
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

    const bool isText2d =
        prototype.Prototype.Data1 == VT_INTERFACE_2DTEXT.d1 &&
        prototype.Prototype.Data2 == VT_INTERFACE_2DTEXT.d2;
    const FakeSlot *slots = isText2d ? kText2dSlots : genericSlots;
    const std::size_t slotCount =
        isText2d ? std::size(kText2dSlots) : std::size(genericSlots);

    std::vector<std::uint8_t> payload(slotCount *
                                      sizeof(BML_BehaviorSlotRecord));
    for (std::size_t index = 0; index < slotCount; ++index) {
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
    if (g_State.DeclaredTargetable)
        layout.BehaviorFlags |= CKBEHAVIOR_TARGETABLE;
    layout.SlotOffset = 0;
    layout.SlotCount = g_State.MalformedLayout
        ? (std::numeric_limits<std::uint32_t>::max)()
        : static_cast<std::uint32_t>(slotCount);
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
    BML_BehaviorPrototypeRef prototype = g_State.LastRunPrototype;
    if (g_State.WrongRunLayoutPrototype)
        ++prototype.Prototype.Data1;
    const std::vector<std::uint8_t> bytes = DeclaredLayout(prototype, *layout);
    layout->Origin = BML_BEHAVIOR_LAYOUT_LIVE;
    layout->LayoutGeneration = g_State.LiveGeneration;
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
    const std::uint32_t nodeCount =
        g_State.DuplicateGraphNames || g_State.DuplicateGraphNode ? 2u : 1u;
    const std::uint32_t linkCount =
        g_State.DuplicateGraphLink || g_State.DuplicateGraphLinkObject
            ? 2u : 1u;
    const std::uint32_t linkOffset =
        nodeCount * sizeof(BML_BehaviorGraphNode);
    const std::uint32_t portOffset = linkOffset +
        linkCount * sizeof(BML_BehaviorGraphLink);
    const std::uint32_t portCount =
        g_State.DuplicateGraphPortNames ? 6u : 5u;
    std::vector<std::uint8_t> payload(
        portOffset + portCount * sizeof(BML_BehaviorGraphPort));

    BML_BehaviorGraphNode node{};
    node.StructSize = sizeof(node);
    node.Id = 101;
    node.Object = g_State.InvalidGraphObject
        ? BML_ObjectRef{root.Domain, 0, 0} : root;
    node.LayoutGeneration = g_State.LiveGeneration;
    node.Prototype = {21, 22};
    node.Priority = 7;
    node.Active = 1;
    node.PortOffset = portOffset;
    node.PortCount = portCount;
    node.Name.Offset = static_cast<std::uint32_t>(payload.size());
    node.Name.Length = 4;
    payload.insert(payload.end(), {'R', 'o', 'o', 't'});

    BML_BehaviorGraphPort port{};
    port.StructSize = sizeof(port);
    port.Node = node.Id;
    port.LayoutGeneration = node.LayoutGeneration;
    port.Kind = BML_BEHAVIOR_SLOT_IN;
    port.Index = 0;
    port.Active = 1;
    port.Name.Offset = static_cast<std::uint32_t>(payload.size());
    port.Name.Length = 3;
    payload.insert(payload.end(), {'R', 'u', 'n'});

    BML_BehaviorGraphPort output{};
    output.StructSize = sizeof(output);
    output.Node = node.Id;
    output.LayoutGeneration = node.LayoutGeneration;
    output.Kind = g_State.DuplicateGraphPort
        ? BML_BEHAVIOR_SLOT_IN : BML_BEHAVIOR_SLOT_OUT;
    output.Index = 0;
    output.Occurrence = g_State.InvalidGraphOccurrence ? -1 : 0;
    output.Active = g_State.InvalidGraphPortFlag ? 2u : 1u;
    output.Name.Offset = static_cast<std::uint32_t>(payload.size());
    output.Name.Length = 4;
    payload.insert(payload.end(), {'D', 'o', 'n', 'e'});

    BML_BehaviorGraphPort pin = port;
    pin.Kind = BML_BEHAVIOR_SLOT_PIN;
    pin.Active = 0;
    pin.Name.Offset = static_cast<std::uint32_t>(payload.size());
    pin.Name.Length = 5;
    payload.insert(payload.end(), {'V', 'a', 'l', 'u', 'e'});

    BML_BehaviorGraphPort computed = pin;
    computed.Index = 1;
    computed.Name.Offset = static_cast<std::uint32_t>(payload.size());
    computed.Name.Length = 8;
    payload.insert(payload.end(), {'C', 'o', 'm', 'p', 'u', 't', 'e', 'd'});

    BML_BehaviorGraphPort pout = pin;
    pout.Kind = BML_BEHAVIOR_SLOT_POUT;
    pout.Name.Offset = static_cast<std::uint32_t>(payload.size());
    pout.Name.Length = 5;
    payload.insert(payload.end(), {'V', 'a', 'l', 'u', 'e'});

    BML_BehaviorGraphLink link{};
    link.StructSize = sizeof(link);
    link.Id = g_State.GraphLinkUsesNodeId ? node.Id : 201;
    link.Object = g_State.GraphLinkUsesNodeObject
        ? node.Object : BML_ObjectRef{31, 32, 33};
    link.SourceNode = g_State.MissingGraphEndpoint ? 999u : node.Id;
    link.SourceKind = g_State.InvalidGraphLinkKind
        ? BML_BEHAVIOR_SLOT_PIN : BML_BEHAVIOR_SLOT_OUT;
    link.SourceIndex = 0;
    link.TargetNode = node.Id;
    link.TargetKind = BML_BEHAVIOR_SLOT_IN;
    link.TargetIndex = 0;
    link.InitialDelay = 2;
    link.RemainingDelay = 1;
    link.Pending = BML_BEHAVIOR_UNKNOWN;

    std::memcpy(payload.data() + nodeOffset, &node, sizeof(node));
    if (nodeCount == 2) {
        BML_BehaviorGraphNode duplicate = node;
        duplicate.Id = g_State.DuplicateGraphNode ? node.Id : 102;
        duplicate.Object = {root.Domain, root.Slot + 1, root.Generation};
        duplicate.Parent = node.Id;
        duplicate.PortOffset = 0;
        duplicate.PortCount = 0;
        std::memcpy(payload.data() + sizeof(node), &duplicate,
                    sizeof(duplicate));
    }
    std::memcpy(payload.data() + linkOffset, &link, sizeof(link));
    if (linkCount == 2) {
        BML_BehaviorGraphLink duplicate = link;
        if (g_State.DuplicateGraphLinkObject)
            duplicate.Id = link.Id + 1;
        std::memcpy(payload.data() + linkOffset + sizeof(link),
                    &duplicate, sizeof(duplicate));
    }
    std::memcpy(payload.data() + portOffset, &port, sizeof(port));
    std::memcpy(payload.data() + portOffset + sizeof(port),
                &output, sizeof(output));
    std::memcpy(payload.data() + portOffset + 2 * sizeof(port),
                &pin, sizeof(pin));
    std::memcpy(payload.data() + portOffset + 3 * sizeof(port),
                &computed, sizeof(computed));
    std::memcpy(payload.data() + portOffset + 4 * sizeof(port),
                &pout, sizeof(pout));
    if (g_State.DuplicateGraphPortNames) {
        BML_BehaviorGraphPort repeated = port;
        repeated.Index = 1;
        repeated.Occurrence = 1;
        std::memcpy(payload.data() + portOffset + 5 * sizeof(port),
                    &repeated, sizeof(repeated));
    }

    graph = {};
    graph.StructSize = sizeof(graph);
    graph.View = g_State.GraphView;
    graph.Root = g_State.WrongGraphRoot
        ? BML_ObjectRef{root.Domain, root.Slot + 99, root.Generation}
        : root;
    graph.Generation = 5;
    graph.Fingerprint = g_State.GraphFingerprint;
    graph.NodeOffset = nodeOffset;
    graph.NodeCount = nodeCount;
    graph.LinkOffset = linkOffset;
    graph.LinkCount = linkCount;
    return payload;
}

int BML_BEHAVIOR_CALL InspectGraph(
    BML_BehaviorSession, BML_ObjectRef root, std::uint32_t view,
    BML_BehaviorGraph *graph, void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    g_State.GraphView = view;
    BML_BehaviorGraph wire{};
    const BML_ObjectRef returnedRoot = g_State.ReturnsAnotherGraph
        ? BML_ObjectRef{root.Domain, root.Slot + 99, root.Generation}
        : root;
    const std::vector<std::uint8_t> bytes = GraphPayload(returnedRoot, wire);
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
    BML_BehaviorSession, BML_ObjectRef, BML_BehaviorLayout *layout,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *payloadSize, BML_BehaviorStatus *status) {
    BML_BehaviorPrototypeRef prototype{};
    prototype.StructSize = sizeof(prototype);
    prototype.Prototype = g_State.WrongNodeLayoutPrototype
        ? BML_BehaviorGuid{22, 22}
        : BML_BehaviorGuid{21, 22};
    prototype.Generation = g_State.ProviderGeneration;
    const std::vector<std::uint8_t> bytes = DeclaredLayout(prototype, *layout);
    layout->Origin = BML_BEHAVIOR_LAYOUT_LIVE;
    layout->LayoutGeneration = g_State.LiveGeneration;
    *payloadSize = static_cast<std::uint32_t>(bytes.size());
    Success(status);
    if (payloadCapacity < bytes.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    if (!bytes.empty())
        std::memcpy(payload, bytes.data(), bytes.size());
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadGraphValue(
    BML_BehaviorSession, BML_ObjectRef, std::uint32_t,
    std::uint64_t layoutGeneration, const BML_BehaviorSelector *slot,
    std::uint32_t,
    BML_BehaviorGraphValue *value, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *payloadSize,
    BML_BehaviorStatus *status) {
    Success(status);
    if (layoutGeneration && layoutGeneration != g_State.LiveGeneration) {
        status->Error = BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
        return BML_ERROR_FAIL;
    }
    const std::string_view name(
        slot->Name.Data ? slot->Name.Data : "", slot->Name.Length);
    *value = {};
    value->StructSize = sizeof(*value);
    value->Type = {CKPGUID_INT.d1, CKPGUID_INT.d2};
    if (name == "Computed" ||
        (slot->Kind == BML_BEHAVIOR_SELECTOR_INDEX && slot->Index == 1)) {
        value->State = BML_BEHAVIOR_VALUE_INDETERMINATE;
        value->Relation = BML_BEHAVIOR_VALUE_OPERATION;
        *payloadSize = 0;
        return BML_OK;
    }
    value->State = BML_BEHAVIOR_VALUE_AVAILABLE;
    value->Relation = BML_BEHAVIOR_VALUE_STORED;
    if (g_State.MalformedObservedObject) {
        value->Type = {CKPGUID_BEOBJECT.d1, CKPGUID_BEOBJECT.d2};
        value->Kind = BML_BEHAVIOR_VALUE_OBJECT;
        value->ValueOffset = 0;
        value->ValueSize = 12;
        *payloadSize = 12;
        if (payloadCapacity < 12)
            return BML_ERROR_BUFFER_TOO_SMALL;
        const std::uint8_t bytes[12] = {7, 0, 0, 0};
        std::memcpy(payload, bytes, sizeof(bytes));
        return BML_OK;
    }
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
    g_State.WatchSpec = *spec;
    g_State.WatchFunction = *callback;
    if (callback->Retain)
        callback->Retain(callback->State);
    *watch = reinterpret_cast<BML_BehaviorWatch>(3);
    return g_State.WatchOpenCode;
}

int BML_BEHAVIOR_CALL CloseWatch(BML_BehaviorWatch) {
    ++g_State.WatchCloses;
    if (g_State.WatchCloseCode != BML_OK)
        return g_State.WatchCloseCode;
    if (g_State.WatchFunction.Release)
        g_State.WatchFunction.Release(g_State.WatchFunction.State);
    g_State.WatchFunction = {};
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadWatch(BML_BehaviorWatch,
                                BML_BehaviorWatchInfo *info,
                                BML_BehaviorStatus *status) {
    ++g_State.WatchReads;
    Init(info);
    info->State = g_State.WatchState;
    Init(&info->Diagnostic);
    if (g_State.MalformedInfoDiagnostic)
        info->Diagnostic.StructSize = 0;
    if (info->State == BML_BEHAVIOR_WATCH_FAILED) {
        info->Diagnostic.Error = BML_BEHAVIOR_ERROR_CALLBACK_FAILED;
        info->Diagnostic.Phase = BML_BEHAVIOR_PHASE_CALLBACK;
    }
    Success(status);
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
        captured.Object = step.Object;
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
    if (g_State.PlanSubmitCode != BML_OK &&
        !g_State.ReturnPlanHandleOnError) {
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
    return g_State.PlanSubmitCode;
}

int BML_BEHAVIOR_CALL ReadPlan(BML_BehaviorSession, BML_BehaviorPlan plan,
                               BML_BehaviorPlanInfo *info,
                               BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PlanReads;
    if (!plan)
        return BML_ERROR_INVALID_HANDLE;
    Init(info);
    info->State = g_State.PlanState;
    info->World = 12;
    info->Matches = 1;
    info->Installations = 1;
    Init(&info->Diagnostic);
    if (g_State.MalformedInfoDiagnostic)
        info->Diagnostic.StructSize = 0;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ClosePlan(BML_BehaviorSession, BML_BehaviorPlan) {
    ++g_State.PlanCloses;
    if (g_State.PlanCloseCode != BML_OK)
        return g_State.PlanCloseCode;
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
    if (g_State.PatchApplyCode != BML_OK &&
        !g_State.ReturnPatchHandleOnError) {
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
    return g_State.PatchApplyCode;
}

int BML_BEHAVIOR_CALL ReadPatch(
    BML_BehaviorSession, BML_BehaviorPatch patch,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.PatchReads;
    if (!patch)
        return BML_ERROR_INVALID_HANDLE;
    Init(info);
    info->State = g_State.PatchState;
    info->Reserved = g_State.MalformedPatchReserved ? 1u : 0u;
    Init(&info->Diagnostic);
    if (g_State.MalformedInfoDiagnostic)
        info->Diagnostic.StructSize = 0;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ClosePatch(BML_BehaviorSession, BML_BehaviorPatch) {
    ++g_State.PatchCloses;
    if (g_State.PatchCloseCode != BML_OK)
        return g_State.PatchCloseCode;
    for (const BML_BehaviorHookFunction &hook : g_State.PatchHooks) {
        if (hook.Release)
            hook.Release(hook.State);
    }
    g_State.PatchHooks.clear();
    return BML_OK;
}

int BML_BEHAVIOR_CALL Reference(BML_BehaviorSession, std::uint32_t object,
                                BML_ObjectRef *outReference,
                                BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.References;
    g_State.ReferencedObject = object;
    if (g_State.ReferenceCode != BML_OK)
        return g_State.ReferenceCode;
    outReference->Domain = 7;
    outReference->Slot = g_State.MalformedReference ? 0u : object;
    outReference->Generation = g_State.MalformedReference ? 0u : 2u;
    return BML_OK;
}

int BML_BEHAVIOR_CALL ResolvePatchNode(BML_BehaviorSession, BML_BehaviorPatch,
                                       std::uint32_t handle,
                                       BML_ObjectRef *outNode,
                                       BML_BehaviorStatus *status) {
    Success(status);
    ++g_State.NodeResolves;
    g_State.ResolvedHandle = handle;
    if (g_State.ResolveNodeCode != BML_OK)
        return g_State.ResolveNodeCode;
    outNode->Domain = 11;
    outNode->Slot = g_State.MalformedResolvedNode ? 0u : handle;
    outNode->Generation = g_State.MalformedResolvedNode ? 0u : 4u;
    return BML_OK;
}

int BML_BEHAVIOR_CALL AttachBlock(BML_BehaviorSession, BML_ObjectRef graph,
                                  const BML_BehaviorBlock *block,
                                  BML_BehaviorRun *run,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status) {
    ++g_State.Attaches;
    g_State.AttachGraph = graph;
    return OpenRun(graph, block, nullptr, run, info, status,
                   BML_BEHAVIOR_RUN_INSTANCE);
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
    &ReadWatch,
    &Reference,
    &ResolvePatchNode,
    &AttachBlock,
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
    *out = g_State.NullInterface ? nullptr : &g_Interface;
    return BML_OK;
}

template <class T>
concept ConfigurableBlock = requires(T &value) {
    value.Settings({{"Value", std::int32_t{1}}});
    value.Frames(Signals(1));
};

template <class T>
concept CanCall = requires(const T &value) {
    value.Call("Run");
};

template <class T>
concept CanContinue = requires(T &value) {
    std::move(value).Continue();
};

template <class T>
concept CanPulse = requires(const T &value) {
    value.Pulse("Run");
};

static_assert(ConfigurableBlock<Block>);
static_assert(CanCall<Block>);
static_assert(CanContinue<Call>);
static_assert(!CanContinue<Task>);
static_assert(!CanContinue<Instance>);
static_assert(!CanPulse<Call>);
static_assert(CanPulse<Task>);
static_assert(CanPulse<Instance>);
static_assert(std::copy_constructible<Block>);
static_assert(!std::copy_constructible<Call>);
static_assert(!std::copy_constructible<Task>);
static_assert(!std::copy_constructible<Instance>);
static_assert(std::is_constructible_v<Value, int>);
static_assert(std::is_constructible_v<Value, float>);
static_assert(!std::is_constructible_v<Value, std::uint32_t>);
static_assert(!std::is_constructible_v<Value, std::uint64_t>);
static_assert(!std::is_constructible_v<Value, double>);

TEST(BehaviorAuthoring, ResultRejectsValueAccessAfterFailure) {
    auto failed = Result<std::string>::Failure(BML_ERROR_FAIL);
    EXPECT_FALSE(failed.HasValue());
    EXPECT_THROW((void) failed.Value(), std::bad_optional_access);
    EXPECT_THROW((void) failed->size(), std::bad_optional_access);
}

TEST(BehaviorAuthoring, OpensAgainstTheCompleteVersionOneContract) {
    g_State = {};
    const std::uint16_t minor = g_Interface.Header.MinorVersion;
    const std::size_t size = g_Interface.Header.StructSize;

    g_Interface.Header.MinorVersion = 99;
    auto future = Session::Open();
    ASSERT_TRUE(future);
    std::move(future).Value().Close();

    g_Interface.Header.StructSize = BML_BEHAVIOR_INTERFACE_1_0_SIZE - 1;
    auto incomplete = Session::Open();
    EXPECT_FALSE(incomplete);
    EXPECT_EQ(incomplete.Code(), BML_ERROR_VERSION_MISMATCH);

    g_Interface.Header.StructSize = size;
    g_Interface.Header.MinorVersion = minor;
    g_State.NullInterface = true;
    auto missing = Session::Open();
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.Code(), BML_ERROR_VERSION_MISMATCH);
}

TEST(BehaviorAuthoring, ContainsMalformedAndRejectedSessionHandles) {
    g_State = {};
    g_State.SessionOpenCode = BML_ERROR_FAIL;
    auto rejected = Session::Open();
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Code(), BML_ERROR_FAIL);
    EXPECT_EQ(g_State.SessionCloses, 1);

    g_State = {};
    g_State.NullSession = true;
    auto missing = Session::Open();
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(g_State.SessionCloses, 0);

    g_State = {};
    g_State.MalformedCallStatus = true;
    auto malformedStatus = Session::Open();
    EXPECT_FALSE(malformedStatus);
    EXPECT_EQ(malformedStatus.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, OwnsBlockTextAndUsesDomainSelectors) {
    g_State = {};
    auto opened = Session::Open("test.mod");
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    std::string settingName = "Caption";
    std::string text = "owned text";
    Block block = session.Use(Prototype(CKGUID(11, 22), 37));
    block.Settings({{settingName, text}}).Frames(EachFrame(8));
    ASSERT_TRUE(block.Validate());
    settingName.assign("changed");
    text.assign("changed");

    auto called = block.Call();
    ASSERT_TRUE(called) << called.Code();
    EXPECT_EQ(g_State.Owner, "test.mod");
    EXPECT_EQ(g_State.SelectorKind, BML_BEHAVIOR_SELECTOR_ONLY);
    EXPECT_EQ(g_State.SettingSelectorKind,
              BML_BEHAVIOR_SELECTOR_UNIQUE_NAME);
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
    ASSERT_EQ(taken->Size(), 1u);
    const Frame frame = (*taken)[0];
    EXPECT_EQ(frame.Sequence(), 7u);
    EXPECT_EQ(frame.GameFrame(), 91u);
    EXPECT_EQ(frame.Continuation(), Continuation::None);
    EXPECT_TRUE(frame.HasOut("Done"));
    auto pout = frame.Pout<std::int32_t>("Value");
    ASSERT_TRUE(pout) << pout.GetStatus().Message;
    EXPECT_EQ(pout.Value(), 42);

    auto continued = std::move(call).Continue();
    ASSERT_TRUE(continued);
    Task task = std::move(continued).Value();
    auto info = task.Info();
    ASSERT_TRUE(info);
    EXPECT_EQ(info.Value().Kind, RunKind::Task);
    EXPECT_EQ(info.Value().State, RunState::Pending);
    EXPECT_EQ(info.Value().PrototypeRef.Id, CKGUID(1, 2));
    EXPECT_EQ(info.Value().PrototypeRef.Generation,
              g_State.ProviderGeneration);
}

TEST(BehaviorAuthoring, RequiresAnOccurrenceForDuplicateFrameNames) {
    g_State = {};
    g_State.DuplicateFrameNames = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto spawned = session.Use(CKGUID(1, 2)).Spawn();
    ASSERT_TRUE(spawned);

    auto taken = spawned->Take();
    ASSERT_TRUE(taken);
    ASSERT_EQ(taken->Size(), 1u);
    const Frame frame = (*taken)[0];

    EXPECT_FALSE(frame.HasOut("Done"));
    EXPECT_FALSE(frame.HasOut(Selector::Only()));
    EXPECT_TRUE(frame.HasOut(Named("Done", 1)));

    auto ambiguous = frame.Pout<std::int32_t>("Value");
    ASSERT_FALSE(ambiguous);
    EXPECT_EQ(ambiguous.GetStatus().Error, Error::QueryAmbiguous);
    auto only = frame.Pout<std::int32_t>(Selector::Only());
    ASSERT_FALSE(only);
    EXPECT_EQ(only.GetStatus().Error, Error::QueryAmbiguous);
    auto second = frame.Pout<std::int32_t>(Named("Value", 1));
    ASSERT_TRUE(second);
    EXPECT_EQ(second.Value(), 84);
}

TEST(BehaviorAuthoring, ReusesFramesStorageAndRejectsTheWholeMalformedBatch) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto spawned = session.Use(CKGUID(1, 2)).Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();

    Frames frames;
    auto first = instance.Take(frames);
    ASSERT_TRUE(first);
    EXPECT_EQ(frames.Size(), 1u);
    EXPECT_EQ(g_State.FrameTakes, 2);

    g_State.FramesAvailable = true;
    auto reused = instance.Take(frames);
    ASSERT_TRUE(reused);
    EXPECT_EQ(frames.Size(), 1u);
    EXPECT_EQ(g_State.FrameTakes, 3);

    g_State.FramesAvailable = true;
    g_State.MalformedFrames = true;
    auto malformed = instance.Take(frames);
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_TRUE(frames.Empty());
}

TEST(BehaviorAuthoring, CopiesBlocksOnWriteAndKeepsValidatedCopiesCached) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block original = session.Use(CKGUID(5, 6));
    original.Settings({{"Caption", "original"}});
    ASSERT_TRUE(original.Validate());
    const int initialReads = g_State.LayoutCalls;

    Block copy = original;
    ASSERT_TRUE(copy.Validate());
    EXPECT_EQ(g_State.LayoutCalls, initialReads);

    copy.Settings({{"Retry", true}});
    ASSERT_TRUE(copy.Validate());
    EXPECT_GT(g_State.LayoutCalls, initialReads);
    const int afterCopyValidation = g_State.LayoutCalls;

    auto originalCall = original.Call();
    ASSERT_TRUE(originalCall);
    EXPECT_EQ(g_State.LayoutCalls, afterCopyValidation);
    EXPECT_EQ(g_State.SettingName, "Caption");
    EXPECT_EQ(g_State.Text, "original");
}

TEST(BehaviorAuthoring, OverridesAConfiguredFramePolicyForOneRun) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(5, 6));
    block.Frames(Signals(8));
    auto called = block.Call("Run", Latest());
    ASSERT_TRUE(called);
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_LATEST);
    EXPECT_EQ(g_State.FrameLimit, 0u);

    auto started = block.Start("Run");
    ASSERT_TRUE(started);
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_SIGNALS);
    EXPECT_EQ(g_State.FrameLimit, 8u);
}

TEST(BehaviorAuthoring, RequestsPoutsIndependentlyOfFrameRetention) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto started = session.Use(CKGUID(1, 2))
        .Frames(Signals(8).Pouts())
        .Start("Run");
    ASSERT_TRUE(started);
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_SIGNALS);
    EXPECT_EQ(g_State.FrameLimit, 8u);
    EXPECT_EQ(g_State.FrameFlags, BML_BEHAVIOR_FRAME_POLICY_POUTS);

    auto overridden = session.Use(CKGUID(1, 2))
        .Start("Run", Latest().Pouts());
    ASSERT_TRUE(overridden);
    EXPECT_EQ(g_State.FrameKind, BML_BEHAVIOR_FRAMES_LATEST);
    EXPECT_EQ(g_State.FrameFlags, BML_BEHAVIOR_FRAME_POLICY_POUTS);
}

TEST(BehaviorAuthoring, KeepsTheValidatedPrototypeGeneration) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(5, 6));
    ASSERT_TRUE(block.Validate());
    EXPECT_EQ(g_State.Generation, 0u);
    const std::uint64_t validated = g_State.ProviderGeneration;
    ++g_State.ProviderGeneration;
    g_State.RejectStaleGeneration = true;

    auto stale = block.Call("Run");
    EXPECT_FALSE(stale);
    EXPECT_EQ(stale.Code(), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(stale.GetStatus().Error, Error::PrototypeChanged);
    EXPECT_EQ(g_State.Generation, validated);
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
        session.Close();
        EXPECT_EQ(g_State.SessionCloses, 0);
        auto admission = instance.Pulse("Create");
        ASSERT_TRUE(admission);
        EXPECT_EQ(admission.Value(), PulseResult::Ran);
        EXPECT_EQ(g_State.Input, "Create");
    }
    EXPECT_EQ(g_State.RunCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, TreatsAcceptedRunRetirementAsClosing) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto spawned = session.Use(CKGUID(3, 4)).Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();

    g_State.RunCloseCode = BML_ERROR_BUSY;
    auto closing = instance.Close();
    ASSERT_TRUE(closing);
    EXPECT_EQ(closing.Value(), CloseState::Closing);
    EXPECT_TRUE(instance);
    g_State.RunCloseCode = BML_OK;
    EXPECT_EQ(instance.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(instance);
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
    ASSERT_TRUE(layout) << layout.GetStatus().Message;
    EXPECT_EQ(layout->Origin, LayoutOrigin::Live);
    EXPECT_EQ(layout->Kind, BehaviorKind::Function);
    EXPECT_EQ(layout->Generation, 9u);
    const Slot *input = layout->Find(SlotKind::In, "Run");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->Index, 0);

    g_State.WrongRunLayoutPrototype = true;
    auto wrongLayout = instance.Layout();
    EXPECT_FALSE(wrongLayout);
    EXPECT_EQ(wrongLayout.Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.WrongRunLayoutPrototype = false;

    auto inspected = instance.Inspect();
    ASSERT_TRUE(inspected) << inspected.GetStatus().Message;
    EXPECT_EQ(inspected->Root().Object().Domain, 71u);
    EXPECT_EQ(inspected->Mode(), View::Logical);
    auto root = inspected->Find("Root");
    ASSERT_TRUE(root) << root.GetStatus().Message;
    EXPECT_EQ(root->Name(), "Root");
}

TEST(BehaviorAuthoring, DiscoversPrototypesAndReadsTheirDeclaredLayout) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    PrototypeQuery query;
    query.Name = "Fixture";
    query.RequiredManagers.push_back(CKGUID(91, 92));

    auto found = session.Prototypes(query);
    ASSERT_TRUE(found) << found.GetStatus().Message;
    ASSERT_EQ(found->size(), 1u);
    EXPECT_EQ(g_State.PrototypeName, "Fixture");
    EXPECT_NE(g_State.PrototypeMatch & BML_BEHAVIOR_MATCH_NAME, 0u);
    EXPECT_NE(g_State.PrototypeMatch & BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS, 0u);
    ASSERT_EQ(g_State.RequiredManagers.size(), 1u);
    EXPECT_EQ(found->front().Ref.Id, CKGUID(17, 18));
    EXPECT_EQ(found->front().Ref.Generation, g_State.ProviderGeneration);
    EXPECT_EQ(found->front().Provider, CKGUID(31, 32));
    EXPECT_EQ(found->front().Name, "Fixture");
    EXPECT_EQ(found->front().Category, "Tests/Behavior");
    ASSERT_EQ(found->front().Managers.size(), 1u);
    EXPECT_TRUE(found->front().Managers.front().Available);

    auto layout = session.Layout(found->front().Ref);
    ASSERT_TRUE(layout) << layout.GetStatus().Message;
    EXPECT_EQ(layout->Origin, LayoutOrigin::Declared);
    EXPECT_EQ(layout->PrototypeRef.Id, CKGUID(17, 18));
    EXPECT_EQ(layout->PrototypeRef.Generation, g_State.ProviderGeneration);
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
    auto bound = instance.Bind(*pinSlot, inspected->Root().Pout("Value"),
                               Relation::Direct);
    ASSERT_TRUE(bound);
    EXPECT_EQ(g_State.BindRelation, BML_BEHAVIOR_VALUE_DIRECT);
    EXPECT_EQ(g_State.BindNode.Domain, inspected->Root().Object().Domain);
    EXPECT_TRUE(g_State.BindSource.empty());

    auto configured = instance.Settings({{"Retry", true}});
    ASSERT_TRUE(configured);
    EXPECT_EQ(configured.Value(), 10u);
    ASSERT_EQ(g_State.ConfiguredSettings.size(), 1u);
    EXPECT_EQ(g_State.ConfiguredSettings[0], "Retry");

    auto stale = instance.Set(*pinSlot, std::int32_t{1});
    EXPECT_FALSE(stale);
    EXPECT_EQ(stale.GetStatus().Error, Error::LayoutChanged);

    auto current = instance.Layout();
    ASSERT_TRUE(current);
    const Slot *currentPin = current->Find(SlotKind::Pin, "Value");
    ASSERT_NE(currentPin, nullptr);
    EXPECT_EQ(currentPin->Generation, 10u);
}

TEST(BehaviorAuthoring, ResolvesLiveSelectorsForEveryRunKind) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Block block = session.Use(CKGUID(1, 2));

    auto called = block.Call("Run");
    auto started = block.Start("Run");
    auto spawned = block.Spawn();
    ASSERT_TRUE(called);
    ASSERT_TRUE(started);
    ASSERT_TRUE(spawned);

    ASSERT_TRUE(called->Set(SlotKind::Pin, Unique("Value"), 1));
    EXPECT_EQ(g_State.SetGeneration, g_State.LiveGeneration);
    ASSERT_TRUE(started->Set(SlotKind::Pin, At(0), 2));
    EXPECT_EQ(g_State.SetGeneration, g_State.LiveGeneration);
    ASSERT_TRUE(spawned->Set(SlotKind::Local, Unique("State"), 3));
    EXPECT_EQ(g_State.SetGeneration, g_State.LiveGeneration);
}

TEST(BehaviorAuthoring, ValidatedBlocksKeepTheNativeSessionAlive) {
    g_State = {};
    {
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();
        Block block = session.Use(CKGUID(5, 6));
        ASSERT_TRUE(block.Validate());
        session.Close();

        EXPECT_EQ(g_State.SessionCloses, 0);
        auto called = block.Call();
        ASSERT_TRUE(called);
    }
    EXPECT_EQ(g_State.RunCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, PinsProviderAndReusesOneBlockAcrossOwners) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Block block = session.Use(CKGUID(7, 8));
    ASSERT_TRUE(block.Validate());

    const BML_ObjectRef firstOwner{1, 2, 3};
    const BML_ObjectRef secondOwner{4, 5, 6};
    auto called = block.Call(firstOwner, Unique("Run"));
    auto started = block.Start(secondOwner, Unique("Run"));
    auto spawned = block.Spawn(firstOwner);
    ASSERT_TRUE(called);
    ASSERT_TRUE(started);
    ASSERT_TRUE(spawned);
    ASSERT_EQ(g_State.Blocks.size(), 3u);
    EXPECT_EQ(g_State.Blocks[0].Prototype.Data1,
              g_State.Blocks[1].Prototype.Data1);
    EXPECT_EQ(g_State.Blocks[0].PinCount, g_State.Blocks[1].PinCount);
    EXPECT_EQ(g_State.Blocks[1].Prototype.Data1,
              g_State.Blocks[2].Prototype.Data1);
    EXPECT_EQ(g_State.Blocks[1].PinCount, g_State.Blocks[2].PinCount);
    ASSERT_EQ(g_State.RunOwners.size(), 3u);
    EXPECT_EQ(g_State.RunOwners[0].Domain, firstOwner.Domain);
    EXPECT_EQ(g_State.RunOwners[1].Domain, secondOwner.Domain);
    EXPECT_EQ(g_State.Generation, g_State.ProviderGeneration);
    EXPECT_EQ(g_State.LayoutCalls, 2);
}

TEST(BehaviorAuthoring, ValidateChecksTheDeclaredLayout) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block missingBlock = session.Use(CKGUID(9, 10));
    missingBlock.Settings({{"Missing", std::int32_t{1}}});
    auto missing = missingBlock.Validate();
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.GetStatus().Error, Error::SlotNotFound);

    Block ambiguousBlock = session.Use(CKGUID(9, 10));
    ambiguousBlock.Settings({{"Duplicate", std::int32_t{1}}});
    auto ambiguous = ambiguousBlock.Validate();
    EXPECT_FALSE(ambiguous);
    EXPECT_EQ(ambiguous.GetStatus().Error, Error::SlotAmbiguous);

    Block wrongKindBlock = session.Use(CKGUID(9, 10));
    wrongKindBlock.Settings({{"Caption", std::int32_t{1}}});
    auto wrongKind = wrongKindBlock.Validate();
    EXPECT_FALSE(wrongKind);
    EXPECT_EQ(wrongKind.GetStatus().Error, Error::TypeMismatch);

    Block unsupportedBlock = session.Use(CKGUID(9, 10));
    unsupportedBlock.Settings({{"Opaque", std::int32_t{1}}});
    auto unsupported = unsupportedBlock.Validate();
    EXPECT_FALSE(unsupported);
    EXPECT_EQ(unsupported.GetStatus().Error, Error::ParameterTypeUnsupported);

    auto missingCall = missingBlock.Call("Run");
    EXPECT_FALSE(missingCall);
    EXPECT_EQ(missingCall.GetStatus().Error, Error::SlotNotFound);

    Block selectedBlock = session.Use(CKGUID(9, 10));
    selectedBlock.Settings({{Named("Duplicate", 1), true}});
    auto selected = selectedBlock.Validate();
    EXPECT_TRUE(selected) << selected.GetStatus().Message;
    EXPECT_EQ(g_State.OpenRuns, 0);
}

TEST(BehaviorAuthoring, ValidateRejectsAnExplicitTargetOnATargetlessPrototype) {
    g_State = {};
    g_State.DeclaredTargetable = false;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(9, 10));
    block.NullTarget(CKPGUID_OBJECT);
    auto validated = block.Validate();
    EXPECT_FALSE(validated);
    EXPECT_EQ(validated.GetStatus().Error, Error::TargetInvalid);
    EXPECT_EQ(validated.GetStatus().Phase, Phase::Target);
    EXPECT_EQ(g_State.OpenRuns, 0);
}

TEST(BehaviorAuthoring, ValidateLeavesDynamicLayoutToTheNativeLifecycle) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block dynamic = session.Use(CKGUID(13, 14));
    dynamic.Settings({{"Extended Layout", true}})
        .Settings({{"Created Later", std::int32_t{4}}})
        .Pins({{"Dynamic Value", std::int32_t{713}}});
    auto validated = dynamic.Validate();
    EXPECT_TRUE(validated) << validated.GetStatus().Message;

    Block dynamicPinBlock = session.Use(CKGUID(13, 14));
    dynamicPinBlock.Pins({{"Dynamic Value", std::int32_t{713}}});
    auto dynamicPin = dynamicPinBlock.Validate();
    EXPECT_TRUE(dynamicPin) << dynamicPin.GetStatus().Message;
}

TEST(BehaviorAuthoring, FallsBackWhenPrototypeDiscoveryIsUnavailable) {
    g_State = {};
    g_State.LayoutUnavailable = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(15, 16));
    block.Settings({{"Runtime Setting", std::int32_t{9}}});
    auto validated = block.Validate();
    EXPECT_FALSE(validated);
    EXPECT_EQ(validated.Code(), BML_ERROR_UNAVAILABLE);
    auto called = block.Call("Run");
    EXPECT_TRUE(called) << called.GetStatus().Message;
    EXPECT_EQ(g_State.Generation, 0u);
    EXPECT_EQ(g_State.LayoutCalls, 2);
    EXPECT_EQ(g_State.OpenRuns, 1);

    auto repeated = block.Call("Run");
    ASSERT_TRUE(repeated) << repeated.GetStatus().Message;
    EXPECT_EQ(g_State.Generation, g_State.ProviderGeneration);

    const std::uint64_t selected = g_State.ProviderGeneration;
    ++g_State.ProviderGeneration;
    g_State.RejectStaleGeneration = true;
    auto stale = block.Call("Run");
    EXPECT_FALSE(stale);
    EXPECT_EQ(g_State.Generation, selected);
    EXPECT_EQ(stale.GetStatus().Error, Error::PrototypeChanged);
}

TEST(BehaviorAuthoring, RequiresAResolvedProviderForDurableEdits) {
    g_State = {};
    g_State.LayoutUnavailable = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Block block = session.Use(CKGUID(15, 16));

    Edit unresolved;
    (void) unresolved.Add(block);
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto rejected = inspected->Apply("unresolved", unresolved);
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Code(), BML_ERROR_UNAVAILABLE);

    ASSERT_TRUE(block.Spawn());
    Edit resolved;
    (void) resolved.Add(block);
    auto applied = inspected->Apply("resolved", resolved);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    ASSERT_FALSE(g_State.PatchSteps.empty());
    EXPECT_EQ(g_State.PatchSteps.front().Prototype.Generation,
              g_State.ProviderGeneration);
}

TEST(BehaviorAuthoring, RejectsMalformedDeclaredLayoutBeforeAllocation) {
    g_State = {};
    g_State.MalformedLayout = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto validated = session.Use(CKGUID(17, 18)).Validate();
    EXPECT_FALSE(validated);
    EXPECT_EQ(validated.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(validated.GetStatus().Error, Error::LayoutUnavailable);
}

TEST(BehaviorAuthoring, ContainsMalformedRunResults) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Block block = session.Use(CKGUID(19, 20));
    ASSERT_TRUE(block.Validate());

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

    g_State.NullRun = false;
    g_State.WrongRunKind = true;
    auto wrongKind = block.Call("Run");
    EXPECT_FALSE(wrongKind);
    EXPECT_EQ(wrongKind.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(g_State.RunCloses, 2);

    g_State.WrongRunKind = false;
    auto spawned = block.Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();
    g_State.MalformedCallStatus = true;
    auto info = instance.Info();
    EXPECT_FALSE(info);
    EXPECT_EQ(info.Code(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(BehaviorAuthoring, RejectsMalformedContinueAndPulseResults) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    {
        auto called = session.Use(CKGUID(19, 20)).Call("Run");
        ASSERT_TRUE(called);
        Call call = std::move(called).Value();
        g_State.WrongContinuationKind = true;
        auto continued = std::move(call).Continue();
        EXPECT_FALSE(continued);
        EXPECT_EQ(continued.Code(), BML_ERROR_MALFORMED_MESSAGE);
        g_State.WrongContinuationKind = false;
    }

    {
        auto called = session.Use(CKGUID(19, 20)).Call("Run");
        ASSERT_TRUE(called);
        Call call = std::move(called).Value();
        g_State.WrongFollowupPrototype = true;
        auto continued = std::move(call).Continue();
        EXPECT_FALSE(continued);
        EXPECT_EQ(continued.Code(), BML_ERROR_MALFORMED_MESSAGE);
        g_State.WrongFollowupPrototype = false;
    }

    auto spawned = session.Use(CKGUID(19, 20)).Spawn();
    ASSERT_TRUE(spawned);
    Instance instance = std::move(spawned).Value();

    g_State.WrongPulseKind = true;
    auto wrongKind = instance.Pulse("Run");
    EXPECT_FALSE(wrongKind);
    EXPECT_EQ(wrongKind.Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.WrongPulseKind = false;

    g_State.InvalidAdmission = true;
    auto wrongAdmission = instance.Pulse("Run");
    EXPECT_FALSE(wrongAdmission);
    EXPECT_EQ(wrongAdmission.Code(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(BehaviorAuthoring, ReadsLogicalAndLiveGraphsWithoutNativePointers) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    const BML_ObjectRef root{41, 42, 43};

    auto inspected = session.Inspect(root);
    ASSERT_TRUE(inspected) << inspected.GetStatus().Message;
    Graph graph = std::move(inspected).Value();
    EXPECT_EQ(graph.Mode(), View::Logical);
    EXPECT_EQ(graph.Root().Object().Domain, root.Domain);
    EXPECT_EQ(graph.Generation(), 5u);
    EXPECT_EQ(graph.Fingerprint(), 0x713u);
    ASSERT_EQ(graph.Nodes().size(), 1u);
    auto foundNode = graph.Find("Root");
    ASSERT_TRUE(foundNode);
    EXPECT_EQ(foundNode->Prototype(), CKGUID(21, 22));
    EXPECT_TRUE(foundNode->Active());
    ASSERT_EQ(foundNode->Ports().size(), 5u);
    EXPECT_EQ(foundNode->Ports()[0].Name(), "Run");
    EXPECT_TRUE(foundNode->Ports()[0].Active());
    ASSERT_EQ(graph.Links().size(), 1u);
    EXPECT_EQ(graph.Links()[0].Source().Node(), 101u);
    EXPECT_EQ(graph.Links()[0].Source().Kind(), SlotKind::Out);
    EXPECT_EQ(graph.Links()[0].Source().Name(), "Done");
    EXPECT_EQ(graph.Links()[0].Source().Occurrence(), 0);
    EXPECT_TRUE(graph.Links()[0].Source().Active());
    EXPECT_EQ(graph.Links()[0].Target().Kind(), SlotKind::In);
    EXPECT_EQ(graph.Links()[0].Target().Name(), "Run");
    EXPECT_TRUE(graph.Links()[0].Target().Active());
    EXPECT_EQ(graph.Links()[0].InitialDelay(), 2);
    EXPECT_EQ(graph.Links()[0].RemainingDelay(), 1);
    EXPECT_EQ(graph.Links()[0].Pending(), TruthValue::Unknown);

    auto live = graph.Live();
    ASSERT_TRUE(live);
    EXPECT_EQ(live.Value().Mode(), View::Live);
    EXPECT_EQ(g_State.GraphView, BML_BEHAVIOR_GRAPH_LIVE);
}

TEST(BehaviorAuthoring, PreservesSelectorCardinalityOnSnapshotNodes) {
    g_State = {};
    g_State.DuplicateGraphPortNames = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    const Node node = inspected->Root();

    EXPECT_EQ(node.In(0).Index(), 0);
    EXPECT_EQ(node.In(Named("Run", 1)).Index(), 1);
    EXPECT_FALSE(node.In("Run"));
    EXPECT_FALSE(node.In());
}

TEST(BehaviorAuthoring, RejectsIncoherentGraphSnapshots) {
    const std::vector<bool FakeState::*> faults = {
        &FakeState::MissingGraphEndpoint,
        &FakeState::DuplicateGraphNode,
        &FakeState::DuplicateGraphPort,
        &FakeState::DuplicateGraphLink,
        &FakeState::DuplicateGraphLinkObject,
        &FakeState::GraphLinkUsesNodeId,
        &FakeState::GraphLinkUsesNodeObject,
        &FakeState::InvalidGraphObject,
        &FakeState::InvalidGraphLinkKind,
        &FakeState::InvalidGraphPortFlag,
        &FakeState::InvalidGraphOccurrence,
        &FakeState::WrongGraphRoot,
    };
    for (std::size_t index = 0; index < faults.size(); ++index) {
        SCOPED_TRACE(index);
        g_State = {};
        g_State.*faults[index] = true;
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();

        auto inspected = session.Inspect({41, 42, 43});
        EXPECT_FALSE(inspected);
        EXPECT_EQ(inspected.Code(), BML_ERROR_MALFORMED_MESSAGE);
    }
}

TEST(BehaviorAuthoring, RejectsAWellFormedSnapshotOfAnotherGraph) {
    g_State = {};
    g_State.ReturnsAnotherGraph = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto inspected = session.Inspect({41, 42, 43});
    EXPECT_FALSE(inspected);
    EXPECT_EQ(inspected.Code(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(BehaviorAuthoring, RequiresExplicitChoiceForDuplicateNodeNames) {
    g_State = {};
    g_State.DuplicateGraphNames = true;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected) << inspected.GetStatus().Message;
    Graph graph = std::move(inspected).Value();

    auto matches = graph.FindAll("Root");
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].Id(), 101u);
    EXPECT_EQ(matches[1].Id(), 102u);

    auto ambiguous = graph.Find("Root");
    EXPECT_FALSE(ambiguous);
    EXPECT_EQ(ambiguous.GetStatus().Error, Error::QueryAmbiguous);
    auto missing = graph.Find("Missing");
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.GetStatus().Error, Error::QueryNotFound);
}

TEST(BehaviorAuthoring, ReadsStoredAndOperationBackedValuesNonForcing) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();
    const Node node = graph.Nodes().front();

    auto layout = graph.Layout(node);
    ASSERT_TRUE(layout);
    EXPECT_EQ(layout->PrototypeRef.Id, node.Prototype());
    g_State.WrongNodeLayoutPrototype = true;
    auto wrongLayout = graph.Layout(node);
    EXPECT_FALSE(wrongLayout);
    EXPECT_EQ(wrongLayout.Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.WrongNodeLayoutPrototype = false;

    auto stored = graph.Read(node.Pin("Value"));
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored.Value().State, ObservationState::Available);
    EXPECT_EQ(stored.Value().Source, Relation::Stored);
    ASSERT_TRUE(stored.Value().Kind.has_value());
    EXPECT_EQ(*stored.Value().Kind, ValueKind::Int32);
    ASSERT_NE(std::get_if<std::int32_t>(&stored.Value().Data), nullptr);
    EXPECT_EQ(*std::get_if<std::int32_t>(&stored.Value().Data), 42);

    auto computed = graph.Read(node.Pin("Computed"));
    ASSERT_TRUE(computed);
    EXPECT_EQ(computed.Value().State, ObservationState::Indeterminate);
    EXPECT_EQ(computed.Value().Source, Relation::Operation);
    EXPECT_FALSE(computed.Value().Kind.has_value());

    g_State.MalformedObservedObject = true;
    auto malformed = graph.Read(node.Pin("Value"));
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.Code(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(BehaviorAuthoring, BindsSnapshotPortsToTheirLayoutGeneration) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Graph graph = std::move(inspected).Value();
    const Node node = graph.Root();
    const Port value = node.Pin("Value");
    ASSERT_TRUE(value);
    EXPECT_EQ(value.LayoutGeneration(), g_State.LiveGeneration);

    ++g_State.LiveGeneration;
    auto read = graph.Read(value);
    EXPECT_FALSE(read);
    EXPECT_EQ(read.GetStatus().Error, Error::LayoutChanged);
    auto layout = graph.Layout(node);
    EXPECT_FALSE(layout);
    EXPECT_EQ(layout.GetStatus().Error, Error::LayoutChanged);
}

TEST(BehaviorAuthoring, RejectsViewsFromAnotherGraphSnapshot) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto first = session.Inspect({41, 42, 43});
    auto second = session.Inspect({41, 42, 43});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    auto read = first->Read(second->Root().Pin("Value"));
    EXPECT_FALSE(read);
    EXPECT_EQ(read.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(read.GetStatus().Error, Error::GraphLocalityInvalid);
    auto layout = first->Layout(second->Root());
    EXPECT_FALSE(layout);
    EXPECT_EQ(layout.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(layout.GetStatus().Error, Error::GraphLocalityInvalid);
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
            GraphChanged{}, [&](const Change &change) {
                changes.push_back(change);
            });
        ASSERT_TRUE(watched) << watched.GetStatus().Message;
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
        EXPECT_EQ(g_State.WatchFunction.Invoke(g_State.WatchFunction.State,
                                                &event),
                  BML_BEHAVIOR_WATCH_OK);
        ASSERT_EQ(changes.size(), 1u);
        EXPECT_EQ(changes[0].Kind, ChangeKind::Graph);
        EXPECT_EQ(changes[0].Sequence, 3u);
        EXPECT_EQ(changes[0].GameFrame, 17u);
        EXPECT_EQ(changes[0].Before, 0x11u);
        EXPECT_EQ(changes[0].After, 0x22u);

        event.CurrentValue.State = BML_BEHAVIOR_VALUE_AVAILABLE;
        event.CurrentValue.Value.Kind = BML_BEHAVIOR_VALUE_OBJECT;
        event.CurrentValue.Value.Data.Object = {7, 0, 0};
        EXPECT_EQ(g_State.WatchFunction.Invoke(g_State.WatchFunction.State,
                                                &event),
                  BML_BEHAVIOR_WATCH_ERROR);
        EXPECT_EQ(changes.size(), 1u);

        auto info = watched->Info();
        ASSERT_TRUE(info);
        EXPECT_EQ(info->State, WatchState::Active);
        EXPECT_EQ(g_State.WatchReads, 1);
    }
    EXPECT_EQ(g_State.WatchCloses, 1);
    EXPECT_EQ(g_State.WatchFunction.Invoke, nullptr);
}

TEST(BehaviorAuthoring, ContainsWatchCallbackFailuresAtTheCSeam) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto watched = inspected->Watch(GraphChanged{}, [](const Change &) {
        throw std::runtime_error("callback failure");
    });
    ASSERT_TRUE(watched);

    BML_BehaviorWatchEvent event{};
    Init(&event);
    event.Kind = BML_BEHAVIOR_WATCH_GRAPH;
    Init(&event.PreviousValue);
    Init(&event.PreviousValue.Value);
    event.PreviousValue.State = BML_BEHAVIOR_VALUE_UNSUPPORTED;
    event.PreviousValue.Relation = BML_BEHAVIOR_VALUE_STORED;
    Init(&event.CurrentValue);
    Init(&event.CurrentValue.Value);
    event.CurrentValue.State = BML_BEHAVIOR_VALUE_UNSUPPORTED;
    event.CurrentValue.Relation = BML_BEHAVIOR_VALUE_STORED;
    EXPECT_EQ(g_State.WatchFunction.Invoke(g_State.WatchFunction.State, &event),
              BML_BEHAVIOR_WATCH_ERROR);
    EXPECT_EQ(g_State.WatchFunction.Invoke(g_State.WatchFunction.State, nullptr),
              BML_BEHAVIOR_WATCH_ERROR);

    g_State.WatchState = BML_BEHAVIOR_WATCH_FAILED;
    auto info = watched->Info();
    ASSERT_TRUE(info);
    EXPECT_EQ(info->State, WatchState::Failed);
    EXPECT_EQ(info->LastStatus.Error, Error::CallbackFailed);
    EXPECT_EQ(info->LastStatus.Phase, Phase::Callback);
}

TEST(BehaviorAuthoring, ClosesAWatchHandleReturnedWithAnError) {
    g_State = {};
    g_State.WatchOpenCode = BML_ERROR_INVALID_PARAMETER;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);

    auto watched = inspected->Watch(GraphChanged{}, [](const Change &) {});
    EXPECT_FALSE(watched);
    EXPECT_EQ(watched.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.WatchCloses, 1);
    EXPECT_EQ(g_State.WatchFunction.Invoke, nullptr);
}

TEST(BehaviorAuthoring, AWatchKeepsTheNativeSessionAlive) {
    g_State = {};
    std::optional<Watch> retained;
    {
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();
        auto inspected = session.Inspect({41, 42, 43});
        ASSERT_TRUE(inspected);
        Graph graph = std::move(inspected).Value();
        auto watched = graph.Watch(GraphChanged{}, [](const Change &) {});
        ASSERT_TRUE(watched);
        retained.emplace(std::move(watched).Value());

        session.Close();
        EXPECT_EQ(g_State.SessionCloses, 0);
    }

    EXPECT_EQ(g_State.SessionCloses, 0);
    EXPECT_EQ(retained->Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.WatchCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, TreatsAcceptedWatchRetirementAsClosing) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto watched = inspected->Watch(GraphChanged{}, [](const Change &) {});
    ASSERT_TRUE(watched);
    Watch watch = std::move(watched).Value();

    g_State.WatchCloseCode = BML_ERROR_BUSY;
    auto closing = watch.Close();
    ASSERT_TRUE(closing);
    EXPECT_EQ(closing.Value(), CloseState::Closing);
    EXPECT_TRUE(watch);
    g_State.WatchCloseCode = BML_OK;
    EXPECT_EQ(watch.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(watch);
}

TEST(BehaviorAuthoring, SubmitsAPatchProgramAsADurablePlan) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto alive = std::make_shared<int>(0);
    std::vector<float> deltas;
    {
        Edit edit;
        const auto counter = edit.Require("Counter_Active", CKGUID(1, 2));
        const auto added = edit.Add(session.Use(CKGUID(3, 4)));
        const auto amount = edit.AppendPin(added, "Amount", CKPGUID_FLOAT);
        const auto link =
            edit.Between(counter.Out(0), edit.Graph().In("Reset"), 2);
        edit.Splice(link, added,
                    {Before("Other", "hud"), After("Third", "sound")})
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

        auto submitted = session.Plan(
            "extra-life", Scripts::One("Gameplay_Events"), edit);
        ASSERT_TRUE(submitted) << submitted.GetStatus().Message;
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
        EXPECT_EQ(require.Prototype.Prototype.Data1, 1u);
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

        auto read = plan.Info();
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

        EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
        EXPECT_EQ(g_State.PlanCloses, 1);
        EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
        EXPECT_EQ(g_State.PlanCloses, 1);
        // The Edit still owns the reference the Loader dropped.
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, ContainsHookCallbackFailuresAtTheCSeam) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Edit edit;
    edit.Tap(edit.Graph().Out(), [] { throw std::runtime_error("failure"); });
    auto submitted = session.Plan(
        "throwing-hook", Scripts::Each("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted);
    ASSERT_EQ(g_State.PlanHooks.size(), 1u);

    BML_BehaviorHookContext context{};
    Init(&context);
    EXPECT_EQ(g_State.PlanHooks[0].Invoke(g_State.PlanHooks[0].State, &context),
              static_cast<int>(HookResult::Error));
    context.Block = {1, 2, 3};
    EXPECT_EQ(g_State.PlanHooks[0].Invoke(g_State.PlanHooks[0].State, &context),
              static_cast<int>(HookResult::Fault));
    context.Script = {1, 0, 0};
    EXPECT_EQ(g_State.PlanHooks[0].Invoke(g_State.PlanHooks[0].State, &context),
              static_cast<int>(HookResult::Error));
    // A missing context is a Loader-side contract violation, not an author
    // fault, so it is still reported as an explicit Error.
    EXPECT_EQ(g_State.PlanHooks[0].Invoke(g_State.PlanHooks[0].State, nullptr),
              static_cast<int>(HookResult::Error));
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
        Edit edit;
        const auto existing = edit.Require("Counter_Active", CKGUID(1, 2));
        const auto added = edit.Add(session.Use(CKGUID(3, 4)));
        const auto amount = edit.AppendPin(added, "Amount", CKPGUID_FLOAT);
        const auto link = edit.Between(existing.Out(), edit.Graph().In());
        edit.Bind(amount, 2.5f)
            .Splice(link, added)
            .Tap(added.Out(), [alive] {});

        auto applied = graph.Apply("one-graph", edit);
        ASSERT_TRUE(applied) << applied.GetStatus().Message;
        Patch patch = std::move(applied).Value();
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

        auto read = patch.Info();
        ASSERT_TRUE(read);
        EXPECT_EQ(read.Value().State, PatchState::Active);
        EXPECT_TRUE(read.Value().Installed());
        EXPECT_EQ(read.Value().Conflicts, 0u);

        EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
        EXPECT_EQ(g_State.PatchCloses, 1);
        EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
        EXPECT_EQ(g_State.PatchCloses, 1);
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, NamesLiveNodesAndLinksByReference) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto reference = session.Reference(static_cast<CK_ID>(77));
    ASSERT_TRUE(reference) << reference.GetStatus().Message;
    EXPECT_EQ(g_State.References, 1);
    EXPECT_EQ(g_State.ReferencedObject, 77u);
    EXPECT_EQ(reference->Domain, 7u);
    EXPECT_EQ(reference->Slot, 77u);
    EXPECT_EQ(reference->Generation, 2u);

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Edit edit;
    Node liveNode = inspected->Nodes().front();
    Link liveLink = inspected->Links().front();
    const auto node = edit.Use(liveNode);
    const auto link = edit.Use(liveLink);
    edit.Splice(link, node);

    auto applied = inspected->Apply("by-reference", edit);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    Patch patch = std::move(applied).Value();

    ASSERT_EQ(g_State.PatchSteps.size(), 3u);
    EXPECT_EQ(g_State.PatchSteps[0].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_USE_NODE));
    EXPECT_EQ(g_State.PatchSteps[0].Object.Domain, 41u);
    EXPECT_EQ(g_State.PatchSteps[0].Object.Slot, 42u);
    EXPECT_EQ(g_State.PatchSteps[1].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_USE_LINK));
    EXPECT_EQ(g_State.PatchSteps[1].Object.Slot, 32u);
    EXPECT_EQ(g_State.PatchSteps[2].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SPLICE));
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, CreatesABlockAndItsLiteralsInOneStatement) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    enum class Mode : std::uint8_t { Off = 0, On = 3 };
    Block block = session.Use(CKGUID(3, 4));
    block.Pins({{"Value", 2.5f}})
        .Settings({{"Caption", "rows"}})
        .Locals({{"State", Mode::On}});
    Edit edit;
    const auto added = edit.Add(block);
    edit.Flow(edit.Graph().Out(), added.In());

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("configured", edit);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    Patch patch = std::move(applied).Value();

    ASSERT_EQ(g_State.PatchSteps.size(), 5u);
    EXPECT_EQ(g_State.PatchSteps[0].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_ADD_BLOCK));
    EXPECT_EQ(g_State.PatchSteps[0].Prototype.Generation,
              g_State.ProviderGeneration);

    const CapturedStep &caption = g_State.PatchSteps[1];
    EXPECT_EQ(caption.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SETTING));
    EXPECT_EQ(caption.Sink.Handle, g_State.PatchSteps[0].Result);
    EXPECT_EQ(caption.Sink.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_SETTING));
    EXPECT_EQ(caption.SinkSlot, "Caption");
    EXPECT_EQ(caption.Value.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_VALUE_UTF8));

    const CapturedStep &amount = g_State.PatchSteps[2];
    EXPECT_EQ(amount.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BIND_VALUE));
    EXPECT_EQ(amount.Sink.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_PIN));
    EXPECT_EQ(amount.SinkSlot, "Value");
    // A double literal is the Float the Virtools parameter was going to hold.
    EXPECT_EQ(amount.Value.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_VALUE_FLOAT32));
    EXPECT_FLOAT_EQ(amount.Value.Data.Float32, 2.5f);

    const CapturedStep &mode = g_State.PatchSteps[3];
    EXPECT_EQ(mode.SinkSlot, "State");
    EXPECT_EQ(mode.Sink.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_LOCAL));
    EXPECT_EQ(mode.Value.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_VALUE_INT32));
    EXPECT_EQ(mode.Value.Data.Int32, 3);
    EXPECT_EQ(g_State.PatchSteps[4].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_FLOW));
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, ReusesOneEditAndKeepsItsBlockSnapshotAndSettingStages) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(3, 4));
    block.Settings({{"Caption", "first"}})
        .Settings({{"Retry", true}});
    Edit edit;
    const auto added = edit.Add(block);
    edit.Flow(edit.Graph().Out(), added.In());

    // Add copied the configured Block; later changes do not alter the Edit.
    block.Pins({{"Value", 9}});

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("same-edit", edit);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    Patch patch = std::move(applied).Value();

    auto submitted = session.Plan(
        "same-edit", Scripts::One("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted) << submitted.GetStatus().Message;
    Plan plan = std::move(submitted).Value();

    ASSERT_EQ(g_State.PatchSteps.size(), 4u);
    ASSERT_EQ(g_State.PlanSteps.size(), g_State.PatchSteps.size());
    EXPECT_EQ(g_State.PatchSteps[0].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_ADD_BLOCK));
    EXPECT_EQ(g_State.PatchSteps[0].Prototype.Generation,
              g_State.ProviderGeneration);
    EXPECT_EQ(g_State.PlanSteps[0].Prototype.Generation,
              g_State.ProviderGeneration);
    EXPECT_EQ(g_State.PatchSteps[1].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SETTING));
    EXPECT_EQ(g_State.PatchSteps[1].Flags & BML_BEHAVIOR_EDIT_SETTING_STAGE,
              0u);
    EXPECT_EQ(g_State.PatchSteps[2].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_SETTING));
    EXPECT_NE(g_State.PatchSteps[2].Flags & BML_BEHAVIOR_EDIT_SETTING_STAGE,
              0u);
    EXPECT_EQ(g_State.PatchSteps[3].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_FLOW));
    for (std::size_t index = 0; index < g_State.PatchSteps.size(); ++index) {
        EXPECT_EQ(g_State.PlanSteps[index].Kind,
                  g_State.PatchSteps[index].Kind);
        EXPECT_EQ(g_State.PlanSteps[index].Flags,
                  g_State.PatchSteps[index].Flags);
        EXPECT_EQ(g_State.PlanSteps[index].SinkSlot,
                  g_State.PatchSteps[index].SinkSlot);
    }
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, KeepsTypedNullValuesInDurablePlans) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Block block = session.Use(CKGUID(3, 4));
    block.NullTarget(CKPGUID_OBJECT);
    Edit edit;
    const auto added = edit.Add(block);
    edit.Bind(added.Pin("Optional", CKPGUID_OBJECT),
              Value::Null(CKPGUID_OBJECT));

    auto submitted = session.Plan(
        "typed-null", Scripts::One("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted) << submitted.GetStatus().Message;
    Plan plan = std::move(submitted).Value();

    ASSERT_EQ(g_State.PlanSteps.size(), 3u);
    EXPECT_EQ(g_State.PlanSteps[0].Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_ADD_BLOCK));
    for (std::size_t index = 1; index < g_State.PlanSteps.size(); ++index) {
        const CapturedStep &binding = g_State.PlanSteps[index];
        EXPECT_EQ(binding.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BIND_VALUE));
        EXPECT_EQ(binding.Value.Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_VALUE_OBJECT));
        EXPECT_EQ(binding.Value.Data.Object.Domain, 0u);
        EXPECT_EQ(binding.Value.Data.Object.Slot, 0u);
        EXPECT_EQ(binding.Value.Data.Object.Generation, 0u);
    }
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, RejectsCrossSessionAndWorldBoundPlans) {
    g_State = {};
    auto firstOpened = Session::Open();
    auto secondOpened = Session::Open();
    ASSERT_TRUE(firstOpened);
    ASSERT_TRUE(secondOpened);
    Session first = std::move(firstOpened).Value();
    Session second = std::move(secondOpened).Value();

    Edit foreign;
    (void) foreign.Add(first.Use(CKGUID(3, 4)));
    auto secondGraph = second.Inspect({41, 42, 43});
    ASSERT_TRUE(secondGraph);
    auto mismatched = secondGraph->Apply("foreign", foreign);
    EXPECT_FALSE(mismatched);
    EXPECT_EQ(mismatched.GetStatus().Error, Error::OwnerUnavailable);
    EXPECT_EQ(g_State.PatchApplies, 0);

    auto firstGraph = first.Inspect({41, 42, 43});
    ASSERT_TRUE(firstGraph);
    Edit worldBound;
    (void) worldBound.Use(firstGraph->Root());
    auto durable = first.Plan(
        "world-bound", Scripts::Each("Gameplay_Events"), worldBound);
    EXPECT_FALSE(durable);
    EXPECT_EQ(durable.GetStatus().Error, Error::WorldBoundValue);
    EXPECT_EQ(g_State.PlanSubmits, 0);
}

TEST(BehaviorAuthoring, RefusesToApplyAStaleGraphSnapshot) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto graph = session.Inspect({41, 42, 43});
    ASSERT_TRUE(graph);
    ++g_State.GraphFingerprint;

    Edit edit;
    auto applied = graph->Apply("stale", edit);
    EXPECT_FALSE(applied);
    EXPECT_EQ(applied.Code(), BML_ERROR_BUSY);
    EXPECT_EQ(applied.GetStatus().Error, Error::GraphChanged);
    EXPECT_EQ(g_State.PatchApplies, 0);
}

TEST(BehaviorAuthoring, SendsALinkToANewDestination) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit edit;
    const auto existing = edit.Require("Counter_Active", CKGUID(1, 2));
    const auto added = edit.Add(session.Use(CKGUID(3, 4)));
    const auto link = edit.Between(existing.Out(), edit.Graph().In("Reset"));
    edit.Redirect(link, added.In(), {After("Other", "hud")});

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("detour", edit);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    Patch patch = std::move(applied).Value();

    ASSERT_EQ(g_State.PatchSteps.size(), 4u);
    const CapturedStep &redirect = g_State.PatchSteps[3];
    EXPECT_EQ(redirect.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_REDIRECT));
    EXPECT_EQ(redirect.Target, g_State.PatchSteps[2].Result);
    EXPECT_EQ(redirect.Sink.Handle, g_State.PatchSteps[1].Result);
    EXPECT_EQ(redirect.Sink.Kind,
              static_cast<std::uint32_t>(BML_BEHAVIOR_SLOT_IN));
    EXPECT_EQ(redirect.Node, 0u);
    ASSERT_EQ(redirect.Ordering.size(), 1u);
    EXPECT_EQ(redirect.Ordering[0].first,
              static_cast<std::uint32_t>(BML_BEHAVIOR_ORDER_AFTER));
    EXPECT_EQ(redirect.Ordering[0].second, "Other/hud");
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, RejectsAReferenceForANullObject) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto missing = session.Reference(static_cast<CKObject *>(nullptr));
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.References, 0);

    g_State.ReferenceCode = BML_ERROR_NOT_FOUND;
    auto destroyed = session.Reference(static_cast<CK_ID>(5));
    EXPECT_FALSE(destroyed);
    EXPECT_EQ(destroyed.Code(), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(g_State.References, 1);

    g_State.ReferenceCode = BML_OK;
    g_State.MalformedReference = true;
    auto malformed = session.Reference(static_cast<CK_ID>(6));
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.Code(), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(g_State.References, 2);
}

TEST(BehaviorAuthoring, ReadsBackTheLiveNodeAnAppliedEditNamed) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit edit;
    const auto added = edit.Add(session.Use(CKGUID(3, 4)));
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("resolved", edit);
    ASSERT_TRUE(applied) << applied.GetStatus().Message;
    Patch patch = std::move(applied).Value();

    auto resolved = patch.Resolve(added);
    ASSERT_TRUE(resolved) << resolved.GetStatus().Message;
    EXPECT_EQ(g_State.NodeResolves, 1);
    ASSERT_FALSE(g_State.PatchSteps.empty());
    EXPECT_EQ(g_State.ResolvedHandle, g_State.PatchSteps.front().Result);
    EXPECT_EQ(resolved->Domain, 11u);
    EXPECT_EQ(resolved->Slot, g_State.ResolvedHandle);
    EXPECT_EQ(resolved->Generation, 4u);

    g_State.MalformedResolvedNode = true;
    auto malformed = patch.Resolve(added);
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.MalformedResolvedNode = false;

    // A queued Patch has no live Node to name yet.
    g_State.ResolveNodeCode = BML_ERROR_BUSY;
    auto pending = patch.Resolve(added);
    EXPECT_FALSE(pending);
    EXPECT_EQ(pending.Code(), BML_ERROR_BUSY);

    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
    auto closed = patch.Resolve(added);
    EXPECT_FALSE(closed);
    EXPECT_EQ(closed.Code(), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(g_State.NodeResolves, 3);
}

TEST(BehaviorAuthoring, RejectsSymbolsFromAnotherEditBeforeSubmission) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit first;
    const auto foreign = first.Require("First");
    Edit second;
    const auto local = second.Require("Second");
    second.Flow(foreign.Out(), local.In());

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("foreign-symbol", second);
    ASSERT_FALSE(applied);
    EXPECT_EQ(applied.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(applied.GetStatus().Error, Error::GraphLocalityInvalid);
    EXPECT_NE(applied.GetStatus().Message.find("different Behavior Edit"),
              std::string::npos);
    EXPECT_EQ(g_State.PatchApplies, 0);
}

TEST(BehaviorAuthoring, PatchRejectsANodeFromAnotherEdit) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit installed;
    const auto local = installed.Add(session.Use(CKGUID(3, 4)));
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    auto applied = inspected->Apply("local-symbol", installed);
    ASSERT_TRUE(applied);
    Patch patch = std::move(applied).Value();

    Edit other;
    const auto foreign = other.Require("Other");
    auto rejected = patch.Resolve(foreign);
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.NodeResolves, 0);

    EXPECT_TRUE(patch.Resolve(local));
    EXPECT_EQ(g_State.NodeResolves, 1);
}

TEST(BehaviorAuthoring, ParksABlockInsideAGraphAndDrivesIt) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    const BML_ObjectRef graph{9, 10, 11};
    Block block = session.Use(CKGUID(3, 4));
    block.Settings({{"Caption", "hello"}});
    auto attached = block.SpawnIn(graph);
    ASSERT_TRUE(attached) << attached.GetStatus().Message;
    Instance instance = std::move(attached).Value();

    EXPECT_EQ(g_State.Attaches, 1);
    EXPECT_EQ(g_State.AttachGraph.Domain, graph.Domain);
    EXPECT_EQ(g_State.AttachGraph.Slot, graph.Slot);
    EXPECT_EQ(g_State.AttachGraph.Generation, graph.Generation);
    ASSERT_EQ(g_State.RunOwners.size(), 1u);
    EXPECT_EQ(g_State.RunOwners[0].Slot, graph.Slot);
    EXPECT_EQ(g_State.SettingName, "Caption");
    EXPECT_EQ(g_State.Text, "hello");

    // The parked Block is driven through its handle, not by the graph.
    auto pulsed = instance.Pulse(Unique("In"));
    ASSERT_TRUE(pulsed) << pulsed.GetStatus().Message;
    EXPECT_EQ(g_State.Input, "In");

    EXPECT_EQ(instance.Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.RunCloses, 1);
}

TEST(BehaviorAuthoring, ReportsAFailureWhenAGraphRefusesTheBlock) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    g_State.RunCode = BML_ERROR_NOT_FOUND;
    auto attached = session.Use(CKGUID(3, 4)).SpawnIn(
        BML_ObjectRef{1, 2, 3});
    EXPECT_FALSE(attached);
    EXPECT_EQ(attached.Code(), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(g_State.Attaches, 1);
    EXPECT_EQ(g_State.RunCloses, 1);
}

TEST(BehaviorAuthoring, BuildsARetailBlockThroughTheBlocksHeader) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    // Every option is a literal and every object slot is empty, so the whole
    // definition travels without one Reference round trip.
    Blocks::Text2D::Options options;
    options.FontIndex = 3;
    options.Text = "hello blocks";
    auto built = Blocks::Text2D::Make(session, options);
    ASSERT_TRUE(built) << built.GetStatus().Message;
    EXPECT_EQ(g_State.References, 0);

    const BML_ObjectRef graph{9, 10, 11};
    auto attached = std::move(built).Value().SpawnIn(graph);
    ASSERT_TRUE(attached) << attached.GetStatus().Message;
    (void) std::move(attached).Value();

    EXPECT_EQ(g_State.Attaches, 1);
    ASSERT_FALSE(g_State.Blocks.empty());
    const BML_BehaviorBlock &block = g_State.Blocks.back();
    EXPECT_EQ(block.Prototype.Data1, VT_INTERFACE_2DTEXT.d1);
    EXPECT_EQ(block.Prototype.Data2, VT_INTERFACE_2DTEXT.d2);
    EXPECT_EQ(block.PinCount, 9u);
    EXPECT_EQ(block.SettingStageCount, 1u);
    EXPECT_EQ(g_State.SettingSelectorKind, BML_BEHAVIOR_SELECTOR_INDEX);
    EXPECT_EQ(g_State.SettingIndex, 0);
}

TEST(BehaviorAuthoring, PutsACallbackOnALinkItNamed) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto alive = std::make_shared<int>(0);
    {
        auto inspected = session.Inspect({41, 42, 43});
        ASSERT_TRUE(inspected);
        Edit edit;
        Link liveLink = inspected->Links().front();
        const auto link = edit.Use(liveLink);
        edit.Before(link, [alive] {});
        auto applied = inspected->Apply("before", edit);
        ASSERT_TRUE(applied) << applied.GetStatus().Message;
        Patch patch = std::move(applied).Value();

        ASSERT_EQ(g_State.PatchSteps.size(), 2u);
        EXPECT_EQ(g_State.PatchSteps[1].Kind,
                  static_cast<std::uint32_t>(BML_BEHAVIOR_EDIT_BEFORE));
        EXPECT_EQ(g_State.PatchSteps[1].Target, g_State.PatchSteps[0].Result);
        EXPECT_TRUE(g_State.PatchSteps[1].HasHook);
        EXPECT_EQ(alive.use_count(), 2);
        EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, RejectsPlanWithoutAScriptBeforeCallingTheLoader) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit edit;
    auto submitted = session.Plan("nameless", Scripts::Each(""), edit);
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
        Edit edit;
        edit.Tap(edit.Graph().Out(0), [alive] {});
        auto submitted = session.Plan(
            "rejected", Scripts::Each("Gameplay_Events"), edit);
        EXPECT_FALSE(submitted);
        EXPECT_EQ(submitted.Code(), BML_ERROR_INVALID_PARAMETER);
        EXPECT_EQ(g_State.PlanSubmits, 1);
        EXPECT_EQ(g_State.PlanCloses, 0);
        // A rejected Plan retained nothing, so only the builder still owns it.
        EXPECT_EQ(alive.use_count(), 2);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(BehaviorAuthoring, ClosesPlanAndPatchHandlesReturnedWithErrors) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    Edit planEdit;
    planEdit.Tap(planEdit.Graph().Out(), [] {});
    g_State.PlanSubmitCode = BML_ERROR_INVALID_PARAMETER;
    g_State.ReturnPlanHandleOnError = true;
    auto submitted = session.Plan(
        "rejected-after-admission", Scripts::Each("Gameplay_Events"),
        planEdit);
    EXPECT_FALSE(submitted);
    EXPECT_EQ(submitted.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.PlanCloses, 1);
    EXPECT_TRUE(g_State.PlanHooks.empty());

    g_State.PatchApplyCode = BML_ERROR_INVALID_PARAMETER;
    g_State.ReturnPatchHandleOnError = true;
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Edit patchEdit;
    patchEdit.Tap(patchEdit.Graph().Out(), [] {});
    auto applied = inspected->Apply("rejected-after-admission", patchEdit);
    EXPECT_FALSE(applied);
    EXPECT_EQ(applied.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_State.PatchCloses, 1);
    EXPECT_TRUE(g_State.PatchHooks.empty());
}

TEST(BehaviorAuthoring, RejectsMalformedWatchPlanAndPatchInfo) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);

    auto watched = inspected->Watch(GraphChanged{}, [](const Change &) {});
    ASSERT_TRUE(watched);
    Watch watch = std::move(watched).Value();

    Edit planEdit;
    auto submitted = session.Plan(
        "info", Scripts::Each("Gameplay_Events"), planEdit);
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();

    Edit patchEdit;
    auto applied = inspected->Apply("info", patchEdit);
    ASSERT_TRUE(applied);
    Patch patch = std::move(applied).Value();

    g_State.WatchState = 99;
    EXPECT_EQ(watch.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.WatchState = BML_BEHAVIOR_WATCH_ACTIVE;
    g_State.MalformedInfoDiagnostic = true;
    EXPECT_EQ(watch.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.MalformedInfoDiagnostic = false;

    g_State.PlanState = 99;
    EXPECT_EQ(plan.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.PlanState = BML_BEHAVIOR_PLAN_ACTIVE;
    g_State.MalformedInfoDiagnostic = true;
    EXPECT_EQ(plan.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.MalformedInfoDiagnostic = false;

    g_State.PatchState = 99;
    EXPECT_EQ(patch.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.PatchState = BML_BEHAVIOR_PATCH_ACTIVE;
    g_State.MalformedInfoDiagnostic = true;
    EXPECT_EQ(patch.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);
    g_State.MalformedInfoDiagnostic = false;
    g_State.MalformedPatchReserved = true;
    EXPECT_EQ(patch.Info().Code(), BML_ERROR_MALFORMED_MESSAGE);

    EXPECT_EQ(watch.Close().Value(), CloseState::Closed);
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
}

TEST(BehaviorAuthoring, PlansKeepTheNativeSessionAlive) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Edit edit;
    auto submitted = session.Plan(
        "inert", Scripts::Each("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();
    ASSERT_TRUE(plan);

    session.Close();
    EXPECT_TRUE(plan);
    EXPECT_TRUE(plan.Info());
    EXPECT_EQ(g_State.SessionCloses, 0);
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.PlanCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, AFailedPlanCloseKeepsTheHandleForRetry) {
    g_State = {};
    g_State.PlanCloseCode = BML_ERROR_FAIL;
    g_State.PlanState = BML_BEHAVIOR_PLAN_CONFLICTED;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Edit edit;
    auto submitted = session.Plan(
        "conflicted", Scripts::Each("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();

    auto failedClose = plan.Close();
    EXPECT_FALSE(failedClose);
    EXPECT_EQ(failedClose.Code(), BML_ERROR_FAIL);
    EXPECT_TRUE(plan);
    auto read = plan.Info();
    ASSERT_TRUE(read);
    EXPECT_EQ(read->State, PlanState::Conflicted);
    EXPECT_EQ(g_State.PlanCloses, 1);

    g_State.PlanCloseCode = BML_OK;
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(plan);
    EXPECT_EQ(g_State.PlanCloses, 2);
}

TEST(BehaviorAuthoring, AClosingPlanKeepsTheHandleUntilItIsClosed) {
    g_State = {};
    g_State.PlanCloseCode = BML_ERROR_BUSY;
    g_State.PlanState = BML_BEHAVIOR_PLAN_RETIRING;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Edit edit;
    auto submitted = session.Plan(
        "closing", Scripts::Each("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();

    auto closing = plan.Close();
    ASSERT_TRUE(closing);
    EXPECT_EQ(closing.Value(), CloseState::Closing);
    EXPECT_TRUE(plan);
    auto read = plan.Info();
    ASSERT_TRUE(read);
    EXPECT_EQ(read->State, PlanState::Retiring);

    g_State.PlanCloseCode = BML_OK;
    EXPECT_EQ(plan.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(plan);
}

TEST(BehaviorAuthoring, AFailedGraphPatchCloseKeepsTheHandleForRetry) {
    g_State = {};
    g_State.PatchCloseCode = BML_ERROR_FAIL;
    g_State.PatchState = BML_BEHAVIOR_PATCH_CONFLICTED;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Edit edit;
    auto applied = inspected->Apply("conflicted", edit);
    ASSERT_TRUE(applied);
    Patch patch = std::move(applied).Value();
    inspected = Result<Graph>{};
    session.Close();
    EXPECT_EQ(g_State.SessionCloses, 0);

    auto failedClose = patch.Close();
    EXPECT_FALSE(failedClose);
    EXPECT_EQ(failedClose.Code(), BML_ERROR_FAIL);
    EXPECT_TRUE(patch);
    auto read = patch.Info();
    ASSERT_TRUE(read);
    EXPECT_EQ(read->State, PatchState::Conflicted);
    EXPECT_EQ(g_State.PatchCloses, 1);

    g_State.PatchCloseCode = BML_OK;
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(patch);
    EXPECT_EQ(g_State.PatchCloses, 2);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, AClosingGraphPatchKeepsTheHandleUntilItIsClosed) {
    g_State = {};
    g_State.PatchCloseCode = BML_ERROR_BUSY;
    g_State.PatchState = BML_BEHAVIOR_PATCH_CLOSING;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Edit edit;
    auto applied = inspected->Apply("closing", edit);
    ASSERT_TRUE(applied);
    Patch patch = std::move(applied).Value();

    auto closing = patch.Close();
    ASSERT_TRUE(closing);
    EXPECT_EQ(closing.Value(), CloseState::Closing);
    EXPECT_TRUE(patch);
    auto read = patch.Info();
    ASSERT_TRUE(read);
    EXPECT_EQ(read->State, PatchState::Closing);

    g_State.PatchCloseCode = BML_OK;
    EXPECT_EQ(patch.Close().Value(), CloseState::Closed);
    EXPECT_FALSE(patch);
}

TEST(BehaviorAuthoring, DestructionRequestsRetirementBeforeReleasingTheSession) {
    g_State = {};
    g_State.PlanCloseCode = BML_ERROR_FAIL;
    {
        auto opened = Session::Open();
        ASSERT_TRUE(opened);
        Session session = std::move(opened).Value();
        Edit edit;
        auto submitted = session.Plan(
            "retiring", Scripts::Each("Gameplay_Events"), edit);
        ASSERT_TRUE(submitted);
        Plan plan = std::move(submitted).Value();
        session.Close();
        EXPECT_EQ(g_State.SessionCloses, 0);
    }
    EXPECT_EQ(g_State.PlanCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, DestructionMayRequestRetirementFromAnotherThread) {
    g_State = {};
    g_State.PlanCloseCode = BML_ERROR_BUSY;
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();
    Edit edit;
    auto submitted = session.Plan(
        "retiring", Scripts::Each("Gameplay_Events"), edit);
    ASSERT_TRUE(submitted);
    Plan plan = std::move(submitted).Value();
    session.Close();

    std::thread retire([plan = std::move(plan)]() mutable {});
    retire.join();

    EXPECT_EQ(g_State.PlanCloses, 1);
    EXPECT_EQ(g_State.SessionCloses, 1);
}

TEST(BehaviorAuthoring, MoveAssignmentRetiresPreviousFacadeOwnership) {
    g_State = {};
    auto opened = Session::Open();
    ASSERT_TRUE(opened);
    Session session = std::move(opened).Value();

    auto firstSpawned = session.Use(CKGUID(1, 2)).Spawn();
    auto secondSpawned = session.Use(CKGUID(3, 4)).Spawn();
    ASSERT_TRUE(firstSpawned);
    ASSERT_TRUE(secondSpawned);
    Instance firstInstance = std::move(firstSpawned).Value();
    Instance secondInstance = std::move(secondSpawned).Value();
    firstInstance = std::move(secondInstance);
    EXPECT_TRUE(firstInstance);
    EXPECT_FALSE(secondInstance);
    EXPECT_EQ(g_State.RunCloses, 1);
    EXPECT_EQ(firstInstance.Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.RunCloses, 2);

    Edit planEdit;
    auto firstSubmitted = session.Plan(
        "first", Scripts::Each("Gameplay_Events"), planEdit);
    auto secondSubmitted = session.Plan(
        "second", Scripts::Each("Gameplay_Events"), planEdit);
    ASSERT_TRUE(firstSubmitted);
    ASSERT_TRUE(secondSubmitted);
    Plan firstPlan = std::move(firstSubmitted).Value();
    Plan secondPlan = std::move(secondSubmitted).Value();
    firstPlan = std::move(secondPlan);
    EXPECT_TRUE(firstPlan);
    EXPECT_FALSE(secondPlan);
    EXPECT_EQ(g_State.PlanCloses, 1);
    EXPECT_EQ(firstPlan.Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.PlanCloses, 2);

    auto inspected = session.Inspect({41, 42, 43});
    ASSERT_TRUE(inspected);
    Edit patchEdit;
    auto firstApplied = inspected->Apply("first", patchEdit);
    auto secondApplied = inspected->Apply("second", patchEdit);
    ASSERT_TRUE(firstApplied);
    ASSERT_TRUE(secondApplied);
    Patch firstPatch = std::move(firstApplied).Value();
    Patch secondPatch = std::move(secondApplied).Value();
    firstPatch = std::move(secondPatch);
    EXPECT_TRUE(firstPatch);
    EXPECT_FALSE(secondPatch);
    EXPECT_EQ(g_State.PatchCloses, 1);
    EXPECT_EQ(firstPatch.Close().Value(), CloseState::Closed);
    EXPECT_EQ(g_State.PatchCloses, 2);
}
