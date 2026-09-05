#ifndef BML_BEHAVIOR_DETAIL_WIRE_HPP
#define BML_BEHAVIOR_DETAIL_WIRE_HPP

#include "BML/Behavior/Graph.hpp"

#include <algorithm>
#include <functional>
#include <new>
#include <utility>

namespace BML::Behavior {

namespace Detail {

struct Wire {
    [[nodiscard]] static BML_BehaviorSelector From(
        const Behavior::Selector &source) noexcept {
        BML_BehaviorSelector selector{};
        selector.StructSize = sizeof(selector);
        selector.Kind = source.m_Kind;
        selector.Index = source.m_Index;
        selector.Occurrence = source.m_Occurrence;
        selector.Name = {
            source.m_Name.data(),
            static_cast<std::uint32_t>(source.m_Name.size())};
        return selector;
    }

    [[nodiscard]] static BML_BehaviorValue From(
        const Behavior::Value &source) noexcept {
        BML_BehaviorValue value{};
        value.StructSize = sizeof(value);
        value.Kind = static_cast<std::uint32_t>(source.m_Kind);
        value.Type = WireGuid(source.m_Type);
        value.Data = source.m_Data;
        if (source.m_Kind == ValueKind::Utf8) {
            value.Data.Utf8 = {
                source.m_Text.data(),
                static_cast<std::uint32_t>(source.m_Text.size())};
        }
        return value;
    }

    [[nodiscard]] static BML_BehaviorSlotRef From(
        const Behavior::Slot &source) noexcept {
        BML_BehaviorSlotRef slot{};
        slot.StructSize = sizeof(slot);
        slot.Kind = static_cast<std::uint32_t>(source.Kind);
        slot.LayoutGeneration = source.Generation;
        slot.Type = WireGuid(source.Type);
        slot.Slot = From(Selector::At(source.Index));
        return slot;
    }
};

struct PortKey {
    std::uint64_t Node = 0;
    std::uint32_t Kind = 0;
    std::int32_t Index = 0;

    friend bool operator==(const PortKey &left,
                           const PortKey &right) noexcept {
        return left.Node == right.Node && left.Kind == right.Kind &&
            left.Index == right.Index;
    }
};

struct PortKeyHash {
    std::size_t operator()(const PortKey &key) const noexcept {
        const std::size_t node = std::hash<std::uint64_t>{}(key.Node);
        const std::uint64_t slot =
            (static_cast<std::uint64_t>(key.Kind) << 32u) |
            static_cast<std::uint32_t>(key.Index);
        const std::size_t port = std::hash<std::uint64_t>{}(slot);
        return node ^ (port + static_cast<std::size_t>(0x9e3779b9u) +
                       (node << 6u) + (node >> 2u));
    }
};

struct ObjectKey {
    explicit ObjectKey(BML_ObjectRef object) noexcept
        : Domain(object.Domain), Slot(object.Slot),
          Generation(object.Generation) {}

    std::uint32_t Domain = 0;
    std::uint32_t Slot = 0;
    std::uint32_t Generation = 0;

    friend bool operator==(const ObjectKey &left,
                           const ObjectKey &right) noexcept {
        return left.Domain == right.Domain && left.Slot == right.Slot &&
            left.Generation == right.Generation;
    }
};

struct ObjectKeyHash {
    std::size_t operator()(const ObjectKey &key) const noexcept {
        std::size_t value = std::hash<std::uint32_t>{}(key.Domain);
        const auto combine = [&](std::uint32_t part) {
            const std::size_t hashed = std::hash<std::uint32_t>{}(part);
            value ^= hashed + static_cast<std::size_t>(0x9e3779b9u) +
                (value << 6u) + (value >> 2u);
        };
        combine(key.Slot);
        combine(key.Generation);
        return value;
    }
};

inline BML_BehaviorString Text(std::string_view value) noexcept {
    return {value.data(), static_cast<std::uint32_t>(value.size())};
}

inline bool KnownError(std::uint32_t value) noexcept;
inline bool KnownPhase(std::uint32_t value) noexcept;
inline bool ValidStatus(const BML_BehaviorStatus &value) noexcept;

inline int WireCode(int code,
                    const BML_BehaviorStatus &status) noexcept {
    return ValidStatus(status) ? code : BML_ERROR_MALFORMED_MESSAGE;
}

inline Status ReadStatus(const BML_BehaviorStatus &source) {
    Status status;
    if (!ValidStatus(source))
        return status;
    status.Error = static_cast<Behavior::Error>(source.Error);
    status.Phase = static_cast<Behavior::Phase>(source.Phase);
    status.CkError = source.CkError;
    status.NativeResult = source.NativeResult;
    status.Prototype = NativeGuid(source.Prototype);
    status.Type = NativeGuid(source.Type);
    const std::size_t length = (std::min)(
        static_cast<std::size_t>(source.MessageLength),
        static_cast<std::size_t>(BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY - 1));
    status.Message.assign(source.Message, length);
    return status;
}

inline BML_BehaviorStatus EmptyStatus() noexcept {
    BML_BehaviorStatus status{};
    status.StructSize = sizeof(status);
    return status;
}

inline BML_BehaviorRunInfo EmptyRunInfo() noexcept {
    BML_BehaviorRunInfo info{};
    info.StructSize = sizeof(info);
    info.Prototype.StructSize = sizeof(info.Prototype);
    info.Status.StructSize = sizeof(info.Status);
    return info;
}

inline bool ValidRunInfo(const BML_BehaviorRunInfo &info) noexcept {
    return info.StructSize >= sizeof(info) &&
        info.Prototype.StructSize >= sizeof(info.Prototype) &&
        ValidStatus(info.Status) &&
        info.Kind >= BML_BEHAVIOR_RUN_CALL &&
        info.Kind <= BML_BEHAVIOR_RUN_INSTANCE &&
        info.State >= BML_BEHAVIOR_RUN_READY &&
        info.State <= BML_BEHAVIOR_RUN_FAILED &&
        (info.Flags & ~BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED) == 0 &&
        (info.Prototype.Prototype.Data1 != 0 ||
         info.Prototype.Prototype.Data2 != 0);
}

inline RunInfo ReadRunInfo(const BML_BehaviorRunInfo &source) {
    return {static_cast<RunKind>(source.Kind),
            static_cast<RunState>(source.State),
            (source.Flags & BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED) != 0
                ? DetachedSupport::Unverified
                : DetachedSupport::Verified,
            Prototype(NativeGuid(source.Prototype.Prototype),
                      source.Prototype.Generation),
            ReadStatus(source.Status)};
}

struct SessionState {
    const BML_BehaviorInterface *Api = nullptr;
    BML_BehaviorSession Handle = nullptr;

