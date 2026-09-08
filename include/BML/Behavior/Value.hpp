#ifndef BML_BEHAVIOR_VALUE_HPP
#define BML_BEHAVIOR_VALUE_HPP

#include "BML/Behavior.h"
#include "BML/TypeConvert.h"
#include "CKAll.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace BML::Behavior {

namespace Detail {
template <class T>
inline constexpr bool FitsBehaviorInt =
    std::is_integral_v<T> && !std::is_same_v<T, bool> &&
    ((std::is_signed_v<T> && sizeof(T) <= 4) ||
     (std::is_unsigned_v<T> && sizeof(T) < 4));

template <class T, bool = std::is_enum_v<T>>
struct EnumFitsBehaviorInt : std::false_type {};

template <class T>
struct EnumFitsBehaviorInt<T, true>
    : std::bool_constant<
          (std::is_signed_v<std::underlying_type_t<T>> &&
           sizeof(std::underlying_type_t<T>) <= 4) ||
          (std::is_unsigned_v<std::underlying_type_t<T>> &&
           sizeof(std::underlying_type_t<T>) < 4)> {};
} // namespace Detail


using ObjectRef = BML_ObjectRef;

class Selector;
class Value;
class Frame;
class Node;
struct Slot;

namespace Detail {

class Run;
struct Wire;

inline BML_BehaviorGuid WireGuid(CKGUID guid) noexcept {
    return {guid.d1, guid.d2};
}

inline CKGUID NativeGuid(BML_BehaviorGuid guid) noexcept {
    return CKGUID(guid.Data1, guid.Data2);
}

inline bool HasGuid(CKGUID guid) noexcept {
    return guid.d1 != 0 || guid.d2 != 0;
}

} // namespace Detail

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
    RedirectConflict = BML_BEHAVIOR_ERROR_REDIRECT_CONFLICT,
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
    CKGUID Prototype{0, 0};
    CKGUID Type{0, 0};
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
    [[nodiscard]] const Behavior::Status &GetStatus() const noexcept {
        return m_Status;
    }

    [[nodiscard]] bool HasValue() const noexcept { return m_Value.has_value(); }
    [[nodiscard]] T &Value() & { return m_Value.value(); }
    [[nodiscard]] const T &Value() const & { return m_Value.value(); }
    template <class U = T,
              std::enable_if_t<std::is_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] T Value() && {
        if constexpr (std::is_move_constructible_v<T>)
            return std::move(m_Value).value();
        else
            return m_Value.value();
    }
    template <class U = T,
              std::enable_if_t<!std::is_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] T Value() && = delete;
    [[nodiscard]] const T &&Value() const && = delete;
    // Consumes the contained value. The Result keeps its code and Status so
    // diagnostics remain readable, but no longer reports a value afterwards.
    [[nodiscard]] T Take() {
        T value(std::move(m_Value).value());
        m_Value.reset();
        return value;
    }
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

template <>
class Result<void> {
public:
    Result() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Code == BML_OK;
    }
    [[nodiscard]] int Code() const noexcept { return m_Code; }
    [[nodiscard]] const Behavior::Status &GetStatus() const noexcept {
        return m_Status;
    }

    static Result Success(Status status = {}) {
        Result result;
        result.m_Code = BML_OK;
        result.m_Status = std::move(status);
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
    Behavior::Status m_Status;
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

private:
    [[nodiscard]] bool Matches(std::int32_t index, std::int32_t occurrence,
                               std::string_view name) const noexcept {
        if (m_Kind == BML_BEHAVIOR_SELECTOR_ONLY)
            return true;
        if (m_Kind == BML_BEHAVIOR_SELECTOR_INDEX)
            return m_Index == index;
        if (m_Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME)
            return m_Name == name;
        return m_Kind == BML_BEHAVIOR_SELECTOR_NAME &&
            m_Name == name && m_Occurrence == occurrence;
    }
    [[nodiscard]] bool RequiresUniqueMatch() const noexcept {
        return m_Kind == BML_BEHAVIOR_SELECTOR_ONLY ||
            m_Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
    }

    std::uint32_t m_Kind = BML_BEHAVIOR_SELECTOR_ONLY;
    std::int32_t m_Index = 0;
    std::int32_t m_Occurrence = 0;
    std::string m_Name;

    friend struct Detail::Wire;
    friend class Frame;
    friend class Node;
    friend class Graph;
};

