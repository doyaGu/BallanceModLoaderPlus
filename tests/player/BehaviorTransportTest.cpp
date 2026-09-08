#include <BML/Behavior.hpp>
#include <BML/Guids/Logics.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Scene.h>

#include "BehaviorTransportFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <bit>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "PlayerProbe.h"

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

bool Same(BML_BehaviorGuid left, CKGUID right) {
    return left.Data1 == static_cast<std::uint32_t>(right.d1) &&
           left.Data2 == static_cast<std::uint32_t>(right.d2);
}

bool Same(BML_ObjectRef left, BML_ObjectRef right) {
    return left.Domain == right.Domain && left.Slot == right.Slot &&
           left.Generation == right.Generation;
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

BML_BehaviorSelector Occurrence(const char *name, int occurrence) {
    BML_BehaviorSelector selector = Dto<BML_BehaviorSelector>();
    selector.Kind = BML_BEHAVIOR_SELECTOR_NAME;
    selector.Name = Text(name);
    selector.Occurrence = occurrence;
    return selector;
}

BML_BehaviorSelector Indexed(int index) {
    BML_BehaviorSelector selector = Dto<BML_BehaviorSelector>();
    selector.Kind = BML_BEHAVIOR_SELECTOR_INDEX;
    selector.Index = index;
    return selector;
}

BML_BehaviorValue Literal(BML_BehaviorGuid type, std::uint32_t kind) {
    BML_BehaviorValue value = Dto<BML_BehaviorValue>();
    value.Type = type;
    value.Kind = kind;
    return value;
}

BML_BehaviorBinding Bind(BML_BehaviorSelector slot,
                         BML_BehaviorValue value) {
    BML_BehaviorBinding binding = Dto<BML_BehaviorBinding>();
    binding.Slot = slot;
    binding.Value = value;
    return binding;
}

struct BlockArguments {
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    BML_BehaviorSettingStage Stage = Dto<BML_BehaviorSettingStage>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();
    BML_BehaviorFramePolicy Frames = Dto<BML_BehaviorFramePolicy>();

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
        Frames.Kind = retention;
        Frames.Limit = limit;
        Frames.Flags = BML_BEHAVIOR_FRAME_POLICY_POUTS;
        Block.PrototypeGeneration = generation;
    }
};

struct EchoBlockArguments {
    std::array<BML_BehaviorBinding, 13> Pins{};
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    BML_BehaviorSettingStage Stage = Dto<BML_BehaviorSettingStage>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();
    BML_BehaviorFramePolicy Frames = Dto<BML_BehaviorFramePolicy>();

    explicit EchoBlockArguments(BML_ObjectRef object,
                                std::uint64_t generation) {
        Retry = Bind(Named("Retry"), Literal(Guid(CKPGUID_BOOL),
                                             BML_BEHAVIOR_VALUE_BOOL));
        Retry.Value.Data.Bool = 0;
        Stage.Settings = &Retry;
        Stage.SettingCount = 1;

        Pins[0] = Bind(Named("Bool"),
                       Literal(Guid(CKPGUID_BOOL), BML_BEHAVIOR_VALUE_BOOL));
        Pins[0].Value.Data.Bool = 0;
        Pins[1] = Bind(Occurrence("Number", 0),
                       Literal(Guid(CKPGUID_INT), BML_BEHAVIOR_VALUE_INT32));
        Pins[1].Value.Data.Int32 = -1234567;
        Pins[2] = Bind(Occurrence("Number", 1),
                       Literal(Guid(CKPGUID_FLOAT), BML_BEHAVIOR_VALUE_FLOAT32));
        Pins[2].Value.Data.Float32 = -2.25f;
        Pins[3] = Bind(Named("Text"),
                       Literal(Guid(CKPGUID_STRING), BML_BEHAVIOR_VALUE_UTF8));
        Pins[3].Value.Data.Utf8 = Text("authored");
        Pins[4] = Bind(Named("Vec2"),
                       Literal(Guid(CKPGUID_2DVECTOR), BML_BEHAVIOR_VALUE_VEC2));
        Pins[4].Value.Data.Vec2 = {-4.25f, 5.5f};
        Pins[5] = Bind(Named("Vector"),
                       Literal(Guid(CKPGUID_VECTOR), BML_BEHAVIOR_VALUE_VEC3));
        Pins[5].Value.Data.Vec3 = {6.25f, -7.5f, 8.75f};
        Pins[6] = Bind(Named("Quaternion"),
                       Literal(Guid(CKPGUID_QUATERNION),
                               BML_BEHAVIOR_VALUE_QUATERNION));
        Pins[6].Value.Data.Quaternion = {-0.1f, 0.2f, -0.3f, 0.4f};
        Pins[7] = Bind(Named("Euler"),
                       Literal(Guid(CKPGUID_EULERANGLES), BML_BEHAVIOR_VALUE_EULER));
        Pins[7].Value.Data.Euler = {0.9f, -0.8f, 0.7f};
        Pins[8] = Bind(Named("Rect"),
                       Literal(Guid(CKPGUID_RECT), BML_BEHAVIOR_VALUE_RECT));
        Pins[8].Value.Data.Rect = {-1.0f, -2.0f, 3.0f, 4.0f};
        Pins[9] = Bind(Named("Color"),
                       Literal(Guid(CKPGUID_COLOR), BML_BEHAVIOR_VALUE_COLOR));
        Pins[9].Value.Data.Color = {0.11f, 0.22f, 0.33f, 0.44f};
        Pins[10] = Bind(Named("Box"),
                        Literal(Guid(CKPGUID_BOX), BML_BEHAVIOR_VALUE_BOX));
        Pins[10].Value.Data.Box = {{-9.0f, -8.0f, -7.0f},
                                   {7.0f, 8.0f, 9.0f}};
        Pins[11] = Bind(Named("Matrix"),
                        Literal(Guid(CKPGUID_MATRIX), BML_BEHAVIOR_VALUE_MAT4));
        Pins[11].Value.Data.Mat4 = {
            0.25f, 1.25f, 2.25f, 3.25f,
            4.25f, 5.25f, 6.25f, 7.25f,
            8.25f, 9.25f, 10.25f, 11.25f,
            12.25f, 13.25f, 14.25f, 15.25f};
        Pins[12] = Bind(Named("Object"),
                        Literal(Guid(CKPGUID_BEOBJECT), BML_BEHAVIOR_VALUE_OBJECT));
        Pins[12].Value.Data.Object = object;

        Block.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
        Block.Target = Dto<BML_BehaviorTarget>();
        Block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        Block.SettingStages = &Stage;
        Block.SettingStageCount = 1;
        Block.Pins = Pins.data();
        Block.PinCount = static_cast<std::uint32_t>(Pins.size());
        Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        Frames.Limit = 64;
        Frames.Flags = BML_BEHAVIOR_FRAME_POLICY_POUTS;
        Block.PrototypeGeneration = generation;
    }
};

struct DynamicBlockArguments {
    BML_BehaviorBinding Extended = Dto<BML_BehaviorBinding>();
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    std::array<BML_BehaviorSettingStage, 2> Stages{};
    BML_BehaviorBinding DynamicPin = Dto<BML_BehaviorBinding>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();
    BML_BehaviorFramePolicy Frames = Dto<BML_BehaviorFramePolicy>();

    explicit DynamicBlockArguments(std::uint64_t generation) {
        Extended = Bind(Named("Extended Layout"),
                        Literal(Guid(CKPGUID_BOOL), BML_BEHAVIOR_VALUE_BOOL));
        Extended.Value.Data.Bool = 1;
        Retry = Bind(Named("Retry"),
                     Literal(Guid(CKPGUID_BOOL), BML_BEHAVIOR_VALUE_BOOL));
        Retry.Value.Data.Bool = 0;
        Stages[0] = Dto<BML_BehaviorSettingStage>();
        Stages[0].Settings = &Extended;
        Stages[0].SettingCount = 1;
        Stages[1] = Dto<BML_BehaviorSettingStage>();
        Stages[1].Settings = &Retry;
        Stages[1].SettingCount = 1;
        DynamicPin = Bind(
            Named("Dynamic Value"),
            Literal(Guid(CKPGUID_INT), BML_BEHAVIOR_VALUE_INT32));
        DynamicPin.Value.Data.Int32 = 713;

        Block.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
        Block.Target = Dto<BML_BehaviorTarget>();
        Block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        Block.SettingStages = Stages.data();
        Block.SettingStageCount = static_cast<std::uint32_t>(Stages.size());
        Block.Pins = &DynamicPin;
        Block.PinCount = 1;
        Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        Frames.Limit = 64;
        Frames.Flags = BML_BEHAVIOR_FRAME_POLICY_POUTS;
        Block.PrototypeGeneration = generation;
    }
};

