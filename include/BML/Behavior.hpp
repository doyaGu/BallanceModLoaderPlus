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

enum class Error : std::uint32_t {
    None = BML_BEHAVIOR_ERROR_NONE,
    OwnerUnavailable = BML_BEHAVIOR_ERROR_OWNER_UNAVAILABLE,
    PrototypeNotFound = BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND,
    RequiredManagerMissing = BML_BEHAVIOR_ERROR_REQUIRED_MANAGER_MISSING,
    CreationFailed = BML_BEHAVIOR_ERROR_CREATION_FAILED,
    InitializationFailed = BML_BEHAVIOR_ERROR_INITIALIZATION_FAILED,
    TargetInvalid = BML_BEHAVIOR_ERROR_TARGET_INVALID,
    CallbackFailed = BML_BEHAVIOR_ERROR_CALLBACK_FAILED,
    SlotNotFound = BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
    SlotAmbiguous = BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
    LayoutChanged = BML_BEHAVIOR_ERROR_LAYOUT_CHANGED,
    TypeMismatch = BML_BEHAVIOR_ERROR_TYPE_MISMATCH,
    ValueInvalid = BML_BEHAVIOR_ERROR_VALUE_INVALID,
    StateInvalid = BML_BEHAVIOR_ERROR_STATE_INVALID,
    NativeError = BML_BEHAVIOR_ERROR_NATIVE_ERROR,
    BreakUnsupported = BML_BEHAVIOR_ERROR_BREAK_UNSUPPORTED,
    PoutUnsupported = BML_BEHAVIOR_ERROR_POUT_UNSUPPORTED,
    PoutUnavailable = BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE,
    FrameQueueFull = BML_BEHAVIOR_ERROR_FRAME_QUEUE_FULL,
    Cancelled = BML_BEHAVIOR_ERROR_CANCELLED,
    PrototypeChanged = BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED,
    PrototypeLoadFailed = BML_BEHAVIOR_ERROR_PROTOTYPE_LOAD_FAILED,
    LayoutUnavailable = BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
    ParameterTypeUnavailable = BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNAVAILABLE,
    ParameterTypeUnsupported = BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED,
    DetachedUnsupported = BML_BEHAVIOR_ERROR_DETACHED_UNSUPPORTED,
    ObserverUnavailable = BML_BEHAVIOR_ERROR_OBSERVER_UNAVAILABLE,
    GraphChanged = BML_BEHAVIOR_ERROR_GRAPH_CHANGED,
    GraphLocalityInvalid = BML_BEHAVIOR_ERROR_GRAPH_LOCALITY_INVALID,
    DelayInvalid = BML_BEHAVIOR_ERROR_DELAY_INVALID,
    SameFrameCycle = BML_BEHAVIOR_ERROR_SAME_FRAME_CYCLE,
    SharedSourceCycle = BML_BEHAVIOR_ERROR_SHARED_SOURCE_CYCLE,
    PushCycle = BML_BEHAVIOR_ERROR_PUSH_CYCLE,
    InterfaceUnsupported = BML_BEHAVIOR_ERROR_INTERFACE_UNSUPPORTED,
    SourceConflict = BML_BEHAVIOR_ERROR_SOURCE_CONFLICT,
    SourceOrderCycle = BML_BEHAVIOR_ERROR_SOURCE_ORDER_CYCLE,
    OrderingTargetMismatch = BML_BEHAVIOR_ERROR_ORDERING_TARGET_MISMATCH,
    OverlayOrderCycle = BML_BEHAVIOR_ERROR_OVERLAY_ORDER_CYCLE,
    LinkNotFound = BML_BEHAVIOR_ERROR_LINK_NOT_FOUND,
    PathAmbiguous = BML_BEHAVIOR_ERROR_PATH_AMBIGUOUS,
    PathCycle = BML_BEHAVIOR_ERROR_PATH_CYCLE,
    QueryNotFound = BML_BEHAVIOR_ERROR_QUERY_NOT_FOUND,
    QueryAmbiguous = BML_BEHAVIOR_ERROR_QUERY_AMBIGUOUS,
    WorldBoundValue = BML_BEHAVIOR_ERROR_WORLD_BOUND_VALUE,
    RevertConflict = BML_BEHAVIOR_ERROR_REVERT_CONFLICT,
    TargetCardinality = BML_BEHAVIOR_ERROR_TARGET_CARDINALITY,
    SourceInvalid = BML_BEHAVIOR_ERROR_SOURCE_INVALID,
    OperationInvalid = BML_BEHAVIOR_ERROR_OPERATION_INVALID,
    Busy = BML_BEHAVIOR_ERROR_BUSY,
    Unavailable = BML_BEHAVIOR_ERROR_UNAVAILABLE,
    WrongThread = BML_BEHAVIOR_ERROR_WRONG_THREAD,
};

enum class Phase : std::uint32_t {
    None = BML_BEHAVIOR_PHASE_NONE,
    Prototype = BML_BEHAVIOR_PHASE_PROTOTYPE,
    Manager = BML_BEHAVIOR_PHASE_MANAGER,
    Creation = BML_BEHAVIOR_PHASE_CREATION,
    Initialization = BML_BEHAVIOR_PHASE_INITIALIZATION,
    Layout = BML_BEHAVIOR_PHASE_LAYOUT,
    Owner = BML_BEHAVIOR_PHASE_OWNER,
    Target = BML_BEHAVIOR_PHASE_TARGET,
    Settings = BML_BEHAVIOR_PHASE_SETTINGS,
    Callback = BML_BEHAVIOR_PHASE_CALLBACK,
    Binding = BML_BEHAVIOR_PHASE_BINDING,
    Execution = BML_BEHAVIOR_PHASE_EXECUTION,
    Teardown = BML_BEHAVIOR_PHASE_TEARDOWN,
    Edit = BML_BEHAVIOR_PHASE_EDIT,
};

struct Status {
    Behavior::Error Error = Behavior::Error::None;
    Behavior::Phase Phase = Behavior::Phase::None;
    std::int32_t CkError = 0;
    std::int32_t NativeResult = 0;
    Guid Prototype;
    Guid Type;
    std::string Message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Error == Behavior::Error::None;
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

    [[nodiscard]] bool HasValue() const noexcept { return m_Value.has_value(); }
    [[nodiscard]] T &Value() & { return m_Value.value(); }
    [[nodiscard]] const T &Value() const & { return m_Value.value(); }
    [[nodiscard]] T &&Value() && { return std::move(m_Value).value(); }
    [[nodiscard]] T *operator->() { return &Value(); }
    [[nodiscard]] const T *operator->() const { return &Value(); }
    [[nodiscard]] T &operator*() & { return Value(); }
    [[nodiscard]] const T &operator*() const & { return Value(); }

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

using SettingStage = std::vector<Binding>;

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
    Ready = BML_BEHAVIOR_RUN_READY,
    Pending = BML_BEHAVIOR_RUN_PENDING,
    Failed = BML_BEHAVIOR_RUN_FAILED,
};

enum class PulseResult : std::uint32_t {
    Ran = BML_BEHAVIOR_ADMISSION_EXECUTED,
    Queued = BML_BEHAVIOR_ADMISSION_QUEUED,
};

enum class DetachedSupport {
    Verified,
    Unverified,
};

enum class Continuation : std::uint32_t {
    None = BML_BEHAVIOR_CONTINUATION_NONE,
    Native = BML_BEHAVIOR_CONTINUATION_NATIVE,
    QueuedInput = BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT,
};

