// Native C++ authoring for Virtools Building Blocks. Behavior.h remains the
// stable C seam; this header owns the strings, arrays, handles, and frame bytes
// that a Mod would otherwise have to manage by hand.
#ifndef BML_BEHAVIOR_HPP
#define BML_BEHAVIOR_HPP

#include "BML/Behavior.h"
#include "BML/TypeConvert.h"

#include "CKAll.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
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
    [[nodiscard]] T *operator->() noexcept { return &*m_Value; }
    [[nodiscard]] const T *operator->() const noexcept { return &*m_Value; }
    [[nodiscard]] T &operator*() & noexcept { return *m_Value; }
    [[nodiscard]] const T &operator*() const & noexcept { return *m_Value; }

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
    bool UnverifiedDetached = false;
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

enum class View : std::uint32_t {
    Logical = BML_BEHAVIOR_GRAPH_LOGICAL,
    Live = BML_BEHAVIOR_GRAPH_LIVE,
};

enum class TruthValue : std::uint32_t {
    No = BML_BEHAVIOR_FALSE,
    Yes = BML_BEHAVIOR_TRUE,
    Unknown = BML_BEHAVIOR_UNKNOWN,
};

enum class ObservationState : std::uint32_t {
    Available = BML_BEHAVIOR_VALUE_AVAILABLE,
    Indeterminate = BML_BEHAVIOR_VALUE_INDETERMINATE,
    Unsupported = BML_BEHAVIOR_VALUE_UNSUPPORTED,
};

enum class Relation : std::uint32_t {
    Stored = BML_BEHAVIOR_VALUE_STORED,
    Direct = BML_BEHAVIOR_VALUE_DIRECT,
    Shared = BML_BEHAVIOR_VALUE_SHARED,
    Operation = BML_BEHAVIOR_VALUE_OPERATION,
};

struct ObservedValue {
    ObservationState State = ObservationState::Unsupported;
    Relation Source = Relation::Stored;
    Guid Type;
    std::optional<ValueKind> Kind;
    PoutData Data;
};

struct Port {
    std::uint64_t Node = 0;
    std::uint32_t Kind = 0;
    std::int32_t Index = -1;
    std::int32_t Occurrence = 0;
    bool Active = false;
    std::string Name;
};

struct Node {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    std::uint64_t Parent = 0;
    Guid Prototype;
    std::int32_t Priority = 0;
    bool Active = false;
    std::string Name;
    std::vector<Port> Ports;
};

struct Endpoint {
    std::uint64_t Node = 0;
    std::uint32_t Kind = 0;
    std::int32_t Index = -1;
};

struct Link {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    Endpoint Source;
    Endpoint Target;
    std::int32_t InitialDelay = 0;
    std::int32_t RemainingDelay = 0;
    TruthValue Pending = TruthValue::Unknown;
};

struct ValueRef {
    BML_ObjectRef Node{};
    std::uint32_t Kind = BML_BEHAVIOR_SLOT_PIN;
    Selector Slot;
};

inline ValueRef pin(BML_ObjectRef node, Selector slot) {
    return {node, BML_BEHAVIOR_SLOT_PIN, std::move(slot)};
}
inline ValueRef pin(BML_ObjectRef node, std::string_view name) {
    return pin(node, Selector::Unique(name));
}
inline ValueRef pout(BML_ObjectRef node, Selector slot) {
    return {node, BML_BEHAVIOR_SLOT_POUT, std::move(slot)};
}
inline ValueRef pout(BML_ObjectRef node, std::string_view name) {
    return pout(node, Selector::Unique(name));
}
inline ValueRef local(BML_ObjectRef node, Selector slot) {
    return {node, BML_BEHAVIOR_SLOT_LOCAL, std::move(slot)};
}
inline ValueRef local(BML_ObjectRef node, std::string_view name) {
    return local(node, Selector::Unique(name));
}
inline ValueRef setting(BML_ObjectRef node, Selector slot) {
    return {node, BML_BEHAVIOR_SLOT_SETTING, std::move(slot)};
}
inline ValueRef setting(BML_ObjectRef node, std::string_view name) {
    return setting(node, Selector::Unique(name));
}
inline ValueRef target(BML_ObjectRef node) {
    return {node, BML_BEHAVIOR_SLOT_TARGET, Selector::Only()};
}

struct GraphChanged {};
inline constexpr GraphChanged graphChanged{};

struct LayoutChanged {
    BML_ObjectRef Node{};
};
inline LayoutChanged layoutChanged(BML_ObjectRef node) { return {node}; }

struct Sampled {
    ValueRef Value;
};
inline Sampled sampled(ValueRef value) { return {std::move(value)}; }

struct Exact {
    ValueRef Value;
};
inline Exact exact(ValueRef value) { return {std::move(value)}; }

enum class ChangeKind : std::uint32_t {
    Graph = BML_BEHAVIOR_WATCH_GRAPH,
    Layout = BML_BEHAVIOR_WATCH_LAYOUT,
    SampledValue = BML_BEHAVIOR_WATCH_SAMPLED_VALUE,
    ExactValue = BML_BEHAVIOR_WATCH_EXACT_VALUE,
};

struct Change {
    ChangeKind Kind = ChangeKind::Graph;
    std::uint64_t Sequence = 0;
    std::uint64_t GameFrame = 0;
    std::uint64_t Before = 0;
    std::uint64_t After = 0;
    ObservedValue PreviousValue;
    ObservedValue CurrentValue;
};

namespace Detail { struct SessionState; }