inline Selector At(std::int32_t index) { return Selector::At(index); }
inline Selector Named(std::string_view name, std::int32_t occurrence) {
    return Selector::Named(name, occurrence);
}
inline Selector Unique(std::string_view name) { return Selector::Unique(name); }

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
    ObjectList = BML_BEHAVIOR_VALUE_OBJECT_LIST,
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
    // Accept only integer domains whose complete range fits a Virtools Int.
    // Wider values must be narrowed explicitly at the authoring boundary.
    template <class T, std::enable_if_t<
                           Detail::FitsBehaviorInt<T>,
                           int> = 0>
    Value(T value) : m_Type(CKPGUID_INT), m_Kind(ValueKind::Int32) {
        m_Data.Int32 = static_cast<std::int32_t>(value);
    }
    template <class T, std::enable_if_t<
                           Detail::EnumFitsBehaviorInt<T>::value,
                           int> = 0>
    Value(T value) : m_Type(CKPGUID_INT), m_Kind(ValueKind::Int32) {
        m_Data.Int32 = static_cast<std::int32_t>(
            static_cast<std::underlying_type_t<T>>(value));
    }
    template <class T, std::enable_if_t<
                           (std::is_integral_v<T> &&
                            !std::is_same_v<T, bool> &&
                            !Detail::FitsBehaviorInt<T>) ||
                           (std::is_floating_point_v<T> &&
                            !std::is_same_v<T, float>) ||
                           (std::is_enum_v<T> &&
                            !Detail::EnumFitsBehaviorInt<T>::value), int> = 0>
    Value(T) = delete;
    Value(const Vx2DVector &value) : Value(Convert::ToVec2(value)) {}
    Value(const VxVector &value) : Value(Convert::ToVec3(value)) {}
    Value(const VxRect &value) : Value(Convert::ToRect(value)) {}
    Value(const VxMatrix &value) : Value(Convert::ToMat4(value)) {}

    static Value Object(CKGUID type, ObjectRef object) {
        Value value;
        value.m_Type = type;
        value.m_Kind = ValueKind::Object;
        value.m_Data.Object = object;
        return value;
    }
    static Value Null(CKGUID type) { return Object(type, {}); }

    template <class T>
    static Value As(CKGUID type, T &&source) {
        Value value(std::forward<T>(source));
        value.m_Type = type;
        return value;
    }

    [[nodiscard]] CKGUID Type() const noexcept { return m_Type; }
    [[nodiscard]] ValueKind Kind() const noexcept { return m_Kind; }
    [[nodiscard]] bool IsNull() const noexcept {
        return m_Kind == ValueKind::Object &&
            m_Data.Object.Domain == 0 && m_Data.Object.Slot == 0 &&
            m_Data.Object.Generation == 0;
    }

private:
    Value() = default;

    CKGUID m_Type;
    ValueKind m_Kind = ValueKind::Int32;
    BML_BehaviorValueData m_Data{};
    std::string m_Text;

    friend struct Detail::Wire;
};

struct SlotValue {
    Selector Slot;
    Behavior::Value Data;

    template <class T>
    SlotValue(Selector slot, T &&value)
        : Slot(std::move(slot)), Data(std::forward<T>(value)) {}
    template <class T>
    SlotValue(std::string_view name, T &&value)
        : Slot(Selector::Unique(name)), Data(std::forward<T>(value)) {}
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_VALUE_HPP
