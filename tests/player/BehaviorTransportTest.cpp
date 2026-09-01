#include <BML/Behavior.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Scene.h>

#include "BehaviorTransportFixtureApi.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

template <typename T>
T Dto() {
    T value{};
    value.StructSize = sizeof(value);
    return value;
}

BML_BehaviorGuid Guid(CKGUID value) {
    return {static_cast<std::uint32_t>(value.d1),
            static_cast<std::uint32_t>(value.d2)};
}

BML_BehaviorString Text(const char *value) {
    return {value, static_cast<std::uint32_t>(std::strlen(value))};
}

BML_BehaviorSelector Named(const char *name) {
    BML_BehaviorSelector selector = Dto<BML_BehaviorSelector>();
    selector.Kind = BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
    selector.Name = Text(name);
    return selector;
}

struct BlockArguments {
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    BML_BehaviorSettingStage Stage = Dto<BML_BehaviorSettingStage>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();

    BlockArguments(bool retry, std::uint32_t retention,
                   std::uint32_t limit, std::uint64_t generation) {
        Retry.Slot = Named("Retry");
        Retry.Value = Dto<BML_BehaviorValue>();
        Retry.Value.Kind = BML_BEHAVIOR_VALUE_BOOL;
        Retry.Value.Type = Guid(CKPGUID_BOOL);
        Retry.Value.Data.Bool = retry ? 1u : 0u;
        Stage.Settings = &Retry;
        Stage.SettingCount = 1;

        Block.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
        Block.Target = Dto<BML_BehaviorTarget>();
        Block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        Block.SettingStages = &Stage;
        Block.SettingStageCount = 1;
        Block.Outcomes = Dto<BML_BehaviorRetention>();
        Block.Outcomes.Kind = retention;
        Block.Outcomes.Limit = limit;
        Block.PrototypeGeneration = generation;
    }
};

std::uint32_t Load32(const std::uint8_t *data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

struct DrainedOutcomes {
    std::vector<BML_BehaviorOutcomeHeader> Headers;
    std::vector<std::uint8_t> Payload;
    bool NonConsumingSizeQuery = false;
};

class BehaviorTransportTest final : public IMod {
public:
    explicit BehaviorTransportTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorTransportTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Transport Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Tests the public bml.behavior interface in Ballance Player";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const void *found = nullptr;
        if (BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                             BML_BEHAVIOR_INTERFACE_MAJOR, &found) != BML_OK) {
            Fail("interface-missing");
            return;
        }
        m_Behavior = static_cast<const BML_BehaviorInterface *>(found);
        if (!BML_IFACE_HAS(m_Behavior, BML_BehaviorInterface, CloseRun)) {
            Fail("interface-incomplete");
            return;
        }
        if (!BML_IFACE_HAS(m_Behavior, BML_BehaviorInterface,
                           ReadLiveLayout)) {
            Fail("prototype-discovery-unavailable");
            return;
        }
        found = nullptr;
        if (BML_GetInterface(BML_SCENE_INTERFACE_ID,
                             BML_SCENE_INTERFACE_MAJOR, &found) == BML_OK)
            m_Scene = static_cast<const BML_SceneInterface *>(found);

        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        if (m_Behavior->OpenSession({}, &m_Session, &status) != BML_OK ||
            !m_Session) {
            Fail("session-open");
            return;
        }
        m_SessionOpenedBeforeLevel = true;
        if (!DiscoverPrototype()) {
            Fail("prototype-discovery");
            return;
        }
    }

    void OnStartLevel() override {
        m_LevelStarted = true;
    }

    void OnProcess() override {
        if (m_Done || !m_LevelStarted || !m_Session)
            return;
        ++m_LevelFrames;
        if (m_LevelFrames == 1)
            BeginRuns();
        else if (m_LevelFrames == 3)
            PulseLatest();
        else if (m_LevelFrames == 5)
            CheckRuns();
    }

    void OnUnload() override {
        CloseRuns();
        if (m_Behavior && m_Session)
            m_Behavior->CloseSession(m_Session);
        m_Session = nullptr;
    }