class Watch {
public:
    Watch() = default;
    ~Watch() { Close(); }
    Watch(const Watch &) = delete;
    Watch &operator=(const Watch &) = delete;
    Watch(Watch &&other) noexcept
        : m_Api(std::exchange(other.m_Api, nullptr)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    Watch &operator=(Watch &&other) noexcept {
        if (this != &other) {
            Close();
            m_Api = std::exchange(other.m_Api, nullptr);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Api && m_Handle;
    }
    void Close() noexcept {
        if (m_Api && m_Handle)
            (void) m_Api->CloseWatch(m_Handle);
        m_Api = nullptr;
        m_Handle = nullptr;
    }

private:
    Watch(const BML_BehaviorInterface *api, BML_BehaviorWatch handle)
        : m_Api(api), m_Handle(handle) {}
    const BML_BehaviorInterface *m_Api = nullptr;
    BML_BehaviorWatch m_Handle = nullptr;

    friend class Graph;
};

class Graph {
public:
    [[nodiscard]] View Mode() const noexcept { return m_View; }
    [[nodiscard]] BML_ObjectRef Root() const noexcept { return m_Root; }
    [[nodiscard]] std::uint64_t Generation() const noexcept {
        return m_Generation;
    }
    [[nodiscard]] std::uint64_t Fingerprint() const noexcept {
        return m_Fingerprint;
    }
    [[nodiscard]] const std::vector<Node> &Nodes() const noexcept {
        return m_Nodes;
    }
    [[nodiscard]] const std::vector<Link> &Links() const noexcept {
        return m_Links;
    }
    [[nodiscard]] const Node *Find(std::string_view name) const noexcept {
        const auto found = std::find_if(
            m_Nodes.begin(), m_Nodes.end(), [&](const Node &node) {
                return node.Name == name;
            });
        return found == m_Nodes.end() ? nullptr : &*found;
    }

    [[nodiscard]] Result<Graph> Logical() const;
    [[nodiscard]] Result<Graph> Live() const;
    [[nodiscard]] Result<ObservedValue> Read(const ValueRef &value) const;

    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        GraphChanged, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        LayoutChanged change, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        Sampled change, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        Exact change, Function &&callback) const;

private:
    static Result<Graph> Read(std::shared_ptr<Detail::SessionState> session,
                              BML_ObjectRef root, View view);
    template <class Function>
    Result<Behavior::Watch> OpenWatch(BML_BehaviorWatchSpec spec,
                                     Function &&callback) const;

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_ObjectRef m_Root{};
    View m_View = View::Logical;
    std::uint64_t m_Generation = 0;
    std::uint64_t m_Fingerprint = 0;
    std::vector<Node> m_Nodes;
    std::vector<Link> m_Links;

    friend class Session;
};

class Session;
class Builder;
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
            static_cast<RunState>(source.State),
            (source.Flags & BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED) != 0,
            ReadStatus(source.Status)};
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

struct BlockDefinition {
    explicit BlockDefinition(Behavior::Prototype prototype)
        : PrototypeRef(prototype) {
        Settings.emplace_back();
    }

    Behavior::Prototype PrototypeRef;
    std::uint32_t TargetKind = BML_BEHAVIOR_TARGET_OWNER;
    Guid TargetType;
    BML_ObjectRef TargetObject{};
    std::vector<std::vector<Binding>> Settings;
    std::vector<Binding> Pins;
    std::vector<Binding> Locals;
    FramePolicy Frames = signals();
};

struct DeclaredSlot {
    std::uint32_t Kind = 0;
    std::uint32_t Flags = 0;
    std::int32_t Index = 0;
    std::int32_t Occurrence = 0;
    Guid Type;
    std::uint32_t ValueKind = 0;
    std::string Name;
};

struct DeclaredLayout {
    Behavior::Prototype PrototypeRef;
    std::vector<DeclaredSlot> Slots;
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

struct CompiledBlock {
    explicit CompiledBlock(BlockDefinition definition);

    BlockDefinition Definition;
    BML_BehaviorBlock Wire{};
    std::vector<std::vector<BML_BehaviorBinding>> SettingBindings;
    std::vector<BML_BehaviorSettingStage> SettingStages;
    std::vector<BML_BehaviorBinding> Pins;
    std::vector<BML_BehaviorBinding> Locals;
};

class Compiler final {
public:
    [[nodiscard]] Result<std::shared_ptr<const CompiledBlock>> operator()(
        const std::shared_ptr<SessionState> &session,
        BlockDefinition definition) const;
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
    Block(const Block &) noexcept = default;
    Block &operator=(const Block &) noexcept = default;
    Block(Block &&) noexcept = default;
    Block &operator=(Block &&) noexcept = default;

