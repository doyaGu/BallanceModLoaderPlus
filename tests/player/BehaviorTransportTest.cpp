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

#include <bit>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
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
        Block.Frames = Dto<BML_BehaviorFramePolicy>();
        Block.Frames.Kind = retention;
        Block.Frames.Limit = limit;
        Block.PrototypeGeneration = generation;
    }
};

struct EchoBlockArguments {
    std::array<BML_BehaviorBinding, 13> Pins{};
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    BML_BehaviorSettingStage Stage = Dto<BML_BehaviorSettingStage>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();

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
        Block.Frames = Dto<BML_BehaviorFramePolicy>();
        Block.Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        Block.Frames.Limit = 64;
        Block.PrototypeGeneration = generation;
    }
};

struct DynamicBlockArguments {
    BML_BehaviorBinding Extended = Dto<BML_BehaviorBinding>();
    BML_BehaviorBinding Retry = Dto<BML_BehaviorBinding>();
    std::array<BML_BehaviorSettingStage, 2> Stages{};
    BML_BehaviorBinding DynamicPin = Dto<BML_BehaviorBinding>();
    BML_BehaviorBlock Block = Dto<BML_BehaviorBlock>();

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
        Block.Frames = Dto<BML_BehaviorFramePolicy>();
        Block.Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        Block.Frames.Limit = 64;
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
            m_Session, {}, &arguments.Block, &input, &run, &info, &status);
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
            m_Session, {}, &arguments.Block, &input, &run, &info, &status);
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
            m_Session, {}, &arguments.Block, &input, &run, &info, &status);
        return result == BML_ERROR_INVALID_PARAMETER && !run &&
            status.Error == BML_BEHAVIOR_ERROR_TYPE_MISMATCH;
    }

    bool OpenCall(BML_BehaviorRun &run, const char *inputName = "Run") {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named(inputName);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool OpenStart(BML_BehaviorRun &run, std::uint32_t limit) {
        BlockArguments arguments(true, BML_BEHAVIOR_FRAMES_EACH_FRAME,
                                 limit, m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Start(m_Session, {}, &arguments.Block, &input,
                                 &run, &info, &status) == BML_OK && run;
    }

    bool OpenPendingCall(BML_BehaviorRun &run) {
        BlockArguments arguments(true, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Run");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
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
        return m_Behavior->Spawn(m_Session, {}, &arguments.Block, &run,
                                 &info, &status) == BML_OK && run;
    }

    bool OpenEcho(BML_BehaviorRun &run) {
        EchoBlockArguments arguments(m_InputObjectRef,
                                     m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Echo");
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool OpenDynamic(BML_BehaviorRun &run) {
        DynamicBlockArguments arguments(m_Prototype.Generation);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Spawn(m_Session, {}, &arguments.Block, &run,
                                 &info, &status) == BML_OK && run;
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
        return m_Behavior->Call(m_Session, owner, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool RejectAmbiguousSelector() {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Named("Duplicate");
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int result = m_Behavior->Call(
            m_Session, {}, &arguments.Block, &input, &run, &info, &status);
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
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool OpenIndexed(BML_BehaviorRun &run) {
        BlockArguments arguments(false, BML_BEHAVIOR_FRAMES_SIGNALS, 64,
                                 m_Prototype.Generation);
        BML_BehaviorSelector input = Indexed(0);
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        return m_Behavior->Call(m_Session, {}, &arguments.Block, &input,
                                &run, &info, &status) == BML_OK && run;
    }

    bool OpenWaitForAll() {
        BML_BehaviorBlock block = Dto<BML_BehaviorBlock>();
        block.Prototype = Guid(VT_LOGICS_WAITFORALL);
        block.Target = Dto<BML_BehaviorTarget>();
        block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
        block.Frames = Dto<BML_BehaviorFramePolicy>();
        block.Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        block.Frames.Limit = 64;
        block.PrototypeGeneration = m_WaitForAllPrototype.Generation;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        if (m_Behavior->Spawn(m_Session, {}, &block, &m_WaitForAll,
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
        block.Frames = Dto<BML_BehaviorFramePolicy>();
        block.Frames.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
        block.Frames.Limit = 64;
        block.PrototypeGeneration = m_GraphPrototype.Generation;
        BML_BehaviorRunInfo info = Dto<BML_BehaviorRunInfo>();
        BML_BehaviorStatus status = Dto<BML_BehaviorStatus>();
        const int spawned = m_Behavior->Spawn(
            m_Session, {}, &block, &m_Graph, &info, &status);
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
        if (!inspected)
            return false;
        BML::Behavior::Graph graph = std::move(inspected).Value();
        // Gameplay.nmo contributes 53 nodes and 60 links. The live graph is
        // intentionally extensible: BML and other loaded mods may append nodes
        // after the file has been loaded, so the disk image is a baseline rather
        // than the final cardinality of the Player graph.
        const bool nmoShape = graph.Find("Gameplay_Events") &&
            graph.Nodes().size() >= 53 && graph.Links().size() >= 60;
        bool delayOne = false;
        bool delayTwo = false;
        bool portablePending = true;
        for (const BML::Behavior::Link &link : graph.Links()) {
            delayOne = delayOne || link.InitialDelay == 1;
            delayTwo = delayTwo || link.InitialDelay == 2;
            if (link.InitialDelay > 0)
                portablePending = portablePending &&
                    link.Pending == BML::Behavior::TruthValue::Unknown;
        }
        auto live = graph.Live();
        const bool liveShape = live &&
            live->Mode() == BML::Behavior::View::Live &&
            live->Nodes().size() == graph.Nodes().size() &&
            live->Links().size() == graph.Links().size() &&
            live->Fingerprint() == graph.Fingerprint();
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
                inspected.Code(), inspected.Detail().Error,
                inspected.Detail().Message.c_str());
            return false;
        }
        BML::Behavior::Graph graph = std::move(inspected).Value();
        const BML::Behavior::Node *rootNode =
            graph.Find("__BML_BehaviorTransport_Graph");
        const BML::Behavior::Node *child = nullptr;
        if (rootNode) {
            const BML::Behavior::Guid fixture(
                BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
            for (const BML::Behavior::Node &node : graph.Nodes()) {
                if (node.Parent == rootNode->Id && node.Prototype == fixture) {
                    child = &node;
                    break;
                }
            }
        }
        if (!rootNode || !child || graph.Nodes().size() != 2 ||
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
                (link.Source.Node == rootNode->Id &&
                 link.Source.Kind == BML_BEHAVIOR_SLOT_IN &&
                 link.Target.Node == child->Id &&
                 link.Target.Kind == BML_BEHAVIOR_SLOT_IN);
            exit = exit ||
                (link.Source.Node == child->Id &&
                 link.Source.Kind == BML_BEHAVIOR_SLOT_OUT &&
                 link.Target.Node == rootNode->Id &&
                 link.Target.Kind == BML_BEHAVIOR_SLOT_OUT);
        }
        if (!entry || !exit) {
            GetLogger()->Error(
                "Behavior watch graph failed: boundary entry=%s exit=%s",
                entry ? "true" : "false", exit ? "true" : "false");
            return false;
        }

        const auto execution = BML::Behavior::local(
            child->Object, "Executions");
        auto baseline = graph.Read(execution);
        const std::int32_t *baselineValue = baseline
            ? std::get_if<std::int32_t>(&baseline->Data) : nullptr;
        if (!baseline ||
            baseline->State != BML::Behavior::ObservationState::Available ||
            baseline->Source != BML::Behavior::Relation::Stored ||
            !baselineValue) {
            GetLogger()->Error(
                "Behavior watch graph failed: value result=%d error=%u state=%u source=%u int=%s message=%s",
                baseline.Code(), baseline.Detail().Error,
                baseline ? static_cast<unsigned>(baseline->State) : 0u,
                baseline ? static_cast<unsigned>(baseline->Source) : 0u,
                baselineValue ? "true" : "false",
                baseline.Detail().Message.c_str());
            return false;
        }
        m_WatchBaseline = *baselineValue;

        auto exact = graph.Watch(
            BML::Behavior::exact(execution), [](const auto &) {});
        m_ExactWatchUnavailable = !exact &&
            exact.Code() == BML_ERROR_UNAVAILABLE;
        auto watched = graph.Watch(
            BML::Behavior::sampled(execution),
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
        if (!watched || !m_ExactWatchUnavailable) {
            GetLogger()->Error(
                "Behavior watch graph failed: sampled=%d sampled_error=%u exact_unavailable=%s exact_result=%d exact_error=%u",
                watched.Code(), watched.Detail().Error,
                m_ExactWatchUnavailable ? "true" : "false", exact.Code(),
                exact.Detail().Error);
            return false;
        }
        m_CppWatch.emplace(std::move(watched).Value());
        m_GraphShapePassed = true;
        return true;
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
        m_CppSession = std::move(opened).Value();
        if (!InspectGameplayGraph())
            return false;
        const BML::Behavior::Prototype prototype(
            BML::Behavior::Guid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID),
            m_Prototype.Generation);

        auto plain = m_CppSession.Use(prototype)
            .Frames(BML::Behavior::signals(4))
            .Compile();
        if (!plain)
            return false;

        auto called = plain->Call("Run");
        if (!called)
            return false;
        BML::Behavior::Call call = std::move(called).Value();
        auto callInfo = call.Read();
        if (!callInfo || !callInfo->UnverifiedDetached)
            return false;
        m_DetachedDiagnosticPassed = true;
        auto taken = call.Take();
        if (!taken || taken.Value().size() != 1)
            return false;
        const BML::Behavior::Frame &frame = taken.Value().front();
        const BML::Behavior::Pout *number = frame.FindPout("Number", 0);
        const std::int32_t *value = number
            ? number->Get<std::int32_t>() : nullptr;
        if (frame.Sequence != 1 ||
            frame.Continuation != BML_BEHAVIOR_CONTINUATION_NONE ||
            !frame.HasOut("Done") || !value || *value != 42)
            return false;

        auto started = m_CppSession.Use(prototype)
            .Setting("Retry", true)
            .Start(BML::Behavior::unique("Run"));
        if (!started)
            return false;
        m_CppStart.emplace(std::move(started).Value());

        auto pending = m_CppSession.Use(prototype)
            .Setting("Retry", true)
            .Call(BML::Behavior::unique("Run"));
        if (!pending)
            return false;
        m_CppCall.emplace(std::move(pending).Value());

        auto spawned = plain->Spawn();
        if (!spawned)
            return false;
        m_CppInstance.emplace(std::move(spawned).Value());
        auto admission = m_CppInstance->Pulse("Run");
        if (!admission ||
            admission.Value() != BML::Behavior::Admission::Executed)
            return false;

        auto dynamic = m_CppSession.Use(prototype)
            .Setting("Extended Layout", true)
            .NextStage()
            .Setting("Retry", false)
            .Pin("Dynamic Value", std::int32_t{713})
            .Spawn();
        if (!dynamic)
            return false;
        BML::Behavior::Instance dynamicInstance = std::move(dynamic).Value();
        auto dynamicAdmission = dynamicInstance.Pulse("Dynamic Run");
        auto dynamicFrames = dynamicInstance.Take();
        const BML::Behavior::Pout *dynamicValue =
            dynamicFrames && dynamicFrames.Value().size() == 1
            ? dynamicFrames.Value().front().FindPout("Dynamic Value")
            : nullptr;
        if (!dynamicAdmission ||
            dynamicAdmission.Value() != BML::Behavior::Admission::Executed ||
            !dynamicFrames || dynamicFrames.Value().size() != 1 ||
            !dynamicFrames.Value().front().HasOut("Dynamic Done") ||
            !dynamicValue || !dynamicValue->Get<std::int32_t>() ||
            *dynamicValue->Get<std::int32_t>() != 713)
            return false;

        const BML::Behavior::Prototype graphPrototype(
            BML::Behavior::Guid(BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID),
            m_GraphPrototype.Generation);
        auto graphSpawned = m_CppSession.Use(graphPrototype)
            .Setting("Run Owned", true)
            .Spawn();
        if (!graphSpawned)
            return false;
        BML::Behavior::Instance graphRun = std::move(graphSpawned).Value();
        auto graphLayout = graphRun.Layout();
        auto graph = graphRun.Inspect();
        if (!graphLayout || !graph ||
            graphLayout->Origin != BML::Behavior::LayoutOrigin::Live ||
            graphLayout->Kind != BML::Behavior::BehaviorKind::Graph ||
            !graphLayout->Find(BML::Behavior::SlotKind::In, "Enter") ||
            !graph->Find("__BML_BehaviorTransport_RunGraph"))
            return false;

        auto targeted = m_CppSession.Use(prototype)
            .TargetOwner()
            .Call(m_InputObjectRef, "Read Target");
        if (!targeted)
            return false;
        BML::Behavior::Call targetCall = std::move(targeted).Value();
        auto targetFrames = targetCall.Take();
        const BML::Behavior::Pout *target =
            targetFrames && targetFrames.Value().size() == 1
            ? targetFrames.Value().front().FindPout("Target") : nullptr;
        const BML_ObjectRef *targetObject = target
            ? target->Get<BML_ObjectRef>() : nullptr;
        return targetObject && Same(*targetObject, m_InputObjectRef);
    }

    bool ContinueCppFacade() {
        if (!m_CppCall)
            return false;
        auto continued = std::move(*m_CppCall).Continue();
        m_CppCall.reset();
        if (!continued)
            return false;
        m_CppContinued.emplace(std::move(continued).Value());
        auto info = m_CppContinued->Read();
        return info && info.Value().Kind == BML::Behavior::RunKind::Task &&
            info.Value().State == BML::Behavior::RunState::Pending;
    }

    bool CheckCppFacade() {
        if (!m_CppStart || !m_CppContinued || !m_CppInstance)
            return false;
        auto startInfo = m_CppStart->Read();
        auto continuedInfo = m_CppContinued->Read();
        auto instanceInfo = m_CppInstance->Read();
        auto startFrames = m_CppStart->Take();
        auto continuedFrames = m_CppContinued->Take();
        auto instanceFrames = m_CppInstance->Take();
        const auto validTask = [](const BML::Behavior::Result<
                                      BML::Behavior::RunInfo> &info,
                                  const BML::Behavior::Result<std::vector<
                                      BML::Behavior::Frame>> &frames) {
            return info && info.Value().Kind == BML::Behavior::RunKind::Task &&
                info.Value().State == BML::Behavior::RunState::Ready &&
                frames && frames.Value().size() == 2 &&
                frames.Value()[0].Sequence == 1 &&
                frames.Value()[0].Continuation != 0 &&
                frames.Value()[1].Sequence == 2 &&
                frames.Value()[1].Continuation ==
                    BML_BEHAVIOR_CONTINUATION_NONE &&
                frames.Value()[1].HasOut("Done");
        };
        return validTask(startInfo, startFrames) &&
            validTask(continuedInfo, continuedFrames) &&
            instanceInfo &&
            instanceInfo.Value().Kind == BML::Behavior::RunKind::Instance &&
            instanceInfo.Value().State == BML::Behavior::RunState::Ready &&
            instanceFrames && instanceFrames.Value().size() == 1 &&
            instanceFrames.Value().front().Continuation ==
                BML_BEHAVIOR_CONTINUATION_NONE &&
            instanceFrames.Value().front().HasOut("Done");
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
        GetLogger()->Info(
            "Behavior watch: status=%s sampled=%s events=%llu exact_unavailable=%s graph_endpoints=%s",
            (m_WatchPassed && m_WatchEventCount == 2 &&
             m_ExactWatchUnavailable && m_GraphShapePassed)
                ? "pass" : "fail",
            m_WatchPassed ? "true" : "false",
            static_cast<unsigned long long>(m_WatchEventCount),
            m_ExactWatchUnavailable ? "true" : "false",
            m_GraphShapePassed ? "true" : "false");
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
            m_WatchPassed && m_WatchEventCount == 2 &&
            m_ExactWatchUnavailable && m_GraphShapePassed &&
            continuedOk && echoOk &&
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
            (m_WatchPassed && m_WatchEventCount == 2 &&
             m_ExactWatchUnavailable && m_GraphShapePassed)
                ? "true" : "false");
        CloseRuns();
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
    CK_ID m_InputObjectId = 0;
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
    bool m_ExactWatchUnavailable = false;
    bool m_WatchPassed = false;
    std::uint64_t m_WatchEventCount = 0;
    std::int32_t m_WatchBaseline = 0;
    bool m_FunctionalPassed = false;
    bool m_ContinueAccepted = false;
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
