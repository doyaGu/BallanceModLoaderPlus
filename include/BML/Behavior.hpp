// Native C++ authoring for Virtools Building Blocks. Behavior.h remains the
// stable C seam; this header owns the strings, arrays, handles, and frame bytes
// that a Mod would otherwise have to manage by hand.
#ifndef BML_BEHAVIOR_HPP
#define BML_BEHAVIOR_HPP

#include "BML/Behavior.h"
#include "BML/TypeConvert.h"

#include "CKAll.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace BML::Behavior {

struct Guid {
    std::uint32_t Data1 = 0;
    std::uint32_t Data2 = 0;

    constexpr Guid() = default;
    constexpr Guid(std::uint32_t data1, std::uint32_t data2) noexcept
        : Data1(data1), Data2(data2) {}
    Guid(CKGUID guid) noexcept : Data1(guid.d1), Data2(guid.d2) {}
    Guid(BML_BehaviorGuid guid) noexcept
        : Data1(guid.Data1), Data2(guid.Data2) {}

    [[nodiscard]] constexpr BML_BehaviorGuid Wire() const noexcept {
        return {Data1, Data2};
    }
    [[nodiscard]] CKGUID Native() const noexcept {
        return CKGUID(Data1, Data2);
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return Data1 != 0 || Data2 != 0;
    }

    friend constexpr bool operator==(Guid, Guid) noexcept = default;
};

struct Status {
    std::uint32_t Error = BML_BEHAVIOR_ERROR_NONE;
    std::uint32_t Phase = BML_BEHAVIOR_PHASE_NONE;
    std::int32_t CkError = 0;
    std::int32_t NativeResult = 0;
    Guid Prototype;
    Guid Type;
    std::string Message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Error == BML_BEHAVIOR_ERROR_NONE;
    }
};

template <class T>
class Result {
public:
    Result() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Code == BML_OK && m_Value.has_value();
    }
    [[nodiscard]] int Code() const noexcept { return m_Code; }
    [[nodiscard]] const Status &Detail() const noexcept { return m_Status; }

    [[nodiscard]] T &Value() & { return *m_Value; }
    [[nodiscard]] const T &Value() const & { return *m_Value; }
    [[nodiscard]] T &&Value() && { return std::move(*m_Value); }

    static Result Success(T value, Status status = {}) {
        Result result;
        result.m_Code = BML_OK;
        result.m_Status = std::move(status);
        result.m_Value.emplace(std::move(value));
        return result;
    }

    static Result Failure(int code, Status status = {}) {
        Result result;
        result.m_Code = code;
        result.m_Status = std::move(status);
        return result;
    }

private:
    int m_Code = BML_ERROR_FAIL;
    Status m_Status;
    std::optional<T> m_Value;
};

class Selector {
public:
    Selector() = default;
    Selector(std::string_view uniqueName)
        : m_Kind(BML_BEHAVIOR_SELECTOR_UNIQUE_NAME), m_Name(uniqueName) {}

    static Selector Only() { return Selector(); }
    static Selector At(std::int32_t index) {
        Selector selector;
        selector.m_Kind = BML_BEHAVIOR_SELECTOR_INDEX;
        selector.m_Index = index;
        return selector;
    }
    static Selector Named(std::string_view name, std::int32_t occurrence) {
        Selector selector;
        selector.m_Kind = BML_BEHAVIOR_SELECTOR_NAME;
        selector.m_Name.assign(name);
        selector.m_Occurrence = occurrence;
        return selector;
    }
    static Selector Unique(std::string_view name) {
        return Selector(name);
    }

    [[nodiscard]] BML_BehaviorSelector Wire() const noexcept {
        BML_BehaviorSelector selector{};
        selector.StructSize = sizeof(selector);
        selector.Kind = m_Kind;
        selector.Index = m_Index;
        selector.Occurrence = m_Occurrence;
        selector.Name = {m_Name.data(), static_cast<std::uint32_t>(m_Name.size())};
        return selector;
    }

private:
    std::uint32_t m_Kind = BML_BEHAVIOR_SELECTOR_ONLY;
    std::int32_t m_Index = 0;
    std::int32_t m_Occurrence = 0;
    std::string m_Name;
};