    [[nodiscard]] Result<Behavior::Call> Call(
        const Selector &input = Selector::Only()) const;
    [[nodiscard]] Result<Behavior::Call> Call(std::string_view input) const {
        return Call(Selector::Unique(input));
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) const;
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, std::string_view input) const {
        return Call(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(
        const Selector &input = Selector::Only()) const;
    [[nodiscard]] Result<Task> Start(std::string_view input) const {
        return Start(Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) const;
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, std::string_view input) const {
        return Start(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Instance> Spawn() const;
    [[nodiscard]] Result<Instance> Spawn(BML_ObjectRef owner) const;

private:
    Block(std::shared_ptr<Detail::SessionState> session,
          std::shared_ptr<const Detail::CompiledBlock> definition)
        : m_Session(std::move(session)), m_Definition(std::move(definition)) {}

    template <class Handle, class Function>
    Result<Handle> Open(Function function, BML_ObjectRef owner,
                        const Selector *input) const;

    std::shared_ptr<Detail::SessionState> m_Session;
    std::shared_ptr<const Detail::CompiledBlock> m_Definition;

    friend class Builder;
};

// Builder terminal methods are the one-shot authoring path. Compile once and
// retain the immutable Block when the same definition is run repeatedly.
class Builder {
public:
    Builder(const Builder &) = default;
    Builder &operator=(const Builder &) = default;
    Builder(Builder &&) noexcept = default;
    Builder &operator=(Builder &&) noexcept = default;

    Builder &TargetOwner() & noexcept {
        m_Definition.TargetKind = BML_BEHAVIOR_TARGET_OWNER;
        m_Definition.TargetType = {};
        m_Definition.TargetObject = {};
        return *this;
    }
    Builder &&TargetOwner() && noexcept {
        static_cast<Builder &>(*this).TargetOwner();
        return std::move(*this);
    }
    Builder &Target(Guid type, BML_ObjectRef object) & noexcept {
        m_Definition.TargetKind = BML_BEHAVIOR_TARGET_OBJECT;
        m_Definition.TargetType = type;
        m_Definition.TargetObject = object;
        return *this;
    }
    Builder &&Target(Guid type, BML_ObjectRef object) && noexcept {
        static_cast<Builder &>(*this).Target(type, object);
        return std::move(*this);
    }
    Builder &NullTarget(Guid type) & noexcept {
        m_Definition.TargetKind = BML_BEHAVIOR_TARGET_NULL;
        m_Definition.TargetType = type;
        m_Definition.TargetObject = {};
        return *this;
    }
    Builder &&NullTarget(Guid type) && noexcept {
        static_cast<Builder &>(*this).NullTarget(type);
        return std::move(*this);
    }
    Builder &Setting(Binding binding) & {
        m_Definition.Settings.back().push_back(std::move(binding));
        return *this;
    }
    Builder &&Setting(Binding binding) && {
        static_cast<Builder &>(*this).Setting(std::move(binding));
        return std::move(*this);
    }
    template <class T>
    Builder &Setting(Selector slot, T &&value) & {
        return Setting({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Builder &&Setting(Selector slot, T &&value) && {
        static_cast<Builder &>(*this).Setting(
            {std::move(slot), std::forward<T>(value)});
        return std::move(*this);
    }
    template <class T>
    Builder &Setting(std::string_view name, T &&value) & {
        return Setting(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    Builder &&Setting(std::string_view name, T &&value) && {
        static_cast<Builder &>(*this).Setting(
            Selector::Unique(name), std::forward<T>(value));
        return std::move(*this);
    }
    Builder &NextStage() & {
        if (!m_Definition.Settings.back().empty())
            m_Definition.Settings.emplace_back();
        return *this;
    }
    Builder &&NextStage() && {
        static_cast<Builder &>(*this).NextStage();
        return std::move(*this);
    }
    Builder &Pin(Binding binding) & {
        m_Definition.Pins.push_back(std::move(binding));
        return *this;
    }
    Builder &&Pin(Binding binding) && {
        static_cast<Builder &>(*this).Pin(std::move(binding));
        return std::move(*this);
    }
    template <class T>
    Builder &Pin(Selector slot, T &&value) & {
        return Pin({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Builder &&Pin(Selector slot, T &&value) && {
        static_cast<Builder &>(*this).Pin(
            {std::move(slot), std::forward<T>(value)});
        return std::move(*this);
    }
    template <class T>
    Builder &Pin(std::string_view name, T &&value) & {
        return Pin(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    Builder &&Pin(std::string_view name, T &&value) && {
        static_cast<Builder &>(*this).Pin(
            Selector::Unique(name), std::forward<T>(value));
        return std::move(*this);
    }
    Builder &Local(Binding binding) & {
        m_Definition.Locals.push_back(std::move(binding));
        return *this;
    }
    Builder &&Local(Binding binding) && {
        static_cast<Builder &>(*this).Local(std::move(binding));
        return std::move(*this);
    }
    template <class T>
    Builder &Local(Selector slot, T &&value) & {
        return Local({std::move(slot), std::forward<T>(value)});
    }
    template <class T>
    Builder &&Local(Selector slot, T &&value) && {
        static_cast<Builder &>(*this).Local(
            {std::move(slot), std::forward<T>(value)});
        return std::move(*this);
    }
    template <class T>
    Builder &Local(std::string_view name, T &&value) & {
        return Local(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    Builder &&Local(std::string_view name, T &&value) && {
        static_cast<Builder &>(*this).Local(
            Selector::Unique(name), std::forward<T>(value));
        return std::move(*this);
    }
    template <class... Bindings>
    Builder &Pins(Bindings &&... bindings) & {
        (Pin(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    template <class... Bindings>
    Builder &&Pins(Bindings &&... bindings) && {
        static_cast<Builder &>(*this).Pins(
            std::forward<Bindings>(bindings)...);
        return std::move(*this);
    }
    template <class... Bindings>
    Builder &Settings(Bindings &&... bindings) & {
        (Setting(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    template <class... Bindings>
    Builder &&Settings(Bindings &&... bindings) && {
        static_cast<Builder &>(*this).Settings(
            std::forward<Bindings>(bindings)...);
        return std::move(*this);
    }
    template <class... Bindings>
    Builder &Locals(Bindings &&... bindings) & {
        (Local(std::forward<Bindings>(bindings)), ...);
        return *this;
    }
    template <class... Bindings>
    Builder &&Locals(Bindings &&... bindings) && {
        static_cast<Builder &>(*this).Locals(
            std::forward<Bindings>(bindings)...);
        return std::move(*this);
    }
    Builder &Frames(FramePolicy policy) & noexcept {
        m_Definition.Frames = policy;
        return *this;
    }
    Builder &&Frames(FramePolicy policy) && noexcept {
        static_cast<Builder &>(*this).Frames(policy);
        return std::move(*this);
    }

    [[nodiscard]] Result<Block> Compile() const &;
    [[nodiscard]] Result<Block> Compile() &&;
    [[nodiscard]] Result<Behavior::Call> Call(
        const Selector &input = Selector::Only()) const &;
    [[nodiscard]] Result<Behavior::Call> Call(
        const Selector &input = Selector::Only()) &&;
    [[nodiscard]] Result<Behavior::Call> Call(std::string_view input) const & {
        return Call(Selector::Unique(input));
    }
    [[nodiscard]] Result<Behavior::Call> Call(std::string_view input) && {
        return std::move(*this).Call(Selector::Unique(input));
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) const &;
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) &&;
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, std::string_view input) const & {
        return Call(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Behavior::Call> Call(
        BML_ObjectRef owner, std::string_view input) && {
        return std::move(*this).Call(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(
        const Selector &input = Selector::Only()) const &;
    [[nodiscard]] Result<Task> Start(
        const Selector &input = Selector::Only()) &&;
    [[nodiscard]] Result<Task> Start(std::string_view input) const & {
        return Start(Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(std::string_view input) && {
        return std::move(*this).Start(Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) const &;
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, const Selector &input = Selector::Only()) &&;
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, std::string_view input) const & {
        return Start(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Task> Start(
        BML_ObjectRef owner, std::string_view input) && {
        return std::move(*this).Start(owner, Selector::Unique(input));
    }
    [[nodiscard]] Result<Instance> Spawn() const &;
    [[nodiscard]] Result<Instance> Spawn() &&;
    [[nodiscard]] Result<Instance> Spawn(BML_ObjectRef owner) const &;
    [[nodiscard]] Result<Instance> Spawn(BML_ObjectRef owner) &&;

private:
    Builder(std::shared_ptr<Detail::SessionState> session,
            Prototype prototype)
        : m_Session(std::move(session)), m_Definition(prototype) {}

    [[nodiscard]] Result<Block> Compile(Detail::BlockDefinition definition) const;

    std::shared_ptr<Detail::SessionState> m_Session;
    Detail::BlockDefinition m_Definition;

    friend class Session;
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
            !BML_IFACE_HAS(api, BML_BehaviorInterface, CloseWatch))
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
    [[nodiscard]] Builder Use(Prototype prototype) const {
        return Builder(m_State, prototype);
    }
    [[nodiscard]] Builder Use(Guid prototype) const {
        return Use(Prototype(prototype));
    }
    [[nodiscard]] Builder Use(CKGUID prototype) const {
        return Use(Prototype(prototype));
    }
    [[nodiscard]] Result<Graph> Inspect(BML_ObjectRef root) const {
        return Graph::Read(m_State, root, View::Logical);
    }
    void Close() noexcept {
        if (m_State)
            m_State->Close();
        m_State.reset();
    }

private:
    std::shared_ptr<Detail::SessionState> m_State;
};

namespace Detail {

inline Status BlockError(std::uint32_t error, std::uint32_t phase,
                         Prototype prototype, Guid type,
                         std::string message) {
    Status status;
    status.Error = error;
    status.Phase = phase;
    status.Prototype = prototype.Id;
    status.Type = type;
    status.Message = std::move(message);
    return status;
}

inline Result<DeclaredLayout> ReadDeclared(
    const std::shared_ptr<SessionState> &session, Prototype prototype) {
    if (!session || !session->Api || !session->Handle)
        return Result<DeclaredLayout>::Failure(BML_ERROR_INVALID_HANDLE);

    BML_BehaviorPrototypeRef requested{};
    requested.StructSize = sizeof(requested);
    requested.Prototype = prototype.Id.Wire();
    requested.Generation = prototype.Generation;
    BML_BehaviorLayout wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint32_t payloadSize = 0;
    int code = session->Api->ReadDeclaredLayout(
        session->Handle, &requested, &wire, nullptr, 0, &payloadSize, &status);
    if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
        return Result<DeclaredLayout>::Failure(code, ReadStatus(status));
    if (code == BML_OK && payloadSize != 0) {
        return Result<DeclaredLayout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, prototype, {},
                       "The declared Layout reported an incomplete payload."));
    }

    std::vector<std::uint8_t> payload(payloadSize);
    if (payloadSize) {
        wire = {};
        wire.StructSize = sizeof(wire);
        status = EmptyStatus();
        std::uint32_t written = 0;
        code = session->Api->ReadDeclaredLayout(
            session->Handle, &requested, &wire, payload.data(), payloadSize,
            &written, &status);
        if (code != BML_OK)
            return Result<DeclaredLayout>::Failure(code, ReadStatus(status));
        if (written != payload.size()) {
            return Result<DeclaredLayout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                           BML_BEHAVIOR_PHASE_LAYOUT, prototype, {},
                           "The declared Layout payload is malformed."));
        }
    }

    const bool samePrototype =
        wire.Prototype.Prototype.Data1 == prototype.Id.Data1 &&
        wire.Prototype.Prototype.Data2 == prototype.Id.Data2;
    if (wire.StructSize < sizeof(wire) ||
        wire.Prototype.StructSize < sizeof(wire.Prototype) ||
        wire.Origin != BML_BEHAVIOR_LAYOUT_DECLARED || !samePrototype ||
        !wire.Prototype.Generation ||
        (prototype.Generation &&
         wire.Prototype.Generation != prototype.Generation)) {
        return Result<DeclaredLayout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, prototype, {},
                       "The declared Layout does not identify the requested Prototype."));
    }

    const Prototype resolved(wire.Prototype.Prototype,
                             wire.Prototype.Generation);
    if (!RecordsFit<BML_BehaviorManagerInfo>(
            payload, wire.ManagerOffset, wire.ManagerCount) ||
        !RecordsFit<BML_BehaviorSlotRecord>(
            payload, wire.SlotOffset, wire.SlotCount)) {
        return Result<DeclaredLayout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, resolved, {},
                       "The declared Layout record ranges are malformed."));
    }
    for (std::uint32_t index = 0; index < wire.ManagerCount; ++index) {
        BML_BehaviorManagerInfo manager{};
        if (!RecordAt(payload, wire.ManagerOffset, index, manager)) {
            return Result<DeclaredLayout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                           BML_BEHAVIOR_PHASE_LAYOUT, resolved, {},
                           "The declared Layout manager list is malformed."));
        }
    }

    DeclaredLayout layout;
    layout.PrototypeRef = resolved;
    layout.Slots.reserve(wire.SlotCount);
    for (std::uint32_t index = 0; index < wire.SlotCount; ++index) {
        BML_BehaviorSlotRecord record{};
        if (!RecordAt(payload, wire.SlotOffset, index, record)) {
            return Result<DeclaredLayout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                           BML_BEHAVIOR_PHASE_LAYOUT, resolved, {},
                           "The declared Layout slot list is malformed."));
        }
        DeclaredSlot slot;
        slot.Kind = record.Kind;
        slot.Flags = record.Flags;
        slot.Index = record.Index;
        slot.Occurrence = record.Occurrence;
        slot.Type = record.Type;
        slot.ValueKind = record.ValueKind;
        if (!TextAt(payload, record.Name.Offset, record.Name.Length,
                    slot.Name)) {
            return Result<DeclaredLayout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                           BML_BEHAVIOR_PHASE_LAYOUT, resolved, {},
                           "A declared Layout slot name is malformed."));
        }
        layout.Slots.push_back(std::move(slot));
    }
    return Result<DeclaredLayout>::Success(std::move(layout),
                                            ReadStatus(status));
}

inline Status CheckDefinition(const BlockDefinition &definition) {
    constexpr std::size_t maximum =
        (std::numeric_limits<std::uint32_t>::max)();
    if (!definition.PrototypeRef.Id) {
        return BlockError(BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND,
                          BML_BEHAVIOR_PHASE_PROTOTYPE,
                          definition.PrototypeRef, {},
                          "A Block requires a Prototype GUID.");
    }
    if ((definition.TargetKind == BML_BEHAVIOR_TARGET_OBJECT ||
         definition.TargetKind == BML_BEHAVIOR_TARGET_NULL) &&
        !definition.TargetType) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, {},
                          "An explicit Target requires a parameter type.");
    }
    if (definition.TargetKind == BML_BEHAVIOR_TARGET_OBJECT &&
        !definition.TargetObject.Domain) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, definition.TargetType,
                          "An explicit Target requires a live object reference.");
    }
    if (definition.TargetKind != BML_BEHAVIOR_TARGET_OWNER &&
        definition.TargetKind != BML_BEHAVIOR_TARGET_OBJECT &&
        definition.TargetKind != BML_BEHAVIOR_TARGET_NULL) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, definition.TargetType,
                          "The Block Target kind is unknown.");
    }
    const bool bounded =
        definition.Frames.Kind == BML_BEHAVIOR_FRAMES_SIGNALS ||
        definition.Frames.Kind == BML_BEHAVIOR_FRAMES_EACH_FRAME;
    if ((bounded && !definition.Frames.Limit) ||
        (!bounded && definition.Frames.Limit) ||
        (!bounded && definition.Frames.Kind != BML_BEHAVIOR_FRAMES_LATEST &&
         definition.Frames.Kind != BML_BEHAVIOR_FRAMES_NONE)) {
        return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                          BML_BEHAVIOR_PHASE_NONE,
                          definition.PrototypeRef, {},
                          "The Block Frame policy is invalid.");
    }
    if (definition.Settings.size() > maximum ||
        definition.Pins.size() > maximum ||
        definition.Locals.size() > maximum) {
        return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                          BML_BEHAVIOR_PHASE_NONE,
                          definition.PrototypeRef, {},
                          "The Block contains too many bindings.");
    }
    for (const auto &stage : definition.Settings) {
        if (stage.size() > maximum) {
            return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              definition.PrototypeRef, {},
                              "A Block contains too many Settings in one stage.");
        }
    }
    return {};
}

inline std::string SelectorLabel(const BML_BehaviorSelector &selector) {
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY)
        return "<only>";
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_INDEX)
        return "#" + std::to_string(selector.Index);
    return "'" + std::string(selector.Name.Data ? selector.Name.Data : "",
                              selector.Name.Length) + "'";
}

inline Status CheckSetting(const DeclaredLayout &layout,
                           const Binding &binding) {
    const BML_BehaviorSelector selector = binding.Slot.Wire();
    std::vector<const DeclaredSlot *> matches;
    for (const DeclaredSlot &slot : layout.Slots) {
        if (slot.Kind != BML_BEHAVIOR_SLOT_SETTING)
            continue;
        if (selector.Kind == BML_BEHAVIOR_SELECTOR_INDEX) {
            if (slot.Index == selector.Index)
                matches.push_back(&slot);
        } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY) {
            matches.push_back(&slot);
        } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_NAME ||
                   selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) {
            const std::string_view name(
                selector.Name.Data ? selector.Name.Data : "",
                selector.Name.Length);
            if (slot.Name == name)
                matches.push_back(&slot);
        } else {
            return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, {},
                              "A Setting selector kind is unknown.");
        }
    }

    if (matches.empty()) {
        return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, {},
                          "Setting " + SelectorLabel(selector) +
                              " is absent from the declared Layout.");
    }
    const DeclaredSlot *slot = nullptr;
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY ||
        selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) {
        if (matches.size() != 1) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, {},
                              "Setting " + SelectorLabel(selector) +
                                  " is ambiguous in the declared Layout.");
        }
        slot = matches.front();
    } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_NAME) {
        if (selector.Occurrence < 0) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, {},
                              "The requested Setting occurrence is absent from the declared Layout.");
        }
        for (const DeclaredSlot *candidate : matches) {
            if (candidate->Occurrence != selector.Occurrence)
                continue;
            if (slot) {
                return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                                  BML_BEHAVIOR_PHASE_SETTINGS,
                                  layout.PrototypeRef, {},
                                  "The requested Setting occurrence is ambiguous in the declared Layout.");
            }
            slot = candidate;
        }
        if (!slot) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, {},
                              "The requested Setting occurrence is absent from the declared Layout.");
        }
    } else {
        if (matches.size() != 1) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, {},
                              "The Setting index is ambiguous in the declared Layout.");
        }
        slot = matches.front();
    }

    if (!(slot->Flags & BML_BEHAVIOR_SLOT_VALUE_SUPPORTED) ||
        !slot->ValueKind) {
        return BlockError(BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, slot->Type,
                          "The Setting parameter type has no public value form.");
    }
    if (slot->ValueKind != static_cast<std::uint32_t>(binding.Value.Kind())) {
        return BlockError(BML_BEHAVIOR_ERROR_TYPE_MISMATCH,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, slot->Type,
                          "The Setting value form does not match the declared Layout.");
    }
    return {};
}