std::uint32_t Load32(const std::uint8_t *data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

struct TakenFrames {
    std::vector<BML_BehaviorRunFrame> Headers;
    std::vector<std::uint8_t> Payload;
    bool NonConsumingSizeQuery = false;
};

struct PrototypeBatch {
    std::vector<BML_BehaviorPrototypeInfo> Records;
    std::vector<std::uint8_t> Payload;
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
        BML::PlayerTest::ProbeReport::Reset();
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
                           CloseWatch)) {
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
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKObject *object = context ? context->CreateObject(
            CKCID_BEOBJECT, "__BML_BehaviorAuthoring_Input",
            CK_OBJECTCREATION_DYNAMIC) : nullptr;
        if (!object || !m_Scene ||
            m_Scene->FindObject("__BML_BehaviorAuthoring_Input",
                                &m_InputObjectRef) != BML_OK ||
            !m_InputObjectRef.Domain) {
            if (context && object)
                context->DestroyObject(object);
            Fail("input-object");
            return;
        }
        m_InputObjectId = object->GetID();
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
            ChangeGraphChildTarget();
        else if (m_LevelFrames == 7)
            CheckRuns();
        else if (m_ObjectCloseRequested && m_LevelFrames >= 8)
            CheckCapturedObject();
    }

    void OnUnload() override {
        CloseRuns();
        DestroyInputObject();
        if (m_Behavior && m_Session)
            m_Behavior->CloseSession(m_Session);
        m_Session = nullptr;
    }

    void DestroyInputObject() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKObject *object = context && m_InputObjectId
            ? context->GetObject(m_InputObjectId) : nullptr;
        if (context && object)
            context->DestroyObject(object);
        m_InputObjectId = 0;
        m_InputObjectRef = {};
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

    bool Find(const BML_BehaviorPrototypeQuery &query,
              PrototypeBatch &batch) const {
        batch = {};
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t count = 0;
        std::uint32_t payloadSize = 0;
        const int measured = m_Behavior->FindPrototypes(
            m_Session, &query, nullptr, 0,
            sizeof(BML_BehaviorPrototypeInfo), nullptr, 0, &count,
            &payloadSize, &status);
        if (measured != (count || payloadSize
                            ? BML_ERROR_BUFFER_TOO_SMALL : BML_OK))
            return false;
        batch.Records.resize(count);
        batch.Payload.resize(payloadSize);
        std::uint32_t actualCount = 0;
        std::uint32_t actualPayloadSize = 0;
        status = Dto<BML_BehaviorStatus>();
        const int read = m_Behavior->FindPrototypes(
            m_Session, &query,
            batch.Records.empty() ? nullptr : batch.Records.data(), count,
            sizeof(BML_BehaviorPrototypeInfo),
            batch.Payload.empty() ? nullptr : batch.Payload.data(),
            payloadSize, &actualCount, &actualPayloadSize, &status);
        if (read != BML_OK || actualCount != count ||
            actualPayloadSize != payloadSize)
            return false;
        for (const BML_BehaviorPrototypeInfo &record : batch.Records) {
            if (record.StructSize < sizeof(record) ||
                record.Ref.StructSize < sizeof(record.Ref))
                return false;
        }
        return true;
    }

    bool FindOne(CKGUID guid, BML_BehaviorPrototypeRef &prototype,
                 std::string_view expectedName = {}) const {
        BML_BehaviorPrototypeQuery query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROTOTYPE;
        query.Prototype = Guid(guid);
        PrototypeBatch batch;
        if (!Find(query, batch) || batch.Records.size() != 1)
            return false;
        const BML_BehaviorPrototypeInfo &record = batch.Records.front();
        if (!expectedName.empty() && Bytes(batch.Payload, record.Name) != expectedName)
            return false;
        prototype = record.Ref;
        return prototype.Generation != 0;
    }

    bool HasLayoutSlot(const BML_BehaviorLayout &layout,
                       const std::vector<std::uint8_t> &payload,
                       std::uint32_t kind, std::string_view name,
                       int occurrence, std::uint32_t valueKind = 0,
                       std::uint32_t requiredFlags = 0) const {
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
                (!valueKind || slot.ValueKind == valueKind) &&
                (slot.Flags & requiredFlags) == requiredFlags)
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
            layout.Kind == BML_BEHAVIOR_KIND_FUNCTION &&
            Bytes(payload, layout.Name) == "BML Behavior Transport Fixture" &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_IN, "Run", 0) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_OUT, "Done", 0) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_SETTING,
                          "Retry", 0, BML_BEHAVIOR_VALUE_BOOL) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_SETTING,
                          "Extended Layout", 0, BML_BEHAVIOR_VALUE_BOOL) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Bool", 0, BML_BEHAVIOR_VALUE_BOOL) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Number", 0, BML_BEHAVIOR_VALUE_INT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Number", 1, BML_BEHAVIOR_VALUE_FLOAT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Text", 0, BML_BEHAVIOR_VALUE_UTF8) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Vec2", 0, BML_BEHAVIOR_VALUE_VEC2) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Vector", 0, BML_BEHAVIOR_VALUE_VEC3) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Quaternion", 0,
                          BML_BEHAVIOR_VALUE_QUATERNION) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Euler", 0, BML_BEHAVIOR_VALUE_EULER) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Rect", 0, BML_BEHAVIOR_VALUE_RECT) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Color", 0, BML_BEHAVIOR_VALUE_COLOR) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Box", 0, BML_BEHAVIOR_VALUE_BOX) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Matrix", 0, BML_BEHAVIOR_VALUE_MAT4) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Object", 0, BML_BEHAVIOR_VALUE_OBJECT) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Bool", 0, BML_BEHAVIOR_VALUE_BOOL) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Number", 0, BML_BEHAVIOR_VALUE_INT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Number", 1, BML_BEHAVIOR_VALUE_FLOAT32) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Text", 0, BML_BEHAVIOR_VALUE_UTF8) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Vec2", 0, BML_BEHAVIOR_VALUE_VEC2) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Vector", 0, BML_BEHAVIOR_VALUE_VEC3) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Quaternion", 0,
                          BML_BEHAVIOR_VALUE_QUATERNION) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Euler", 0, BML_BEHAVIOR_VALUE_EULER) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Rect", 0, BML_BEHAVIOR_VALUE_RECT) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Color", 0, BML_BEHAVIOR_VALUE_COLOR) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Box", 0, BML_BEHAVIOR_VALUE_BOX) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Matrix", 0, BML_BEHAVIOR_VALUE_MAT4) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Object", 0, BML_BEHAVIOR_VALUE_OBJECT) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Target", 0, BML_BEHAVIOR_VALUE_OBJECT) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_LOCAL,
                          "Executions", 0, BML_BEHAVIOR_VALUE_INT32) &&
            (origin == BML_BEHAVIOR_LAYOUT_LIVE ||
             HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_TARGET,
                           "Target", 0, BML_BEHAVIOR_VALUE_OBJECT));
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
        const bool valid = result == BML_OK && layout.LayoutGeneration != 0 &&
            ValidateLayout(layout, payload, BML_BEHAVIOR_LAYOUT_LIVE);
        if (!valid)
            LogLayout("live", layout, payload, result, status);
        return valid;
    }

    void LogLayout(const char *label, const BML_BehaviorLayout &layout,
                   const std::vector<std::uint8_t> &payload, int result,
                   const BML_BehaviorStatus &status) {
        GetLogger()->Error(
            "Behavior %s layout invalid: result=%d error=%u phase=%u origin=%u kind=%u generation=%llu slots=%u name=%.*s",
            label, result, status.Error, status.Phase, layout.Origin,
            layout.Kind, static_cast<unsigned long long>(layout.LayoutGeneration),
            layout.SlotCount, static_cast<int>(Bytes(payload, layout.Name).size()),
            Bytes(payload, layout.Name).data());
        for (std::uint32_t index = 0; index < layout.SlotCount; ++index) {
            const std::uint64_t offset =
                static_cast<std::uint64_t>(layout.SlotOffset) +
                static_cast<std::uint64_t>(index) *
                    sizeof(BML_BehaviorSlotRecord);
            if (offset + sizeof(BML_BehaviorSlotRecord) > payload.size())
                break;
            BML_BehaviorSlotRecord slot{};
            std::memcpy(&slot, payload.data() + offset, sizeof(slot));
            const std::string_view name = Bytes(payload, slot.Name);
            GetLogger()->Error(
                "Behavior %s slot: kind=%u flags=%u index=%d occurrence=%d value=%u name=%.*s",
                label, slot.Kind, slot.Flags, slot.Index, slot.Occurrence,
                slot.ValueKind, static_cast<int>(name.size()), name.data());
        }
    }

    bool ReadDynamicLiveLayout(BML_BehaviorRun run) {
        BML_BehaviorLayout layout = Dto<BML_BehaviorLayout>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        if (m_Behavior->ReadLiveLayout(run, &layout, nullptr, 0,
                                       &payloadSize, &status) !=
                BML_ERROR_BUFFER_TOO_SMALL ||
            !payloadSize)
            return false;
        std::vector<std::uint8_t> payload(payloadSize);
        layout = Dto<BML_BehaviorLayout>();
        status = Dto<BML_BehaviorStatus>();
        if (m_Behavior->ReadLiveLayout(
                run, &layout, payload.data(), payloadSize, &payloadSize,
                &status) != BML_OK ||
            !ValidateLayout(layout, payload, BML_BEHAVIOR_LAYOUT_LIVE))
            return false;
        const std::uint32_t dynamic = BML_BEHAVIOR_SLOT_DYNAMIC;
        return HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_IN,
                             "Dynamic Run", 0, 0, dynamic) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_OUT,
                          "Dynamic Done", 0, 0, dynamic) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_PIN,
                          "Dynamic Value", 0, BML_BEHAVIOR_VALUE_INT32,
                          dynamic | BML_BEHAVIOR_SLOT_VALUE_SUPPORTED) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_POUT,
                          "Dynamic Value", 0, BML_BEHAVIOR_VALUE_INT32,
                          dynamic | BML_BEHAVIOR_SLOT_VALUE_SUPPORTED);
    }

    bool ReadGraphLayout() {
        BML_BehaviorLayout layout = Dto<BML_BehaviorLayout>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        if (m_Behavior->ReadDeclaredLayout(
                m_Session, &m_GraphPrototype, &layout, nullptr, 0,
                &payloadSize, &status) != BML_ERROR_BUFFER_TOO_SMALL ||
            !payloadSize)
            return false;
        std::vector<std::uint8_t> payload(payloadSize);
        layout = Dto<BML_BehaviorLayout>();
        status = Dto<BML_BehaviorStatus>();
        if (m_Behavior->ReadDeclaredLayout(
                m_Session, &m_GraphPrototype, &layout, payload.data(),
                payloadSize, &payloadSize, &status) != BML_OK)
            return false;
        if (layout.Origin != BML_BEHAVIOR_LAYOUT_DECLARED ||
            layout.Kind != BML_BEHAVIOR_KIND_CALLBACK ||
            layout.CompatibleClass != CKCID_3DENTITY ||
            !Same(layout.Prototype.Prototype,
                  BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID) ||
            layout.Prototype.Generation != m_GraphPrototype.Generation ||
            Bytes(payload, layout.Name) != "BML Behavior Graph Fixture" ||
            Bytes(payload, layout.Category) != "BML/Test/Graph" ||
            !HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_IN,
                           "Enter", 0) ||
            !HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_OUT,
                           "Exit", 0) ||
            layout.ManagerCount != 1 ||
            static_cast<std::uint64_t>(layout.ManagerOffset) +
                    sizeof(BML_BehaviorManagerInfo) > payload.size())
            return false;
        BML_BehaviorManagerInfo manager{};
        std::memcpy(&manager, payload.data() + layout.ManagerOffset,
                    sizeof(manager));
        return manager.StructSize >= sizeof(manager) &&
            Same(manager.Guid, TIME_MANAGER_GUID) && manager.Available == 1;
    }

    bool ReadGraphLiveLayout(BML_BehaviorRun run) {
        BML_BehaviorLayout layout = Dto<BML_BehaviorLayout>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        if (m_Behavior->ReadLiveLayout(run, &layout, nullptr, 0,
                                       &payloadSize, &status) !=
                BML_ERROR_BUFFER_TOO_SMALL ||
            !payloadSize)
            return false;
        std::vector<std::uint8_t> payload(payloadSize);
        layout = Dto<BML_BehaviorLayout>();
        status = Dto<BML_BehaviorStatus>();
        return m_Behavior->ReadLiveLayout(
                   run, &layout, payload.data(), payloadSize, &payloadSize,
                   &status) == BML_OK &&
            layout.Origin == BML_BEHAVIOR_LAYOUT_LIVE &&
            layout.Kind == BML_BEHAVIOR_KIND_GRAPH &&
            layout.LayoutGeneration != 0 &&
            layout.Prototype.Generation == m_GraphPrototype.Generation &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_IN, "Enter", 0) &&
            HasLayoutSlot(layout, payload, BML_BEHAVIOR_SLOT_OUT, "Exit", 0);
    }

    bool ValidateCatalogQueries() {
        BML_BehaviorPrototypeQuery query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROVIDER |
                      BML_BEHAVIOR_MATCH_PROVIDER_GUID;
        query.Provider = Text("BEHAVIORTRANSPORTFIXTURE");
        query.ProviderGuid = Guid(BML_BEHAVIOR_TRANSPORT_PLUGIN_GUID);
        PrototypeBatch provider;
        if (!Find(query, provider) || provider.Records.size() != 2 ||
            !Same(provider.Records[0].Ref.Prototype,
                  BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID) ||
            !Same(provider.Records[1].Ref.Prototype,
                  BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID) ||
            Bytes(provider.Payload, provider.Records[0].ProviderName) !=
                "BehaviorTransportFixture" ||
            Bytes(provider.Payload, provider.Records[1].ProviderName) !=
                "BehaviorTransportFixture")
            return false;

        query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_NAME |
                      BML_BEHAVIOR_MATCH_CATEGORY;
        query.Name = Text("BML Behavior Graph Fixture");
        query.Category = Text("BML/Test/Graph");
        PrototypeBatch graph;
        if (!Find(query, graph) || graph.Records.size() != 1 ||
            !Same(graph.Records[0].Ref.Prototype,
                  BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID))
            return false;
        m_GraphPrototype = graph.Records[0].Ref;

        const BML_BehaviorGuid timeManager = Guid(TIME_MANAGER_GUID);
        query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROVIDER_GUID |
                      BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS;
        query.ProviderGuid = Guid(BML_BEHAVIOR_TRANSPORT_PLUGIN_GUID);
        query.RequiredManagers = &timeManager;
        query.RequiredManagerCount = 1;
        PrototypeBatch managers;
        if (!Find(query, managers) || managers.Records.size() != 1 ||
            !Same(managers.Records[0].Ref.Prototype,
                  BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID))
            return false;

        query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROTOTYPE |
                      BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS;
        query.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID);
        query.CompatibleClass = CKCID_3DOBJECT;
        PrototypeBatch compatible;
        if (!Find(query, compatible) || compatible.Records.size() != 1)
            return false;

        query = Dto<BML_BehaviorPrototypeQuery>();
        query.Match = BML_BEHAVIOR_MATCH_PROTOTYPE;
        query.Prototype = {0x7fffffffu, 0x13572468u};
        PrototypeBatch absent;
        if (!Find(query, absent) || !absent.Records.empty() ||
            !absent.Payload.empty())
            return false;

        if (!FindOne(VT_LOGICS_WAITFORALL, m_WaitForAllPrototype))
            return false;

        BML_BehaviorPrototypeRef stale = m_Prototype;
        ++stale.Generation;
        BML_BehaviorLayout untouched = Dto<BML_BehaviorLayout>();
        untouched.Origin = 0x7fffffffu;
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t payloadSize = 0;
        const int staleResult = m_Behavior->ReadDeclaredLayout(
            m_Session, &stale, &untouched, nullptr, 0, &payloadSize,
            &status);
        return staleResult == BML_ERROR_FAIL &&
            status.Error == BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED &&
            status.Phase == BML_BEHAVIOR_PHASE_PROTOTYPE &&
            untouched.Origin == 0x7fffffffu && ReadGraphLayout();
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
        m_CatalogPassed = ValidateCatalogQueries() &&
            ReadDeclaredLayout() && RejectProviderOwnedValue() &&
            RejectStaleProviderRun() && RejectMismatchedValueKind();
        return m_CatalogPassed;
    }

    bool RejectProviderOwnedValue() {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        arguments.Retry.Value.Type = Guid(BML_BEHAVIOR_PROVIDER_VALUE_GUID);
        arguments.Retry.Value.Kind = BML_BEHAVIOR_VALUE_INT32;
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &arguments.Frames, &input,
            &run, &info, &status);
        return result == BML_ERROR_INVALID_PARAMETER && !run &&
            status.Error == BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED &&
            status.Phase == BML_BEHAVIOR_PHASE_BINDING;
    }

    bool RejectStaleProviderRun() {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation + 1);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &arguments.Frames, &input,
            &run, &info, &status);
        return result == BML_ERROR_FAIL && !run &&
            status.Error == BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED &&
            status.Phase == BML_BEHAVIOR_PHASE_PROTOTYPE;
    }

    bool RejectMismatchedValueKind() {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        arguments.Retry.Value.Kind = BML_BEHAVIOR_VALUE_INT32;
        arguments.Retry.Value.Data.Int32 = 1;
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &arguments.Frames, &input,
            &run, &info, &status);
        return result == BML_ERROR_INVALID_PARAMETER && !run &&
            status.Error == BML_BEHAVIOR_ERROR_TYPE_MISMATCH;
    }

    bool OpenCall(BML_BehaviorRun &run, const char *inputName = "Run") {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named(inputName);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenStart(BML_BehaviorRun &run, std::uint32_t limit) {
        BlockArguments arguments(true, BML_BEHAVIOR_FRAMES_EACH_FRAME,
                                 limit, m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Start(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenPendingCall(BML_BehaviorRun &run) {
        BlockArguments arguments(true, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run &&
            info.Kind == BML_BEHAVIOR_RUN_CALL &&
            info.State == BML_BEHAVIOR_RUN_PENDING;
    }

    bool OpenInstance(BML_BehaviorRun &run, std::uint32_t retention,
                      std::uint32_t limit) {
        BlockArguments arguments(false, retention, limit,
                                 m_Prototype.Generation);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Spawn(
                   m_Session, {}, &arguments.Block, &arguments.Frames,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenEcho(BML_BehaviorRun &run) {
        EchoBlockArguments arguments(m_InputObjectRef,
                                     m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Echo");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenDynamic(BML_BehaviorRun &run) {
        DynamicBlockArguments arguments(m_Prototype.Generation);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Spawn(
                   m_Session, {}, &arguments.Block, &arguments.Frames,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenTarget(BML_BehaviorRun &run, std::uint32_t targetKind,
                    BML_ObjectRef owner, BML_ObjectRef target) {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        arguments.Block.Target.Kind = targetKind;
        arguments.Block.Target.Type = Guid(CKPGUID_BEOBJECT);
        arguments.Block.Target.Object = target;
        BML_BehaviorSelector input = Named("Read Target");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, owner, &arguments.Block, &arguments.Frames,
                   &input, &run, &info, &status) == BML_OK && run;
    }

    bool RejectAmbiguousSelector() {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Duplicate");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &arguments.Frames, &input,
            &run, &info, &status);
        return result == BML_ERROR_FAIL && !run &&
            status.Error == BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS &&
            status.Phase == BML_BEHAVIOR_PHASE_EXECUTION;
    }

    bool OpenDuplicateOccurrence(BML_BehaviorRun &run) {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Occurrence("Duplicate", 1);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenIndexed(BML_BehaviorRun &run) {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Indexed(0);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(
                   m_Session, {}, &arguments.Block, &arguments.Frames, &input,
                   &run, &info, &status) == BML_OK && run;
    }

    bool OpenWaitForAll() {
        BML_BehaviorBlock block = Dto<BML_BehaviorBlock>();
        block.Prototype = Guid(VT_LOGICS_WAITFORALL);
        block.Target = Dto<BML_BehaviorTarget>();
        block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        block.PrototypeGeneration = m_WaitForAllPrototype.Generation;
        BML_BehaviorFramePolicy frames = Dto<BML_BehaviorFramePolicy>();
        frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        frames.Limit = 64;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        if (m_Behavior->Spawn(m_Session, {}, &block, &frames, &m_WaitForAll,
                              &info, &status) != BML_OK || !m_WaitForAll)
            return false;
        return Pulse(m_WaitForAll, "In 0",
                     BML_BEHAVIOR_ADMISSION_EXECUTED) &&
            Pulse(m_WaitForAll, "In 1", BML_BEHAVIOR_ADMISSION_QUEUED);
    }

    bool OpenGraph() {
        BML_BehaviorBlock block = Dto<BML_BehaviorBlock>();
        block.Prototype = Guid(BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID);
        block.Target = Dto<BML_BehaviorTarget>();
        block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        block.PrototypeGeneration = m_GraphPrototype.Generation;
        BML_BehaviorFramePolicy frames = Dto<BML_BehaviorFramePolicy>();
        frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        frames.Limit = 64;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int spawned = m_Behavior->Spawn(
            m_Session, {}, &block, &frames, &m_Graph, &info, &status);
        if (spawned != BML_OK || !m_Graph) {
            const HMODULE fixture = GetModuleHandleA(
                "BehaviorTransportFixture.dll");
            const auto readTrace = fixture
                ? reinterpret_cast<BML_BehaviorTransportReadGraphTrace>(
                      GetProcAddress(fixture,
                                     "BMLBehaviorTransportReadGraphTrace"))
                : nullptr;
            const BML_BehaviorTransportGraphTrace *trace = readTrace
                ? readTrace() : nullptr;
            GetLogger()->Error(
                "Behavior graph spawn failed: result=%d error=%u phase=%u ck=%d native=%d graph_stage=%d graph_error=%d message=%s",
                spawned, status.Error, status.Phase, status.CkError,
                status.NativeResult, trace ? trace->Stage : -1,
                trace ? trace->Error : CKERR_NOTINITIALIZED,
                status.Message);
            return false;
        }
        if (!ReadGraphLiveLayout(m_Graph)) {
            GetLogger()->Error("Behavior graph live layout failed");
            return false;
        }
        const bool pulsed = Pulse(
            m_Graph, "Enter", BML_BEHAVIOR_ADMISSION_EXECUTED);
        if (!pulsed)
            GetLogger()->Error("Behavior graph pulse failed");
        return pulsed;
    }

    bool InspectGameplayGraph() {
        if (!m_Scene)
            return false;
        BML_ObjectRef root{};
        if (m_Scene->FindObject("Gameplay_Events", &root) != BML_OK ||
            !root.Domain)
            return false;
        auto inspected = m_CppSession.Inspect(root);
        if (!inspected) {
            GetLogger()->Error(
                "Behavior inspect failed: result=%d error=%u message=%s",
                inspected.Code(),
                static_cast<unsigned>(inspected.GetStatus().Error),
                inspected.GetStatus().Message.c_str());
            return false;
        }
        BML::Behavior::Graph graph = inspected.Take();
        // Gameplay.nmo contributes 53 nodes and 60 links. The live graph is
        // intentionally extensible: BML and other loaded mods may append nodes
        // after the file has been loaded, so the disk image is a baseline rather
        // than the final cardinality of the Player graph.
        const BML::Behavior::Node gameplayEvents = graph.Root();
        const bool nmoShape = gameplayEvents &&
            gameplayEvents.Name() == "Gameplay_Events" &&
            graph.Nodes().size() >= 53 && graph.Links().size() >= 60;
        bool delayOne = false;
        bool delayTwo = false;
        bool portablePending = true;
        for (const BML::Behavior::Link &link : graph.Links()) {
            delayOne = delayOne || link.InitialDelay() == 1;
            delayTwo = delayTwo || link.InitialDelay() == 2;
            if (link.InitialDelay() > 0)
                portablePending = portablePending &&
                    link.Pending() == BML::Behavior::TruthValue::Unknown;
        }
        auto live = graph.Live();
        // Live contains Logical rather than equalling it. A Splice, a Tap, a
        // Before, or an After adds Loader-owned Blocks and continuation Links
        // that the Logical view deliberately hides, so the only invariant is
        // that every Node and Link an author can see is also
        // physically there.
        bool liveShape = live &&
            live->Mode() == BML::Behavior::View::Live &&
            live->Nodes().size() >= graph.Nodes().size() &&
            live->Links().size() >= graph.Links().size();
        if (liveShape) {
            std::set<std::uint64_t> liveNodes;
            for (const BML::Behavior::Node &node : live->Nodes())
                liveNodes.insert(node.Id());
            std::set<std::uint64_t> liveLinks;
            for (const BML::Behavior::Link &link : live->Links())
                liveLinks.insert(link.Id());
            for (const BML::Behavior::Node &node : graph.Nodes())
                liveShape = liveShape && liveNodes.count(node.Id()) == 1;
            for (const BML::Behavior::Link &link : graph.Links())
                liveShape = liveShape && liveLinks.count(link.Id()) == 1;
        }
        m_InspectPassed = nmoShape && delayOne && delayTwo &&
            portablePending && liveShape;
        GetLogger()->Info(
            "Behavior inspect: status=%s graph=Gameplay_Events nodes=%u links=%u template_nodes=53 template_links=60 delay_1=%s delay_2=%s pending=unknown live=%s",
            m_InspectPassed ? "pass" : "fail",
            static_cast<unsigned>(graph.Nodes().size()),
            static_cast<unsigned>(graph.Links().size()),
            delayOne ? "true" : "false", delayTwo ? "true" : "false",
            liveShape ? "true" : "false");
        return m_InspectPassed;
    }

    bool OpenGraphWatch() {
        if (!m_Scene) {
            GetLogger()->Error("Behavior watch graph failed: scene-unavailable");
            return false;
        }
        BML_ObjectRef root{};
        if (m_Scene->FindObject("__BML_BehaviorTransport_Graph", &root) !=
                BML_OK || !root.Domain) {
            GetLogger()->Error("Behavior watch graph failed: root-not-found");
            return false;
        }
        auto inspected = m_CppSession.Inspect(root);
        if (!inspected) {
            GetLogger()->Error(
                "Behavior watch graph failed: inspect result=%d error=%u message=%s",
                inspected.Code(),
                static_cast<unsigned>(inspected.GetStatus().Error),
                inspected.GetStatus().Message.c_str());
            return false;
        }
        BML::Behavior::Graph graph = inspected.Take();
        const BML::Behavior::Node rootNode = graph.Root();
        BML::Behavior::Node child;
        if (rootNode) {
            const CKGUID fixture(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
            for (const BML::Behavior::Node &node : graph.Nodes()) {
                if (node.Parent() == rootNode.Id() &&
                    node.Prototype() == fixture) {
                    child = node;
                    break;
                }
            }
        }
        if (!rootNode ||
            rootNode.Name() != "__BML_BehaviorTransport_Graph" ||
            !child || graph.Nodes().size() != 2 ||
            graph.Links().size() != 2) {
            GetLogger()->Error(
                "Behavior watch graph failed: shape root=%s child=%s nodes=%u links=%u",
                rootNode ? "true" : "false", child ? "true" : "false",
                static_cast<unsigned>(graph.Nodes().size()),
                static_cast<unsigned>(graph.Links().size()));
            return false;
        }
        bool entry = false;
        bool exit = false;
        for (const BML::Behavior::Link &link : graph.Links()) {
            entry = entry ||
                (link.Source().Node() == rootNode.Id() &&
                 link.Source().Kind() == BML::Behavior::SlotKind::In &&
                 link.Target().Node() == child.Id() &&
                 link.Target().Kind() == BML::Behavior::SlotKind::In);
            exit = exit ||
                (link.Source().Node() == child.Id() &&
                 link.Source().Kind() == BML::Behavior::SlotKind::Out &&
                 link.Target().Node() == rootNode.Id() &&
                 link.Target().Kind() == BML::Behavior::SlotKind::Out);
        }
        if (!entry || !exit) {
            GetLogger()->Error(
                "Behavior watch graph failed: boundary entry=%s exit=%s",
                entry ? "true" : "false", exit ? "true" : "false");
            return false;
        }

        const auto execution = child.Local("Executions");
        auto baseline = graph.Read(execution);
        const std::int32_t *baselineValue = baseline
            ? std::get_if<std::int32_t>(&baseline->Data) : nullptr;
        if (!baseline ||
            baseline->State != BML::Behavior::ObservationState::Available ||
            baseline->Source != BML::Behavior::Relation::Stored ||
            !baselineValue) {
            GetLogger()->Error(
                "Behavior watch graph failed: value result=%d error=%u state=%u source=%u int=%s message=%s",
                baseline.Code(),
                static_cast<unsigned>(baseline.GetStatus().Error),
                baseline ? static_cast<unsigned>(baseline->State) : 0u,
                baseline ? static_cast<unsigned>(baseline->Source) : 0u,
                baselineValue ? "true" : "false",
                baseline.GetStatus().Message.c_str());
            return false;
        }
        m_WatchBaseline = *baselineValue;
        BML_SceneObjectInfo childInfo{};
        if (m_Scene->ReadObject(child.Object(), &childInfo) != BML_OK ||
            childInfo.Id == 0) {
            GetLogger()->Error("Behavior watch graph failed: child-identity");
            return false;
        }
        m_WatchNodeId = static_cast<CK_ID>(childInfo.Id);

        auto watched = graph.Watch(
            BML::Behavior::Sampled(execution),
            [this](const BML::Behavior::Change &change) {
                const auto *previous = std::get_if<std::int32_t>(
                    &change.PreviousValue.Data);
                const auto *current = std::get_if<std::int32_t>(
                    &change.CurrentValue.Data);
                const bool valid = change.Kind ==
                        BML::Behavior::ChangeKind::SampledValue &&
                    change.Sequence == m_WatchEventCount + 1 && previous &&
                    current &&
                    *previous == m_WatchBaseline +
                        static_cast<std::int32_t>(m_WatchEventCount) &&
                    *current == *previous + 1;
                m_WatchPassed = m_WatchEventCount == 0
                    ? valid : m_WatchPassed && valid;
                ++m_WatchEventCount;
                GetLogger()->Info(
                    "Behavior watch event: sequence=%llu kind=%u previous=%d current=%d baseline=%d status=%s",
                    static_cast<unsigned long long>(change.Sequence),
                    static_cast<unsigned>(change.Kind),
                    previous ? *previous : -1, current ? *current : -1,
                    m_WatchBaseline, valid ? "pass" : "fail");
            });
        auto failedWatch = graph.Watch(
            BML::Behavior::Sampled(execution),
            [](const BML::Behavior::Change &) {
                throw std::runtime_error("player watch failure");
            });
        auto layoutWatch = graph.Watch(
            BML::Behavior::LayoutChanged(child),
            [this](const BML::Behavior::Change &change) {
                const bool valid = change.Kind ==
                        BML::Behavior::ChangeKind::Layout &&
                    change.Sequence == m_LayoutWatchEventCount + 1 &&
                    change.Before != change.After;
                m_LayoutWatchPassed = m_LayoutWatchEventCount == 0
                    ? valid : m_LayoutWatchPassed && valid;
                ++m_LayoutWatchEventCount;
                GetLogger()->Info(
                    "Behavior layout watch event: sequence=%llu kind=%u before=%llu after=%llu status=%s",
                    static_cast<unsigned long long>(change.Sequence),
                    static_cast<unsigned>(change.Kind),
                    static_cast<unsigned long long>(change.Before),
                    static_cast<unsigned long long>(change.After),
                    valid ? "pass" : "fail");
            });
        if (!watched || !failedWatch || !layoutWatch) {
            GetLogger()->Error(
                "Behavior watch graph failed: sampled=%d sampled_error=%u failure_watch=%d failure_error=%u layout_watch=%d layout_error=%u",
                watched.Code(),
                static_cast<unsigned>(watched.GetStatus().Error),
                failedWatch.Code(),
                static_cast<unsigned>(failedWatch.GetStatus().Error),
                layoutWatch.Code(),
                static_cast<unsigned>(layoutWatch.GetStatus().Error));
            return false;
        }
        m_CppWatch.emplace(watched.Take());
        m_CppFailedWatch.emplace(failedWatch.Take());
        m_CppLayoutWatch.emplace(layoutWatch.Take());
        m_GraphShapePassed = true;
        return true;
    }

    // The Target slot belongs to the live Layout, and the Layout fingerprint
    // hashes both its presence and its type, so adding it has to reach the
    // Layout Watch. Only CKBehavior::UseTarget adds or removes that slot: the
    // published seam binds an existing Target and never creates one. The graph
    // is deliberately not pulsed again after this frame, so the new Target
    // stays unbound and the child never executes without a target.
    void ChangeGraphChildTarget() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKObject *object = context && m_WatchNodeId
            ? context->GetObject(m_WatchNodeId) : nullptr;
        if (!object || !CKIsChildClassOf(object, CKCID_BEHAVIOR)) {
            GetLogger()->Error(
                "Behavior layout target failed: node id=%d resolved=%s",
                static_cast<int>(m_WatchNodeId), object ? "true" : "false");
            Fail("layout-target-node");
            return;
        }
        auto *child = static_cast<CKBehavior *>(object);
        if (child->IsUsingTarget() || !child->IsTargetable()) {
            GetLogger()->Error(
                "Behavior layout target failed: state targetable=%s using=%s",
                child->IsTargetable() ? "true" : "false",
                child->IsUsingTarget() ? "true" : "false");
            Fail("layout-target-state");
            return;
        }
        const CKERROR used = child->UseTarget(TRUE);
        CKParameterIn *target = child->GetTargetParameter();
        if (used != CK_OK || !target) {
            GetLogger()->Error(
                "Behavior layout target failed: use ck=%d target=%s",
                static_cast<int>(used), target ? "true" : "false");
            Fail("layout-target-change");
            return;
        }
        m_LayoutTargetChanged = true;
        GetLogger()->Info(
            "Behavior layout target: changed=true using=%s type=%08x%08x",
            child->IsUsingTarget() ? "true" : "false",
            static_cast<unsigned>(target->GetGUID().d1),
            static_cast<unsigned>(target->GetGUID().d2));
    }

    bool CloseMakesRunStale() {
        BML_BehaviorRun run = nullptr;
        if (!OpenInstance(run, BML_BEHAVIOR_FRAMES_SIGNALS, 4))
            return false;
        if (m_Behavior->CloseRun(run) != BML_OK)
            return false;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->ReadRun(run, &info, &status) ==
                   BML_ERROR_INVALID_HANDLE &&
            status.Error == BML_BEHAVIOR_ERROR_STATE_INVALID;
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

    bool RunCppFacade() {
        auto opened = BML::Behavior::Session::Open();
        if (!opened)
            return false;
        m_CppSession = opened.Take();
        if (!InspectGameplayGraph())
            return false;
        const BML::Behavior::Prototype prototype(
            CKGUID(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID),
            m_Prototype.Generation);

        auto plain = m_CppSession.Use(prototype);
        if (!plain.Validate())
            return false;

        auto called = plain.Call("Run", BML::Behavior::Signals(4).Pouts());
        if (!called)
            return false;
        BML::Behavior::Call call = called.Take();
        auto callInfo = call.Info();
        if (!callInfo || callInfo->Detached !=
                BML::Behavior::DetachedSupport::Unverified)
            return false;
        m_DetachedDiagnosticPassed = true;
        auto taken = call.TakeFrames();
        if (!taken || taken->Size() != 1)
            return false;
        const BML::Behavior::Frame frame = (*taken)[0];
        const auto value = frame.Pout<std::int32_t>(
            BML::Behavior::Named("Number", 0));
        if (frame.Sequence() != 1 ||
            frame.Continuation() != BML::Behavior::Continuation::None ||
            !frame.HasOut("Done") || !value || value.Value() != 42)
            return false;

        auto source = call.Inspect();
        auto bound = plain.Spawn(BML::Behavior::Signals().Pouts());
        if (!source || !bound) {
            GetLogger()->Error(
                "Behavior live edit: stage=bind-open source=%d bound=%d",
                source.Code(), bound.Code());
            return false;
        }
        BML::Behavior::Instance boundInstance = bound.Take();
        auto boundLayout = boundInstance.Layout();
        const BML::Behavior::Slot *boundPin = boundLayout
            ? boundLayout->Find(
                  BML::Behavior::SlotKind::Pin, "Number", 0)
            : nullptr;
        if (!boundPin) {
            GetLogger()->Error("Behavior live edit: stage=bind-layout");
            return false;
        }
        auto boundRelation = boundInstance.Bind(
            *boundPin, source->Root().Pout(BML::Behavior::Named("Number", 0)));
        if (!boundRelation) {
            GetLogger()->Error(
                "Behavior live edit: stage=bind code=%d error=%u message=%s",
                boundRelation.Code(),
                static_cast<unsigned>(boundRelation.GetStatus().Error),
                boundRelation.GetStatus().Message.c_str());
            return false;
        }
        auto boundPulse = boundInstance.Pulse("Echo Number");
        if (!boundPulse) {
            GetLogger()->Error(
                "Behavior live edit: stage=bind-pulse code=%d error=%u message=%s",
                boundPulse.Code(),
                static_cast<unsigned>(boundPulse.GetStatus().Error),
                boundPulse.GetStatus().Message.c_str());
            return false;
        }
        auto boundFrames = boundInstance.TakeFrames();
        const auto boundNumber = boundFrames && boundFrames->Size() == 1
            ? (*boundFrames)[0].Pout<std::int32_t>(
                  BML::Behavior::Named("Number", 0))
            : BML::Behavior::Result<std::int32_t>::Failure(BML_ERROR_FAIL);
        if (!boundFrames || boundFrames->Size() != 1 ||
            (*boundFrames)[0].Error() != BML::Behavior::Error::None ||
            !(*boundFrames)[0].HasOut("Done") ||
            !boundNumber || boundNumber.Value() != 42) {
            GetLogger()->Error(
                "Behavior live edit: stage=bind-value frames=%u value=%d",
                boundFrames ? static_cast<unsigned>(boundFrames->Size()) : 0,
                boundNumber ? boundNumber.Value() : -1);
            return false;
        }

        auto sharedSourceBlock = m_CppSession.Use(prototype);
        sharedSourceBlock.Pins({
            {BML::Behavior::Named("Number", 0), std::int32_t{84}}});
        auto sharedSource = sharedSourceBlock.Spawn();
        auto sharedTarget = plain.Spawn(BML::Behavior::Signals().Pouts());
        if (!sharedSource || !sharedTarget)
            return false;
        BML::Behavior::Instance sharedSourceInstance =
            sharedSource.Take();
        BML::Behavior::Instance sharedTargetInstance =
            sharedTarget.Take();
        auto sharedSourceGraph = sharedSourceInstance.Inspect();
        auto sharedTargetLayout = sharedTargetInstance.Layout();
        const BML::Behavior::Slot *sharedTargetPin = sharedTargetLayout
            ? sharedTargetLayout->Find(
                  BML::Behavior::SlotKind::Pin, "Number", 0)
            : nullptr;
        if (!sharedSourceGraph || !sharedTargetPin)
            return false;
        auto shared = sharedTargetInstance.Bind(
            *sharedTargetPin,
            sharedSourceGraph->Root().Pin(
                BML::Behavior::Named("Number", 0)),
            BML::Behavior::Relation::Shared);
        if (!shared || !sharedTargetInstance.Settings({
                           {"Extended Layout", true}}))
            return false;
        auto sharedTargetGraph = sharedTargetInstance.Inspect();
        auto sharedValue = sharedTargetGraph
            ? sharedTargetGraph->Read(sharedTargetGraph->Root().Pin(
                  BML::Behavior::Named("Number", 0)))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        const std::int32_t *sharedNumber = sharedValue
            ? std::get_if<std::int32_t>(&sharedValue->Data) : nullptr;
        auto sharedPulse = sharedTargetInstance.Pulse("Echo Number");
        auto sharedFrames = sharedTargetInstance.TakeFrames();
        const auto sharedPout = sharedFrames && sharedFrames->Size() == 1
            ? (*sharedFrames)[0].Pout<std::int32_t>(
                  BML::Behavior::Named("Number", 0))
            : BML::Behavior::Result<std::int32_t>::Failure(BML_ERROR_FAIL);
        if (!sharedValue ||
            sharedValue->Source != BML::Behavior::Relation::Shared ||
            !sharedNumber || *sharedNumber != 84 || !sharedPulse ||
            sharedPulse.Value() != BML::Behavior::PulseResult::Ran ||
            !sharedFrames || sharedFrames->Size() != 1 ||
            (*sharedFrames)[0].Error() != BML::Behavior::Error::None ||
            !(*sharedFrames)[0].HasOut("Done") ||
            !sharedPout || sharedPout.Value() != 84)
            return false;

        auto retryBlock = m_CppSession.Use(prototype);
        retryBlock.Settings({{"Retry", true}});
        auto started = retryBlock.Start(BML::Behavior::Unique("Run"));
        if (!started)
            return false;
        m_CppStart.emplace(started.Take());

        auto pending = retryBlock.Call(BML::Behavior::Unique("Run"));
        if (!pending)
            return false;
        m_CppCall.emplace(pending.Take());

        auto spawned = plain.Spawn(BML::Behavior::Signals(4).Pouts());
        if (!spawned)
            return false;
        m_CppInstance.emplace(spawned.Take());
        auto admission = m_CppInstance->Pulse("Run");
        if (!admission ||
            admission.Value() != BML::Behavior::PulseResult::Ran)
            return false;

        auto dynamicBlock = m_CppSession.Use(prototype);
        dynamicBlock.Pins({
            {BML::Behavior::Named("Number", 0), std::int32_t{321}}});
        auto dynamic = dynamicBlock.Spawn(
            BML::Behavior::Signals(4).Pouts());
        if (!dynamic) {
            GetLogger()->Error(
                "Behavior live edit: stage=configure-open code=%d error=%u message=%s",
                dynamic.Code(),
                static_cast<unsigned>(dynamic.GetStatus().Error),
                dynamic.GetStatus().Message.c_str());
            return false;
        }
        BML::Behavior::Instance dynamicInstance = dynamic.Take();
        auto before = dynamicInstance.Layout();
        const BML::Behavior::Slot *oldNumber = before
            ? before->Find(
                  BML::Behavior::SlotKind::Pin, "Number", 0) : nullptr;
        if (!oldNumber) {
            GetLogger()->Error("Behavior live edit: stage=configure-layout-before");
            return false;
        }
        const BML::Behavior::Slot staleNumber = *oldNumber;
        auto extended = dynamicInstance.Settings({{"Extended Layout", true}});
        auto configured = dynamicInstance.Settings({{"Retry", true}});
        if (!extended || !configured ||
            configured.Value() <= before->Generation) {
            GetLogger()->Error(
                "Behavior live edit: stage=configure code=%d error=%u generation=%llu before=%llu message=%s",
                configured.Code(),
                static_cast<unsigned>(configured.GetStatus().Error),
                configured ? static_cast<unsigned long long>(configured.Value()) : 0,
                static_cast<unsigned long long>(before->Generation),
                configured.GetStatus().Message.c_str());
            return false;
        }
        auto stale = dynamicInstance.Set(staleNumber, std::int32_t{1});
        if (stale || stale.GetStatus().Error !=
                BML::Behavior::Error::LayoutChanged) {
            GetLogger()->Error(
                "Behavior live edit: stage=stale accepted=%s code=%d error=%u message=%s",
                stale ? "true" : "false", stale.Code(),
                static_cast<unsigned>(stale.GetStatus().Error),
                stale.GetStatus().Message.c_str());
            return false;
        }
        auto after = dynamicInstance.Layout();
        const BML::Behavior::Slot *dynamicPin = after
            ? after->Find(BML::Behavior::SlotKind::Pin, "Dynamic Value")
            : nullptr;
        const BML::Behavior::Slot *executions = after
            ? after->Find(BML::Behavior::SlotKind::Local, "Executions")
            : nullptr;
        if (!dynamicPin || !executions ||
            dynamicPin->Generation != configured.Value() ||
            executions->Generation != configured.Value()) {
            GetLogger()->Error(
                "Behavior live edit: stage=configure-layout-after pin=%s generation=%llu configured=%llu",
                dynamicPin ? "true" : "false",
                dynamicPin ? static_cast<unsigned long long>(dynamicPin->Generation) : 0,
                static_cast<unsigned long long>(configured.Value()));
            return false;
        }
        auto setDynamic = dynamicInstance.Set(
            *dynamicPin, std::int32_t{713});
        auto setExecutions = dynamicInstance.Set(
            *executions, std::int32_t{5});
        if (!setDynamic || !setExecutions) {
            GetLogger()->Error(
                "Behavior live edit: stage=set code=%d error=%u local_code=%d local_error=%u message=%s",
                setDynamic.Code(),
                static_cast<unsigned>(setDynamic.GetStatus().Error),
                setExecutions.Code(),
                static_cast<unsigned>(setExecutions.GetStatus().Error),
                setDynamic.GetStatus().Message.c_str());
            return false;
        }
        auto dynamicGraph = dynamicInstance.Inspect();
        auto observedDynamic = dynamicGraph
            ? dynamicGraph->Read(dynamicGraph->Root().Pin(
                  BML::Behavior::Named("Dynamic Value", 0)))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        const std::int32_t *observedValue = observedDynamic
            ? std::get_if<std::int32_t>(&observedDynamic->Data) : nullptr;
        auto replayedNumber = dynamicGraph
            ? dynamicGraph->Read(dynamicGraph->Root().Pin(
                  BML::Behavior::Named("Number", 0)))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        const std::int32_t *replayedValue = replayedNumber
            ? std::get_if<std::int32_t>(&replayedNumber->Data) : nullptr;
        auto configuredRetry = dynamicGraph
            ? dynamicGraph->Read(dynamicGraph->Root().Setting("Retry"))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        const bool *retryValue = configuredRetry
            ? std::get_if<bool>(&configuredRetry->Data) : nullptr;
        auto localBefore = dynamicGraph
            ? dynamicGraph->Read(dynamicGraph->Root().Local("Executions"))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        const std::int32_t *executionsBefore = localBefore
            ? std::get_if<std::int32_t>(&localBefore->Data) : nullptr;
        if (!observedValue || *observedValue != 713 ||
            !replayedValue || *replayedValue != 321 ||
            replayedNumber->Source != BML::Behavior::Relation::Direct ||
            !retryValue || !*retryValue ||
            !executionsBefore || *executionsBefore != 5) {
            GetLogger()->Error(
                "Behavior live edit: stage=set-read code=%d error=%u state=%u relation=%u value=%d replay=%d replay_relation=%u index=%d",
                observedDynamic.Code(),
                static_cast<unsigned>(observedDynamic.GetStatus().Error),
                observedDynamic
                    ? static_cast<unsigned>(observedDynamic->State) : 0,
                observedDynamic
                    ? static_cast<unsigned>(observedDynamic->Source) : 0,
                observedValue ? *observedValue : -1,
                replayedValue ? *replayedValue : -1,
                replayedNumber
                    ? static_cast<unsigned>(replayedNumber->Source) : 0,
                dynamicPin->Index);
            return false;
        }
        auto dynamicAdmission = dynamicInstance.Pulse("Dynamic Run");
        auto dynamicFrames = dynamicInstance.TakeFrames();
        const auto dynamicValue = dynamicFrames && dynamicFrames->Size() == 1
            ? (*dynamicFrames)[0].Pout<std::int32_t>("Dynamic Value")
            : BML::Behavior::Result<std::int32_t>::Failure(BML_ERROR_FAIL);
        auto postDynamic = dynamicGraph->Read(dynamicGraph->Root().Pout(
            BML::Behavior::Named("Dynamic Value", 0)));
        const std::int32_t *postValue = postDynamic
            ? std::get_if<std::int32_t>(&postDynamic->Data) : nullptr;
        auto localAfter = dynamicGraph->Read(
            dynamicGraph->Root().Local("Executions"));
        const std::int32_t *executionsAfter = localAfter
            ? std::get_if<std::int32_t>(&localAfter->Data) : nullptr;
        if (!dynamicAdmission ||
            dynamicAdmission.Value() != BML::Behavior::PulseResult::Ran ||
            !dynamicFrames || dynamicFrames->Size() != 1 ||
            (*dynamicFrames)[0].Error() !=
                BML::Behavior::Error::None ||
            (*dynamicFrames)[0].Continuation() !=
                BML::Behavior::Continuation::None ||
            !(*dynamicFrames)[0].HasOut("Dynamic Done") ||
            !(*dynamicFrames)[0].HasOut("Done") ||
            !dynamicValue || dynamicValue.Value() != 713 ||
            !executionsAfter || *executionsAfter != 6) {
            GetLogger()->Error(
                "Behavior live edit: stage=dynamic-execute admission=%d frames=%u value=%d post=%d out=%s",
                dynamicAdmission.Code(),
                dynamicFrames ? static_cast<unsigned>(dynamicFrames->Size()) : 0,
                dynamicValue ? dynamicValue.Value() : -1,
                postValue ? *postValue : -1,
                dynamicFrames && dynamicFrames->Size() == 1 &&
                    (*dynamicFrames)[0].HasOut("Dynamic Done")
                    ? "true" : "false");
            return false;
        }
        GetLogger()->Info(
            "Behavior live edit: status=pass direct=true shared=true configure=multi-stage stale=true pin=true local=true replay=true");

        auto mutatingBlock = m_CppSession.Use(prototype);
        mutatingBlock.Settings({{"Extended Layout", true}});
        auto mutating = mutatingBlock.Spawn();
        if (!mutating)
            return false;
        BML::Behavior::Instance mutatingInstance =
            mutating.Take();
        auto mutationBefore = mutatingInstance.Inspect();
        auto mutationLayoutBefore = mutatingInstance.Layout();
        auto mutationAdmission = mutatingInstance.Pulse("Change Layout");
        auto mutationFrames = mutatingInstance.TakeFrames();
        auto staleAfterMutation = mutationBefore
            ? mutationBefore->Read(
                  mutationBefore->Root().Local("Executions"))
            : BML::Behavior::Result<BML::Behavior::ObservedValue>::Failure(
                  BML_ERROR_FAIL);
        auto mutationLayoutAfter = mutatingInstance.Layout();
        auto mutationAfter = mutatingInstance.Inspect();
        const BML::Behavior::Slot *createdDuringExecute = mutationLayoutAfter
            ? mutationLayoutAfter->Find(
                  BML::Behavior::SlotKind::Local,
                  "Created During Execute")
            : nullptr;
        if (!mutationBefore || !mutationLayoutBefore || !mutationAdmission ||
            mutationAdmission.Value() != BML::Behavior::PulseResult::Ran ||
            !mutationFrames || mutationFrames->Size() != 1 ||
            staleAfterMutation ||
            staleAfterMutation.GetStatus().Error !=
                BML::Behavior::Error::LayoutChanged ||
            !mutationLayoutAfter || !mutationAfter || !createdDuringExecute ||
            mutationLayoutAfter->Generation <=
                mutationLayoutBefore->Generation ||
            mutationAfter->Generation() <= mutationBefore->Generation() ||
            mutationAfter->Fingerprint() == mutationBefore->Fingerprint()) {
            GetLogger()->Error(
                "Behavior live edit: stage=execute-layout layout_before=%llu layout_after=%llu graph_before=%llu graph_after=%llu fingerprint_changed=%s error=%u",
                mutationLayoutBefore
                    ? static_cast<unsigned long long>(
                          mutationLayoutBefore->Generation)
                    : 0,
                mutationLayoutAfter
                    ? static_cast<unsigned long long>(
                          mutationLayoutAfter->Generation)
                    : 0,
                mutationBefore
                    ? static_cast<unsigned long long>(
                          mutationBefore->Generation())
                    : 0,
                mutationAfter
                    ? static_cast<unsigned long long>(
                          mutationAfter->Generation())
                    : 0,
                mutationBefore && mutationAfter &&
                    mutationBefore->Fingerprint() != mutationAfter->Fingerprint()
                    ? "true" : "false",
                static_cast<unsigned>(
                    staleAfterMutation.GetStatus().Error));
            return false;
        }

        const BML::Behavior::Prototype graphPrototype(
            CKGUID(BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID),
            m_GraphPrototype.Generation);
        auto graphBlock = m_CppSession.Use(graphPrototype);
        graphBlock.Settings({{"Run Owned", true}});
        auto graphSpawned = graphBlock.Spawn();
        if (!graphSpawned)
            return false;
        BML::Behavior::Instance graphRun = graphSpawned.Take();
        auto graphLayout = graphRun.Layout();
        auto graph = graphRun.Inspect();
        if (!graphLayout || !graph ||
            graphLayout->Origin != BML::Behavior::LayoutOrigin::Live ||
            graphLayout->Kind != BML::Behavior::BehaviorKind::Graph ||
            !graphLayout->Find(BML::Behavior::SlotKind::In, "Enter") ||
            graph->Root().Name() != "__BML_BehaviorTransport_RunGraph")
            return false;

        auto targeted = m_CppSession.Use(prototype)
            .TargetOwner()
            .Call(m_InputObjectRef, "Read Target",
                  BML::Behavior::Signals(4).Pouts());
        if (!targeted)
            return false;
        BML::Behavior::Call targetCall = targeted.Take();
        auto targetFrames = targetCall.TakeFrames();
        const auto targetObject = targetFrames && targetFrames->Size() == 1
            ? (*targetFrames)[0].Pout<BML_ObjectRef>("Target")
            : BML::Behavior::Result<BML_ObjectRef>::Failure(BML_ERROR_FAIL);
        return targetObject && Same(targetObject.Value(), m_InputObjectRef);
    }

    bool ContinueCppFacade() {
        if (!m_CppCall)
            return false;
        auto continued = m_CppCall->Continue();
        m_CppCall.reset();
        if (!continued)
            return false;
        m_CppContinued.emplace(continued.Take());
        auto info = m_CppContinued->Info();
        return info && info.Value().Kind == BML::Behavior::RunKind::Task &&
            info.Value().State == BML::Behavior::RunState::Pending;
    }

    bool CheckCppFacade() {
        if (!m_CppStart || !m_CppContinued || !m_CppInstance)
            return false;
        auto startInfo = m_CppStart->Info();
        auto continuedInfo = m_CppContinued->Info();
        auto instanceInfo = m_CppInstance->Info();
        auto startFrames = m_CppStart->TakeFrames();
        auto continuedFrames = m_CppContinued->TakeFrames();
        auto instanceFrames = m_CppInstance->TakeFrames();
        const auto validTask = [](const BML::Behavior::Result<
                                      BML::Behavior::RunInfo> &info,
                                  const BML::Behavior::Result<
                                      BML::Behavior::Frames> &frames) {
            return info && info.Value().Kind == BML::Behavior::RunKind::Task &&
                info.Value().State == BML::Behavior::RunState::Ready &&
                frames && frames->Size() == 2 &&
                (*frames)[0].Sequence() == 1 &&
                (*frames)[0].Continuation() !=
                    BML::Behavior::Continuation::None &&
                (*frames)[1].Sequence() == 2 &&
                (*frames)[1].Continuation() ==
                    BML::Behavior::Continuation::None &&
                (*frames)[1].HasOut("Done");
        };
        return validTask(startInfo, startFrames) &&
            validTask(continuedInfo, continuedFrames) &&
            instanceInfo &&
            instanceInfo.Value().Kind == BML::Behavior::RunKind::Instance &&
            instanceInfo.Value().State == BML::Behavior::RunState::Ready &&
            instanceFrames && instanceFrames->Size() == 1 &&
            (*instanceFrames)[0].Continuation() ==
                BML::Behavior::Continuation::None &&
            (*instanceFrames)[0].HasOut("Done");
    }

    void BeginRuns() {
        const auto require = [&](bool passed, const char *name) {
            if (!passed)
                GetLogger()->Error("Behavior functional admission failed: %s",
                                   name);
            return passed;
        };
        m_CppFacadePassed = require(RunCppFacade(), "cpp-facade");
        if (!m_CppFacadePassed ||
            !require(OpenCall(m_Call), "call") ||
            !require(OpenStart(m_Start, 64), "start") ||
            !require(OpenCall(m_Object, "Make Object"), "object") ||
            !require(OpenInstance(m_Latest, BML_BEHAVIOR_FRAMES_LATEST, 0),
                     "latest") ||
            !require(OpenStart(m_QueueFull, 1), "queue-full") ||
            !require(OpenPendingCall(m_ContinuedCall), "pending-call") ||
            !require(OpenEcho(m_Echo), "echo") ||
            !require(OpenDynamic(m_Dynamic), "dynamic-spawn") ||
            !require(OpenTarget(m_TargetOwner, BML_BEHAVIOR_TARGET_OWNER,
                                m_InputObjectRef, {}), "target-owner") ||
            !require(OpenTarget(m_TargetObject, BML_BEHAVIOR_TARGET_OBJECT, {},
                                m_InputObjectRef), "target-object") ||
            !require(OpenTarget(m_TargetNull, BML_BEHAVIOR_TARGET_NULL, {}, {}),
                     "target-null") ||
            !require(RejectAmbiguousSelector(), "selector-ambiguous") ||
            !require(OpenDuplicateOccurrence(m_DuplicateOccurrence),
                     "selector-occurrence") ||
            !require(OpenIndexed(m_Indexed), "selector-index") ||
            !require(OpenWaitForAll(), "wait-for-all") ||
            !require(OpenGraph(), "graph") ||
            !require(OpenGraphWatch(), "graph-watch") ||
            !require(CloseMakesRunStale(), "close-stale") ||
            !require(ReadLiveLayout(m_Latest), "live-layout") ||
            !require(ReadDynamicLiveLayout(m_Dynamic), "dynamic-layout") ||
            !require(Pulse(m_Dynamic, "Dynamic Run",
                           BML_BEHAVIOR_ADMISSION_EXECUTED), "dynamic-pulse") ||
            !require(Pulse(m_Latest, "Run",
                           BML_BEHAVIOR_ADMISSION_EXECUTED), "pulse-executed") ||
            !require(Pulse(m_Latest, "Run",
                           BML_BEHAVIOR_ADMISSION_QUEUED), "pulse-queued")) {
            Fail("admission");
        }
    }

    void PulseLatest() {
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        m_ContinueAccepted = m_Behavior->Continue(
            m_ContinuedCall, &info, &status) == BML_OK &&
            info.Kind == BML_BEHAVIOR_RUN_TASK &&
            info.State == BML_BEHAVIOR_RUN_PENDING;
        m_CppFacadePassed = m_CppFacadePassed && ContinueCppFacade();
        if (!m_ContinueAccepted || !m_CppFacadePassed ||
            !Pulse(m_Latest, "Run", BML_BEHAVIOR_ADMISSION_EXECUTED) ||
            !Pulse(m_Graph, "Enter", BML_BEHAVIOR_ADMISSION_EXECUTED))
            Fail("latest-pulse");
    }

    bool Take(BML_BehaviorRun run, TakenFrames &out) {
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        std::uint32_t headerCount = 0;
        std::uint32_t payloadSize = 0;
        const int measured = m_Behavior->TakeFrames(
            run, nullptr, 0, sizeof(BML_BehaviorRunFrame), nullptr, 0,
            &headerCount, &payloadSize, &status);
        if (measured != BML_ERROR_BUFFER_TOO_SMALL || !headerCount)
            return false;

        std::vector<BML_BehaviorRunFrame> untouchedHeaders(headerCount);
        std::memset(untouchedHeaders.data(), 0xa5,
                    untouchedHeaders.size() * sizeof(untouchedHeaders[0]));
        std::vector<std::uint8_t> untouchedPayload(payloadSize, 0xa5);
        const std::uint32_t shortHeaderCapacity = payloadSize == 0
            ? headerCount - 1 : headerCount;
        const std::uint32_t shortPayloadCapacity = payloadSize == 0
            ? 0 : payloadSize - 1;
        std::uint32_t repeatedHeaderCount = 0;
        std::uint32_t repeatedPayloadSize = 0;
        status = Dto<BML_BehaviorStatus>();
        const int shortRead = m_Behavior->TakeFrames(
            run, untouchedHeaders.data(), shortHeaderCapacity,
            sizeof(BML_BehaviorRunFrame),
            untouchedPayload.empty() ? nullptr : untouchedPayload.data(),
            shortPayloadCapacity, &repeatedHeaderCount, &repeatedPayloadSize,
            &status);
        const auto untouched = [](const auto &bytes) {
            if (bytes.empty())
                return true;
            const auto *begin = reinterpret_cast<const std::uint8_t *>(
                bytes.data());
            const std::size_t size = bytes.size() * sizeof(bytes[0]);
            return std::all_of(begin, begin + size,
                               [](std::uint8_t value) {
                                   return value == 0xa5;
                               });
        };
        if (shortRead != BML_ERROR_BUFFER_TOO_SMALL ||
            repeatedHeaderCount != headerCount ||
            repeatedPayloadSize != payloadSize ||
            !untouched(untouchedHeaders) || !untouched(untouchedPayload))
            return false;

        out.Headers.resize(headerCount);
        out.Payload.resize(payloadSize);
        std::uint32_t secondHeaderCount = 0;
        std::uint32_t secondPayloadSize = 0;
        status = Dto<BML_BehaviorStatus>();
        const int taken = m_Behavior->TakeFrames(
            run, out.Headers.data(), headerCount,
            sizeof(BML_BehaviorRunFrame), out.Payload.data(), payloadSize,
            &secondHeaderCount, &secondPayloadSize, &status);
        out.NonConsumingSizeQuery = taken == BML_OK &&
            secondHeaderCount == headerCount && secondPayloadSize == payloadSize;
        return out.NonConsumingSizeQuery;
    }

    template <typename T>
    bool Record(const TakenFrames &out, std::uint32_t offset,
                std::uint32_t index, T &record) const {
        const std::uint64_t position = static_cast<std::uint64_t>(offset) +
            static_cast<std::uint64_t>(index) * sizeof(T);
        if (position + sizeof(T) > out.Payload.size())
            return false;
        std::memcpy(&record, out.Payload.data() + position, sizeof(T));
        return record.StructSize >= sizeof(T);
    }

    std::string_view Bytes(const TakenFrames &out, std::uint32_t offset,
                           std::uint32_t length) const {
        if (static_cast<std::uint64_t>(offset) + length > out.Payload.size())
            return {};
        return {reinterpret_cast<const char *>(out.Payload.data() + offset),
                length};
    }

    bool ValidateValues(const TakenFrames &out,
                        const BML_BehaviorRunFrame &header,
                        BML_ObjectRef *objectReference = nullptr,
                        std::uint32_t *valueMask = nullptr,
                        bool authored = false,
                        const BML_ObjectRef *expectedTarget = nullptr) const {
        if (header.PoutCount < 14)
            return false;
        std::uint32_t mask = 0;
        const auto floats = [&](std::string_view value,
                                std::initializer_list<float> expected) {
            if (value.size() != expected.size() * sizeof(float))
                return false;
            const auto *bytes = reinterpret_cast<const std::uint8_t *>(
                value.data());
            std::size_t index = 0;
            for (float expectedValue : expected) {
                const float actual = std::bit_cast<float>(
                    Load32(bytes + index * sizeof(float)));
                if (actual != expectedValue)
                    return false;
                ++index;
            }
            return true;
        };
        const auto objectRef = [&](std::string_view value,
                                   BML_ObjectRef &reference) {
            if (value.size() != 12)
                return false;
            const auto *bytes = reinterpret_cast<const std::uint8_t *>(
                value.data());
            reference = {Load32(bytes), Load32(bytes + 4),
                         Load32(bytes + 8)};
            return true;
        };
        for (std::uint32_t index = 0; index < header.PoutCount; ++index) {
            BML_BehaviorPoutRecord record{};
            if (!Record(out, header.PoutOffset, index, record))
                return false;
            if (record.ValueOffset % BML_BEHAVIOR_VALUE_ALIGNMENT != 0)
                return false;
            const std::string_view name = Bytes(
                out, record.NameOffset, record.NameLength);
            const std::string_view value = Bytes(
                out, record.ValueOffset, record.ValueSize);
            if (record.Kind == BML_BEHAVIOR_VALUE_BOOL && name == "Bool" &&
                value.size() == 4 &&
                Load32(reinterpret_cast<const std::uint8_t *>(value.data())) ==
                    (authored ? 0u : 1u)) {
                mask |= 1u << 0;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_INT32 && name == "Number" &&
                record.Occurrence == 0 && value.size() == 4) {
                const std::int32_t actual = static_cast<std::int32_t>(
                    Load32(reinterpret_cast<const std::uint8_t *>(value.data())));
                if (actual == (authored ? -1234567 : 42))
                    mask |= 1u << 1;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_FLOAT32 &&
                       name == "Number" && record.Occurrence == 1 &&
                       value.size() == 4) {
                const float number = std::bit_cast<float>(
                    Load32(reinterpret_cast<const std::uint8_t *>(value.data())));
                if (number == (authored ? -2.25f : 1.5f))
                    mask |= 1u << 2;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_UTF8 &&
                       name == "Text" &&
                       value == (authored ? "authored" : "transport")) {
                mask |= 1u << 3;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_VEC2 &&
                       name == "Vec2" &&
                       (authored ? floats(value, {-4.25f, 5.5f})
                                 : floats(value, {4.0f, 5.0f}))) {
                mask |= 1u << 4;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_VEC3 &&
                       name == "Vector" &&
                       (authored ? floats(value, {6.25f, -7.5f, 8.75f})
                                 : floats(value, {1.0f, 2.0f, 3.0f}))) {
                mask |= 1u << 5;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_QUATERNION &&
                       name == "Quaternion" &&
                       (authored ? floats(value, {-0.1f, 0.2f, -0.3f, 0.4f})
                                 : floats(value, {0.1f, 0.2f, 0.3f, 0.4f}))) {
                mask |= 1u << 6;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_EULER &&
                       name == "Euler" &&
                       (authored ? floats(value, {0.9f, -0.8f, 0.7f})
                                 : floats(value, {0.5f, 0.6f, 0.7f}))) {
                mask |= 1u << 7;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_RECT &&
                       name == "Rect" &&
                       (authored ? floats(value, {-1.0f, -2.0f, 3.0f, 4.0f})
                                 : floats(value, {1.0f, 2.0f, 3.0f, 4.0f}))) {
                mask |= 1u << 8;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_COLOR &&
                       name == "Color" &&
                       (authored ? floats(value, {0.11f, 0.22f, 0.33f, 0.44f})
                                 : floats(value, {0.2f, 0.4f, 0.6f, 0.8f}))) {
                mask |= 1u << 9;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_BOX &&
                       name == "Box" &&
                       (authored ? floats(value, {-9.0f, -8.0f, -7.0f,
                                                   7.0f, 8.0f, 9.0f})
                                 : floats(value, {-1.0f, -2.0f, -3.0f,
                                                   4.0f, 5.0f, 6.0f}))) {
                mask |= 1u << 10;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_MAT4 &&
                       name == "Matrix") {
                bool matrix = value.size() == 16 * sizeof(float);
                const auto *bytes = reinterpret_cast<const std::uint8_t *>(
                    value.data());
                for (int component = 0; matrix && component < 16; ++component) {
                    const float actual = std::bit_cast<float>(
                        Load32(bytes + component * sizeof(float)));
                    const float expected = authored
                        ? static_cast<float>(component) + 0.25f
                        : static_cast<float>((component / 4) * 10 +
                                             component % 4);
                    matrix = actual == expected;
                }
                if (matrix)
                    mask |= 1u << 11;
            } else if (record.Kind == BML_BEHAVIOR_VALUE_OBJECT &&
                       name == "Object" && value.size() == 12) {
                BML_ObjectRef reference{};
                if (objectRef(value, reference) &&
                    (!authored || Same(reference, m_InputObjectRef))) {
                    mask |= 1u << 12;
                    if (objectReference && reference.Domain)
                        *objectReference = reference;
                }
            } else if (record.Kind == BML_BEHAVIOR_VALUE_OBJECT &&
                       name == "Target" && value.size() == 12) {
                BML_ObjectRef reference{};
                if (objectRef(value, reference) &&
                    (!expectedTarget || Same(reference, *expectedTarget)))
                    mask |= 1u << 13;
            }
        }
        if (valueMask)
            *valueMask = mask;
        return mask == 0x3fffu;
    }

    bool HasOut(const TakenFrames &out,
                const BML_BehaviorRunFrame &header,
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

    bool HasIntPout(const TakenFrames &out,
                    const BML_BehaviorRunFrame &header,
                    std::string_view expectedName,
                    std::int32_t expectedValue) const {
        for (std::uint32_t index = 0; index < header.PoutCount; ++index) {
            BML_BehaviorPoutRecord record{};
            if (!Record(out, header.PoutOffset, index, record))
                return false;
            if (record.Kind != BML_BEHAVIOR_VALUE_INT32 ||
                Bytes(out, record.NameOffset, record.NameLength) != expectedName)
                continue;
            const std::string_view value = Bytes(
                out, record.ValueOffset, record.ValueSize);
            return value.size() == 4 &&
                static_cast<std::int32_t>(Load32(
                    reinterpret_cast<const std::uint8_t *>(value.data()))) ==
                    expectedValue;
        }
        return false;
    }

    bool RunIs(BML_BehaviorRun run, std::uint32_t kind,
               std::uint32_t state) const {
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->ReadRun(run, &info, &status) == BML_OK &&
            info.Kind == kind && info.State == state;
    }

    void CheckRuns() {
        m_CppFacadePassed = m_CppFacadePassed && CheckCppFacade();
        const auto failedWatch = m_CppFailedWatch
            ? m_CppFailedWatch->Info()
            : BML::Behavior::Result<BML::Behavior::WatchInfo>::Failure(
                  BML_ERROR_INVALID_HANDLE);
        m_WatchFailurePassed = failedWatch &&
            failedWatch->State == BML::Behavior::WatchState::Failed &&
            failedWatch->LastStatus.Error ==
                BML::Behavior::Error::CallbackFailed;
        GetLogger()->Info(
            "Behavior watch: status=%s sampled=%s events=%llu callback_failure=%s graph_endpoints=%s layout_target=%s layout_events=%llu",
            WatchPassed() ? "pass" : "fail",
            m_WatchPassed ? "true" : "false",
            static_cast<unsigned long long>(m_WatchEventCount),
            m_WatchFailurePassed ? "true" : "false",
            m_GraphShapePassed ? "true" : "false",
            (m_LayoutTargetChanged && m_LayoutWatchPassed) ? "true" : "false",
            static_cast<unsigned long long>(m_LayoutWatchEventCount));
        TakenFrames call;
        TakenFrames start;
        TakenFrames object;
        TakenFrames latest;
        TakenFrames queueFull;
        TakenFrames continued;
        TakenFrames echo;
        TakenFrames dynamic;
        TakenFrames targetOwner;
        TakenFrames targetObject;
        TakenFrames targetNull;
        TakenFrames duplicate;
        TakenFrames indexed;
        TakenFrames waitForAll;
        TakenFrames graph;
        const bool taken = Take(m_Call, call) && Take(m_Start, start) &&
            Take(m_Object, object) && Take(m_Latest, latest) &&
            Take(m_QueueFull, queueFull) &&
            Take(m_ContinuedCall, continued) && Take(m_Echo, echo) &&
            Take(m_Dynamic, dynamic) &&
            Take(m_TargetOwner, targetOwner) &&
            Take(m_TargetObject, targetObject) &&
            Take(m_TargetNull, targetNull) &&
            Take(m_DuplicateOccurrence, duplicate) &&
            Take(m_Indexed, indexed) && Take(m_WaitForAll, waitForAll) &&
            Take(m_Graph, graph);
        if (!taken) {
            Fail("take");
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
        const bool objectLive = objectWire && m_Scene &&
            m_Scene->ReadObject(captured, &objectInfo) == BML_OK;

        const bool latestOk = latest.Headers.size() == 1 &&
            latest.Headers[0].Sequence == 3;
        const bool queueFullOk = queueFull.Headers.size() == 2 &&
            queueFull.Headers[0].Sequence == 1 &&
            queueFull.Headers[1].Sequence == 2 &&
            queueFull.Headers[1].Continuation ==
                BML_BEHAVIOR_CONTINUATION_NONE &&
            queueFull.Headers[1].Error ==
                BML_BEHAVIOR_ERROR_FRAME_QUEUE_FULL;

        const bool continuedOk = m_ContinueAccepted &&
            continued.Headers.size() == 2 &&
            continued.Headers[0].Sequence == 1 &&
            continued.Headers[1].Sequence == 2 &&
            (continued.Headers[0].Continuation &
             BML_BEHAVIOR_CONTINUATION_NATIVE) != 0 &&
            HasOut(continued, continued.Headers[1], "Done") &&
            ValidateValues(continued, continued.Headers[1]);
        const bool echoOk = echo.Headers.size() == 1 &&
            HasOut(echo, echo.Headers[0], "Done") &&
            ValidateValues(echo, echo.Headers[0], nullptr, nullptr, true);
        const bool dynamicOk = dynamic.Headers.size() == 1 &&
            HasOut(dynamic, dynamic.Headers[0], "Dynamic Done") &&
            HasIntPout(dynamic, dynamic.Headers[0], "Dynamic Value", 713) &&
            ValidateValues(dynamic, dynamic.Headers[0]);
        const BML_ObjectRef nullReference{};
        const bool targetsOk = targetOwner.Headers.size() == 1 &&
            targetObject.Headers.size() == 1 && targetNull.Headers.size() == 1 &&
            ValidateValues(targetOwner, targetOwner.Headers[0], nullptr,
                           nullptr, false, &m_InputObjectRef) &&
            ValidateValues(targetObject, targetObject.Headers[0], nullptr,
                           nullptr, false, &m_InputObjectRef) &&
            ValidateValues(targetNull, targetNull.Headers[0], nullptr,
                           nullptr, false, &nullReference);
        const bool selectorsOk = duplicate.Headers.size() == 1 &&
            indexed.Headers.size() == 1 &&
            HasOut(duplicate, duplicate.Headers[0], "Done") &&
            HasOut(indexed, indexed.Headers[0], "Done") &&
            ValidateValues(duplicate, duplicate.Headers[0]) &&
            ValidateValues(indexed, indexed.Headers[0]);
        const bool waitForAllOk = waitForAll.Headers.size() == 2 &&
            waitForAll.Headers[0].Sequence == 1 &&
            waitForAll.Headers[1].Sequence == 2 &&
            waitForAll.Headers[1].OutCount == 1;
        bool graphOut = false;
        for (const BML_BehaviorRunFrame &header : graph.Headers)
            graphOut = graphOut || HasOut(graph, header, "Exit");
        const bool graphOk = !graph.Headers.empty() &&
            graph.Headers.front().Sequence == 1 && graphOut &&
            RunIs(m_Graph, BML_BEHAVIOR_RUN_INSTANCE,
                  BML_BEHAVIOR_RUN_READY);
        const bool statesOk =
            RunIs(m_Call, BML_BEHAVIOR_RUN_CALL,
                  BML_BEHAVIOR_RUN_READY) &&
            RunIs(m_Start, BML_BEHAVIOR_RUN_TASK,
                  BML_BEHAVIOR_RUN_READY) &&
            RunIs(m_Dynamic, BML_BEHAVIOR_RUN_INSTANCE,
                  BML_BEHAVIOR_RUN_READY) &&
            ReadLiveLayout(m_Call) && ReadLiveLayout(m_Start) &&
            ReadLiveLayout(m_Object);

        GetLogger()->Info(
            "Behavior transport detail: call=%s call_count=%u call_out=%s call_values=%u start=%s start_count=%u start_out=%s start_values=%u object=%s object_count=%u object_values=%u captured=%u:%u:%u object_at_capture=%s latest=%s latest_count=%u latest_sequence=%llu queue=%s queue_count=%u queue_error=%u",
            callOk ? "true" : "false", static_cast<unsigned>(call.Headers.size()),
            callOut ? "true" : "false", callMask,
            startOk ? "true" : "false", static_cast<unsigned>(start.Headers.size()),
            startOut ? "true" : "false", startMask,
            objectWire ? "true" : "false", static_cast<unsigned>(object.Headers.size()),
            objectMask,
            captured.Domain, captured.Slot, captured.Generation,
            objectLive ? "live" : "invalid",
            latestOk ? "true" : "false", static_cast<unsigned>(latest.Headers.size()),
            latest.Headers.empty() ? 0ull :
                static_cast<unsigned long long>(latest.Headers[0].Sequence),
            queueFullOk ? "true" : "false",
            static_cast<unsigned>(queueFull.Headers.size()),
            queueFull.Headers.size() < 2 ? 0u : queueFull.Headers[1].Error);

        GetLogger()->Info(
            "Behavior functional detail: catalog=%s cpp_facade=%s detached=%s all_values=%s continue=%s dynamic_layout=%s targets=%s selectors=%s wait_for_all=%s graph=%s run_ownership=%s",
            m_CatalogPassed ? "true" : "false",
            m_CppFacadePassed ? "true" : "false",
            m_DetachedDiagnosticPassed ? "true" : "false",
            echoOk ? "true" : "false",
            continuedOk ? "true" : "false",
            dynamicOk ? "true" : "false",
            targetsOk ? "true" : "false",
            selectorsOk ? "true" : "false",
            waitForAllOk ? "true" : "false",
            graphOk ? "true" : "false",
            statesOk ? "true" : "false");

        m_FunctionalPassed = m_CppFacadePassed &&
            m_DetachedDiagnosticPassed && m_InspectPassed &&
            WatchPassed() && continuedOk && echoOk &&
            dynamicOk && targetsOk && selectorsOk && waitForAllOk &&
            graphOk && statesOk;
        m_TransportPassed = callOk && startOk && objectLive && latestOk &&
            queueFullOk && m_FunctionalPassed &&
            m_SessionOpenedBeforeLevel && m_CatalogPassed;
        m_WirePassed = call.NonConsumingSizeQuery && start.NonConsumingSizeQuery &&
            object.NonConsumingSizeQuery && latest.NonConsumingSizeQuery &&
            queueFull.NonConsumingSizeQuery &&
            continued.NonConsumingSizeQuery && echo.NonConsumingSizeQuery &&
            dynamic.NonConsumingSizeQuery &&
            targetOwner.NonConsumingSizeQuery &&
            targetObject.NonConsumingSizeQuery &&
            targetNull.NonConsumingSizeQuery && duplicate.NonConsumingSizeQuery &&
            indexed.NonConsumingSizeQuery && waitForAll.NonConsumingSizeQuery &&
            graph.NonConsumingSizeQuery;
        if (!m_TransportPassed)
            Fail("transport");
        else if (!m_WirePassed)
            Fail("wire");
        else if (m_Behavior->CloseRun(m_Object) != BML_OK)
            Fail("object-close");
        else {
            m_Object = nullptr;
            m_CapturedObjectRef = captured;
            m_ObjectCloseRequested = true;
        }
    }

    void CheckCapturedObject() {
        BML_SceneObjectInfo objectInfo{};
        m_ObjectRefPassed = m_Scene && m_CapturedObjectRef.Domain &&
            m_Scene->ReadObject(m_CapturedObjectRef, &objectInfo) ==
                BML_ERROR_OBJECT_INVALID;
        if (m_ObjectRefPassed) {
            GetLogger()->Info(
                "Behavior capture-time object: live_before_close=true stale_after_close=true");
            Finish(true, "complete");
        } else if (m_LevelFrames >= 30) {
            Fail("object-ref");
        }
    }

    void CloseRuns() {
        if (!m_Behavior)
            return;
        for (BML_BehaviorRun *run : {&m_Call, &m_Start, &m_Object,
                                     &m_Latest, &m_QueueFull,
                                     &m_ContinuedCall, &m_Echo, &m_Dynamic,
                                     &m_TargetOwner, &m_TargetObject,
                                     &m_TargetNull, &m_DuplicateOccurrence,
                                     &m_Indexed, &m_WaitForAll, &m_Graph}) {
            if (*run) {
                m_Behavior->CloseRun(*run);
                *run = nullptr;
            }
        }
    }

    // The sampled Watch, its deliberately failing twin, the graph endpoints and
    // the Layout Watch that observes the Target slot change all have to hold.
    bool WatchPassed() const {
        return m_WatchPassed && m_WatchEventCount == 2 &&
            m_WatchFailurePassed && m_GraphShapePassed &&
            m_LayoutTargetChanged && m_LayoutWatchPassed &&
            m_LayoutWatchEventCount == 1;
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
            "Behavior transport: status=%s reason=%s transport=%s wire=%s object_ref=%s session_after_reset=%s catalog=%s detached=%s inspect=%s watch=%s",
            passed ? "pass" : "fail", reason,
            m_TransportPassed ? "true" : "false",
            m_WirePassed ? "true" : "false",
            m_ObjectRefPassed ? "true" : "false",
            m_SessionOpenedBeforeLevel ? "true" : "false",
            m_CatalogPassed ? "true" : "false",
            m_DetachedDiagnosticPassed ? "true" : "false",
            m_InspectPassed ? "true" : "false",
            WatchPassed() ? "true" : "false");
        CloseRuns();
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    const BML_BehaviorInterface *m_Behavior = nullptr;
    const BML_SceneInterface *m_Scene = nullptr;
    BML_BehaviorSession m_Session = nullptr;
    BML_BehaviorPrototypeRef m_Prototype = Dto<BML_BehaviorPrototypeRef>();
    BML_BehaviorPrototypeRef m_GraphPrototype = Dto<BML_BehaviorPrototypeRef>();
    BML_BehaviorPrototypeRef m_WaitForAllPrototype = Dto<BML_BehaviorPrototypeRef>();
    BML_BehaviorRun m_Call = nullptr;
    BML_BehaviorRun m_Start = nullptr;
    BML_BehaviorRun m_Object = nullptr;
    BML_BehaviorRun m_Latest = nullptr;
    BML_BehaviorRun m_QueueFull = nullptr;
    BML_BehaviorRun m_ContinuedCall = nullptr;
    BML_BehaviorRun m_Echo = nullptr;
    BML_BehaviorRun m_Dynamic = nullptr;
    BML_BehaviorRun m_TargetOwner = nullptr;
    BML_BehaviorRun m_TargetObject = nullptr;
    BML_BehaviorRun m_TargetNull = nullptr;
    BML_BehaviorRun m_DuplicateOccurrence = nullptr;
    BML_BehaviorRun m_Indexed = nullptr;
    BML_BehaviorRun m_WaitForAll = nullptr;
    BML_BehaviorRun m_Graph = nullptr;
    BML::Behavior::Session m_CppSession;
    std::optional<BML::Behavior::Call> m_CppCall;
    std::optional<BML::Behavior::Task> m_CppStart;
    std::optional<BML::Behavior::Task> m_CppContinued;
    std::optional<BML::Behavior::Instance> m_CppInstance;
    std::optional<BML::Behavior::Watch> m_CppWatch;
    std::optional<BML::Behavior::Watch> m_CppFailedWatch;
    std::optional<BML::Behavior::Watch> m_CppLayoutWatch;
    CK_ID m_InputObjectId = 0;
    CK_ID m_WatchNodeId = 0;
    BML_ObjectRef m_InputObjectRef{};
    BML_ObjectRef m_CapturedObjectRef{};
    int m_LevelFrames = 0;
    bool m_LevelStarted = false;
    bool m_SessionOpenedBeforeLevel = false;
    bool m_TransportPassed = false;
    bool m_WirePassed = false;
    bool m_ObjectRefPassed = false;
    bool m_ObjectCloseRequested = false;
    bool m_CatalogPassed = false;
    bool m_CppFacadePassed = false;
    bool m_DetachedDiagnosticPassed = false;
    bool m_InspectPassed = false;
    bool m_GraphShapePassed = false;
    bool m_WatchPassed = false;
    bool m_WatchFailurePassed = false;
    bool m_LayoutTargetChanged = false;
    bool m_LayoutWatchPassed = false;
    std::uint64_t m_WatchEventCount = 0;
    std::uint64_t m_LayoutWatchEventCount = 0;
    std::int32_t m_WatchBaseline = 0;
    bool m_FunctionalPassed = false;
    bool m_ContinueAccepted = false;
    bool m_Done = false;
    bool m_Passed = false;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorTransportTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