inline Selector at(std::int32_t index) { return Selector::At(index); }
inline Selector named(std::string_view name, std::int32_t occurrence) {
    return Selector::Named(name, occurrence);
}
inline Selector unique(std::string_view name) { return Selector::Unique(name); }

enum class ValueKind : std::uint32_t {
    Bool = BML_BEHAVIOR_VALUE_BOOL,
    Int32 = BML_BEHAVIOR_VALUE_INT32,
    Float32 = BML_BEHAVIOR_VALUE_FLOAT32,
    Utf8 = BML_BEHAVIOR_VALUE_UTF8,
    Vec2 = BML_BEHAVIOR_VALUE_VEC2,
    Vec3 = BML_BEHAVIOR_VALUE_VEC3,
    Quaternion = BML_BEHAVIOR_VALUE_QUATERNION,
    Euler = BML_BEHAVIOR_VALUE_EULER,
    Rect = BML_BEHAVIOR_VALUE_RECT,
    Color = BML_BEHAVIOR_VALUE_COLOR,
    Box = BML_BEHAVIOR_VALUE_BOX,
    Mat4 = BML_BEHAVIOR_VALUE_MAT4,
    Object = BML_BEHAVIOR_VALUE_OBJECT,
};

class Value {
public:
    Value(bool value) : m_Type(CKPGUID_BOOL), m_Kind(ValueKind::Bool) {
        m_Data.Bool = value ? 1u : 0u;
    }
    Value(std::int32_t value) : m_Type(CKPGUID_INT), m_Kind(ValueKind::Int32) {
        m_Data.Int32 = value;
    }
    Value(float value) : m_Type(CKPGUID_FLOAT), m_Kind(ValueKind::Float32) {
        m_Data.Float32 = value;
    }
    Value(const char *value) : Value(std::string_view(value ? value : "")) {}
    Value(std::string value) : m_Type(CKPGUID_STRING), m_Kind(ValueKind::Utf8),
                               m_Text(std::move(value)) {}
    Value(std::string_view value) : Value(std::string(value)) {}
    Value(BML_Vec2 value) : m_Type(CKPGUID_2DVECTOR), m_Kind(ValueKind::Vec2) {
        m_Data.Vec2 = value;
    }
    Value(BML_Vec3 value) : m_Type(CKPGUID_VECTOR), m_Kind(ValueKind::Vec3) {
        m_Data.Vec3 = value;
    }
    Value(BML_Quaternion value)
        : m_Type(CKPGUID_QUATERNION), m_Kind(ValueKind::Quaternion) {
        m_Data.Quaternion = value;
    }
    Value(BML_Euler value)
        : m_Type(CKPGUID_EULERANGLES), m_Kind(ValueKind::Euler) {
        m_Data.Euler = value;
    }
    Value(BML_Rect value) : m_Type(CKPGUID_RECT), m_Kind(ValueKind::Rect) {
        m_Data.Rect = value;
    }
    Value(BML_Color value) : m_Type(CKPGUID_COLOR), m_Kind(ValueKind::Color) {
        m_Data.Color = value;
    }
    Value(BML_Box value) : m_Type(CKPGUID_BOX), m_Kind(ValueKind::Box) {
        m_Data.Box = value;
    }
    Value(BML_Mat4 value) : m_Type(CKPGUID_MATRIX), m_Kind(ValueKind::Mat4) {
        m_Data.Mat4 = value;
    }
    Value(const Vx2DVector &value) : Value(Convert::ToVec2(value)) {}
    Value(const VxVector &value) : Value(Convert::ToVec3(value)) {}
    Value(const VxMatrix &value) : Value(Convert::ToMat4(value)) {}

    static Value Object(Guid type, BML_ObjectRef object) {
        Value value;
        value.m_Type = type;
        value.m_Kind = ValueKind::Object;
        value.m_Data.Object = object;
        return value;
    }
    static Value Null(Guid type) { return Object(type, {}); }

    template <class T>
    static Value As(Guid type, T &&source) {
        Value value(std::forward<T>(source));
        value.m_Type = type;
        return value;
    }

    [[nodiscard]] Guid Type() const noexcept { return m_Type; }
    [[nodiscard]] ValueKind Kind() const noexcept { return m_Kind; }