private:
    std::string_view Bytes(const std::vector<std::uint8_t> &payload,
                           BML_BehaviorText text) const {
        if (static_cast<std::uint64_t>(text.Offset) + text.Length >
            payload.size())
            return {};
        return {reinterpret_cast<const char *>(payload.data() + text.Offset),
                text.Length};
    }

    bool HasLayoutSlot(const BML_BehaviorLayout &layout,
                       const std::vector<std::uint8_t> &payload,
                       std::uint32_t kind, std::string_view name,
                       int occurrence, std::uint32_t valueKind = 0) const {
        for (std::uint32_t index = 0; index < layout.SlotCount; ++index) {
            const std::uint64_t offset =
                static_cast<std::uint64_t>(layout.SlotOffset) +
                static_cast<std::uint64_t>(index) *
                    sizeof(BML_BehaviorSlotRecord);
            if (offset + sizeof(BML_BehaviorSlotRecord) > payload.size())
                return false;
            BML_BehaviorSlotRecord slot{};
            std::memcpy(&slot, payload.data() + offset, sizeof(slot));
            if (slot.StructSize < sizeof(slot))
                return false;
            if (slot.Kind == kind && slot.Occurrence == occurrence &&
                Bytes(payload, slot.Name) == name &&
                (!valueKind || slot.ValueKind == valueKind))
                return true;
        }
        return false;
    }

    bool ValidateLayout(const BML_BehaviorLayout &layout,
                        const std::vector<std::uint8_t> &payload,
                        std::uint32_t origin) const {
        return layout.Origin == origin &&
            layout.Prototype.Prototype.Data1 ==
                Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID).Data1 &&
            layout.Prototype.Prototype.Data2 ==
                Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID).Data2 &&
            layout.Prototype.Generation == m_Prototype.Generation &&
            layout.Kind == BML_BEHAVIOR_PROTOTYPE_FUNCTION &&
            Bytes(payload, layout.Name) == "BML Behavior Transport Fixture" &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_IN, "Run", 0) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_OUT, "Done", 0) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_SETTING,
                          "Retry", 0, BML_BEHAVIOR_VALUE_BOOL) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Number", 0, BML_BEHAVIOR_VALUE_INT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Number", 1, BML_BEHAVIOR_VALUE_FLOAT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Object", 0, BML_BEHAVIOR_VALUE_OBJECT);
    }

    bool ReadDeclaredLayout() {
        BML_BehaviorLayout layout = Dto<BML_BehaviorLayout>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        int result = m_Behavior->ReadDeclaredLayout(
            m_Session, &m_Prototype, &layout, nullptr, 0, &payloadSize,
            &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL || !payloadSize)
            return false;
        std::vector<std::uint8_t> payload(payloadSize);
        layout.Origin = 0x7fffffffu;
        status = Dto<BML_BehaviorStatus>();
        std::uint32_t shortSize = 0;
        result = m_Behavior->ReadDeclaredLayout(
            m_Session, &m_Prototype, &layout, payload.data(), payloadSize - 1,
            &shortSize, &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL ||
            shortSize != payloadSize || layout.Origin != 0x7fffffffu)
            return false;
        layout = Dto<BML_BehaviorLayout>();
        status = Dto<BML_BehaviorStatus>();
        result = m_Behavior->ReadDeclaredLayout(
            m_Session, &m_Prototype, &layout, payload.data(), payloadSize,
            &payloadSize, &status);
        return result == BML_OK &&
            ValidateLayout(layout, payload, BML_BEHAVIOR_LAYOUT_DECLARED);
    }

    bool ReadLiveLayout(BML_BehaviorRun run) {
        BML_BehaviorLayout layout = Dto<BML_BehaviorLayout>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        int result = m_Behavior->ReadLiveLayout(
            run, &layout, nullptr, 0, &payloadSize, &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL || !payloadSize)
            return false;
        std::vector<std::uint8_t> payload(payloadSize);
        layout.Origin = 0x7fffffffu;
        status = Dto<BML_BehaviorStatus>();
        std::uint32_t shortSize = 0;
        result = m_Behavior->ReadLiveLayout(
            run, &layout, payload.data(), payloadSize - 1, &shortSize,
            &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL ||
            shortSize != payloadSize || layout.Origin != 0x7fffffffu)
            return false;
        layout = Dto<BML_BehaviorLayout>();
        status = Dto<BML_BehaviorStatus>();
        result = m_Behavior->ReadLiveLayout(
            run, &layout, payload.data(), payloadSize, &payloadSize, &status);
        return result == BML_OK && layout.LayoutGeneration != 0 &&
            ValidateLayout(layout, payload, BML_BEHAVIOR_LAYOUT_LIVE);
    }

    bool DiscoverPrototype() {
        BML_BehaviorPrototypeQuery query =
            Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROTOTYPE;
        query.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t count = 0;
        std::uint32_t payloadSize = 0;
        int result = m_Behavior->FindPrototypes(
            m_Session, &query, nullptr, 0,
            sizeof(BML_BehaviorPrototypeInfo), nullptr, 0, &count,
            &payloadSize, &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL || count != 1 ||
            !payloadSize)
            return false;
        BML_BehaviorPrototypeInfo prototype{};
        std::vector<std::uint8_t> payload(payloadSize);
        prototype.StructSize = 0x7fffffffu;
        status = Dto<BML_BehaviorStatus>();
        std::uint32_t shortCount = 0;
        std::uint32_t shortSize = 0;
        result = m_Behavior->FindPrototypes(
            m_Session, &query, &prototype, 1,
            sizeof(BML_BehaviorPrototypeInfo), payload.data(),
            payloadSize - 1, &shortCount, &shortSize, &status);
        if (result != BML_ERROR_BUFFER_TOO_SMALL || shortCount != 1 ||
            shortSize != payloadSize || prototype.StructSize != 0x7fffffffu)
            return false;
        prototype = {};
        status = Dto<BML_BehaviorStatus>();
        result = m_Behavior->FindPrototypes(
            m_Session, &query, &prototype, 1,
            sizeof(BML_BehaviorPrototypeInfo), payload.data(), payloadSize,
            &count, &payloadSize, &status);
        if (result != BML_OK || count != 1 ||
            prototype.StructSize < sizeof(prototype) ||
            prototype.Ref.Generation == 0 ||
            Bytes(payload, prototype.Name) !=
                "BML Behavior Transport Fixture" ||
            Bytes(payload, prototype.Category) != "BML/Test")
            return false;
        m_Prototype = prototype.Ref;
        m_CatalogPassed = ReadDeclaredLayout() &&
            RejectProviderOwnedValue();
        return m_CatalogPassed;
    }

    bool RejectProviderOwnedValue() {
        BlockArguments arguments(false, BML_BEHAVIOR_RETENTION_SIGNALS, 64,
                                 m_Prototype.Generation);
        arguments.Retry.Value.Type = Guid(BML_BEHAVIOR_PROVIDER_VALUE_GUID);
        arguments.Retry.Value.Kind = BML_BEHAVIOR_VALUE_INT32;
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &input, &run, &info, &status);
        return result == BML_ERROR_INVALID_PARAMETER && !run &&
            status.Error == BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED &&
            status.Phase == BML_BEHAVIOR_PHASE_BINDING;
    }

    bool OpenCall(BML_BehaviorRun &run, const char *inputName = "Run",
                  bool version1Block = false) {
        BlockArguments arguments(false, BML_BEHAVIOR_RETENTION_SIGNALS, 64,
                                 m_Prototype.Generation);
        if (version1Block)
            arguments.Block.StructSize =
                offsetof(BML_BehaviorBlock, Outcomes) +
                sizeof(BML_BehaviorRetention);
        BML_BehaviorSelector input = Named(inputName);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool OpenStart(BML_BehaviorRun &run, std::uint32_t limit) {
        BlockArguments arguments(true, BML_BEHAVIOR_RETENTION_EACH_FRAME,
                                 limit, m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Start(m_Session, {}, &arguments.Block, &input,
                                 &run, &info, &status) == BML_OK && run;
    }

    bool OpenInstance(BML_BehaviorRun &run, std::uint32_t retention,
                      std::uint32_t limit) {
        BlockArguments arguments(false, retention, limit,
                                 m_Prototype.Generation);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Spawn(m_Session, {}, &arguments.Block, &run,
                                 &info, &status) == BML_OK && run;
    }

    bool Pulse(BML_BehaviorRun run, const char *name,
               std::uint32_t expectedAdmission) {
        BML_BehaviorSelector input = Named(name);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t admission = 0;
        return m_Behavior->Pulse(run, &input, &admission, &info, &status) ==
                   BML_OK &&
               admission == expectedAdmission;
    }

    void BeginRuns() {
        const bool opened = OpenCall(m_Call, "Run", true) &&
            OpenStart(m_Start, 64) &&
            OpenCall(m_Object, "Make Object") &&
            OpenInstance(m_Latest, BML_BEHAVIOR_RETENTION_LATEST, 0) &&
            OpenStart(m_QueueFull, 1);
        const bool pulsed = opened &&
            ReadLiveLayout(m_Latest) &&
            Pulse(m_Latest, "Run", BML_BEHAVIOR_ADMISSION_EXECUTED) &&
            Pulse(m_Latest, "Run", BML_BEHAVIOR_ADMISSION_QUEUED);
        if (!pulsed)
            Fail("admission");
    }

    void PulseLatest() {
        if (!Pulse(m_Latest, "Run", BML_BEHAVIOR_ADMISSION_EXECUTED))
            Fail("latest-pulse");
    }

    bool Drain(BML_BehaviorRun run, DrainedOutcomes &out) {
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t headerCount = 0;
        std::uint32_t payloadSize = 0;
        const int measured = m_Behavior->DrainOutcomes(
            run, nullptr, 0, sizeof(BML_BehaviorOutcomeHeader), nullptr, 0,
            &headerCount, &payloadSize, &status);
        if (measured != BML_ERROR_BUFFER_TOO_SMALL || !headerCount)
            return false;
        out.Headers.resize(headerCount);
        out.Payload.resize(payloadSize);
        std::uint32_t secondHeaderCount = 0;
        std::uint32_t secondPayloadSize = 0;
        status = Dto<BML_BehaviorStatus>();
        const int drained = m_Behavior->DrainOutcomes(
            run, out.Headers.data(), headerCount,
            sizeof(BML_BehaviorOutcomeHeader), out.Payload.data(), payloadSize,
            &secondHeaderCount, &secondPayloadSize, &status);
        out.NonConsumingSizeQuery = drained == BML_OK &&
            secondHeaderCount == headerCount && secondPayloadSize == payloadSize;
        return out.NonConsumingSizeQuery;
    }

    template <typename T>
    bool Record(const DrainedOutcomes &out, std::uint32_t offset,
                std::uint32_t index, T &record) const {
        const std::uint64_t position = static_cast<std::uint64_t>(offset) +
            static_cast<std::uint64_t>(index) * sizeof(T);
        if (position + sizeof(T) > out.Payload.size())
            return false;
        std::memcpy(&record, out.Payload.data() + position, sizeof(T));
        return record.StructSize >= sizeof(T);
    }

    std::string_view Bytes(const DrainedOutcomes &out, std::uint32_t offset,
                           std::uint32_t length) const {
        if (static_cast<std::uint64_t>(offset) + length > out.Payload.size())
            return {};
        return {reinterpret_cast<const char *>(out.Payload.data() + offset),
                length};
    }

    bool ValidateValues(const DrainedOutcomes &out,
                        const BML_BehaviorOutcomeHeader &header,
                        BML_ObjectRef *objectReference = nullptr,
                        std::uint32_t *valueMask = nullptr) const {
        if (header.PoutCount != 5)
            return false;
        bool integer = false;
        bool real = false;
        bool vector = false;
        bool text = false;
        bool object = false;
        for (std::uint32_t index = 0; index < header.PoutCount; ++index) {
            BML_BehaviorPoutRecord record{};
            if (!Record(out, header.PoutOffset, index, record))
                return false;
            const std::string_view name = Bytes(
                out, record.NameOffset, record.NameLength);
            const std::string_view value = Bytes(
                out, record.ValueOffset, record.ValueSize);
            if (record.Kind == BML_BEHAVIOR_VALUE_INT32 && name == "Number" &&
                record.Occurrence == 0 && value.size() == 4) {
                integer = Load32(reinterpret_cast<const std::uint8_t *>(value.data())) == 42;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_FLOAT32 &&
                       name == "Number" && record.Occurrence == 1 &&
                       value.size() == 4) {
                const float number = std::bit_cast<float>(
                    Load32(reinterpret_cast<const std::uint8_t *>(value.data())));
                real = number == 1.5f;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_VEC3 &&
                       name == "Vector" && value.size() == 12) {
                const auto *bytes = reinterpret_cast<const std::uint8_t *>(value.data());
                vector = std::bit_cast<float>(Load32(bytes)) == 1.0f &&
                         std::bit_cast<float>(Load32(bytes + 4)) == 2.0f &&
                         std::bit_cast<float>(Load32(bytes + 8)) == 3.0f;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_UTF8 &&
                       name == "Text") {
                text = value == "transport";
            } else if (record.Kind == BML_BEHAVIOR_VALUE_OBJECT &&
                       name == "Object" && value.size() == 12) {
                const auto *bytes = reinterpret_cast<const std::uint8_t *>(value.data());
                BML_ObjectRef reference{Load32(bytes), Load32(bytes + 4),
                                        Load32(bytes + 8)};
                object = true;
                if (objectReference && reference.Domain)
                    *objectReference = reference;
            }
        }
        const std::uint32_t mask = (integer ? 1u : 0u) |
            (real ? 2u : 0u) | (vector ? 4u : 0u) |
            (text ? 8u : 0u) | (object ? 16u : 0u);
        if (valueMask)
            *valueMask = mask;
        return mask == 31u;
    }

    bool HasOut(const DrainedOutcomes &out,
                const BML_BehaviorOutcomeHeader &header,
                std::string_view expected) const {
        for (std::uint32_t index = 0; index < header.OutCount; ++index) {
            BML_BehaviorOutRecord record{};
            if (!Record(out, header.OutOffset, index, record))
                return false;
            if (Bytes(out, record.NameOffset, record.NameLength) == expected)
                return true;
        }
        return false;
    }

    void CheckRuns() {
        DrainedOutcomes call;
        DrainedOutcomes start;
        DrainedOutcomes object;
        DrainedOutcomes latest;
        DrainedOutcomes queueFull;
        const bool drained = Drain(m_Call, call) && Drain(m_Start, start) &&
            Drain(m_Object, object) && Drain(m_Latest, latest) &&
            Drain(m_QueueFull, queueFull);
        if (!drained) {
            Fail("drain");
            return;
        }

        std::uint32_t callMask = 0;
        std::uint32_t startMask = 0;
        std::uint32_t objectMask = 0;
        const bool callOut = call.Headers.size() == 1 &&
            HasOut(call, call.Headers[0], "Done");
        const bool callValues = call.Headers.size() == 1 &&
            ValidateValues(call, call.Headers[0], nullptr, &callMask);
        const bool callOk = call.Headers.size() == 1 &&
            call.Headers[0].Sequence == 1 &&
            callOut && callValues;
        const bool startOut = start.Headers.size() == 2 &&
            HasOut(start, start.Headers[1], "Done");
        const bool startValues = start.Headers.size() == 2 &&
            ValidateValues(start, start.Headers[1], nullptr, &startMask);
        const bool startOk = start.Headers.size() == 2 &&
            start.Headers[0].Sequence == 1 && start.Headers[1].Sequence == 2 &&
            (start.Headers[0].Continuation &
             BML_BEHAVIOR_CONTINUATION_NATIVE) != 0 &&
            startOut && startValues;

        BML_ObjectRef captured{};
        const bool objectWire = object.Headers.size() == 1 &&
            object.Headers[0].Sequence == 1 &&
            ValidateValues(object, object.Headers[0], &captured, &objectMask) &&
            captured.Domain != 0 && HasOut(object, object.Headers[0], "Done");
        BML_SceneObjectInfo objectInfo{};
        const bool objectStale = objectWire && m_Scene &&
            m_Scene->ReadObject(captured, &objectInfo) == BML_ERROR_OBJECT_INVALID;

        const bool latestOk = latest.Headers.size() == 1 &&
            latest.Headers[0].Sequence == 3;
        const bool queueFullOk = queueFull.Headers.size() == 2 &&
            queueFull.Headers[0].Sequence == 1 &&
            queueFull.Headers[1].Sequence == 2 &&
            queueFull.Headers[1].Terminal &&
            queueFull.Headers[1].Error ==
                BML_BEHAVIOR_ERROR_OUTCOME_LIMIT_REACHED;

        GetLogger()->Info(
            "Behavior transport detail: call=%s call_count=%u call_out=%s call_values=%u start=%s start_count=%u start_out=%s start_values=%u object=%s object_count=%u object_values=%u captured=%u:%u:%u stale=%s latest=%s latest_count=%u latest_sequence=%llu queue=%s queue_count=%u queue_error=%u",
            callOk ? "true" : "false", static_cast<unsigned>(call.Headers.size()),
            callOut ? "true" : "false", callMask,
            startOk ? "true" : "false", static_cast<unsigned>(start.Headers.size()),
            startOut ? "true" : "false", startMask,
            objectWire ? "true" : "false", static_cast<unsigned>(object.Headers.size()),
            objectMask,
            captured.Domain, captured.Slot, captured.Generation,
            objectStale ? "true" : "false",
            latestOk ? "true" : "false", static_cast<unsigned>(latest.Headers.size()),
            latest.Headers.empty() ? 0ull :
                static_cast<unsigned long long>(latest.Headers[0].Sequence),
            queueFullOk ? "true" : "false",
            static_cast<unsigned>(queueFull.Headers.size()),
            queueFull.Headers.size() < 2 ? 0u : queueFull.Headers[1].Error);

        m_TransportPassed = callOk && startOk && objectWire && latestOk &&
            queueFullOk && m_SessionOpenedBeforeLevel && m_CatalogPassed;
        m_WirePassed = call.NonConsumingSizeQuery && start.NonConsumingSizeQuery &&
            object.NonConsumingSizeQuery && latest.NonConsumingSizeQuery &&
            queueFull.NonConsumingSizeQuery;
        m_ObjectRefPassed = objectStale;
        if (!m_TransportPassed)
            Fail("transport");
        else if (!m_WirePassed)
            Fail("wire");
        else if (!m_ObjectRefPassed)
            Fail("object-ref");
        else
            Finish(true, "complete");
    }

    void CloseRuns() {
        if (!m_Behavior)
            return;
        for (BML_BehaviorRun *run : {&m_Call, &m_Start, &m_Object,
                                     &m_Latest, &m_QueueFull}) {
            if (*run) {
                m_Behavior->CloseRun(*run);
                *run = nullptr;
            }
        }
    }

    void Fail(const char *reason) {
        Finish(false, reason);
    }

    void Finish(bool passed, const char *reason) {
        if (m_Done)
            return;
        m_Done = true;
        m_Passed = passed;
        GetLogger()->Info(
            "Behavior transport: status=%s reason=%s transport=%s wire=%s object_ref=%s session_after_reset=%s catalog=%s",
            passed ? "pass" : "fail", reason,
            m_TransportPassed ? "true" : "false",
            m_WirePassed ? "true" : "false",
            m_ObjectRefPassed ? "true" : "false",
            m_SessionOpenedBeforeLevel ? "true" : "false",
            m_CatalogPassed ? "true" : "false");
        CloseRuns();
    }

    const BML_BehaviorInterface *m_Behavior = nullptr;
    const BML_SceneInterface *m_Scene = nullptr;
    BML_BehaviorSession m_Session = nullptr;
    BML_BehaviorPrototypeRef m_Prototype = Dto<BML_BehaviorPrototypeRef>();
    BML_BehaviorRun m_Call = nullptr;
    BML_BehaviorRun m_Start = nullptr;
    BML_BehaviorRun m_Object = nullptr;
    BML_BehaviorRun m_Latest = nullptr;
    BML_BehaviorRun m_QueueFull = nullptr;
    int m_LevelFrames = 0;
    bool m_LevelStarted = false;
    bool m_SessionOpenedBeforeLevel = false;
    bool m_TransportPassed = false;
    bool m_WirePassed = false;
    bool m_ObjectRefPassed = false;
    bool m_CatalogPassed = false;
    bool m_Done = false;
    bool m_Passed = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorTransportTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