[[nodiscard]] constexpr Continuation operator|(Continuation left,
                                                Continuation right) noexcept {
    return static_cast<Continuation>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool Has(Continuation value,
                                 Continuation flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0;
}

struct RunInfo {
    RunKind Kind = RunKind::Instance;
    RunState State = RunState::Ready;
    DetachedSupport Detached = DetachedSupport::Verified;
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
    Behavior::Error Error = Behavior::Error::None;
    Behavior::Phase Phase = Behavior::Phase::None;
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
    Behavior::Continuation Continuation = Behavior::Continuation::None;
    Behavior::Error Error = Behavior::Error::None;
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

// Filters the Prototype registrations visible in the running Player. An empty
// query returns every registration. optional distinguishes an absent filter
// from an intentionally empty text value.
struct PrototypeQuery {
    std::optional<Guid> Id;
    std::optional<std::string> Name;
    std::optional<std::string> Category;
    std::optional<std::string> Provider;
    std::optional<Guid> ProviderId;
    std::optional<std::int32_t> CompatibleClass;
    std::vector<Guid> RequiredManagers;
};

enum class LayoutOrigin : std::uint32_t {
    Declared = BML_BEHAVIOR_LAYOUT_DECLARED,
    Live = BML_BEHAVIOR_LAYOUT_LIVE,
};

enum class BehaviorKind : std::uint32_t {
    Function = BML_BEHAVIOR_KIND_FUNCTION,
    Callback = BML_BEHAVIOR_KIND_CALLBACK,
    Graph = BML_BEHAVIOR_KIND_GRAPH,
};

enum class SlotKind : std::uint32_t {
    In = BML_BEHAVIOR_SLOT_IN,
    Out = BML_BEHAVIOR_SLOT_OUT,
    Pin = BML_BEHAVIOR_SLOT_PIN,
    Pout = BML_BEHAVIOR_SLOT_POUT,
    Setting = BML_BEHAVIOR_SLOT_SETTING,
    Local = BML_BEHAVIOR_SLOT_LOCAL,
    Target = BML_BEHAVIOR_SLOT_TARGET,
};

struct Manager {
    Guid Id;
    bool Available = false;
};

struct PrototypeInfo {
    Prototype Ref;
    Guid Provider;
    std::uint32_t Version = 0;
    std::int32_t CompatibleClass = 0;
    std::string Name;
    std::string Category;
    std::string ProviderName;
    std::string Author;
    std::string Description;
    std::vector<Manager> Managers;
};

struct Slot {
    SlotKind Kind = SlotKind::Pin;
    std::uint64_t Generation = 0;
    bool Dynamic = false;
    std::int32_t Index = 0;
    std::int32_t Occurrence = 0;
    Guid Type;
    std::optional<ValueKind> Value;
    std::string Name;
    std::string TypeName;

    [[nodiscard]] BML_BehaviorSlotRef Wire() const noexcept {
        BML_BehaviorSlotRef slot{};
        slot.StructSize = sizeof(slot);
        slot.Kind = static_cast<std::uint32_t>(Kind);
        slot.LayoutGeneration = Generation;
        slot.Type = Type.Wire();
        slot.Slot = Selector::At(Index).Wire();
        return slot;
    }
};

struct Layout {
    LayoutOrigin Origin = LayoutOrigin::Declared;
    Prototype PrototypeRef;
    std::uint64_t Generation = 0;
    BehaviorKind Kind = BehaviorKind::Function;
    bool MaterializedNow = false;
    std::int32_t CompatibleClass = 0;
    std::uint32_t PrototypeFlags = 0;
    std::uint32_t BehaviorFlags = 0;
    Guid TargetType;
    std::string Name;
    std::string Category;
    std::string Provider;
    std::string Author;
    std::string Description;
    std::vector<Manager> Managers;
    std::vector<Slot> Slots;

    [[nodiscard]] const Slot *Find(
        SlotKind kind, std::string_view name,
        std::int32_t occurrence = 0) const noexcept {
        const auto found = std::find_if(
            Slots.begin(), Slots.end(), [&](const Slot &slot) {
                return slot.Kind == kind && slot.Name == name &&
                       slot.Occurrence == occurrence;
            });
        return found == Slots.end() ? nullptr : &*found;
    }
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

enum class ChangeKind : std::uint32_t {
    Graph = BML_BEHAVIOR_WATCH_GRAPH,
    Layout = BML_BEHAVIOR_WATCH_LAYOUT,
    SampledValue = BML_BEHAVIOR_WATCH_SAMPLED_VALUE,
};

enum class WatchState : std::uint32_t {
    Active = BML_BEHAVIOR_WATCH_ACTIVE,
    Failed = BML_BEHAVIOR_WATCH_FAILED,
};

struct WatchInfo {
    WatchState State = WatchState::Active;
    Status Diagnostic;
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

// A Plan is durable authoring intent: one exact script name, one symbolic edit,
// and the Loader reconciling the two as scripts load, reload, and are deleted.
enum class PlanState : std::uint32_t {
    Reconciling = BML_BEHAVIOR_PLAN_RECONCILING,
    Active = BML_BEHAVIOR_PLAN_ACTIVE,
    Unsatisfied = BML_BEHAVIOR_PLAN_UNSATISFIED,
    Conflicted = BML_BEHAVIOR_PLAN_CONFLICTED,
    Retiring = BML_BEHAVIOR_PLAN_RETIRING,
};

struct PlanInfo {
    PlanState State = PlanState::Reconciling;
    // The world epoch this Plan was last reconciled against.
    std::uint64_t World = 0;
    std::uint32_t Matches = 0;
    std::uint32_t Installations = 0;
    // Why the Plan is Unsatisfied or Conflicted.
    Behavior::Status Diagnostic;

    [[nodiscard]] bool Installed() const noexcept {
        return State == PlanState::Active && Installations != 0;
    }
};

enum class GraphPatchState : std::uint32_t {
    Pending = BML_BEHAVIOR_PATCH_PENDING,
    Active = BML_BEHAVIOR_PATCH_ACTIVE,
    Closing = BML_BEHAVIOR_PATCH_CLOSING,
    Conflicted = BML_BEHAVIOR_PATCH_CONFLICTED,
    Closed = BML_BEHAVIOR_PATCH_CLOSED,
    Failed = BML_BEHAVIOR_PATCH_FAILED,
};

struct GraphPatchInfo {
    GraphPatchState State = GraphPatchState::Pending;
    std::uint32_t Conflicts = 0;
    Behavior::Status Diagnostic;

    [[nodiscard]] bool Installed() const noexcept {
        return State == GraphPatchState::Active;
    }
};

// What one Hook callback receives. Every reference is live for the duration of
// the call only.
struct HookEvent {
    float DeltaTime = 0.0f;
    // The Hook Block being executed.
    BML_ObjectRef Block{};
    // The root script that owns the Block, when the Loader can name it.
    BML_ObjectRef Script{};
    // The object the Block is attached to.
    BML_ObjectRef Owner{};
};

enum class HookResult : int {
    // Stops the enclosing chain. The facade also returns this when an author
    // callback throws, so no C++ exception crosses the C seam.
    Error = -1,
    Ok = BML_BEHAVIOR_HOOK_OK,
    // Keep the Hook Block active for one more frame. Its Outs still activate.
    AgainNextFrame = BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME,
};

enum class CloseState {
    Closing,
    Closed,
};

// Places one Patch relative to another Patch spliced onto the same link.
struct PatchOrder {
    std::uint32_t Kind = BML_BEHAVIOR_ORDER_BEFORE;
    std::string Owner;
    std::string Name;
};

inline PatchOrder before(std::string_view owner, std::string_view name) {
    return {BML_BEHAVIOR_ORDER_BEFORE, std::string(owner), std::string(name)};
}
inline PatchOrder after(std::string_view owner, std::string_view name) {
    return {BML_BEHAVIOR_ORDER_AFTER, std::string(owner), std::string(name)};
}

namespace Detail {
struct SessionState;
class Run;
template <class Final>
class EditBuilder;
}

class PlanBuilder;
class PatchBuilder;

class Watch {
public:
    Watch() = default;
    ~Watch();
    Watch(const Watch &) = delete;
    Watch &operator=(const Watch &) = delete;
    Watch(Watch &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    Watch &operator=(Watch &&other) noexcept {
        if (this != &other) {
            Watch previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Handle;
    }
    [[nodiscard]] Result<WatchInfo> Read() const;
    [[nodiscard]] Result<CloseState> Close() noexcept;

private:
    Watch(std::shared_ptr<Detail::SessionState> session,
          BML_BehaviorWatch handle)
        : m_Session(std::move(session)), m_Handle(handle) {}
    std::shared_ptr<Detail::SessionState> m_Session;
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
    [[nodiscard]] std::vector<Node> FindAll(
        std::string_view name) const {
        std::vector<Node> matches;
        for (const Node &node : m_Nodes) {
            if (node.Name == name)
                matches.push_back(node);
        }
        return matches;
    }
    [[nodiscard]] Result<Node> Find(std::string_view name) const {
        const Node *match = nullptr;
        for (const Node &node : m_Nodes) {
            if (node.Name != name)
                continue;
            if (match) {
                Status status;
                status.Error = Error::QueryAmbiguous;
                status.Message = "More than one Behavior node is named '" +
                    std::string(name) + "'.";
                return Result<Node>::Failure(BML_ERROR_FAIL,
                                              std::move(status));
            }
            match = &node;
        }
        if (!match) {
            Status status;
            status.Error = Error::QueryNotFound;
            status.Message = "No Behavior node is named '" +
                std::string(name) + "'.";
            return Result<Node>::Failure(BML_ERROR_NOT_FOUND,
                                          std::move(status));
        }
        return Result<Node>::Success(*match);
    }

    [[nodiscard]] Result<Graph> Logical() const;
    [[nodiscard]] Result<Graph> Live() const;
    [[nodiscard]] Result<ObservedValue> Read(const ValueRef &value) const;
    [[nodiscard]] PatchBuilder Patch(std::string_view name) const;

    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        GraphChanged, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        LayoutChanged change, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        Sampled change, Function &&callback) const;
private:
    static Result<Graph> Read(std::shared_ptr<Detail::SessionState> session,
                              BML_ObjectRef root, View view);
    static Result<Graph> ReadRun(
        std::shared_ptr<Detail::SessionState> session,
        BML_BehaviorRun run, View view);
    static Result<Graph> Decode(
        std::shared_ptr<Detail::SessionState> session, View view,
        const BML_BehaviorGraph &wire,
        const std::vector<std::uint8_t> &payload,
        const BML_BehaviorStatus &status);
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
    friend class Detail::Run;
};

class Session;
class Builder;
class Block;
class Call;
class Task;
class Instance;
class Hook;
class Plan;
class GraphPatch;

namespace Detail {

inline BML_BehaviorString Text(std::string_view value) noexcept {
    return {value.data(), static_cast<std::uint32_t>(value.size())};
}

inline Status ReadStatus(const BML_BehaviorStatus &source) {
    Status status;
    status.Error = static_cast<Behavior::Error>(source.Error);
    status.Phase = static_cast<Behavior::Phase>(source.Phase);
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
            (source.Flags & BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED) != 0
                ? DetachedSupport::Unverified
                : DetachedSupport::Verified,
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
        !RecordsFit<BML_BehaviorManagerInfo>(
            payload, wire.ManagerOffset, wire.ManagerCount) ||
        !RecordsFit<BML_BehaviorSlotRecord>(
            payload, wire.SlotOffset, wire.SlotCount))
        return false;

    Behavior::Layout decoded;
    decoded.Origin = static_cast<LayoutOrigin>(wire.Origin);
    decoded.PrototypeRef = Prototype(
        wire.Prototype.Prototype, wire.Prototype.Generation);
    decoded.Generation = wire.LayoutGeneration;
    decoded.Kind = static_cast<BehaviorKind>(wire.Kind);
    decoded.MaterializedNow =
        (wire.Flags & BML_BEHAVIOR_LAYOUT_MATERIALIZED_NOW) != 0;
    decoded.CompatibleClass = wire.CompatibleClass;
    decoded.PrototypeFlags = wire.PrototypeFlags;
    decoded.BehaviorFlags = wire.BehaviorFlags;
    decoded.TargetType = wire.TargetType;
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
            {Guid(record.Guid), record.Available != 0});
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
        slot.Type = record.Type;
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
        frame.Continuation = static_cast<Behavior::Continuation>(
            header.Continuation);
        frame.Error = static_cast<Behavior::Error>(header.Error);

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
            diagnostic.Error = static_cast<Behavior::Error>(record.Error);
            diagnostic.Phase = static_cast<Behavior::Phase>(record.Phase);
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

    [[nodiscard]] Result<RunInfo> Read() const {
        if (!*this)
            return Result<RunInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        const int code = m_Session->Api->ReadRun(m_Handle, &info, &status);
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
        int code = m_Session->Api->TakeFrames(
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
            code = m_Session->Api->TakeFrames(
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
        const BML_BehaviorSelector selector = input.Wire();
        BML_BehaviorRunInfo info = EmptyRunInfo();
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t admission = 0;
        const int code = m_Session->Api->Pulse(
            m_Handle, &selector, &admission, &info, &status);
        if (code != BML_OK)
            return Result<PulseResult>::Failure(code, ReadStatus(status));
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
        const Behavior::Slot &slot, const ValueRef &source,
        Relation relation = Relation::Direct) const;
    [[nodiscard]] Result<std::uint64_t> Configure(
        const std::vector<SettingStage> &stages) const;

private:
    Run(std::shared_ptr<SessionState> session,
        BML_BehaviorRun handle) noexcept
        : m_Session(std::move(session)), m_Handle(handle) {}

    void MoveFrom(Run &other) noexcept {
        m_Session = std::move(other.m_Session);
        m_Handle = std::exchange(other.m_Handle, nullptr);
    }

    std::shared_ptr<SessionState> m_Session;
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

inline Watch::~Watch() { (void) Close(); }

inline Result<WatchInfo> Watch::Read() const {
    if (!m_Session || !m_Session->Api || !m_Handle ||
        !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, ReadWatch))
        return Result<WatchInfo>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorWatchInfo info{};
    info.StructSize = sizeof(info);
    info.Diagnostic.StructSize = sizeof(info.Diagnostic);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = m_Session->Api->ReadWatch(m_Handle, &info, &status);
    if (code != BML_OK)
        return Result<WatchInfo>::Failure(code, Detail::ReadStatus(status));
    if (info.StructSize < sizeof(info))
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
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const ValueRef &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        const std::vector<SettingStage> &stages) const {
        return m_Run.Configure(stages);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        std::initializer_list<Binding> stage) const {
        return Configure({SettingStage(stage)});
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }
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
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    [[nodiscard]] Result<PulseResult> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<PulseResult> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const ValueRef &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        const std::vector<SettingStage> &stages) const {
        return m_Run.Configure(stages);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        std::initializer_list<Binding> stage) const {
        return Configure({SettingStage(stage)});
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }

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
    [[nodiscard]] Result<Behavior::Layout> Layout() const {
        return m_Run.Layout();
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return m_Run.Inspect(view);
    }
    [[nodiscard]] Result<PulseResult> Pulse(const Selector &input) const {
        return m_Run.Pulse(input);
    }
    [[nodiscard]] Result<PulseResult> Pulse(std::string_view input) const {
        return Pulse(Selector::Unique(input));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        const Behavior::Slot &slot, T &&value) const {
        return m_Run.Set(slot, Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    [[nodiscard]] Result<std::uint64_t> Set(
        SlotKind kind, const Selector &slot, T &&value) const {
        return m_Run.Set(kind, slot,
                         Behavior::Value(std::forward<T>(value)));
    }
    [[nodiscard]] Result<std::uint64_t> Bind(
        const Behavior::Slot &slot, const ValueRef &source,
        Relation relation = Relation::Direct) const {
        return m_Run.Bind(slot, source, relation);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        const std::vector<SettingStage> &stages) const {
        return m_Run.Configure(stages);
    }
    [[nodiscard]] Result<std::uint64_t> Configure(
        std::initializer_list<Binding> stage) const {
        return Configure({SettingStage(stage)});
    }
    [[nodiscard]] Result<CloseState> Close() noexcept { return m_Run.Close(); }

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

// Builder execution methods are the one-shot authoring path. Compile once and
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

namespace Detail {

inline PlanInfo ReadPlanInfo(const BML_BehaviorPlanInfo &source) {
    PlanInfo info;
    info.State = static_cast<PlanState>(source.State);
    info.World = source.World;
    info.Matches = source.Matches;
    info.Installations = source.Installations;
    info.Diagnostic = ReadStatus(source.Diagnostic);
    return info;
}

inline GraphPatchInfo ReadPatchInfo(const BML_BehaviorPatchInfo &source) {
    GraphPatchInfo info;
    info.State = static_cast<GraphPatchState>(source.State);
    info.Conflicts = source.Conflicts;
    info.Diagnostic = ReadStatus(source.Diagnostic);
    return info;
}

// Holds the caller reference to one author callback. The Loader takes a
// reference of its own while it accepts an edit, so this record only has to
// outlive the PatchBuilder that carries it.
struct HookHolder {
    HookHolder() = default;
    HookHolder(const HookHolder &) = delete;
    HookHolder &operator=(const HookHolder &) = delete;
    ~HookHolder() {
        if (Function.Release)
            Function.Release(Function.State);
    }

    BML_BehaviorHookFunction Function{};
};

// Resolves the return type of whichever call the callback supports, without
// forming the other one.
template <class Callable, bool TakesEvent, bool TakesNothing>
struct HookReturn {
    using Type = void;
};
template <class Callable, bool TakesNothing>
struct HookReturn<Callable, true, TakesNothing> {
    using Type = std::invoke_result_t<Callable &, const HookEvent &>;
};
template <class Callable>
struct HookReturn<Callable, false, true> {
    using Type = std::invoke_result_t<Callable &>;
};

template <class Function>
struct HookFunction {
    using Callable = std::decay_t<Function>;
    static constexpr bool TakesEvent =
        std::is_invocable_v<Callable &, const HookEvent &>;
    static_assert(TakesEvent || std::is_invocable_v<Callable &>,
                  "A Behavior Hook callback takes a HookEvent or nothing.");
    using Returned = typename HookReturn<
        Callable, TakesEvent, std::is_invocable_v<Callable &>>::Type;
    static_assert(std::is_void_v<Returned> ||
                  std::is_same_v<Returned, HookResult>,
                  "A Behavior Hook callback returns void or HookResult.");

    explicit HookFunction(Function &&function)
        : Value(std::forward<Function>(function)) {}

    static void BML_BEHAVIOR_CALL Retain(void *state) {
        ++static_cast<HookFunction *>(state)->References;
    }
    static void BML_BEHAVIOR_CALL Release(void *state) {
        auto *self = static_cast<HookFunction *>(state);
        if (self->References.fetch_sub(1) == 1)
            delete self;
    }
    static int BML_BEHAVIOR_CALL Invoke(
        void *state, const BML_BehaviorHookContext *source) noexcept {
        try {
            if (!state || !source || source->StructSize < sizeof(*source))
                return static_cast<int>(HookResult::Error);
            HookEvent event;
            event.DeltaTime = source->DeltaTime;
            event.Block = source->Block;
            event.Script = source->Script;
            event.Owner = source->Owner;
            auto &callable = static_cast<HookFunction *>(state)->Value;
            if constexpr (std::is_void_v<Returned>) {
                if constexpr (TakesEvent)
                    std::invoke(callable, event);
                else
                    std::invoke(callable);
                return BML_BEHAVIOR_HOOK_OK;
            } else if constexpr (TakesEvent) {
                return static_cast<int>(std::invoke(callable, event));
            } else {
                return static_cast<int>(std::invoke(callable));
            }
        } catch (...) {
            return static_cast<int>(HookResult::Error);
        }
    }

    std::atomic<std::uint32_t> References{1};
    Callable Value;
};

} // namespace Detail

// One author callback, shareable across the steps of one PatchBuilder. It runs
// on the game thread inside the execution the game itself drives. It may close
// its Patch, Plan, Session, or Mod: callback admission closes immediately, the
// request never waits for this invocation, and native teardown plus Release
// occur later at a game-thread safe point.
class Hook {
public:
    Hook() = default;

    template <class Function,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Function>, Hook>>>
    Hook(Function &&callback) {
        using Holder = Detail::HookFunction<Function>;
        auto holder = std::make_unique<Holder>(std::forward<Function>(callback));
        auto record = std::make_shared<Detail::HookHolder>();
        record->Function.StructSize = sizeof(record->Function);
        record->Function.State = holder.get();
        record->Function.Retain = &Holder::Retain;
        record->Function.Release = &Holder::Release;
        record->Function.Invoke = &Holder::Invoke;
        (void) holder.release();
        m_Record = std::move(record);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Record != nullptr;
    }

private:
    std::shared_ptr<Detail::HookHolder> m_Record;

    template <class Final>
    friend class Detail::EditBuilder;
};

// A durable authoring intent the Loader owns. Closing the handle retires every
// installation the Plan still holds. A conflict keeps the handle readable;
// retirement continues at later Behavior safe points even if this value dies.
class Plan {
public:
    Plan() = default;
    ~Plan() { (void) Close(); }
    Plan(const Plan &) = delete;
    Plan &operator=(const Plan &) = delete;
    Plan(Plan &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    Plan &operator=(Plan &&other) noexcept {
        if (this != &other) {
            Plan previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }
    // A Plan the Loader accepted is Reconciling until the next frame installs
    // it, so read the state rather than assuming the edit is already live.
    [[nodiscard]] Result<PlanInfo> Read() const {
        if (!*this)
            return Result<PlanInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorPlanInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->ReadPlan(
            m_Session->Handle, m_Handle, &wire, &status);
        if (code != BML_OK)
            return Result<PlanInfo>::Failure(code, Detail::ReadStatus(status));
        if (wire.StructSize < sizeof(wire))
            return Result<PlanInfo>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        return Result<PlanInfo>::Success(Detail::ReadPlanInfo(wire),
                                        Detail::ReadStatus(status));
    }
    // Reverts what the Plan still owns. If a live graph prevents the inverse,
    // the handle remains valid so Read can describe the conflict and Close can
    // be retried after the graph is restored to the expected after-image.
    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        const int code = m_Session->Api->ClosePlan(
            m_Session->Handle, m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

private:
    Plan(std::shared_ptr<Detail::SessionState> session,
         BML_BehaviorPlan handle)
        : m_Session(std::move(session)), m_Handle(handle) {}

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorPlan m_Handle = nullptr;

    friend class PlanBuilder;
};

// One reversible Patch on a specific live graph. It does not follow script
// names into another world; use Plan when the same intent must be reconciled
// again after loading or reset.
class GraphPatch {
public:
    GraphPatch() = default;
    ~GraphPatch() { (void) Close(); }
    GraphPatch(const GraphPatch &) = delete;
    GraphPatch &operator=(const GraphPatch &) = delete;
    GraphPatch(GraphPatch &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    GraphPatch &operator=(GraphPatch &&other) noexcept {
        if (this != &other) {
            GraphPatch previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }
    [[nodiscard]] Result<GraphPatchInfo> Read() const {
        if (!*this)
            return Result<GraphPatchInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorPatchInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->ReadPatch(
            m_Session->Handle, m_Handle, &wire, &status);
        if (code != BML_OK)
            return Result<GraphPatchInfo>::Failure(
                code, Detail::ReadStatus(status));
        if (wire.StructSize < sizeof(wire))
            return Result<GraphPatchInfo>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<GraphPatchInfo>::Success(
            Detail::ReadPatchInfo(wire), Detail::ReadStatus(status));
    }
    // A revert conflict keeps this handle live for Read and a later retry.
    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        const int code = m_Session->Api->ClosePatch(
            m_Session->Handle, m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

private:
    GraphPatch(std::shared_ptr<Detail::SessionState> session,
               BML_BehaviorPatch handle)
        : m_Session(std::move(session)), m_Handle(handle) {}

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorPatch m_Handle = nullptr;

    friend class PatchBuilder;
};

// Shared symbolic graph-edit language. PlanBuilder chooses matching scripts and
// persists the edit; PatchBuilder applies it to one live graph.
namespace Detail {

template <class Final>
class EditBuilder {
public:
    // Addresses one port of a node the program named.
    struct Port {
        std::uint32_t Handle = 0;
        // A BML_BehaviorSlotKind, or zero when Handle is an appended slot.
        std::uint32_t Kind = 0;
        Guid Type;
        Behavior::Selector Slot;

        [[nodiscard]] BML_BehaviorPortRef Wire() const noexcept {
            BML_BehaviorPortRef port{};
            port.StructSize = sizeof(port);
            port.Handle = Handle;
            port.Kind = Kind;
            port.Type = Type.Wire();
            port.Slot = Slot.Wire();
            return port;
        }
    };

    // A node of the matched script, either required or added by the program.
    struct Node {
        std::uint32_t Id = 0;

        [[nodiscard]] Port In(Behavior::Selector slot = {}) const {
            return {Id, BML_BEHAVIOR_SLOT_IN, {}, std::move(slot)};
        }
        [[nodiscard]] Port In(std::int32_t index) const {
            return In(Behavior::Selector::At(index));
        }
        [[nodiscard]] Port In(std::string_view name) const {
            return In(Behavior::Selector::Unique(name));
        }
        [[nodiscard]] Port Out(Behavior::Selector slot = {}) const {
            return {Id, BML_BEHAVIOR_SLOT_OUT, {}, std::move(slot)};
        }
        [[nodiscard]] Port Out(std::int32_t index) const {
            return Out(Behavior::Selector::At(index));
        }
        [[nodiscard]] Port Out(std::string_view name) const {
            return Out(Behavior::Selector::Unique(name));
        }
        [[nodiscard]] Port Pin(Behavior::Selector slot, Guid type = {}) const {
            return {Id, BML_BEHAVIOR_SLOT_PIN, type, std::move(slot)};
        }
        [[nodiscard]] Port Pin(std::int32_t index, Guid type = {}) const {
            return Pin(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Pin(std::string_view name, Guid type = {}) const {
            return Pin(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Pout(Behavior::Selector slot, Guid type = {}) const {
            return {Id, BML_BEHAVIOR_SLOT_POUT, type, std::move(slot)};
        }
        [[nodiscard]] Port Pout(std::int32_t index, Guid type = {}) const {
            return Pout(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Pout(std::string_view name, Guid type = {}) const {
            return Pout(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Local(Behavior::Selector slot, Guid type = {}) const {
            return {Id, BML_BEHAVIOR_SLOT_LOCAL, type, std::move(slot)};
        }
        [[nodiscard]] Port Local(std::int32_t index, Guid type = {}) const {
            return Local(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Local(std::string_view name, Guid type = {}) const {
            return Local(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Setting(Behavior::Selector slot) const {
            return {Id, BML_BEHAVIOR_SLOT_SETTING, {}, std::move(slot)};
        }
        [[nodiscard]] Port Setting(std::int32_t index) const {
            return Setting(Behavior::Selector::At(index));
        }
        [[nodiscard]] Port Setting(std::string_view name) const {
            return Setting(Behavior::Selector::Unique(name));
        }
        [[nodiscard]] Port Target() const {
            return {Id, BML_BEHAVIOR_SLOT_TARGET, {}, {}};
        }
    };

    // One behavior link of the matched script.
    struct Link {
        std::uint32_t Id = 0;
    };

    // The unique non-branching chain leaving one port, which is where a Hook
    // that must run after a whole sequence belongs.
    struct Path {
        std::uint32_t Id = 0;
    };

    // A slot the program appended. An appended slot has no author-visible index
    // until the edit compiles, so it is addressed by handle instead.
    struct Slot {
        std::uint32_t Id = 0;

        [[nodiscard]] Port Ref() const { return Port{Id, 0, {}, {}}; }
        operator Port() const { return Ref(); }
    };

    EditBuilder(const EditBuilder &) = default;
    EditBuilder &operator=(const EditBuilder &) = default;
    EditBuilder(EditBuilder &&) noexcept = default;
    EditBuilder &operator=(EditBuilder &&) noexcept = default;

    // The target graph itself. Its ports are the entry and exit of the graph.
    [[nodiscard]] Node Graph() const noexcept {
        return Node{BML_BEHAVIOR_EDIT_GRAPH};
    }

    // Names the one node carrying this name, and this Prototype when one is
    // given. No match, or more than one, leaves the Plan Unsatisfied rather
    // than installing a guess.
    [[nodiscard]] Node Require(std::string_view name, Guid prototype = {}) {
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_NODE);
        step.Name.assign(name);
        step.Prototype = prototype;
        return Node{step.Result};
    }
    [[nodiscard]] Node Require(Guid prototype) {
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_NODE);
        step.Prototype = prototype;
        return Node{step.Result};
    }
    // Names the one existing link between these ports.
    [[nodiscard]] Link Between(Port source, Port sink) {
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_LINK);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        return Link{step.Result};
    }
    // Names the one existing link between these ports that also carries this
    // delay, in frames.
    [[nodiscard]] Link Between(Port source, Port sink, std::int32_t delay) {
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_LINK);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        step.Flags |= BML_BEHAVIOR_EDIT_HAS_DELAY;
        step.Delay = delay;
        return Link{step.Result};
    }
    [[nodiscard]] Path Follow(Port source) {
        Step &step = Define(BML_BEHAVIOR_EDIT_FOLLOW);
        step.Source = std::move(source);
        return Path{step.Result};
    }
    // Creates one Block of this Prototype inside the matched script.
    [[nodiscard]] Node Add(Guid prototype) {
        Step &step = Define(BML_BEHAVIOR_EDIT_ADD_BLOCK);
        step.Prototype = prototype;
        return Node{step.Result};
    }
    [[nodiscard]] Slot AppendIn(Node owner, std::string_view name) {
        return Append(owner, BML_BEHAVIOR_SLOT_IN, name, {});
    }
    [[nodiscard]] Slot AppendOut(Node owner, std::string_view name) {
        return Append(owner, BML_BEHAVIOR_SLOT_OUT, name, {});
    }
    [[nodiscard]] Slot AppendPin(Node owner, std::string_view name, Guid type) {
        return Append(owner, BML_BEHAVIOR_SLOT_PIN, name, type);
    }
    [[nodiscard]] Slot AppendPout(Node owner, std::string_view name, Guid type) {
        return Append(owner, BML_BEHAVIOR_SLOT_POUT, name, type);
    }
    [[nodiscard]] Slot AppendLocal(Node owner, std::string_view name,
                                   Guid type) {
        return Append(owner, BML_BEHAVIOR_SLOT_LOCAL, name, type);
    }

    // Adds a behavior link, delayed by whole frames.
    Final &Flow(Port source, Port sink, std::int32_t delay = 0) {
        Step &step = Define(BML_BEHAVIOR_EDIT_FLOW, 0);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        step.Delay = delay;
        return Self();
    }
    // Adds a behavior link that may close a same-frame cycle. Without this the
    // Loader rejects a cycle instead of installing one.
    Final &FlowCycle(Port source, Port sink, std::int32_t delay = 0) {
        Flow(std::move(source), std::move(sink), delay);
        m_Steps.back().Flags |= BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
        return Self();
    }
    // Writes an owned literal into a port. World-bound object values are not
    // part of the symbolic edit language.
    Final &Bind(Port sink, Behavior::Value value) {
        Step &step = Define(BML_BEHAVIOR_EDIT_BIND_VALUE, 0);
        step.Sink = std::move(sink);
        step.Value.emplace(std::move(value));
        return Self();
    }
    // Makes the sink read the source directly.
    Final &Bind(Port sink, Port source) {
        Step &step = Define(BML_BEHAVIOR_EDIT_BIND_PORT, 0);
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        return Self();
    }
    // Makes the sink share the parameter the source reads.
    Final &Share(Port sink, Port source) {
        Step &step = Define(BML_BEHAVIOR_EDIT_SHARE, 0);
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        return Self();
    }
    // Copies the source into the sink after each execution of the node that
    // owns the source.
    Final &Push(Port source, Port sink) {
        Step &step = Define(BML_BEHAVIOR_EDIT_PUSH, 0);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        return Self();
    }
    // Runs the callback on every link leaving this port, before whatever those
    // links reach. The Hook Block activates its Out once the callback returns.
    Final &Tap(Port source, Hook hook) {
        Step &step = Define(BML_BEHAVIOR_EDIT_TAP, 0);
        step.Source = std::move(source);
        step.Hook = std::move(hook.m_Record);
        return Self();
    }
    // Runs the callback once the chain named by the path has finished.
    Final &After(Path path, Hook hook) {
        Step &step = Define(BML_BEHAVIOR_EDIT_AFTER, 0);
        step.Target = path.Id;
        step.Hook = std::move(hook.m_Record);
        return Self();
    }
    // Runs the callback once the chain leaving this port has finished.
    Final &After(Port source, Hook hook) {
        return After(Follow(std::move(source)), std::move(hook));
    }
    // Reroutes a link through a Block, keeping the delay of the link. Ordering
    // places this Patch relative to the Patches of other Mods spliced onto the
    // same link; a Patch no one submitted constrains nothing.
    Final &Splice(Link link, Node through,
                  std::vector<PatchOrder> ordering = {}) {
        Step &step = Define(BML_BEHAVIOR_EDIT_SPLICE, 0);
        step.Target = link.Id;
        step.Node = through.Id;
        step.Ordering = std::move(ordering);
        return Self();
    }
    // Reroutes a link into the sink and out of the source, which is how one
    // Block with several Ins and Outs carries more than one splice.
    Final &Splice(Link link, Port sink, Port source,
                  std::vector<PatchOrder> ordering = {}) {
        Step &step = Define(BML_BEHAVIOR_EDIT_SPLICE, 0);
        step.Target = link.Id;
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        step.Ordering = std::move(ordering);
        return Self();
    }

protected:
    struct Step {
        std::uint32_t Kind = 0;
        std::uint32_t Result = 0;
        std::uint32_t Flags = 0;
        std::uint32_t Target = 0;
        std::uint32_t Node = 0;
        std::uint32_t SlotKind = 0;
        std::int32_t Delay = 0;
        std::string Name;
        Guid Prototype;
        Guid Type;
        Port Source;
        Port Sink;
        std::optional<Behavior::Value> Value;
        std::shared_ptr<Detail::HookHolder> Hook;
        std::vector<PatchOrder> Ordering;
    };

    struct WireProgram {
        std::vector<BML_BehaviorEditOrder> Ordering;
        std::vector<BML_BehaviorEditStep> Steps;
    };

    EditBuilder(std::shared_ptr<SessionState> session,
                std::string_view name, BML_ObjectRef graph = {})
        : m_Session(std::move(session)), m_Name(name), m_Graph(graph) {}

    [[nodiscard]] Final &Self() noexcept {
        return static_cast<Final &>(*this);
    }

    Step &Define(std::uint32_t kind) {
        return Define(kind, m_NextHandle++);
    }
    Step &Define(std::uint32_t kind, std::uint32_t result) {
        Step step;
        step.Kind = kind;
        step.Result = result;
        m_Steps.push_back(std::move(step));
        return m_Steps.back();
    }
    Slot Append(Node owner, std::uint32_t slotKind, std::string_view name,
                Guid type) {
        Step &step = Define(BML_BEHAVIOR_EDIT_APPEND_SLOT);
        step.Target = owner.Id;
        step.SlotKind = slotKind;
        step.Name.assign(name);
        step.Type = type;
        return Slot{step.Result};
    }
    void Encode(WireProgram &out) const;

    std::shared_ptr<SessionState> m_Session;
    std::string m_Name;
    BML_ObjectRef m_Graph{};
    std::string m_Script;
    std::uint32_t m_Targets = BML_BEHAVIOR_TARGETS_EACH;
    std::uint32_t m_NextHandle = BML_BEHAVIOR_EDIT_GRAPH + 1u;
    std::vector<Step> m_Steps;

};

} // namespace Detail

class PlanBuilder final : public Detail::EditBuilder<PlanBuilder> {
public:
    // Installs into every live script carrying this exact name.
    PlanBuilder &On(std::string_view script) {
        m_Script.assign(script);
        m_Targets = BML_BEHAVIOR_TARGETS_EACH;
        return *this;
    }
    // Installs into the single live script carrying this exact name. More than
    // one live match leaves the Plan Unsatisfied instead of choosing one.
    PlanBuilder &OnSingle(std::string_view script) {
        m_Script.assign(script);
        m_Targets = BML_BEHAVIOR_TARGETS_ONE;
        return *this;
    }

    // Hands the program to the Loader, which validates all of it, retains the
    // callbacks it accepted, and installs on the next frame. Submitting a name
    // that is already live replaces the Plan carrying it.
    [[nodiscard]] Result<Behavior::Plan> Submit() const;

private:
    PlanBuilder(std::shared_ptr<Detail::SessionState> session,
                std::string_view name)
        : Detail::EditBuilder<PlanBuilder>(std::move(session), name) {}

    friend class Session;
};

class PatchBuilder final : public Detail::EditBuilder<PatchBuilder> {
public:
    // Applies this edit once to a specific logical graph.
    [[nodiscard]] Result<GraphPatch> Apply() const {
        return Apply(m_Graph);
    }
    [[nodiscard]] Result<GraphPatch> Apply(BML_ObjectRef graph) const;

private:
    PatchBuilder(std::shared_ptr<Detail::SessionState> session,
                 std::string_view name, BML_ObjectRef graph = {})
        : Detail::EditBuilder<PatchBuilder>(
              std::move(session), name, graph) {}

    friend class Graph;
    friend class Session;
};

inline PatchBuilder Graph::Patch(std::string_view name) const {
    return PatchBuilder(m_Session, name, m_Root);
}

template <class Final>
inline void Detail::EditBuilder<Final>::Encode(WireProgram &out) const {
    std::size_t orderCount = 0;
    for (const Step &step : m_Steps)
        orderCount += step.Ordering.size();
    // Both arrays are sized up front, so nothing a step points at moves.
    out.Ordering.clear();
    out.Ordering.reserve(orderCount);
    for (const Step &step : m_Steps) {
        for (const PatchOrder &order : step.Ordering) {
            BML_BehaviorEditOrder wire{};
            wire.StructSize = sizeof(wire);
            wire.Kind = order.Kind;
            wire.Owner = Detail::Text(order.Owner);
            wire.Name = Detail::Text(order.Name);
            out.Ordering.push_back(wire);
        }
    }

    out.Steps.clear();
    out.Steps.reserve(m_Steps.size());
    std::size_t consumed = 0;
    for (const Step &step : m_Steps) {
        BML_BehaviorEditStep wire{};
        wire.StructSize = sizeof(wire);
        wire.Kind = step.Kind;
        wire.Result = step.Result;
        wire.Flags = step.Flags;
        wire.Target = step.Target;
        wire.Node = step.Node;
        wire.SlotKind = step.SlotKind;
        wire.Delay = step.Delay;
        wire.Name = Detail::Text(step.Name);
        wire.Prototype = step.Prototype.Wire();
        wire.Type = step.Type.Wire();
        wire.Source = step.Source.Wire();
        wire.Sink = step.Sink.Wire();
        if (step.Value)
            wire.Value = step.Value->Wire();
        wire.Hook = step.Hook ? &step.Hook->Function : nullptr;
        wire.OrderCount = static_cast<std::uint32_t>(step.Ordering.size());
        wire.Ordering = wire.OrderCount
            ? out.Ordering.data() + consumed : nullptr;
        consumed += step.Ordering.size();
        out.Steps.push_back(wire);
    }
}

inline Result<Plan> PlanBuilder::Submit() const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Plan>::Failure(BML_ERROR_INVALID_HANDLE);
    if (m_Name.empty() || m_Script.empty())
        return Result<Plan>::Failure(BML_ERROR_INVALID_PARAMETER);
    try {
        WireProgram program;
        Encode(program);

        BML_BehaviorPlanSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Targets = m_Targets;
        spec.Name = Detail::Text(m_Name);
        spec.Script = Detail::Text(m_Script);
        spec.Steps = program.Steps.empty() ? nullptr : program.Steps.data();
        spec.StepCount = static_cast<std::uint32_t>(program.Steps.size());

        BML_BehaviorPlan handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->SubmitPlan(
            m_Session->Handle, &spec, &handle, nullptr, &status);
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_Session->Api->ClosePlan(m_Session->Handle, handle);
            return Result<Plan>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Plan>::Success(Plan(m_Session, handle),
                                     Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Plan>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Plan>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<GraphPatch> PatchBuilder::Apply(BML_ObjectRef graph) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<GraphPatch>::Failure(BML_ERROR_INVALID_HANDLE);
    if (m_Name.empty() || !graph.Domain)
        return Result<GraphPatch>::Failure(BML_ERROR_INVALID_PARAMETER);
    if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, ClosePatch))
        return Result<GraphPatch>::Failure(BML_ERROR_VERSION_MISMATCH);
    try {
        WireProgram program;
        Encode(program);

        BML_BehaviorPatchSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = Detail::Text(m_Name);
        spec.Graph = graph;
        spec.Steps = program.Steps.empty() ? nullptr : program.Steps.data();
        spec.StepCount = static_cast<std::uint32_t>(program.Steps.size());

        BML_BehaviorPatch handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->ApplyPatch(
            m_Session->Handle, &spec, &handle, nullptr, &status);
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_Session->Api->ClosePatch(m_Session->Handle, handle);
            return Result<GraphPatch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<GraphPatch>::Success(
            GraphPatch(m_Session, handle), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<GraphPatch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<GraphPatch>::Failure(BML_ERROR_FAIL);
    }
}

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
        // The facade needs the complete 1.0 contract. Later minor members are
        // probed at their call sites instead of rejecting an older loader here.
        if (!api || !BML_BEHAVIOR_HAS_1_0(api))
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
    [[nodiscard]] Result<std::vector<PrototypeInfo>> Prototypes(
        const PrototypeQuery &query = {}) const;
    [[nodiscard]] Result<Behavior::Layout> Layout(
        Prototype prototype) const;
    [[nodiscard]] Result<Behavior::Layout> Layout(Guid prototype) const {
        return Layout(Prototype(prototype));
    }
    [[nodiscard]] Result<Behavior::Layout> Layout(CKGUID prototype) const {
        return Layout(Prototype(prototype));
    }
    [[nodiscard]] Result<Graph> Inspect(BML_ObjectRef root) const {
        return Graph::Read(m_State, root, View::Logical);
    }
    // Opens a durable edit named within this Mod. Submitting a name that is
    // already live replaces the Plan carrying it.
    [[nodiscard]] Behavior::PlanBuilder Plan(std::string_view name) const {
        return Behavior::PlanBuilder(m_State, name);
    }
    // Opens an edit for one graph. The returned builder shares the same
    // symbolic language as Plan; call Apply(graph) when it is complete.
    [[nodiscard]] Behavior::PatchBuilder Patch(std::string_view name) const {
        return Behavior::PatchBuilder(m_State, name);
    }
    void Close() noexcept {
        // Values created from this Session hold their own lease. Releasing the
        // Session value stops this object from admitting work without
        // invalidating Blocks, Runs, Watches, Plans, or Patches that still own
        // native state. The last lease closes the C Session.
        m_State.reset();
    }

    // The escape hatch to the C ABI, for whatever this facade does not cover
    // yet. Both stay valid until Close, and neither transfers ownership.
    [[nodiscard]] BML_BehaviorSession Handle() const noexcept {
        return m_State ? m_State->Handle : nullptr;
    }
    [[nodiscard]] const BML_BehaviorInterface *Api() const noexcept {
        return m_State ? m_State->Api : nullptr;
    }

private:
    std::shared_ptr<Detail::SessionState> m_State;
};

namespace Detail {

inline Status BlockError(std::uint32_t error, std::uint32_t phase,
                         Prototype prototype, Guid type,
                         std::string message) {
    Status status;
    status.Error = static_cast<Behavior::Error>(error);
    status.Phase = static_cast<Behavior::Phase>(phase);
    status.Prototype = prototype.Id;
    status.Type = type;
    status.Message = std::move(message);
    return status;
}

inline Result<std::vector<PrototypeInfo>> FindPrototypes(
    const std::shared_ptr<SessionState> &session,
    const PrototypeQuery &query) {
    if (!session || !session->Api || !session->Handle)
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(session->Api, BML_BehaviorInterface,
                       FindPrototypes))
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_VERSION_MISMATCH);

    try {
        if (query.RequiredManagers.size() >
            (std::numeric_limits<std::uint32_t>::max)())
            return Result<std::vector<PrototypeInfo>>::Failure(
                BML_ERROR_INVALID_PARAMETER);
        std::vector<BML_BehaviorGuid> managers;
        managers.reserve(query.RequiredManagers.size());
        for (Guid manager : query.RequiredManagers)
            managers.push_back(manager.Wire());

        BML_BehaviorPrototypeQuery wireQuery{};
        wireQuery.StructSize = sizeof(wireQuery);
        if (query.Id) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROTOTYPE;
            wireQuery.Prototype = query.Id->Wire();
        }
        if (query.Name) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_NAME;
            wireQuery.Name = Text(*query.Name);
        }
        if (query.Category) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_CATEGORY;
            wireQuery.Category = Text(*query.Category);
        }
        if (query.Provider) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROVIDER;
            wireQuery.Provider = Text(*query.Provider);
        }
        if (query.ProviderId) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROVIDER_GUID;
            wireQuery.ProviderGuid = query.ProviderId->Wire();
        }
        if (query.CompatibleClass) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS;
            wireQuery.CompatibleClass = *query.CompatibleClass;
        }
        if (!managers.empty()) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS;
            wireQuery.RequiredManagers = managers.data();
            wireQuery.RequiredManagerCount =
                static_cast<std::uint32_t>(managers.size());
        }

        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t count = 0;
        std::uint32_t payloadSize = 0;
        int code = session->Api->FindPrototypes(
            session->Handle, &wireQuery, nullptr, 0,
            sizeof(BML_BehaviorPrototypeInfo), nullptr, 0, &count,
            &payloadSize, &status);
        if (code == BML_OK && count == 0 && payloadSize == 0)
            return Result<std::vector<PrototypeInfo>>::Success(
                {}, ReadStatus(status));
        if (code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<std::vector<PrototypeInfo>>::Failure(
                code, ReadStatus(status));

        std::vector<BML_BehaviorPrototypeInfo> records(count);
        std::vector<std::uint8_t> payload(payloadSize);
        status = EmptyStatus();
        std::uint32_t writtenCount = 0;
        std::uint32_t writtenBytes = 0;
        code = session->Api->FindPrototypes(
            session->Handle, &wireQuery, records.data(), count,
            sizeof(BML_BehaviorPrototypeInfo), payload.data(), payloadSize,
            &writtenCount, &writtenBytes, &status);
        if (code != BML_OK)
            return Result<std::vector<PrototypeInfo>>::Failure(
                code, ReadStatus(status));
        if (writtenCount != count || writtenBytes != payloadSize)
            return Result<std::vector<PrototypeInfo>>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

        std::vector<PrototypeInfo> found;
        found.reserve(records.size());
        for (const BML_BehaviorPrototypeInfo &record : records) {
            if (record.StructSize < sizeof(record) ||
                record.Ref.StructSize < sizeof(record.Ref) ||
                !RecordsFit<BML_BehaviorManagerInfo>(
                    payload, record.ManagerOffset, record.ManagerCount))
                return Result<std::vector<PrototypeInfo>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

            PrototypeInfo info;
            info.Ref = Prototype(record.Ref.Prototype, record.Ref.Generation);
            info.Provider = record.Provider;
            info.Version = record.Version;
            info.CompatibleClass = record.CompatibleClass;
            if (!TextAt(payload, record.Name.Offset, record.Name.Length,
                        info.Name) ||
                !TextAt(payload, record.Category.Offset, record.Category.Length,
                        info.Category) ||
                !TextAt(payload, record.ProviderName.Offset,
                        record.ProviderName.Length, info.ProviderName) ||
                !TextAt(payload, record.Author.Offset, record.Author.Length,
                        info.Author) ||
                !TextAt(payload, record.Description.Offset,
                        record.Description.Length, info.Description))
                return Result<std::vector<PrototypeInfo>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

            info.Managers.reserve(record.ManagerCount);
            for (std::uint32_t index = 0; index < record.ManagerCount; ++index) {
                BML_BehaviorManagerInfo manager{};
                if (!RecordAt(payload, record.ManagerOffset, index, manager))
                    return Result<std::vector<PrototypeInfo>>::Failure(
                        BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
                info.Managers.push_back(
                    {manager.Guid, manager.Available != 0});
            }
            found.push_back(std::move(info));
        }
        return Result<std::vector<PrototypeInfo>>::Success(
            std::move(found), ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::vector<PrototypeInfo>>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Layout> ReadDeclared(
    const std::shared_ptr<SessionState> &session, Prototype prototype) {
    if (!session || !session->Api || !session->Handle)
        return Result<Behavior::Layout>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(session->Api, BML_BehaviorInterface,
                       ReadDeclaredLayout))
        return Result<Behavior::Layout>::Failure(BML_ERROR_VERSION_MISMATCH);

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
        return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
    if (code == BML_OK && payloadSize != 0) {
        return Result<Behavior::Layout>::Failure(
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
            return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
        if (written != payload.size()) {
            return Result<Behavior::Layout>::Failure(
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
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, prototype, {},
                       "The declared Layout does not identify the requested Prototype."));
    }

    const Prototype resolved(wire.Prototype.Prototype,
                             wire.Prototype.Generation);
    Behavior::Layout layout;
    if (!ReadLayout(wire, payload, layout)) {
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, resolved, {},
                       "The declared Layout payload is malformed."));
    }
    return Result<Behavior::Layout>::Success(std::move(layout),
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

inline Status CheckSetting(const Behavior::Layout &layout,
                           const Binding &binding) {
    const BML_BehaviorSelector selector = binding.Slot.Wire();
    std::vector<const Behavior::Slot *> matches;
    for (const Behavior::Slot &slot : layout.Slots) {
        if (slot.Kind != SlotKind::Setting)
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
    const Behavior::Slot *slot = nullptr;
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
        for (const Behavior::Slot *candidate : matches) {
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

    if (!slot->Value) {
        return BlockError(BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, slot->Type,
                          "The Setting parameter type has no public value form.");
    }
    if (*slot->Value != binding.Value.Kind()) {
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

inline Result<std::vector<PrototypeInfo>> Session::Prototypes(
    const PrototypeQuery &query) const {
    return Detail::FindPrototypes(m_State, query);
}

inline Result<Behavior::Layout> Session::Layout(Prototype prototype) const {
    return Detail::ReadDeclared(m_State, prototype);
}

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
    static int BML_BEHAVIOR_CALL Invoke(
        void *state, const BML_BehaviorWatchEvent *source) noexcept {
        try {
            Change change;
            if (!state || !ReadChange(source, change))
                return BML_BEHAVIOR_WATCH_ERROR;
            std::invoke(static_cast<WatchFunction *>(state)->Value, change);
            return BML_BEHAVIOR_WATCH_OK;
        } catch (...) {
            return BML_BEHAVIOR_WATCH_ERROR;
        }
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
        return Decode(std::move(session), view, wire, payload, status);
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::ReadRun(
    std::shared_ptr<Detail::SessionState> session,
    BML_BehaviorRun run, View view) {
    if (!session || !session->Api || !session->Handle || !run ||
        !BML_IFACE_HAS(session->Api, BML_BehaviorInterface, InspectRun))
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorGraph wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = session->Api->InspectRun(
            run, static_cast<std::uint32_t>(view), &wire, nullptr, 0,
            &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = session->Api->InspectRun(
                run, static_cast<std::uint32_t>(view), &wire,
                payload.data(), payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<Graph>::Failure(code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        }
        return Decode(std::move(session), view, wire, payload, status);
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::Decode(
    std::shared_ptr<Detail::SessionState> session, View view,
    const BML_BehaviorGraph &wire,
    const std::vector<std::uint8_t> &payload,
    const BML_BehaviorStatus &status) {
    if (wire.StructSize < sizeof(wire) ||
        wire.View != static_cast<std::uint32_t>(view) ||
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
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            Port port;
            port.Node = portRecord.Node;
            port.Kind = portRecord.Kind;
            port.Index = portRecord.Index;
            port.Occurrence = portRecord.Occurrence;
            port.Active = portRecord.Active != 0;
            if (!Detail::TextAt(payload, portRecord.Name.Offset,
                                portRecord.Name.Length, port.Name))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
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
            record.InitialDelay, record.RemainingDelay,
            static_cast<TruthValue>(record.Pending)});
    }
    return Result<Graph>::Success(std::move(graph),
                                  Detail::ReadStatus(status));
}

inline Result<Behavior::Layout> Detail::Run::Layout() const {
    if (!*this)
        return Result<Behavior::Layout>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorLayout wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadLiveLayout(
            m_Handle, &wire, nullptr, 0, &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = EmptyStatus();
            std::uint32_t written = 0;
            code = m_Session->Api->ReadLiveLayout(
                m_Handle, &wire, payload.data(), payloadSize,
                &written, &status);
            if (code != BML_OK)
                return Result<Behavior::Layout>::Failure(
                    code, ReadStatus(status));
            if (written != payload.size())
                return Result<Behavior::Layout>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        Behavior::Layout layout;
        if (!ReadLayout(wire, payload, layout) ||
            layout.Origin != LayoutOrigin::Live)
            return Result<Behavior::Layout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<Behavior::Layout>::Success(
            std::move(layout), ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Detail::Run::Inspect(View view) const {
    if (!*this)
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    return Graph::ReadRun(m_Session, m_Handle, view);
}

inline Result<std::uint64_t> Detail::Run::Set(
    const Behavior::Slot &slot, const Behavior::Value &value) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Set))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = slot.Wire();
    const BML_BehaviorValue wire = value.Wire();
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Set(
        m_Handle, &target, &wire, &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Set(
    SlotKind kind, const Selector &slot,
    const Behavior::Value &value) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Set))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target{};
    target.StructSize = sizeof(target);
    target.Kind = static_cast<std::uint32_t>(kind);
    target.Type = value.Type().Wire();
    target.Slot = slot.Wire();
    const BML_BehaviorValue wire = value.Wire();
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Set(
        m_Handle, &target, &wire, &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Bind(
    const Behavior::Slot &slot, const ValueRef &source,
    Relation relation) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Bind))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = slot.Wire();
    BML_BehaviorValueRef value{};
    value.StructSize = sizeof(value);
    value.Kind = source.Kind;
    value.Node = source.Node;
    value.Slot = source.Slot.Wire();
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Bind(
        m_Handle, &target, &value,
        static_cast<std::uint32_t>(relation), &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Configure(
    const std::vector<SettingStage> &stages) const {
    if (!*this || stages.empty() ||
        !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Configure))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        std::vector<std::vector<BML_BehaviorBinding>> bindings;
        std::vector<BML_BehaviorSettingStage> wireStages;
        bindings.reserve(stages.size());
        wireStages.reserve(stages.size());
        for (const SettingStage &stage : stages) {
            std::vector<BML_BehaviorBinding> wireBindings;
            wireBindings.reserve(stage.size());
            for (const Binding &binding : stage) {
                BML_BehaviorBinding wire{};
                wire.StructSize = sizeof(wire);
                wire.Slot = binding.Slot.Wire();
                wire.Value = binding.Value.Wire();
                wireBindings.push_back(wire);
            }
            bindings.push_back(std::move(wireBindings));
        }
        for (const auto &stage : bindings) {
            BML_BehaviorSettingStage wire{};
            wire.StructSize = sizeof(wire);
            wire.Settings = stage.empty() ? nullptr : stage.data();
            wire.SettingCount = static_cast<std::uint32_t>(stage.size());
            wireStages.push_back(wire);
        }
        BML_BehaviorStatus status = EmptyStatus();
        std::uint64_t generation = 0;
        const int code = m_Session->Api->Configure(
            m_Handle, wireStages.data(),
            static_cast<std::uint32_t>(wireStages.size()),
            &generation, &status);
        return code == BML_OK
            ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
            : Result<std::uint64_t>::Failure(code, ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<std::uint64_t>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::uint64_t>::Failure(BML_ERROR_FAIL);
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
            Behavior::Watch(m_Session, handle),
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
            Handle(Detail::Run(m_Session, run)),
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
            Instance(Detail::Run(m_Session, run)),
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
    const int code = m_Run.m_Session->Api->Continue(
        m_Run.m_Handle, &info, &status);
    if (code != BML_OK)
        return Result<Task>::Failure(code, Detail::ReadStatus(status));
    return Result<Task>::Success(Task(std::move(m_Run)),
                                 Detail::ReadStatus(status));
}

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_HPP