    [[nodiscard]] BML_BehaviorValue Wire() const noexcept {
        BML_BehaviorValue value{};
        value.StructSize = sizeof(value);
        value.Kind = static_cast<std::uint32_t>(m_Kind);
        value.Type = m_Type.Wire();
        value.Data = m_Data;
        if (m_Kind == ValueKind::Utf8)
            value.Data.Utf8 = {m_Text.data(),
                               static_cast<std::uint32_t>(m_Text.size())};
        return value;
    }

private:
    Value() = default;

    Guid m_Type;
    ValueKind m_Kind = ValueKind::Int32;
    BML_BehaviorValueData m_Data{};
    std::string m_Text;
};

struct Binding {
    Selector Slot;
    Behavior::Value Value;

    template <class T>
    Binding(Selector slot, T &&value)
        : Slot(std::move(slot)), Value(std::forward<T>(value)) {}
};

template <class T>
Binding pin(Selector slot, T &&value) {
    return {std::move(slot), std::forward<T>(value)};
}
template <class T>
Binding pin(std::string_view name, T &&value) {
    return {Selector::Unique(name), std::forward<T>(value)};
}
template <class T>
Binding setting(Selector slot, T &&value) {
    return {std::move(slot), std::forward<T>(value)};
}
template <class T>
Binding setting(std::string_view name, T &&value) {
    return {Selector::Unique(name), std::forward<T>(value)};
}
template <class T>
Binding local(Selector slot, T &&value) {
    return {std::move(slot), std::forward<T>(value)};
}
template <class T>
Binding local(std::string_view name, T &&value) {
    return {Selector::Unique(name), std::forward<T>(value)};
}

struct FramePolicy {
    std::uint32_t Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
    std::uint32_t Limit = 64;
};

inline FramePolicy signals(std::uint32_t limit = 64) {
    return {BML_BEHAVIOR_FRAMES_SIGNALS, limit};
}
inline FramePolicy eachFrame(std::uint32_t limit) {
    return {BML_BEHAVIOR_FRAMES_EACH_FRAME, limit};
}
inline FramePolicy latest() { return {BML_BEHAVIOR_FRAMES_LATEST, 0}; }
inline FramePolicy ignore() { return {BML_BEHAVIOR_FRAMES_NONE, 0}; }

enum class RunKind : std::uint32_t {
    Call = BML_BEHAVIOR_RUN_CALL,
    Task = BML_BEHAVIOR_RUN_TASK,
    Instance = BML_BEHAVIOR_RUN_INSTANCE,
};

enum class RunState : std::uint32_t {
    Completed = BML_BEHAVIOR_RUN_COMPLETED,
    Pending = BML_BEHAVIOR_RUN_PENDING,
    Queued = BML_BEHAVIOR_RUN_QUEUED,
    Failed = BML_BEHAVIOR_RUN_FAILED,
};

enum class Admission : std::uint32_t {
    Executed = BML_BEHAVIOR_ADMISSION_EXECUTED,
    Queued = BML_BEHAVIOR_ADMISSION_QUEUED,
};

struct RunInfo {
    RunKind Kind = RunKind::Instance;
    RunState State = RunState::Completed;
    Status Detail;
};

struct Out {
    std::int32_t Index = 0;
    std::int32_t Occurrence = 0;
    std::string Name;
};

using PoutData = std::variant<std::monostate, bool, std::int32_t, float,
                              std::string, BML_Vec2, BML_Vec3,
                              BML_Quaternion, BML_Euler, BML_Rect, BML_Color,
                              BML_Box, BML_Mat4, BML_ObjectRef>;

struct Pout {
    std::int32_t Index = 0;
    std::int32_t Occurrence = 0;
    Guid Type;
    ValueKind Kind = ValueKind::Int32;
    std::string Name;
    PoutData Data;

    template <class T>
    [[nodiscard]] const T *Get() const noexcept {
        return std::get_if<T>(&Data);
    }
};

struct Diagnostic {
    std::uint32_t Error = BML_BEHAVIOR_ERROR_NONE;
    std::uint32_t Phase = BML_BEHAVIOR_PHASE_NONE;
    std::int32_t CkError = 0;
    std::int32_t NativeResult = 0;
    Guid Prototype;
    Guid Type;
    std::string Message;
};