    ~SessionState() {
        if (Api && Handle)
            (void) Api->CloseSession(Handle);
    }
};

struct BlockSpec {
    explicit BlockSpec(Behavior::Prototype prototype)
        : PrototypeRef(prototype) {}

    Behavior::Prototype PrototypeRef;
    std::uint32_t TargetKind = BML_BEHAVIOR_TARGET_OWNER;
    CKGUID TargetType{0, 0};
    BML_ObjectRef TargetObject{};
    std::vector<std::vector<SlotValue>> Settings;
    std::vector<SlotValue> Pins;
    std::vector<SlotValue> Locals;
};

template <class T>
bool RecordAt(const std::vector<std::uint8_t> &payload,
              std::uint32_t offset, std::uint32_t index, T &record) noexcept {
    const std::uint64_t at = static_cast<std::uint64_t>(offset) +
        static_cast<std::uint64_t>(index) * sizeof(T);
    if (at + sizeof(T) > payload.size())
        return false;
    std::memcpy(&record, payload.data() + at, sizeof(T));
    return record.StructSize >= sizeof(T);
}

template <class T>
bool RecordsFit(const std::vector<std::uint8_t> &payload,
                std::uint32_t offset, std::uint32_t count) noexcept {
    const std::uint64_t end = static_cast<std::uint64_t>(offset) +
        static_cast<std::uint64_t>(count) * sizeof(T);
    return end <= payload.size();
}

inline bool BytesAt(const std::vector<std::uint8_t> &payload,
                    std::uint32_t offset, std::uint32_t size,
                    const std::uint8_t *&data) noexcept {
    if (static_cast<std::uint64_t>(offset) + size > payload.size())
        return false;
    data = payload.data() + offset;
    return true;
}

inline bool TextAt(const std::vector<std::uint8_t> &payload,
                   std::uint32_t offset, std::uint32_t size,
                   std::string &text) {
    if (!size) {
        text.clear();
        return true;
    }
    const std::uint8_t *data = nullptr;
    if (!BytesAt(payload, offset, size, data))
        return false;
    text.assign(reinterpret_cast<const char *>(data), size);
    return true;
}

inline bool KnownSlotKind(std::uint32_t kind) noexcept {
    return kind >= BML_BEHAVIOR_SLOT_IN && kind <= BML_BEHAVIOR_SLOT_TARGET;
}

inline bool KnownTruth(std::uint32_t value) noexcept {
    return value == BML_BEHAVIOR_FALSE || value == BML_BEHAVIOR_TRUE ||
        value == BML_BEHAVIOR_UNKNOWN;
}

inline bool KnownFlag(std::uint32_t value) noexcept {
    return value == 0 || value == 1;
}

inline bool ValidObjectRef(BML_ObjectRef value) noexcept {
    return value.Domain == 0
        ? value.Slot == 0 && value.Generation == 0
        : value.Slot != 0 && value.Generation != 0;
}

inline bool KnownError(std::uint32_t value) noexcept {
    return value <= BML_BEHAVIOR_ERROR_REDIRECT_CONFLICT;
}

inline bool KnownPhase(std::uint32_t value) noexcept {
    return value <= BML_BEHAVIOR_PHASE_EDIT;
}

inline bool ValidStatus(const BML_BehaviorStatus &value) noexcept {
    return value.StructSize >= sizeof(value) && KnownError(value.Error) &&
        KnownPhase(value.Phase);
}

inline bool KnownWatchState(std::uint32_t value) noexcept {
    return value >= BML_BEHAVIOR_WATCH_ACTIVE &&
        value <= BML_BEHAVIOR_WATCH_FAILED;
}

inline bool KnownPlanState(std::uint32_t value) noexcept {
    return value >= BML_BEHAVIOR_PLAN_RECONCILING &&
        value <= BML_BEHAVIOR_PLAN_RETIRING;
}

inline bool KnownPatchState(std::uint32_t value) noexcept {
    return value >= BML_BEHAVIOR_PATCH_PENDING &&
        value <= BML_BEHAVIOR_PATCH_FAILED;
}

inline bool KnownObservationState(std::uint32_t value) noexcept {
    return value == BML_BEHAVIOR_VALUE_AVAILABLE ||
        value == BML_BEHAVIOR_VALUE_INDETERMINATE ||
        value == BML_BEHAVIOR_VALUE_UNSUPPORTED;
}

inline bool KnownRelation(std::uint32_t value) noexcept {
    return value >= BML_BEHAVIOR_VALUE_STORED &&
        value <= BML_BEHAVIOR_VALUE_OPERATION;
}

inline bool KnownValueKind(std::uint32_t kind) noexcept {
    switch (kind) {
    case BML_BEHAVIOR_VALUE_BOOL:
    case BML_BEHAVIOR_VALUE_INT32:
    case BML_BEHAVIOR_VALUE_FLOAT32:
    case BML_BEHAVIOR_VALUE_UTF8:
    case BML_BEHAVIOR_VALUE_VEC2:
    case BML_BEHAVIOR_VALUE_VEC3:
    case BML_BEHAVIOR_VALUE_QUATERNION:
    case BML_BEHAVIOR_VALUE_EULER:
    case BML_BEHAVIOR_VALUE_RECT:
    case BML_BEHAVIOR_VALUE_COLOR:
    case BML_BEHAVIOR_VALUE_BOX:
    case BML_BEHAVIOR_VALUE_MAT4:
    case BML_BEHAVIOR_VALUE_OBJECT:
        return true;
    default:
        return false;
    }
}

inline bool ReadLayout(const BML_BehaviorLayout &wire,
                       const std::vector<std::uint8_t> &payload,
                       Behavior::Layout &layout) {
    if (wire.StructSize < sizeof(wire) ||
        wire.Prototype.StructSize < sizeof(wire.Prototype) ||
        (wire.Origin != BML_BEHAVIOR_LAYOUT_DECLARED &&
         wire.Origin != BML_BEHAVIOR_LAYOUT_LIVE) ||
        (wire.Kind != BML_BEHAVIOR_KIND_FUNCTION &&
         wire.Kind != BML_BEHAVIOR_KIND_CALLBACK &&
         wire.Kind != BML_BEHAVIOR_KIND_GRAPH) ||
        wire.Flags != 0 ||
        !RecordsFit<BML_BehaviorManagerInfo>(
            payload, wire.ManagerOffset, wire.ManagerCount) ||
        !RecordsFit<BML_BehaviorSlotRecord>(
            payload, wire.SlotOffset, wire.SlotCount))
        return false;

    Behavior::Layout decoded;
    decoded.Origin = static_cast<LayoutOrigin>(wire.Origin);
    decoded.PrototypeRef = Prototype(
        NativeGuid(wire.Prototype.Prototype), wire.Prototype.Generation);
    decoded.Generation = wire.LayoutGeneration;
    decoded.Kind = static_cast<BehaviorKind>(wire.Kind);
    decoded.CompatibleClass = wire.CompatibleClass;
    decoded.PrototypeFlags = wire.PrototypeFlags;
    decoded.BehaviorFlags = wire.BehaviorFlags;
    decoded.TargetType = NativeGuid(wire.TargetType);
    if (!TextAt(payload, wire.Name.Offset, wire.Name.Length, decoded.Name) ||
        !TextAt(payload, wire.Category.Offset, wire.Category.Length,
                decoded.Category) ||
        !TextAt(payload, wire.ProviderName.Offset, wire.ProviderName.Length,
                decoded.Provider) ||
        !TextAt(payload, wire.Author.Offset, wire.Author.Length,
                decoded.Author) ||
        !TextAt(payload, wire.Description.Offset, wire.Description.Length,
                decoded.Description))
        return false;

    decoded.Managers.reserve(wire.ManagerCount);
    for (std::uint32_t index = 0; index < wire.ManagerCount; ++index) {
        BML_BehaviorManagerInfo record{};
        if (!RecordAt(payload, wire.ManagerOffset, index, record))
            return false;
        decoded.Managers.push_back(
            {NativeGuid(record.Guid), record.Available != 0});
    }

    decoded.Slots.reserve(wire.SlotCount);
    for (std::uint32_t index = 0; index < wire.SlotCount; ++index) {
        BML_BehaviorSlotRecord record{};
        if (!RecordAt(payload, wire.SlotOffset, index, record) ||
            !KnownSlotKind(record.Kind))
            return false;
        Slot slot;
        slot.Kind = static_cast<SlotKind>(record.Kind);
        slot.Generation = wire.LayoutGeneration;
        slot.Dynamic = (record.Flags & BML_BEHAVIOR_SLOT_DYNAMIC) != 0;
        slot.Index = record.Index;
        slot.Occurrence = record.Occurrence;
        slot.Type = NativeGuid(record.Type);
        if ((record.Flags & BML_BEHAVIOR_SLOT_VALUE_SUPPORTED) != 0) {
            if (!KnownValueKind(record.ValueKind))
                return false;
            slot.Value = static_cast<ValueKind>(record.ValueKind);
        } else if (record.ValueKind != 0) {
            return false;
        }
        if (!TextAt(payload, record.Name.Offset, record.Name.Length,
                    slot.Name) ||
            !TextAt(payload, record.TypeName.Offset, record.TypeName.Length,
                    slot.TypeName))
            return false;
        decoded.Slots.push_back(std::move(slot));
    }
    layout = std::move(decoded);
    return true;
}

inline std::uint32_t Load32(const std::uint8_t *data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
}

inline float LoadFloat(const std::uint8_t *data) noexcept {
    const std::uint32_t bits = Load32(data);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template <class T>
bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                T &value) noexcept;

template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Vec2 &value) noexcept {
    if (size != 8) return false;
    value = {LoadFloat(data), LoadFloat(data + 4)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Vec3 &value) noexcept {
    if (size != 12) return false;
    value = {LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Quaternion &value) noexcept {
    if (size != 16) return false;
    value = {LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8),
             LoadFloat(data + 12)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Euler &value) noexcept {
    if (size != 12) return false;
    value = {LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Rect &value) noexcept {
    if (size != 16) return false;
    value = {LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8),
             LoadFloat(data + 12)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Color &value) noexcept {
    if (size != 16) return false;
    value = {LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8),
             LoadFloat(data + 12)};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Box &value) noexcept {
    if (size != 24) return false;
    value = {{LoadFloat(data), LoadFloat(data + 4), LoadFloat(data + 8)},
             {LoadFloat(data + 12), LoadFloat(data + 16), LoadFloat(data + 20)}};
    return true;
}
template <>
inline bool LoadFloats(const std::uint8_t *data, std::uint32_t size,
                       BML_Mat4 &value) noexcept {
    if (size != 64) return false;
    float *fields = &value.m00;
    for (std::size_t index = 0; index < 16; ++index)
        fields[index] = LoadFloat(data + index * 4);
    return true;
}

inline bool ReadPoutData(const BML_BehaviorPoutRecord &record,
                         const std::vector<std::uint8_t> &payload,
                         PoutData &value) {
    const std::uint8_t *data = nullptr;
    if (!BytesAt(payload, record.ValueOffset, record.ValueSize, data))
        return false;
    switch (record.Kind) {
    case BML_BEHAVIOR_VALUE_BOOL:
        if (record.ValueSize != 4 || Load32(data) > 1u) return false;
        value = Load32(data) != 0;
        return true;
    case BML_BEHAVIOR_VALUE_INT32:
        if (record.ValueSize != 4) return false;
        value = static_cast<std::int32_t>(Load32(data));
        return true;
    case BML_BEHAVIOR_VALUE_FLOAT32:
        if (record.ValueSize != 4) return false;
        value = LoadFloat(data);
        return true;
    case BML_BEHAVIOR_VALUE_UTF8:
        value = std::string(reinterpret_cast<const char *>(data),
                            record.ValueSize);
        return true;
    case BML_BEHAVIOR_VALUE_OBJECT:
        if (record.ValueSize != 12) return false;
        {
            const BML_ObjectRef object{
                Load32(data), Load32(data + 4), Load32(data + 8)};
            if (!ValidObjectRef(object))
                return false;
            value = object;
        }
        return true;
    case BML_BEHAVIOR_VALUE_VEC2: {
        BML_Vec2 decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_VEC3: {
        BML_Vec3 decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_QUATERNION: {
        BML_Quaternion decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_EULER: {
        BML_Euler decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_RECT: {
        BML_Rect decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_COLOR: {
        BML_Color decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_BOX: {
        BML_Box decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    case BML_BEHAVIOR_VALUE_MAT4: {
        BML_Mat4 decoded{};
        if (!LoadFloats(data, record.ValueSize, decoded)) return false;
        value = decoded;
        return true;
    }
    default:
        return false;
    }
}

} // namespace Detail

inline bool Frames::Accept(std::size_t count, std::size_t payloadSize) noexcept {
    if (count > m_Headers.size() || payloadSize > m_Payload.size())
        return false;
    m_Count = count;
    m_PayloadSize = payloadSize;

    std::uint64_t previousSequence = 0;
    bool first = true;
    for (std::size_t frameIndex = 0; frameIndex < m_Count; ++frameIndex) {
        const BML_BehaviorRunFrame &header = m_Headers[frameIndex];
        if (header.StructSize < sizeof(header) || !header.Sequence ||
            !Detail::KnownError(header.Error) ||
            (!first && header.Sequence <= previousSequence) ||
            (header.Continuation & ~(BML_BEHAVIOR_CONTINUATION_NATIVE |
                                     BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT)) != 0)
            return false;
        first = false;
        previousSequence = header.Sequence;

        if ((header.OutCount &&
             header.OutOffset % alignof(BML_BehaviorOutRecord) != 0) ||
            (header.PoutCount &&
             header.PoutOffset % alignof(BML_BehaviorPoutRecord) != 0) ||
            (header.DiagnosticCount &&
             header.DiagnosticOffset % alignof(BML_BehaviorDiagnosticRecord) != 0))
            return false;

        for (std::uint32_t index = 0; index < header.OutCount; ++index) {
            BML_BehaviorOutRecord record{};
            const std::uint8_t *name = nullptr;
            if (!Record(header.OutOffset, index, record) ||
                record.Index < 0 || record.Occurrence < 0 ||
                !Bytes(record.NameOffset, record.NameLength, name))
                return false;
        }

        for (std::uint32_t index = 0; index < header.PoutCount; ++index) {
            BML_BehaviorPoutRecord record{};
            const std::uint8_t *name = nullptr;
            const std::uint8_t *value = nullptr;
            if (!Record(header.PoutOffset, index, record) ||
                record.Index < 0 || record.Occurrence < 0 ||
                !Detail::KnownValueKind(record.Kind) ||
                record.ValueOffset % BML_BEHAVIOR_VALUE_ALIGNMENT != 0 ||
                !Bytes(record.NameOffset, record.NameLength, name) ||
                !Bytes(record.ValueOffset, record.ValueSize, value))
                return false;
            const std::uint32_t expected = [&]() noexcept {
                switch (record.Kind) {
                case BML_BEHAVIOR_VALUE_BOOL:
                case BML_BEHAVIOR_VALUE_INT32:
                case BML_BEHAVIOR_VALUE_FLOAT32: return 4u;
                case BML_BEHAVIOR_VALUE_VEC2: return 8u;
                case BML_BEHAVIOR_VALUE_VEC3:
                case BML_BEHAVIOR_VALUE_EULER:
                case BML_BEHAVIOR_VALUE_OBJECT: return 12u;
                case BML_BEHAVIOR_VALUE_QUATERNION:
                case BML_BEHAVIOR_VALUE_RECT:
                case BML_BEHAVIOR_VALUE_COLOR: return 16u;
                case BML_BEHAVIOR_VALUE_BOX: return 24u;
                case BML_BEHAVIOR_VALUE_MAT4: return 64u;
                case BML_BEHAVIOR_VALUE_UTF8: return record.ValueSize;
                default: return UINT32_MAX;
                }
            }();
            if (record.ValueSize != expected ||
                (record.Kind == BML_BEHAVIOR_VALUE_BOOL &&
                 Detail::Load32(value) > 1u) ||
                (record.Kind == BML_BEHAVIOR_VALUE_OBJECT &&
                 !Detail::ValidObjectRef(
                     {Detail::Load32(value), Detail::Load32(value + 4),
                      Detail::Load32(value + 8)})))
                return false;
        }

        for (std::uint32_t index = 0; index < header.DiagnosticCount; ++index) {
            BML_BehaviorDiagnosticRecord record{};
            const std::uint8_t *message = nullptr;
            if (!Record(header.DiagnosticOffset, index, record) ||
                !Detail::KnownError(record.Error) ||
                !Detail::KnownPhase(record.Phase) ||
                !Bytes(record.MessageOffset, record.MessageLength, message))
                return false;
        }
    }
    return true;
}

inline std::string_view Out::Name() const noexcept {
    return m_Frames
        ? m_Frames->Text(m_Record.NameOffset, m_Record.NameLength)
        : std::string_view{};
}

inline std::string_view Pout::Name() const noexcept {
    return m_Frames
        ? m_Frames->Text(m_Record.NameOffset, m_Record.NameLength)
        : std::string_view{};
}

template <class T>
inline Result<T> Pout::Get() const {
    if (!m_Frames)
        return Result<T>::Failure(BML_ERROR_INVALID_HANDLE);
    const std::uint8_t *data = nullptr;
    if (!m_Frames->Bytes(m_Record.ValueOffset, m_Record.ValueSize, data))
        return Result<T>::Failure(BML_ERROR_MALFORMED_MESSAGE);

    if constexpr (std::is_same_v<T, bool>) {
        if (m_Record.Kind != BML_BEHAVIOR_VALUE_BOOL)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        return Result<T>::Success(Detail::Load32(data) != 0);
    } else if constexpr ((std::is_integral_v<T> &&
                          !std::is_same_v<T, bool>) || std::is_enum_v<T>) {
        if (m_Record.Kind != BML_BEHAVIOR_VALUE_INT32)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        return Result<T>::Success(static_cast<T>(
            static_cast<std::int32_t>(Detail::Load32(data))));
    } else if constexpr (std::is_floating_point_v<T>) {
        if (m_Record.Kind != BML_BEHAVIOR_VALUE_FLOAT32)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        return Result<T>::Success(static_cast<T>(Detail::LoadFloat(data)));
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        if (m_Record.Kind != BML_BEHAVIOR_VALUE_UTF8)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        return Result<T>::Success(std::string_view(
            reinterpret_cast<const char *>(data), m_Record.ValueSize));
    } else if constexpr (std::is_same_v<T, BML_ObjectRef>) {
        if (m_Record.Kind != BML_BEHAVIOR_VALUE_OBJECT)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        return Result<T>::Success(
            {Detail::Load32(data), Detail::Load32(data + 4),
             Detail::Load32(data + 8)});
    } else if constexpr (std::is_same_v<T, BML_Vec2> ||
                         std::is_same_v<T, BML_Vec3> ||
                         std::is_same_v<T, BML_Quaternion> ||
                         std::is_same_v<T, BML_Euler> ||
                         std::is_same_v<T, BML_Rect> ||
                         std::is_same_v<T, BML_Color> ||
                         std::is_same_v<T, BML_Box> ||
                         std::is_same_v<T, BML_Mat4>) {
        constexpr ValueKind expected = [] {
            if constexpr (std::is_same_v<T, BML_Vec2>) return ValueKind::Vec2;
            if constexpr (std::is_same_v<T, BML_Vec3>) return ValueKind::Vec3;
            if constexpr (std::is_same_v<T, BML_Quaternion>) return ValueKind::Quaternion;
            if constexpr (std::is_same_v<T, BML_Euler>) return ValueKind::Euler;
            if constexpr (std::is_same_v<T, BML_Rect>) return ValueKind::Rect;
            if constexpr (std::is_same_v<T, BML_Color>) return ValueKind::Color;
            if constexpr (std::is_same_v<T, BML_Box>) return ValueKind::Box;
            return ValueKind::Mat4;
        }();
        if (Kind() != expected)
            return Result<T>::Failure(BML_ERROR_TYPE_MISMATCH);
        T value{};
        if (!Detail::LoadFloats(data, m_Record.ValueSize, value))
            return Result<T>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        return Result<T>::Success(value);
    } else if constexpr (std::is_same_v<T, Vx2DVector>) {
        auto value = Get<BML_Vec2>();
        return value ? Result<T>::Success(Convert::ToVxVector(value.Value()))
                     : Result<T>::Failure(value.Code(), value.GetStatus());
    } else if constexpr (std::is_same_v<T, VxVector>) {
        auto value = Get<BML_Vec3>();
        return value ? Result<T>::Success(Convert::ToVxVector(value.Value()))
                     : Result<T>::Failure(value.Code(), value.GetStatus());
    } else if constexpr (std::is_same_v<T, VxRect>) {
        auto value = Get<BML_Rect>();
        return value
            ? Result<T>::Success(VxRect(value->left, value->top,
                                        value->right, value->bottom))
            : Result<T>::Failure(value.Code(), value.GetStatus());
    } else if constexpr (std::is_same_v<T, VxMatrix>) {
        auto value = Get<BML_Mat4>();
        return value ? Result<T>::Success(Convert::ToVxMatrix(value.Value()))
                     : Result<T>::Failure(value.Code(), value.GetStatus());
    } else {
        static_assert(!sizeof(T), "Unsupported Behavior Pout value type");
    }
}

inline std::uint64_t Frame::Sequence() const noexcept {
    return m_Frames->Header(m_Index).Sequence;
}
inline std::uint64_t Frame::GameFrame() const noexcept {
    return m_Frames->Header(m_Index).Frame;
}
inline std::int32_t Frame::NativeResult() const noexcept {
    return m_Frames->Header(m_Index).NativeResult;
}
inline Behavior::Continuation Frame::Continuation() const noexcept {
    return static_cast<Behavior::Continuation>(
        m_Frames->Header(m_Index).Continuation);
}
inline Behavior::Error Frame::Error() const noexcept {
    return static_cast<Behavior::Error>(m_Frames->Header(m_Index).Error);
}
inline std::size_t Frame::OutCount() const noexcept {
    return m_Frames->Header(m_Index).OutCount;
}
inline std::size_t Frame::PoutCount() const noexcept {
    return m_Frames->Header(m_Index).PoutCount;
}
inline std::size_t Frame::StatusCount() const noexcept {
    return m_Frames->Header(m_Index).DiagnosticCount;
}
inline Out Frame::GetOut(std::size_t index) const {
    const auto &header = m_Frames->Header(m_Index);
    if (index >= header.OutCount)
        throw std::out_of_range("Behavior Out index is out of range.");
    BML_BehaviorOutRecord record{};
    if (!m_Frames->Record(header.OutOffset,
                          static_cast<std::uint32_t>(index), record))
        throw std::logic_error("Behavior Frames changed while a view was live.");
    return Out(m_Frames, record);
}
inline Frame::PoutView Frame::GetPout(std::size_t index) const {
    const auto &header = m_Frames->Header(m_Index);
    if (index >= header.PoutCount)
        throw std::out_of_range("Behavior Pout index is out of range.");
    BML_BehaviorPoutRecord record{};
    if (!m_Frames->Record(header.PoutOffset,
                          static_cast<std::uint32_t>(index), record))
        throw std::logic_error("Behavior Frames changed while a view was live.");
    return Frame::PoutView{m_Frames, record};
}
inline Status Frame::GetStatus(std::size_t index) const {
    const auto &header = m_Frames->Header(m_Index);
    if (index >= header.DiagnosticCount)
        throw std::out_of_range("Behavior Status index is out of range.");
    BML_BehaviorDiagnosticRecord record{};
    if (!m_Frames->Record(header.DiagnosticOffset,
                          static_cast<std::uint32_t>(index), record))
        throw std::logic_error("Behavior Frames changed while a view was live.");
    Status status;
    status.Error = static_cast<Behavior::Error>(record.Error);
    status.Phase = static_cast<Behavior::Phase>(record.Phase);
    status.CkError = record.CkError;
    status.NativeResult = record.NativeResult;
    status.Prototype = Detail::NativeGuid(record.Prototype);
    status.Type = Detail::NativeGuid(record.Type);
    const std::string_view message = m_Frames->Text(
        record.MessageOffset, record.MessageLength);
    status.Message.assign(message.data(), message.size());
    return status;
}
inline bool Frame::HasOut(const Selector &selector) const {
    bool found = false;
    for (std::size_t index = 0; index < OutCount(); ++index) {
        const Out out = GetOut(index);
        if (!selector.Matches(out.Index(), out.Occurrence(), out.Name()))
            continue;
        if (!selector.RequiresUniqueMatch())
            return true;
        if (found)
            return false;
        found = true;
    }
    return found;
}
template <class T>
inline Result<T> Frame::Pout(const Selector &selector) const {
    std::optional<std::size_t> found;
    for (std::size_t index = 0; index < PoutCount(); ++index) {
        auto value = GetPout(index);
        if (!selector.Matches(
                value.Index(), value.Occurrence(), value.Name()))
            continue;
        if (!selector.RequiresUniqueMatch())
            return value.Get<T>();
        if (found) {
            Status status;
            status.Error = Error::QueryAmbiguous;
            status.Message = "More than one Behavior Pout matches the selector.";
            return Result<T>::Failure(BML_ERROR_FAIL, std::move(status));
        }
        found = index;
    }
    if (found)
        return GetPout(*found).Get<T>();
    return Result<T>::Failure(BML_ERROR_NOT_FOUND);
}

namespace Detail {

// A Run owns one native Behavior instance. Ready means no continuation is
// waiting; Failed means execution cannot continue. Neither state releases the
// instance. Close/destruction performs lifecycle teardown at a safe point.
class Run {
public:
    Run() = default;
    ~Run() { (void) Close(); }
    Run(const Run &) = delete;
    Run &operator=(const Run &) = delete;

    Run(Run &&other) noexcept { MoveFrom(other); }
    Run &operator=(Run &&other) noexcept {
        if (this != &other) {
            Run previous(std::move(*this));
            MoveFrom(other);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }

    [[nodiscard]] Result<RunInfo> Info() const {
        if (!*this)
            return Result<RunInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        const int code = WireCode(
            m_Session->Api->ReadRun(m_Handle, &info, &status), status);
        if (code != BML_OK)
            return Result<RunInfo>::Failure(code, ReadStatus(status));
        if (!Matches(info, m_Kind))
            return Result<RunInfo>::Failure(BML_ERROR_MALFORMED_MESSAGE,
                                            ReadStatus(status));
        return Result<RunInfo>::Success(ReadRunInfo(info), ReadStatus(status));
    }

    [[nodiscard]] Result<Frames> Take() {
        Frames frames;
        Result<void> taken = Take(frames);
        if (!taken)
            return Result<Frames>::Failure(taken.Code(), taken.GetStatus());
        return Result<Frames>::Success(std::move(frames), taken.GetStatus());
    }

    [[nodiscard]] Result<void> Take(Frames &frames) {
        if (!*this)
            return Result<void>::Failure(BML_ERROR_INVALID_HANDLE);
        frames.Clear();
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t frameCount = 0;
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->TakeFrames(
            m_Handle,
            frames.m_Headers.empty() ? nullptr : frames.m_Headers.data(),
            static_cast<std::uint32_t>(frames.m_Headers.size()),
            sizeof(BML_BehaviorRunFrame),
            frames.m_Payload.empty() ? nullptr : frames.m_Payload.data(),
            static_cast<std::uint32_t>(frames.m_Payload.size()),
            &frameCount, &payloadSize, &status);
        code = WireCode(code, status);
        try {
            if (code == BML_ERROR_BUFFER_TOO_SMALL) {
                frames.Reserve(frameCount, payloadSize);
                status = EmptyStatus();
                std::uint32_t writtenFrames = 0;
                std::uint32_t writtenBytes = 0;
                code = m_Session->Api->TakeFrames(
                    m_Handle,
                    frames.m_Headers.empty() ? nullptr : frames.m_Headers.data(),
                    static_cast<std::uint32_t>(frames.m_Headers.size()),
                    sizeof(BML_BehaviorRunFrame),
                    frames.m_Payload.empty() ? nullptr : frames.m_Payload.data(),
                    static_cast<std::uint32_t>(frames.m_Payload.size()),
                    &writtenFrames, &writtenBytes, &status);
                code = WireCode(code, status);
                frameCount = writtenFrames;
                payloadSize = writtenBytes;
            }
            if (code != BML_OK)
                return Result<void>::Failure(code, ReadStatus(status));
            if (!frames.Accept(frameCount, payloadSize)) {
                frames.Clear();
                return Result<void>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
            }
            return Result<void>::Success(ReadStatus(status));
        } catch (const std::bad_alloc &) {
            frames.Clear();
            return Result<void>::Failure(BML_ERROR_OUT_OF_MEMORY);
        } catch (...) {
            frames.Clear();
            return Result<void>::Failure(BML_ERROR_FAIL);
        }
    }

    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        const int code = m_Session->Api->CloseRun(m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

    [[nodiscard]] Result<PulseResult> Pulse(const Selector &input) const {
        if (!*this)
            return Result<PulseResult>::Failure(BML_ERROR_INVALID_HANDLE);
        const BML_BehaviorSelector selector = Wire::From(input);
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t admission = 0;
        const int code = WireCode(
            m_Session->Api->Pulse(
                m_Handle, &selector, &admission, &info, &status),
            status);
        if (code != BML_OK)
            return Result<PulseResult>::Failure(code, ReadStatus(status));
        if (!Matches(info, m_Kind) ||
            (admission != BML_BEHAVIOR_ADMISSION_EXECUTED &&
             admission != BML_BEHAVIOR_ADMISSION_QUEUED))
            return Result<PulseResult>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
        return Result<PulseResult>::Success(static_cast<PulseResult>(admission),
                                            ReadStatus(status));
    }

    [[nodiscard]] Result<Behavior::Layout> Layout() const;
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const;
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, const Behavior::Value &value) const;
    [[nodiscard]] Result<std::uint64_t> Set(
        SlotKind kind, const Selector &slot,
        const Behavior::Value &value) const;
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const Port &source,
        Relation relation = Relation::Direct) const;
    [[nodiscard]] Result<std::uint64_t> Settings(
        std::initializer_list<SlotValue> values) const;

private:
    Run(std::shared_ptr<SessionState> session,
        BML_BehaviorRun handle, RunKind kind, Prototype prototype) noexcept
        : m_Session(std::move(session)), m_Handle(handle), m_Kind(kind),
          m_Prototype(prototype) {}

    [[nodiscard]] bool Matches(const BML_BehaviorRunInfo &info,
                               RunKind kind) const noexcept {
        if (!ValidRunInfo(info) ||
            info.Kind != static_cast<std::uint32_t>(kind))
            return false;
        const CKGUID prototype = NativeGuid(info.Prototype.Prototype);
        return prototype == m_Prototype.Id &&
            info.Prototype.Generation == m_Prototype.Generation;
    }

    void MoveFrom(Run &other) noexcept {
        m_Session = std::move(other.m_Session);
        m_Handle = std::exchange(other.m_Handle, nullptr);
        m_Kind = other.m_Kind;
        m_Prototype = other.m_Prototype;
    }

    std::shared_ptr<SessionState> m_Session;
    BML_BehaviorRun m_Handle = nullptr;
    RunKind m_Kind = RunKind::Instance;
    Prototype m_Prototype;

    friend class ::BML::Behavior::Block;
    friend class ::BML::Behavior::Call;
};

struct CompiledBlock {
    explicit CompiledBlock(BlockSpec spec,
                           bool declared = false);

    BlockSpec Spec;
    bool Declared = false;
    BML_BehaviorBlock Wire{};
    std::vector<std::vector<BML_BehaviorBinding>> SettingBindings;
    std::vector<BML_BehaviorSettingStage> SettingStages;
    std::vector<BML_BehaviorBinding> Pins;
    std::vector<BML_BehaviorBinding> Locals;
};

struct BlockState {
    explicit BlockState(BlockSpec spec)
        : Spec(std::move(spec)) {}

    BlockSpec Spec;
    mutable std::shared_ptr<const CompiledBlock> Compiled;
};

class Compiler final {
public:
    [[nodiscard]] Result<std::shared_ptr<const CompiledBlock>> operator()(
        const std::shared_ptr<SessionState> &session,
        BlockSpec spec, bool requireDeclared = false) const;
};

} // namespace Detail

inline Watch::~Watch() { (void) Close(); }

inline Result<WatchInfo> Watch::Info() const {
    if (!m_Session || !m_Session->Api || !m_Handle ||
        !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, ReadWatch))
        return Result<WatchInfo>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorWatchInfo info{};
    info.StructSize = sizeof(info);
    info.Diagnostic.StructSize = sizeof(info.Diagnostic);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->ReadWatch(m_Handle, &info, &status), status);
    if (code != BML_OK)
        return Result<WatchInfo>::Failure(code, Detail::ReadStatus(status));
    if (info.StructSize < sizeof(info) ||
        !Detail::KnownWatchState(info.State) ||
        !Detail::ValidStatus(info.Diagnostic))
        return Result<WatchInfo>::Failure(BML_ERROR_MALFORMED_MESSAGE);
    return Result<WatchInfo>::Success(
        {static_cast<WatchState>(info.State),
         Detail::ReadStatus(info.Diagnostic)},
        Detail::ReadStatus(status));
}

inline Result<CloseState> Watch::Close() noexcept {
    if (!m_Handle) {
        m_Session.reset();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
    const int code = m_Session->Api->CloseWatch(m_Handle);
    if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
        m_Handle = nullptr;
        m_Session.reset();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (code == BML_ERROR_BUSY)
        return Result<CloseState>::Success(CloseState::Closing);
    return Result<CloseState>::Failure(code);
}


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_DETAIL_WIRE_HPP