inline CompiledBlock::CompiledBlock(BlockDefinition definition)
    : Definition(std::move(definition)) {
    while (Definition.Settings.size() > 1 &&
           Definition.Settings.back().empty())
        Definition.Settings.pop_back();
    if (Definition.Settings.size() == 1 && Definition.Settings.front().empty())
        Definition.Settings.clear();

    SettingBindings.reserve(Definition.Settings.size());
    SettingStages.reserve(Definition.Settings.size());
    for (const std::vector<Binding> &stage : Definition.Settings) {
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
        stage.Settings = bindings.empty() ? nullptr : bindings.data();
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
    add(Definition.Pins, Pins);
    add(Definition.Locals, Locals);

    Wire.StructSize = sizeof(Wire);
    Wire.Prototype = Definition.PrototypeRef.Id.Wire();
    Wire.Target.StructSize = sizeof(Wire.Target);
    Wire.Target.Kind = Definition.TargetKind;
    Wire.Target.Type = Definition.TargetType.Wire();
    Wire.Target.Object = Definition.TargetObject;
    Wire.SettingStages = SettingStages.empty() ? nullptr : SettingStages.data();
    Wire.SettingStageCount = static_cast<std::uint32_t>(SettingStages.size());
    Wire.Pins = Pins.empty() ? nullptr : Pins.data();
    Wire.PinCount = static_cast<std::uint32_t>(Pins.size());
    Wire.Locals = Locals.empty() ? nullptr : Locals.data();
    Wire.LocalCount = static_cast<std::uint32_t>(Locals.size());
    Wire.Frames.StructSize = sizeof(Wire.Frames);
    Wire.Frames.Kind = Definition.Frames.Kind;
    Wire.Frames.Limit = Definition.Frames.Limit;
    Wire.PrototypeGeneration = Definition.PrototypeRef.Generation;
}

inline Result<std::shared_ptr<const CompiledBlock>> Compiler::operator()(
    const std::shared_ptr<SessionState> &session,
    BlockDefinition definition) const {
    if (!session || !session->Api || !session->Handle) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    }
    Status checked = CheckDefinition(definition);
    if (!checked) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_PARAMETER, std::move(checked));
    }
    auto declared = ReadDeclared(session, definition.PrototypeRef);
    if (!declared) {
        if (declared.Code() == BML_ERROR_UNAVAILABLE) {
            std::shared_ptr<const CompiledBlock> block =
                std::make_shared<CompiledBlock>(std::move(definition));
            return Result<std::shared_ptr<const CompiledBlock>>::Success(
                std::move(block));
        }
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            declared.Code(), declared.Detail());
    }
    if (!definition.Settings.empty()) {
        for (const Binding &setting : definition.Settings.front()) {
            checked = CheckSetting(declared.Value(), setting);
            if (!checked) {
                return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                    BML_ERROR_FAIL, std::move(checked));
            }
        }
    }
    definition.PrototypeRef = declared.Value().PrototypeRef;
    std::shared_ptr<const CompiledBlock> block =
        std::make_shared<CompiledBlock>(std::move(definition));
    return Result<std::shared_ptr<const CompiledBlock>>::Success(
        std::move(block), declared.Detail());
}

} // namespace Detail