struct Frame {
    std::uint64_t Sequence = 0;
    std::uint64_t GameFrame = 0;
    std::int32_t NativeResult = 0;
    std::uint32_t Continuation = BML_BEHAVIOR_CONTINUATION_NONE;
    bool Terminal = false;
    std::uint32_t Error = BML_BEHAVIOR_ERROR_NONE;
    std::vector<Out> Outs;
    std::vector<Pout> Pouts;
    std::vector<Diagnostic> Diagnostics;

    [[nodiscard]] bool HasOut(std::string_view name) const noexcept {
        return std::any_of(Outs.begin(), Outs.end(), [&](const Out &out) {
            return out.Name == name;
        });
    }

    [[nodiscard]] const Pout *FindPout(std::string_view name,
                                      std::int32_t occurrence = 0) const noexcept {
        for (const Pout &pout : Pouts) {
            if (pout.Name == name && pout.Occurrence == occurrence)
                return &pout;
        }
        return nullptr;
    }
};

struct Prototype {
    Guid Id;
    std::uint64_t Generation = 0;

    Prototype() = default;
    Prototype(Guid id, std::uint64_t generation = 0)
        : Id(id), Generation(generation) {}
    Prototype(CKGUID id, std::uint64_t generation = 0)
        : Id(id), Generation(generation) {}
};

class Session;
class Block;
class Call;
class Task;
class Instance;

namespace Detail {

inline BML_BehaviorString Text(std::string_view value) noexcept {
    return {value.data(), static_cast<std::uint32_t>(value.size())};
}

inline Status ReadStatus(const BML_BehaviorStatus &source) {
    Status status;
    status.Error = source.Error;
    status.Phase = source.Phase;
    status.CkError = source.CkError;
    status.NativeResult = source.NativeResult;
    status.Prototype = source.Prototype;
    status.Type = source.Type;
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
    info.Status.StructSize = sizeof(info.Status);
    return info;
}

inline RunInfo ReadRunInfo(const BML_BehaviorRunInfo &source) {
    return {static_cast<RunKind>(source.Kind),
            static_cast<RunState>(source.State), ReadStatus(source.Status)};
}

struct SessionState {
    const BML_BehaviorInterface *Api = nullptr;
    BML_BehaviorSession Handle = nullptr;

    ~SessionState() { Close(); }

    void Close() noexcept {
        if (!Api || !Handle)
            return;
        (void) Api->CloseSession(Handle);
        Handle = nullptr;
    }
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
    const std::uint8_t *data = nullptr;
    if (!BytesAt(payload, offset, size, data))
        return false;
    text.assign(reinterpret_cast<const char *>(data), size);
    return true;
}

inline std::uint32_t Load32(const std::uint8_t *data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
}

inline float LoadFloat(const std::uint8_t *data) noexcept {
    return std::bit_cast<float>(Load32(data));
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
        if (record.ValueSize != 4) return false;
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
        value = BML_ObjectRef{Load32(data), Load32(data + 4), Load32(data + 8)};
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

inline bool ReadFrames(const std::vector<BML_BehaviorRunFrame> &headers,
                       const std::vector<std::uint8_t> &payload,
                       std::vector<Frame> &frames) {
    frames.reserve(headers.size());
    for (const BML_BehaviorRunFrame &header : headers) {
        if (header.StructSize < sizeof(header))
            return false;
        Frame frame;
        frame.Sequence = header.Sequence;
        frame.GameFrame = header.Frame;
        frame.NativeResult = header.NativeResult;
        frame.Continuation = header.Continuation;
        frame.Terminal = header.Terminal != 0;
        frame.Error = header.Error;

        frame.Outs.reserve(header.OutCount);
        for (std::uint32_t index = 0; index < header.OutCount; ++index) {
            BML_BehaviorOutRecord record{};
            if (!RecordAt(payload, header.OutOffset, index, record))
                return false;
            Out out{record.Index, record.Occurrence, {}};
            if (!TextAt(payload, record.NameOffset, record.NameLength, out.Name))
                return false;
            frame.Outs.push_back(std::move(out));
        }

        frame.Pouts.reserve(header.PoutCount);
        for (std::uint32_t index = 0; index < header.PoutCount; ++index) {
            BML_BehaviorPoutRecord record{};
            if (!RecordAt(payload, header.PoutOffset, index, record))
                return false;
            Pout pout;
            pout.Index = record.Index;
            pout.Occurrence = record.Occurrence;
            pout.Type = record.Type;
            pout.Kind = static_cast<ValueKind>(record.Kind);
            if (!TextAt(payload, record.NameOffset, record.NameLength, pout.Name) ||
                !ReadPoutData(record, payload, pout.Data))
                return false;
            frame.Pouts.push_back(std::move(pout));
        }

        frame.Diagnostics.reserve(header.DiagnosticCount);
        for (std::uint32_t index = 0; index < header.DiagnosticCount; ++index) {
            BML_BehaviorDiagnosticRecord record{};
            if (!RecordAt(payload, header.DiagnosticOffset, index, record))
                return false;
            Diagnostic diagnostic;
            diagnostic.Error = record.Error;
            diagnostic.Phase = record.Phase;
            diagnostic.CkError = record.CkError;
            diagnostic.NativeResult = record.NativeResult;
            diagnostic.Prototype = record.Prototype;
            diagnostic.Type = record.Type;
            if (!TextAt(payload, record.MessageOffset, record.MessageLength,
                        diagnostic.Message))
                return false;
            frame.Diagnostics.push_back(std::move(diagnostic));
        }
        frames.push_back(std::move(frame));
    }
    return true;
}

class Run {
public:
    Run() = default;
    ~Run() { (void) Close(); }
    Run(const Run &) = delete;
    Run &operator=(const Run &) = delete;

    Run(Run &&other) noexcept { MoveFrom(other); }
    Run &operator=(Run &&other) noexcept {
        if (this != &other) {
            (void) Close();
            MoveFrom(other);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Api && m_Handle;
    }

    [[nodiscard]] Result<RunInfo> Read() const {
        if (!*this)
            return Result<RunInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        const int code = m_Api->ReadRun(m_Handle, &info, &status);
        if (code != BML_OK)
            return Result<RunInfo>::Failure(code, ReadStatus(status));
        return Result<RunInfo>::Success(ReadRunInfo(info), ReadStatus(status));
    }

    [[nodiscard]] Result<std::vector<Frame>> Take() {
        if (!*this)
            return Result<std::vector<Frame>>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t frameCount = 0;
        std::uint32_t payloadSize = 0;
        int code = m_Api->TakeFrames(
            m_Handle, nullptr, 0, sizeof(BML_BehaviorRunFrame), nullptr, 0,
            &frameCount, &payloadSize, &status);
        if (code == BML_OK && frameCount == 0)
            return Result<std::vector<Frame>>::Success({}, ReadStatus(status));
        if (code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<std::vector<Frame>>::Failure(code, ReadStatus(status));

        try {
            std::vector<BML_BehaviorRunFrame> headers(frameCount);
            std::vector<std::uint8_t> payload(payloadSize);
            status = EmptyStatus();
            std::uint32_t writtenFrames = 0;
            std::uint32_t writtenBytes = 0;
            code = m_Api->TakeFrames(
                m_Handle, headers.data(), frameCount,
                sizeof(BML_BehaviorRunFrame), payload.data(), payloadSize,
                &writtenFrames, &writtenBytes, &status);
            if (code != BML_OK)
                return Result<std::vector<Frame>>::Failure(code,
                                                            ReadStatus(status));
            if (writtenFrames != frameCount || writtenBytes != payloadSize)
                return Result<std::vector<Frame>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
            std::vector<Frame> frames;
            if (!ReadFrames(headers, payload, frames))
                return Result<std::vector<Frame>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
            return Result<std::vector<Frame>>::Success(std::move(frames),
                                                       ReadStatus(status));
        } catch (const std::bad_alloc &) {
            return Result<std::vector<Frame>>::Failure(BML_ERROR_OUT_OF_MEMORY);
        } catch (...) {
            return Result<std::vector<Frame>>::Failure(BML_ERROR_FAIL);
        }
    }

    int Close() noexcept {
        if (!m_Api || !m_Handle)
            return BML_OK;
        const int code = m_Api->CloseRun(m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE)
            m_Handle = nullptr;
        return code;
    }

    [[nodiscard]] Result<Admission> Pulse(const Selector &input) const {
        if (!*this)
            return Result<Admission>::Failure(BML_ERROR_INVALID_HANDLE);
        const BML_BehaviorSelector selector = input.Wire();
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t admission = 0;
        const int code = m_Api->Pulse(m_Handle, &selector, &admission,
                                      &info, &status);
        if (code != BML_OK)
            return Result<Admission>::Failure(code, ReadStatus(status));
        return Result<Admission>::Success(static_cast<Admission>(admission),
                                          ReadStatus(status));
    }

private:
    Run(const BML_BehaviorInterface *api, BML_BehaviorRun handle) noexcept
        : m_Api(api), m_Handle(handle) {}

    void MoveFrom(Run &other) noexcept {
        m_Api = std::exchange(other.m_Api, nullptr);
        m_Handle = std::exchange(other.m_Handle, nullptr);
    }

    const BML_BehaviorInterface *m_Api = nullptr;
    BML_BehaviorRun m_Handle = nullptr;

    friend class ::BML::Behavior::Block;
    friend class ::BML::Behavior::Call;
};

struct BlockWire {
    explicit BlockWire(const Block &source);

    BML_BehaviorBlock Block{};
    std::vector<std::vector<BML_BehaviorBinding>> SettingBindings;
    std::vector<BML_BehaviorSettingStage> SettingStages;
    std::vector<BML_BehaviorBinding> Pins;
    std::vector<BML_BehaviorBinding> Locals;
};

} // namespace Detail

class Call {
public:
    Call(Call &&) noexcept = default;
    Call &operator=(Call &&) noexcept = default;
    Call(const Call &) = delete;
    Call &operator=(const Call &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Read() const { return m_Run.Read(); }
    [[nodiscard]] Result<std::vector<Frame>> Take() { return m_Run.Take(); }
    int Close() noexcept { return m_Run.Close(); }
    [[nodiscard]] Result<Task> Continue() &&;

private:
    explicit Call(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
};

class Task {
public:
    Task(Task &&) noexcept = default;
    Task &operator=(Task &&) noexcept = default;
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Read() const { return m_Run.Read(); }
    [[nodiscard]] Result<std::vector<Frame>> Take() { return m_Run.Take(); }
    [[nodiscard]] Result<Admission> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<Admission> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    int Close() noexcept { return m_Run.Close(); }

private:
    explicit Task(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
    friend class Call;
};

class Instance {
public:
    Instance(Instance &&) noexcept = default;
    Instance &operator=(Instance &&) noexcept = default;
    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_Run);
    }
    [[nodiscard]] Result<RunInfo> Read() const { return m_Run.Read(); }
    [[nodiscard]] Result<std::vector<Frame>> Take() { return m_Run.Take(); }
    [[nodiscard]] Result<Admission> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<Admission> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    int Close() noexcept { return m_Run.Close(); }

private:
    explicit Instance(Detail::Run run) : m_Run(std::move(run)) {}
    Detail::Run m_Run;

    friend class Block;
};

class Block {
public:
    Block &Owner(BML_ObjectRef owner) noexcept {
        m_Owner = owner;
        return *this;
    }
    Block &TargetOwner() noexcept {
        m_TargetKind = BML_BEHAVIOR_TARGET_OWNER;
        m_TargetType = {};
        m_TargetObject = {};
        return *this;
    }
    Block &Target(Guid type, BML_ObjectRef object) noexcept {
        m_TargetKind = BML_BEHAVIOR_TARGET_OBJECT;
        m_TargetType = type;
        m_TargetObject = object;
        return *this;
    }
    Block &NullTarget(Guid type) noexcept {
        m_TargetKind = BML_BEHAVIOR_TARGET_NULL;
        m_TargetType = type;
        m_TargetObject = {};
        return *this;
    }
    Block &Setting(Binding binding) {
        m_Settings.back().push_back(std::move(binding));
        return *this;
    }
    template <class T>
    Block &Setting(Selector slot, T &&value) {
        return Setting({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Block &Setting(std::string_view name, T &&value) {
        return Setting(Selector::Unique(name), std::forward<T>(value));
    }
    Block &NextStage() {
        m_Settings.emplace_back();
        return *this;
    }
    Block &Pin(Binding binding) {
        m_Pins.push_back(std::move(binding));
        return *this;
    }
    template <class T>
    Block &Pin(Selector slot, T &&value) {
        return Pin({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Block &Pin(std::string_view name, T &&value) {
        return Pin(Selector::Unique(name), std::forward<T>(value));
    }
    Block &Local(Binding binding) {
        m_Locals.push_back(std::move(binding));
        return *this;
    }
    template <class T>
    Block &Local(Selector slot, T &&value) {
        return Local({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Block &Local(std::string_view name, T &&value) {
        return Local(Selector::Unique(name), std::forward<T>(value));
    }
    template <class... Bindings>
    Block &Pins(Bindings &&... bindings) {
        (Pin(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    template <class... Bindings>
    Block &Settings(Bindings &&... bindings) {
        (Setting(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    template <class... Bindings>
    Block &Locals(Bindings &&... bindings) {
        (Local(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    Block &Frames(FramePolicy policy) noexcept {
        m_Frames = policy;
        return *this;
    }

    [[nodiscard]] Result<Behavior::Call> Call(
        Selector input = Selector::Only()) const;
    [[nodiscard]] Result<Task> Start(
        Selector input = Selector::Only()) const;
    [[nodiscard]] Result<Instance> Spawn() const;

private:
    Block(std::shared_ptr<Detail::SessionState> session, Prototype prototype)
        : m_Session(std::move(session)), m_Prototype(prototype) {
        m_Settings.emplace_back();
    }

    template <class Handle, class Function>
    Result<Handle> Open(Function function, const Selector *input) const {
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<Handle>::Failure(BML_ERROR_INVALID_HANDLE);
        try {
            Detail::BlockWire wire(*this);
            BML_BehaviorSelector selector{};
            const BML_BehaviorSelector *selectorPointer = nullptr;
            if (input) {
                selector = input->Wire();
                selectorPointer = &selector;
            }
            BML_BehaviorRun run = nullptr;
            BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
            BML_BehaviorStatus status = Detail::EmptyStatus();
            const int code = function(
                m_Session->Handle, m_Owner, &wire.Block, selectorPointer,
                &run, &info, &status);
            if (code != BML_OK || !run)
                return Result<Handle>::Failure(code, Detail::ReadStatus(status));
            return Result<Handle>::Success(
                Handle(Detail::Run(m_Session->Api, run)),
                Detail::ReadStatus(status));
        } catch (const std::bad_alloc &) {
            return Result<Handle>::Failure(BML_ERROR_OUT_OF_MEMORY);
        } catch (...) {
            return Result<Handle>::Failure(BML_ERROR_FAIL);
        }
    }

    std::shared_ptr<Detail::SessionState> m_Session;
    Prototype m_Prototype;
    BML_ObjectRef m_Owner{};
    std::uint32_t m_TargetKind = BML_BEHAVIOR_TARGET_OWNER;
    Guid m_TargetType;
    BML_ObjectRef m_TargetObject{};
    std::vector<std::vector<Binding>> m_Settings;
    std::vector<Binding> m_Pins;
    std::vector<Binding> m_Locals;
    FramePolicy m_Frames = signals();

    friend class Session;
    friend struct Detail::BlockWire;
};

class Session {
public:
    Session() = default;
    ~Session() { Close(); }
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) noexcept = default;
    Session &operator=(Session &&other) noexcept {
        if (this != &other) {
            Close();
            m_State = std::move(other.m_State);
        }
        return *this;
    }

    [[nodiscard]] static Result<Session> Open(std::string_view ownerId = {}) {
        const void *found = nullptr;
        int code = BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                                    BML_BEHAVIOR_INTERFACE_MAJOR, &found);
        if (code != BML_OK)
            return Result<Session>::Failure(code);
        const auto *api = static_cast<const BML_BehaviorInterface *>(found);
        if (!api || api->Header.MinorVersion < BML_BEHAVIOR_INTERFACE_MINOR ||
            !BML_IFACE_HAS(api, BML_BehaviorInterface, ReadLiveLayout))
            return Result<Session>::Failure(BML_ERROR_VERSION_MISMATCH);

        BML_BehaviorSession handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        code = api->OpenSession(Detail::Text(ownerId), &handle, &status);
        if (code != BML_OK || !handle)
            return Result<Session>::Failure(code, Detail::ReadStatus(status));
        try {
            Session session;
            session.m_State = std::make_shared<Detail::SessionState>();
            session.m_State->Api = api;
            session.m_State->Handle = handle;
            return Result<Session>::Success(std::move(session),
                                            Detail::ReadStatus(status));
        } catch (const std::bad_alloc &) {
            (void) api->CloseSession(handle);
            return Result<Session>::Failure(BML_ERROR_OUT_OF_MEMORY);
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_State && m_State->Api && m_State->Handle;
    }
    [[nodiscard]] Block Use(Prototype prototype) const {
        return Block(m_State, prototype);
    }
    [[nodiscard]] Block Use(Guid prototype) const {
        return Use(Prototype(prototype));
    }
    [[nodiscard]] Block Use(CKGUID prototype) const {
        return Use(Prototype(prototype));
    }
    void Close() noexcept {
        if (m_State)
            m_State->Close();
        m_State.reset();
    }

private:
    std::shared_ptr<Detail::SessionState> m_State;
};

inline Detail::BlockWire::BlockWire(const Behavior::Block &source) {
    SettingBindings.reserve(source.m_Settings.size());
    SettingStages.reserve(source.m_Settings.size());
    for (const std::vector<Binding> &stage : source.m_Settings) {
        std::vector<BML_BehaviorBinding> bindings;
        bindings.reserve(stage.size());
        for (const Binding &binding : stage) {
            BML_BehaviorBinding wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = binding.Slot.Wire();
            wire.Value = binding.Value.Wire();
            bindings.push_back(wire);
        }
        SettingBindings.push_back(std::move(bindings));
    }
    for (const auto &bindings : SettingBindings) {
        BML_BehaviorSettingStage stage{};
        stage.StructSize = sizeof(stage);
        stage.Settings = bindings.data();
        stage.SettingCount = static_cast<std::uint32_t>(bindings.size());
        SettingStages.push_back(stage);
    }
    const auto add = [](const std::vector<Binding> &from,
                        std::vector<BML_BehaviorBinding> &to) {
        to.reserve(from.size());
        for (const Binding &binding : from) {
            BML_BehaviorBinding wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = binding.Slot.Wire();
            wire.Value = binding.Value.Wire();
            to.push_back(wire);
        }
    };
    add(source.m_Pins, Pins);
    add(source.m_Locals, Locals);

    Block.StructSize = sizeof(Block);
    Block.Prototype = source.m_Prototype.Id.Wire();
    Block.Target.StructSize = sizeof(Block.Target);
    Block.Target.Kind = source.m_TargetKind;
    Block.Target.Type = source.m_TargetType.Wire();
    Block.Target.Object = source.m_TargetObject;
    Block.SettingStages = SettingStages.data();
    Block.SettingStageCount = static_cast<std::uint32_t>(SettingStages.size());
    Block.Pins = Pins.data();
    Block.PinCount = static_cast<std::uint32_t>(Pins.size());
    Block.Locals = Locals.data();
    Block.LocalCount = static_cast<std::uint32_t>(Locals.size());
    Block.Frames.StructSize = sizeof(Block.Frames);
    Block.Frames.Kind = source.m_Frames.Kind;
    Block.Frames.Limit = source.m_Frames.Limit;
    Block.PrototypeGeneration = source.m_Prototype.Generation;
}

inline Result<Behavior::Call> Block::Call(Selector input) const {
    if (!m_Session || !m_Session->Api)
        return Result<Behavior::Call>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Behavior::Call>(m_Session->Api->Call, &input);
}

inline Result<Task> Block::Start(Selector input) const {
    if (!m_Session || !m_Session->Api)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Task>(m_Session->Api->Start, &input);
}

inline Result<Instance> Block::Spawn() const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Instance>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        Detail::BlockWire wire(*this);
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->Spawn(
            m_Session->Handle, m_Owner, &wire.Block, &run, &info, &status);
        if (code != BML_OK || !run)
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        return Result<Instance>::Success(
            Instance(Detail::Run(m_Session->Api, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Task> Call::Continue() && {
    if (!m_Run)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = m_Run.m_Api->Continue(m_Run.m_Handle, &info, &status);
    if (code != BML_OK)
        return Result<Task>::Failure(code, Detail::ReadStatus(status));
    return Result<Task>::Success(Task(std::move(m_Run)),
                                 Detail::ReadStatus(status));
}

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_HPP