namespace Detail {

inline bool ReadObserved(const BML_BehaviorGraphValue &source,
                         const std::vector<std::uint8_t> &payload,
                         ObservedValue &out) {
    if (source.StructSize < sizeof(source))
        return false;
    out = {};
    out.State = static_cast<ObservationState>(source.State);
    out.Source = static_cast<Relation>(source.Relation);
    out.Type = source.Type;
    if (source.State != BML_BEHAVIOR_VALUE_AVAILABLE)
        return source.Kind == 0 && source.ValueSize == 0;
    if (source.Kind < BML_BEHAVIOR_VALUE_BOOL ||
        source.Kind > BML_BEHAVIOR_VALUE_OBJECT)
        return false;
    out.Kind = static_cast<ValueKind>(source.Kind);
    BML_BehaviorPoutRecord value{};
    value.StructSize = sizeof(value);
    value.Kind = source.Kind;
    value.ValueOffset = source.ValueOffset;
    value.ValueSize = source.ValueSize;
    return ReadPoutData(value, payload, out.Data);
}

inline bool ReadWatchValue(const BML_BehaviorWatchValue &source,
                           ObservedValue &out) {
    if (source.StructSize < sizeof(source) ||
        source.Value.StructSize < sizeof(source.Value))
        return false;
    out = {};
    out.State = static_cast<ObservationState>(source.State);
    out.Source = static_cast<Relation>(source.Relation);
    out.Type = source.Value.Type;
    if (source.State != BML_BEHAVIOR_VALUE_AVAILABLE)
        return source.Value.Kind == 0;
    if (source.Value.Kind < BML_BEHAVIOR_VALUE_BOOL ||
        source.Value.Kind > BML_BEHAVIOR_VALUE_OBJECT)
        return false;
    out.Kind = static_cast<ValueKind>(source.Value.Kind);
    switch (source.Value.Kind) {
    case BML_BEHAVIOR_VALUE_BOOL:
        out.Data = source.Value.Data.Bool != 0;
        break;
    case BML_BEHAVIOR_VALUE_INT32:
        out.Data = source.Value.Data.Int32;
        break;
    case BML_BEHAVIOR_VALUE_FLOAT32:
        out.Data = source.Value.Data.Float32;
        break;
    case BML_BEHAVIOR_VALUE_UTF8:
        if (!source.Value.Data.Utf8.Data && source.Value.Data.Utf8.Length)
            return false;
        out.Data = std::string(
            source.Value.Data.Utf8.Data ? source.Value.Data.Utf8.Data : "",
            source.Value.Data.Utf8.Length);
        break;
    case BML_BEHAVIOR_VALUE_VEC2: out.Data = source.Value.Data.Vec2; break;
    case BML_BEHAVIOR_VALUE_VEC3: out.Data = source.Value.Data.Vec3; break;
    case BML_BEHAVIOR_VALUE_QUATERNION:
        out.Data = source.Value.Data.Quaternion; break;
    case BML_BEHAVIOR_VALUE_EULER: out.Data = source.Value.Data.Euler; break;
    case BML_BEHAVIOR_VALUE_RECT: out.Data = source.Value.Data.Rect; break;
    case BML_BEHAVIOR_VALUE_COLOR: out.Data = source.Value.Data.Color; break;
    case BML_BEHAVIOR_VALUE_BOX: out.Data = source.Value.Data.Box; break;
    case BML_BEHAVIOR_VALUE_MAT4: out.Data = source.Value.Data.Mat4; break;
    case BML_BEHAVIOR_VALUE_OBJECT: out.Data = source.Value.Data.Object; break;
    default: return false;
    }
    return true;
}

inline bool ReadChange(const BML_BehaviorWatchEvent *source, Change &out) {
    if (!source || source->StructSize < sizeof(*source))
        return false;
    out = {};
    out.Kind = static_cast<ChangeKind>(source->Kind);
    out.Sequence = source->Sequence;
    out.GameFrame = source->Frame;
    out.Before = source->Before;
    out.After = source->After;
    return ReadWatchValue(source->PreviousValue, out.PreviousValue) &&
           ReadWatchValue(source->CurrentValue, out.CurrentValue);
}

template <class Function>
struct WatchFunction {
    using Callable = std::decay_t<Function>;

    explicit WatchFunction(Function &&function)
        : Value(std::forward<Function>(function)) {}

    static void BML_BEHAVIOR_CALL Retain(void *state) {
        ++static_cast<WatchFunction *>(state)->References;
    }
    static void BML_BEHAVIOR_CALL Release(void *state) {
        auto *self = static_cast<WatchFunction *>(state);
        if (self->References.fetch_sub(1) == 1)
            delete self;
    }
    static void BML_BEHAVIOR_CALL Invoke(
        void *state, const BML_BehaviorWatchEvent *source) {
        Change change;
        if (!ReadChange(source, change))
            throw std::runtime_error("The Behavior Watch event is malformed.");
        std::invoke(static_cast<WatchFunction *>(state)->Value, change);
    }

    std::atomic<std::uint32_t> References{1};
    Callable Value;
};

} // namespace Detail

inline Result<Graph> Graph::Read(
    std::shared_ptr<Detail::SessionState> session,
    BML_ObjectRef root, View view) {
    if (!session || !session->Api || !session->Handle || !root.Domain)
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorGraph wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = session->Api->Inspect(
            session->Handle, root, static_cast<std::uint32_t>(view),
            &wire, nullptr, 0, &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = session->Api->Inspect(
                session->Handle, root, static_cast<std::uint32_t>(view),
                &wire, payload.data(), payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<Graph>::Failure(code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        }
        if (wire.StructSize < sizeof(wire) || wire.View !=
                static_cast<std::uint32_t>(view) ||
            !Detail::RecordsFit<BML_BehaviorGraphNode>(
                payload, wire.NodeOffset, wire.NodeCount) ||
            !Detail::RecordsFit<BML_BehaviorGraphLink>(
                payload, wire.LinkOffset, wire.LinkCount))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);

        Graph graph;
        graph.m_Session = std::move(session);
        graph.m_Root = wire.Root;
        graph.m_View = view;
        graph.m_Generation = wire.Generation;
        graph.m_Fingerprint = wire.Fingerprint;
        graph.m_Nodes.reserve(wire.NodeCount);
        for (std::uint32_t index = 0; index < wire.NodeCount; ++index) {
            BML_BehaviorGraphNode record{};
            if (!Detail::RecordAt(payload, wire.NodeOffset, index, record) ||
                !Detail::RecordsFit<BML_BehaviorGraphPort>(
                    payload, record.PortOffset, record.PortCount))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            Node node;
            node.Id = record.Id;
            node.Object = record.Object;
            node.Parent = record.Parent;
            node.Prototype = record.Prototype;
            node.Priority = record.Priority;
            node.Active = record.Active != 0;
            if (!Detail::TextAt(payload, record.Name.Offset,
                                record.Name.Length, node.Name))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            node.Ports.reserve(record.PortCount);
            for (std::uint32_t portIndex = 0;
                 portIndex < record.PortCount; ++portIndex) {
                BML_BehaviorGraphPort portRecord{};
                if (!Detail::RecordAt(payload, record.PortOffset,
                                      portIndex, portRecord))
                    return Result<Graph>::Failure(
                        BML_ERROR_MALFORMED_MESSAGE);
                Port port;
                port.Node = portRecord.Node;
                port.Kind = portRecord.Kind;
                port.Index = portRecord.Index;
                port.Occurrence = portRecord.Occurrence;
                port.Active = portRecord.Active != 0;
                if (!Detail::TextAt(payload, portRecord.Name.Offset,
                                    portRecord.Name.Length, port.Name))
                    return Result<Graph>::Failure(
                        BML_ERROR_MALFORMED_MESSAGE);
                node.Ports.push_back(std::move(port));
            }
            graph.m_Nodes.push_back(std::move(node));
        }
        graph.m_Links.reserve(wire.LinkCount);
        for (std::uint32_t index = 0; index < wire.LinkCount; ++index) {
            BML_BehaviorGraphLink record{};
            if (!Detail::RecordAt(payload, wire.LinkOffset, index, record))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            graph.m_Links.push_back({
                record.Id, record.Object,
                {record.SourceNode, record.SourceKind, record.SourceIndex},
                {record.TargetNode, record.TargetKind, record.TargetIndex},
                record.InitialDelay,
                record.RemainingDelay,
                static_cast<TruthValue>(record.Pending)});
        }
        return Result<Graph>::Success(std::move(graph),
                                      Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::Logical() const {
    return Read(m_Session, m_Root, View::Logical);
}

inline Result<Graph> Graph::Live() const {
    return Read(m_Session, m_Root, View::Live);
}

inline Result<ObservedValue> Graph::Read(const ValueRef &value) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle ||
        !value.Node.Domain)
        return Result<ObservedValue>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorSelector selector = value.Slot.Wire();
        BML_BehaviorGraphValue wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadValue(
            m_Session->Handle, value.Node, value.Kind, &selector,
            BML_BEHAVIOR_READ_NON_FORCING, &wire, nullptr, 0,
            &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<ObservedValue>::Failure(
                code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = m_Session->Api->ReadValue(
                m_Session->Handle, value.Node, value.Kind, &selector,
                BML_BEHAVIOR_READ_NON_FORCING, &wire, payload.data(),
                payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<ObservedValue>::Failure(
                    code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<ObservedValue>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        ObservedValue observed;
        if (!Detail::ReadObserved(wire, payload, observed))
            return Result<ObservedValue>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<ObservedValue>::Success(
            std::move(observed), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<ObservedValue>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<ObservedValue>::Failure(BML_ERROR_FAIL);
    }
}

template <class Function>
Result<Behavior::Watch> Graph::OpenWatch(
    BML_BehaviorWatchSpec spec, Function &&callback) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Behavior::Watch>::Failure(BML_ERROR_INVALID_HANDLE);
    using Holder = Detail::WatchFunction<Function>;
    Holder *holder = nullptr;
    try {
        holder = new Holder(std::forward<Function>(callback));
        BML_BehaviorWatchFunction function{};
        function.StructSize = sizeof(function);
        function.State = holder;
        function.Retain = &Holder::Retain;
        function.Release = &Holder::Release;
        function.Invoke = &Holder::Invoke;
        BML_BehaviorWatch handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->Watch(
            m_Session->Handle, &spec, &function, &handle, &status);
        Holder::Release(holder);
        holder = nullptr;
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_Session->Api->CloseWatch(handle);
            return Result<Behavior::Watch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Behavior::Watch>::Success(
            Behavior::Watch(m_Session->Api, handle),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        if (holder)
            Holder::Release(holder);
        return Result<Behavior::Watch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        if (holder)
            Holder::Release(holder);
        return Result<Behavior::Watch>::Failure(BML_ERROR_FAIL);
    }
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    GraphChanged, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_GRAPH;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Root = m_Root;
    spec.Slot.StructSize = sizeof(spec.Slot);
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    LayoutChanged change, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_LAYOUT;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Node = change.Node;
    spec.Slot.StructSize = sizeof(spec.Slot);
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    Sampled change, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_SAMPLED_VALUE;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Node = change.Value.Node;
    spec.SlotKind = change.Value.Kind;
    spec.Slot = change.Value.Slot.Wire();
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    Exact change, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_EXACT_VALUE;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Node = change.Value.Node;
    spec.SlotKind = change.Value.Kind;
    spec.Slot = change.Value.Slot.Wire();
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

inline Result<Block> Builder::Compile(Detail::BlockDefinition definition) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Block>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        auto compiled = Detail::Compiler{}(m_Session, std::move(definition));
        if (!compiled)
            return Result<Block>::Failure(compiled.Code(), compiled.Detail());
        return Result<Block>::Success(
            Block(m_Session, std::move(compiled).Value()),
            compiled.Detail());
    } catch (const std::bad_alloc &) {
        return Result<Block>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Block>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Block> Builder::Compile() const & {
    try {
        return Compile(m_Definition);
    } catch (const std::bad_alloc &) {
        return Result<Block>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Block>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Block> Builder::Compile() && {
    return Compile(std::move(m_Definition));
}

template <class Handle, class Function>
Result<Handle> Block::Open(Function function, BML_ObjectRef owner,
                           const Selector *input) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle || !m_Definition)
        return Result<Handle>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
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
            m_Session->Handle, owner, &m_Definition->Wire, selectorPointer,
            &run, &info, &status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Handle>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Handle>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<Handle>::Success(
            Handle(Detail::Run(m_Session->Api, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Handle>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Handle>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Call> Block::Call(const Selector &input) const {
    return Call(BML_ObjectRef{}, input);
}

inline Result<Behavior::Call> Block::Call(BML_ObjectRef owner,
                                           const Selector &input) const {
    if (!m_Session || !m_Session->Api)
        return Result<Behavior::Call>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Behavior::Call>(m_Session->Api->Call, owner, &input);
}

inline Result<Task> Block::Start(const Selector &input) const {
    return Start(BML_ObjectRef{}, input);
}

inline Result<Task> Block::Start(BML_ObjectRef owner,
                                 const Selector &input) const {
    if (!m_Session || !m_Session->Api)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Task>(m_Session->Api->Start, owner, &input);
}

inline Result<Instance> Block::Spawn() const {
    return Spawn(BML_ObjectRef{});
}

inline Result<Instance> Block::Spawn(BML_ObjectRef owner) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle || !m_Definition)
        return Result<Instance>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->Spawn(
            m_Session->Handle, owner, &m_Definition->Wire,
            &run, &info, &status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<Instance>::Success(
            Instance(Detail::Run(m_Session->Api, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Call> Builder::Call(const Selector &input) const & {
    auto block = Compile();
    if (!block)
        return Result<Behavior::Call>::Failure(block.Code(), block.Detail());
    return block->Call(input);
}

inline Result<Behavior::Call> Builder::Call(BML_ObjectRef owner,
                                             const Selector &input) const & {
    auto block = Compile();
    if (!block)
        return Result<Behavior::Call>::Failure(block.Code(), block.Detail());
    return block->Call(owner, input);
}

inline Result<Behavior::Call> Builder::Call(const Selector &input) && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Behavior::Call>::Failure(block.Code(), block.Detail());
    return block->Call(input);
}

inline Result<Behavior::Call> Builder::Call(BML_ObjectRef owner,
                                             const Selector &input) && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Behavior::Call>::Failure(block.Code(), block.Detail());
    return block->Call(owner, input);
}

inline Result<Task> Builder::Start(const Selector &input) const & {
    auto block = Compile();
    if (!block)
        return Result<Task>::Failure(block.Code(), block.Detail());
    return block->Start(input);
}

inline Result<Task> Builder::Start(BML_ObjectRef owner,
                                   const Selector &input) const & {
    auto block = Compile();
    if (!block)
        return Result<Task>::Failure(block.Code(), block.Detail());
    return block->Start(owner, input);
}

inline Result<Task> Builder::Start(const Selector &input) && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Task>::Failure(block.Code(), block.Detail());
    return block->Start(input);
}

inline Result<Task> Builder::Start(BML_ObjectRef owner,
                                   const Selector &input) && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Task>::Failure(block.Code(), block.Detail());
    return block->Start(owner, input);
}

inline Result<Instance> Builder::Spawn() const & {
    auto block = Compile();
    if (!block)
        return Result<Instance>::Failure(block.Code(), block.Detail());
    return block->Spawn();
}

inline Result<Instance> Builder::Spawn(BML_ObjectRef owner) const & {
    auto block = Compile();
    if (!block)
        return Result<Instance>::Failure(block.Code(), block.Detail());
    return block->Spawn(owner);
}

inline Result<Instance> Builder::Spawn() && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Instance>::Failure(block.Code(), block.Detail());
    return block->Spawn();
}

inline Result<Instance> Builder::Spawn(BML_ObjectRef owner) && {
    auto block = std::move(*this).Compile();
    if (!block)
        return Result<Instance>::Failure(block.Code(), block.Detail());
    return block->Spawn(owner);
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
