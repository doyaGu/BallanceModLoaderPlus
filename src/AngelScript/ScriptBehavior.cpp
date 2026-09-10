#include "ScriptStringInterop.h"
#include "ScriptBehavior.h"

#include <atomic>
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

#include "Api/ObjectRefs.h"
#include "BML/Behavior.hpp"
#include "CKAngelScriptAdapter.h"
#include "Loader/ModContext.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"

namespace BML {

namespace Authoring = BML::Behavior;

std::string FailureText(const char *operation, int code,
                        const Authoring::Status &status) {
    std::string text = operation ? operation : "Behavior operation";
    text += " failed";
    if (!status.Message.empty()) {
        text += ": ";
        text += status.Message;
    } else if (const char *message = BML_GetErrorString(code)) {
        text += ": ";
        text += message;
    }
    return text;
}

template <class T>
bool Accept(const char *operation, const Authoring::Result<T> &result) {
    if (result)
        return true;
    const std::string text = FailureText(operation, result.Code(),
                                         result.GetStatus());
    ScriptStringInterop::RaiseActiveException(text.c_str());
    return false;
}

template <class T, class... Args>
T *NewScriptObject(const char *description, Args &&...args) {
    try {
        T *object = new (std::nothrow) T(std::forward<Args>(args)...);
        if (object)
            return object;
        std::string message = "Out of memory creating ";
        message += description;
        message += ".";
        ScriptStringInterop::RaiseActiveException(message.c_str());
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseNativeException(description, "out of memory");
    } catch (const std::exception &error) {
        ScriptStringInterop::RaiseNativeException(description, error.what());
    } catch (...) {
        ScriptStringInterop::RaiseNativeException(description,
                                                   "unknown C++ exception");
    }
    return nullptr;
}

class ScriptRef {
public:
    void AddRef() noexcept {
        m_Refs.fetch_add(1, std::memory_order_relaxed);
    }
    void Release() noexcept {
        if (m_Refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }

protected:
    virtual ~ScriptRef() = default;

private:
    std::atomic<int> m_Refs{1};
};

class ScriptBehaviorResource {
public:
    virtual ~ScriptBehaviorResource() = default;
    virtual void Retire() noexcept = 0;
    [[nodiscard]] virtual bool IsOpen() const noexcept = 0;
};

struct ScriptBehaviorSelector {
    Authoring::Selector Value;

    ScriptBehaviorSelector() = default;
    explicit ScriptBehaviorSelector(Authoring::Selector value)
        : Value(std::move(value)) {}
};

struct ScriptBehaviorFramePolicy {
    Authoring::FramePolicy Value = Authoring::Signals();

    ScriptBehaviorFramePolicy Pouts(bool include) const noexcept {
        return {Value.Pouts(include)};
    }
};

Authoring::Value QuaternionValue(const VxQuaternion &value) {
    return Authoring::Value(BML_Quaternion{
        value.x, value.y, value.z, value.w});
}

Authoring::Value EulerValue(const VxVector &value) {
    return Authoring::Value(BML_Euler{value.x, value.y, value.z});
}

Authoring::Value ColorValue(const VxColor &value) {
    return Authoring::Value(BML_Color{
        value.r, value.g, value.b, value.a});
}

Authoring::Value BoxValue(const VxBbox &value) {
    return Authoring::Value(BML_Box{
        {value.Min.x, value.Min.y, value.Min.z},
        {value.Max.x, value.Max.y, value.Max.z}});
}

VxQuaternion ToQuaternion(const BML_Quaternion &value) {
    VxQuaternion result;
    result.x = value.x;
    result.y = value.y;
    result.z = value.z;
    result.w = value.w;
    return result;
}

VxColor ToColor(const BML_Color &value) {
    VxColor result;
    result.r = value.r;
    result.g = value.g;
    result.b = value.b;
    result.a = value.a;
    return result;
}

VxBbox ToBox(const BML_Box &value) {
    VxBbox result;
    result.Min = Convert::ToVxVector(value.Min);
    result.Max = Convert::ToVxVector(value.Max);
    return result;
}

class ScriptBehaviorValue final {
public:
    ScriptBehaviorValue() : Value(0) {}
    explicit ScriptBehaviorValue(Authoring::Value value)
        : Value(std::move(value)) {}

    CKGUID Type() const noexcept { return Value.Type(); }
    int Kind() const noexcept { return static_cast<int>(Value.Kind()); }
    bool IsNull() const noexcept { return Value.IsNull(); }

    Authoring::Value Value;
};

class ScriptBehaviorSlotValue final {
public:
    ScriptBehaviorSlotValue()
        : Slot(Authoring::Selector::Only()), Data(Authoring::Value(0)) {}
    ScriptBehaviorSlotValue(Authoring::Selector slot, Authoring::Value value)
        : Slot(std::move(slot)), Data(std::move(value)) {}

    Authoring::Selector Slot;
    Authoring::Value Data;
};

void ConstructValue(ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue();
}
void ConstructValueBool(bool value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueInt(int value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueFloat(float value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueVec2(const Vx2DVector &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueVec3(const VxVector &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueRect(const VxRect &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueMatrix(const VxMatrix &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueString(const std::string &value,
                          ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(Authoring::Value(value));
}
void ConstructValueQuaternion(const VxQuaternion &value,
                              ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(QuaternionValue(value));
}
void ConstructValueColor(const VxColor &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(ColorValue(value));
}
void ConstructValueBox(const VxBbox &value, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(BoxValue(value));
}
void ConstructValueObject(const CKGUID &type, CKObject *object,
                          ScriptBehaviorValue *self) {
    Authoring::Value value = Authoring::Value::Null(type);
    if (object) {
        ModContext *context = BML_GetModContext();
        const BML_ObjectRef reference = context
            ? context->ObjectRefs().Issue(object) : BML_ObjectRef{};
        if (!reference.Domain) {
            ScriptStringInterop::RaiseActiveException(
                "Cannot reference the CKObject used by a Behavior Value.");
        } else {
            value = Authoring::Value::Object(type, reference);
        }
    }
    new (self) ScriptBehaviorValue(std::move(value));
}
void CopyValue(const ScriptBehaviorValue &other, ScriptBehaviorValue *self) {
    new (self) ScriptBehaviorValue(other);
}
void DestructValue(ScriptBehaviorValue *self) { self->~ScriptBehaviorValue(); }
ScriptBehaviorValue &AssignValue(const ScriptBehaviorValue &other,
                                 ScriptBehaviorValue *self) {
    return *self = other;
}
ScriptBehaviorValue MakeEuler(const VxVector &value) {
    return ScriptBehaviorValue(EulerValue(value));
}

void ConstructSlotValue(ScriptBehaviorSlotValue *self) {
    new (self) ScriptBehaviorSlotValue();
}
void ConstructSlotValueAt(const ScriptBehaviorSelector &slot,
                          const ScriptBehaviorValue &value,
                          ScriptBehaviorSlotValue *self) {
    new (self) ScriptBehaviorSlotValue(slot.Value, value.Value);
}
void ConstructSlotValueNamed(const std::string &slot,
                             const ScriptBehaviorValue &value,
                             ScriptBehaviorSlotValue *self) {
    new (self) ScriptBehaviorSlotValue(
        Authoring::Selector::Unique(slot), value.Value);
}
void CopySlotValue(const ScriptBehaviorSlotValue &other,
                   ScriptBehaviorSlotValue *self) {
    new (self) ScriptBehaviorSlotValue(other);
}
void DestructSlotValue(ScriptBehaviorSlotValue *self) {
    self->~ScriptBehaviorSlotValue();
}
ScriptBehaviorSlotValue &AssignSlotValue(
    const ScriptBehaviorSlotValue &other, ScriptBehaviorSlotValue *self) {
    return *self = other;
}

void ConstructSelector(ScriptBehaviorSelector *self) {
    new (self) ScriptBehaviorSelector();
}
void CopySelector(const ScriptBehaviorSelector &other,
                  ScriptBehaviorSelector *self) {
    new (self) ScriptBehaviorSelector(other);
}
void DestructSelector(ScriptBehaviorSelector *self) { self->~ScriptBehaviorSelector(); }
ScriptBehaviorSelector &AssignSelector(const ScriptBehaviorSelector &other,
                                        ScriptBehaviorSelector *self) {
    return *self = other;
}
void ConstructFramePolicy(ScriptBehaviorFramePolicy *self) {
    new (self) ScriptBehaviorFramePolicy();
}
void CopyFramePolicy(const ScriptBehaviorFramePolicy &other,
                     ScriptBehaviorFramePolicy *self) {
    new (self) ScriptBehaviorFramePolicy(other);
}
void DestructFramePolicy(ScriptBehaviorFramePolicy *self) {
    self->~ScriptBehaviorFramePolicy();
}
ScriptBehaviorFramePolicy &AssignFramePolicy(
    const ScriptBehaviorFramePolicy &other,
    ScriptBehaviorFramePolicy *self) {
    return *self = other;
}

ScriptBehaviorSelector Only() {
    return ScriptBehaviorSelector(Authoring::Selector::Only());
}
ScriptBehaviorSelector At(int index) {
    return ScriptBehaviorSelector(Authoring::Selector::At(index));
}
ScriptBehaviorSelector Named(const std::string &name, int occurrence) {
    return ScriptBehaviorSelector(Authoring::Selector::Named(name, occurrence));
}
ScriptBehaviorSelector Unique(const std::string &name) {
    return ScriptBehaviorSelector(Authoring::Selector::Unique(name));
}

ScriptBehaviorFramePolicy Signals(unsigned int limit) {
    return {Authoring::Signals(limit)};
}
ScriptBehaviorFramePolicy EachFrame(unsigned int limit) {
    return {Authoring::EachFrame(limit)};
}
ScriptBehaviorFramePolicy Latest() { return {Authoring::Latest()}; }
ScriptBehaviorFramePolicy Ignore() { return {Authoring::Ignore()}; }

class BlockResource final : public ScriptBehaviorResource {
public:
    explicit BlockResource(Authoring::Block block)
        : Value(std::move(block)) {}
    ~BlockResource() override { Retire(); }

    void Retire() noexcept override { Value.reset(); }
    [[nodiscard]] bool IsOpen() const noexcept override {
        return Value.has_value();
    }

    std::optional<Authoring::Block> Value;
};

class RunResource final : public ScriptBehaviorResource {
public:
    enum class Kind { Call, Task, Instance };
    using ValueType = std::variant<std::monostate, Authoring::Call,
                                   Authoring::Task, Authoring::Instance>;

    template <class T>
    RunResource(Kind kind, T value) : Type(kind), Value(std::move(value)) {}
    ~RunResource() override { Retire(); }

    void Retire() noexcept override {
        std::visit([](auto &run) {
            using T = std::decay_t<decltype(run)>;
            if constexpr (!std::is_same_v<T, std::monostate>)
                (void) run.Close();
        }, Value);
        Value.emplace<std::monostate>();
    }
    [[nodiscard]] bool IsOpen() const noexcept override {
        return !std::holds_alternative<std::monostate>(Value);
    }

    Kind Type;
    ValueType Value;
};

template <class T>
class OwnedBehaviorResource final : public ScriptBehaviorResource {
public:
    explicit OwnedBehaviorResource(T value) : Value(std::move(value)) {}
    ~OwnedBehaviorResource() override { Retire(); }

    void Retire() noexcept override { Value.reset(); }
    [[nodiscard]] bool IsOpen() const noexcept override {
        return Value.has_value();
    }

    std::optional<T> Value;
};

using GraphResource = OwnedBehaviorResource<Authoring::Graph>;
using EditResource = OwnedBehaviorResource<Authoring::Edit>;
using PatchResource = OwnedBehaviorResource<Authoring::Patch>;
using PlanResource = OwnedBehaviorResource<Authoring::Plan>;
using ScriptResource = OwnedBehaviorResource<Authoring::Script>;
using WatchResource = OwnedBehaviorResource<Authoring::Watch>;

class ScriptBehaviorState final {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    std::string OwnerId;
    bool Active = false;
    std::atomic<std::size_t> CallbackHolders{0};
    std::optional<Authoring::Session> Session;
    std::vector<std::weak_ptr<ScriptBehaviorResource>> Resources;

    Authoring::Session *GetSession() {
        if (!Active || !Owner || !Context) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior authoring is not active for this Script Mod.");
            return nullptr;
        }
        if (!Session) {
            const char *ownerId = Owner->GetID();
            auto opened = Authoring::Session::Open(ownerId ? ownerId : "");
            if (!Accept("Open Behavior session", opened))
                return nullptr;
            Session.emplace(opened.Take());
        }
        return &*Session;
    }

    void Keep(const std::shared_ptr<ScriptBehaviorResource> &resource) {
        Resources.emplace_back(resource);
        if (Resources.size() < 64)
            return;
        auto out = Resources.begin();
        for (auto it = Resources.begin(); it != Resources.end(); ++it) {
            if (!it->expired())
                *out++ = std::move(*it);
        }
        Resources.erase(out, Resources.end());
    }

    BML::Behavior::Internal::Status Retire() {
        if (!Active)
            return {};
        Active = false;
        for (auto &weak : Resources) {
            if (auto resource = weak.lock())
                resource->Retire();
        }
        Resources.clear();
        Session.reset();
        BML::Behavior::Internal::Status status;
        if (Context && !OwnerId.empty())
            status = Context->RetireBehaviorOwner(OwnerId);
        if (status && CallbackHolders.load(std::memory_order_acquire) != 0) {
            status = BML::Behavior::Internal::Status(
                BML::Behavior::Internal::Error::CallbackFailed,
                CKERR_INVALIDPARAMETER, CKBR_BEHAVIORERROR,
                "Behavior callbacks remained referenced after owner retirement.");
        }
        Owner = nullptr;
        return status;
    }
};

template <class T>
Authoring::Result<T> InvalidHandle() {
    return Authoring::Result<T>::Failure(BML_ERROR_INVALID_HANDLE);
}

Authoring::Session *SessionFor(
    const std::shared_ptr<ScriptBehaviorState> &state) {
    return state ? state->GetSession() : nullptr;
}

bool ReadSlotValues(const std::shared_ptr<ScriptBehaviorState> &state,
                    const void *array,
                    std::vector<Authoring::SlotValue> &values) {
    if (!state || !state->Active || !state->Owner || !array) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior SlotValue array is not available.");
        return false;
    }
    const auto &api = state->Owner->GetRuntimeForFacade().GetApi();
    if (!api.ArrayGetSize || !api.ArrayGetConstElementAddress) {
        ScriptStringInterop::RaiseActiveException(
            "CKAngelScript does not expose array access.");
        return false;
    }
    CKDWORD count = 0;
    if (api.ArrayGetSize(const_cast<void *>(array), &count) != CKAS_OK) {
        ScriptStringInterop::RaiseActiveException(
            "Cannot read Behavior SlotValue array size.");
        return false;
    }
    try {
        std::vector<Authoring::SlotValue> copied;
        copied.reserve(count);
        for (CKDWORD index = 0; index < count; ++index) {
            const void *element = nullptr;
            if (api.ArrayGetConstElementAddress(array, index, &element) !=
                    CKAS_OK || !element) {
                ScriptStringInterop::RaiseActiveException(
                    "Cannot read a Behavior SlotValue array element.");
                return false;
            }
            const auto &value =
                *static_cast<const ScriptBehaviorSlotValue *>(element);
            copied.emplace_back(value.Slot, value.Data);
        }
        values = std::move(copied);
        return true;
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory copying Behavior SlotValue array.");
        return false;
    }
}

class ScriptBehaviorFrames;
class ScriptBehaviorFrame;
class ScriptBehaviorObjectRef;
class ScriptBehaviorObjectList;
class ScriptBehaviorCall;
class ScriptBehaviorTask;
class ScriptBehaviorInstance;
class ScriptBehaviorLayout;
class ScriptBehaviorSlot;
class ScriptBehaviorGraph;
class ScriptBehaviorNode;
class ScriptBehaviorPort;
class ScriptBehaviorLink;
class ScriptBehaviorObservedValue;
class ScriptBehaviorWatch;
class ScriptBehaviorChange;
class ScriptBehaviorHookEvent;
class ScriptBehaviorEdit;
class ScriptBehaviorEditGraph;
class ScriptBehaviorEditNode;
class ScriptBehaviorEditNodes;
class ScriptBehaviorEditPort;
class ScriptBehaviorEditPorts;
class ScriptBehaviorEditLink;
class ScriptBehaviorEditPath;
class ScriptBehaviorEditOperation;
class ScriptBehaviorPatch;
class ScriptBehaviorPlan;
class ScriptBehaviorScript;

class ScriptBehaviorNodePattern final {
public:
    ScriptBehaviorNodePattern() = default;
    explicit ScriptBehaviorNodePattern(Authoring::Selector selector)
        : Value(std::move(selector)) {}

    ScriptBehaviorNodePattern &Prototype(const CKGUID &prototype) {
        Value.Prototype(prototype);
        return *this;
    }
    ScriptBehaviorNodePattern &Kind(int kind) {
        if (kind < static_cast<int>(Authoring::BehaviorKind::Function) ||
            kind > static_cast<int>(Authoring::BehaviorKind::Graph)) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior kind is invalid.");
            return *this;
        }
        Value.Kind(static_cast<Authoring::BehaviorKind>(kind));
        return *this;
    }
    ScriptBehaviorNodePattern &Ins(int count) { Value.Ins(count); return *this; }
    ScriptBehaviorNodePattern &Outs(int count) { Value.Outs(count); return *this; }
    ScriptBehaviorNodePattern &Pins(int count) { Value.Pins(count); return *this; }
    ScriptBehaviorNodePattern &Pouts(int count) { Value.Pouts(count); return *this; }
    ScriptBehaviorNodePattern &Settings(int count) {
        Value.Settings(count); return *this;
    }
    ScriptBehaviorNodePattern &Locals(int count) {
        Value.Locals(count); return *this;
    }
    ScriptBehaviorNodePattern &Pin(const ScriptBehaviorSelector &slot,
                                   const ScriptBehaviorValue &value) {
        Value.Pin(slot.Value, value.Value); return *this;
    }
    ScriptBehaviorNodePattern &Pout(const ScriptBehaviorSelector &slot,
                                    const ScriptBehaviorValue &value) {
        Value.Pout(slot.Value, value.Value); return *this;
    }
    ScriptBehaviorNodePattern &Setting(const ScriptBehaviorSelector &slot,
                                       const ScriptBehaviorValue &value) {
        Value.Setting(slot.Value, value.Value); return *this;
    }
    ScriptBehaviorNodePattern &Local(const ScriptBehaviorSelector &slot,
                                     const ScriptBehaviorValue &value) {
        Value.Local(slot.Value, value.Value); return *this;
    }
    ScriptBehaviorNodePattern &Target(const ScriptBehaviorValue &value) {
        Value.Target(value.Value); return *this;
    }

    Authoring::NodePattern Value;
};

void ConstructNodePattern(ScriptBehaviorNodePattern *self) {
    new (self) ScriptBehaviorNodePattern();
}
void ConstructNodePatternSelector(const ScriptBehaviorSelector &selector,
                                  ScriptBehaviorNodePattern *self) {
    new (self) ScriptBehaviorNodePattern(selector.Value);
}
void ConstructNodePatternName(const std::string &name,
                              ScriptBehaviorNodePattern *self) {
    new (self) ScriptBehaviorNodePattern(Authoring::Selector::Unique(name));
}
void CopyNodePattern(const ScriptBehaviorNodePattern &other,
                     ScriptBehaviorNodePattern *self) {
    new (self) ScriptBehaviorNodePattern(other);
}
void DestructNodePattern(ScriptBehaviorNodePattern *self) {
    self->~ScriptBehaviorNodePattern();
}
ScriptBehaviorNodePattern &AssignNodePattern(
    const ScriptBehaviorNodePattern &other, ScriptBehaviorNodePattern *self) {
    return *self = other;
}

class ScriptBehaviorSlot final : public ScriptRef {
public:
    ScriptBehaviorSlot(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<Authoring::Layout> layout,
                       std::size_t index)
        : m_State(std::move(state)), m_Layout(std::move(layout)),
          m_Index(index) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Layout &&
               m_Index < m_Layout->Slots.size();
    }
    int Kind() const noexcept {
        return IsValid() ? static_cast<int>(Value().Kind) : -1;
    }
    std::uint64_t Generation() const noexcept {
        return IsValid() ? Value().Generation : 0;
    }
    bool Dynamic() const noexcept { return IsValid() && Value().Dynamic; }
    int Index() const noexcept { return IsValid() ? Value().Index : -1; }
    int Occurrence() const noexcept {
        return IsValid() ? Value().Occurrence : -1;
    }
    CKGUID Type() const noexcept {
        return IsValid() ? Value().Type : CKGUID(0, 0);
    }
    int ValueKind() const noexcept {
        return IsValid() && Value().Value
            ? static_cast<int>(*Value().Value) : -1;
    }
    std::string Name() const {
        return IsValid() ? Value().Name : std::string{};
    }
    std::string TypeName() const {
        return IsValid() ? Value().TypeName : std::string{};
    }
    const Authoring::Slot &Value() const noexcept {
        static const Authoring::Slot invalid{};
        return IsValid() ? m_Layout->Slots[m_Index] : invalid;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<Authoring::Layout> m_Layout;
    std::size_t m_Index = 0;
};

class ScriptBehaviorLayout final : public ScriptRef {
public:
    ScriptBehaviorLayout(std::shared_ptr<ScriptBehaviorState> state,
                         Authoring::Layout layout)
        : m_State(std::move(state)),
          m_Layout(std::make_shared<Authoring::Layout>(std::move(layout))) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Layout;
    }
    int Origin() const noexcept {
        return IsValid() ? static_cast<int>(m_Layout->Origin) : -1;
    }
    std::uint64_t Generation() const noexcept {
        return IsValid() ? m_Layout->Generation : 0;
    }
    int Kind() const noexcept {
        return IsValid() ? static_cast<int>(m_Layout->Kind) : -1;
    }
    int CompatibleClass() const noexcept {
        return IsValid() ? m_Layout->CompatibleClass : 0;
    }
    unsigned int PrototypeFlags() const noexcept {
        return IsValid() ? m_Layout->PrototypeFlags : 0;
    }
    unsigned int BehaviorFlags() const noexcept {
        return IsValid() ? m_Layout->BehaviorFlags : 0;
    }
    CKGUID Prototype() const noexcept {
        return IsValid() ? m_Layout->PrototypeRef.Id : CKGUID(0, 0);
    }
    std::uint64_t PrototypeGeneration() const noexcept {
        return IsValid() ? m_Layout->PrototypeRef.Generation : 0;
    }
    CKGUID TargetType() const noexcept {
        return IsValid() ? m_Layout->TargetType : CKGUID(0, 0);
    }
    std::string Name() const {
        return IsValid() ? m_Layout->Name : std::string{};
    }
    std::string Category() const {
        return IsValid() ? m_Layout->Category : std::string{};
    }
    std::string Provider() const {
        return IsValid() ? m_Layout->Provider : std::string{};
    }
    std::string Author() const {
        return IsValid() ? m_Layout->Author : std::string{};
    }
    std::string Description() const {
        return IsValid() ? m_Layout->Description : std::string{};
    }
    int SlotCount() const noexcept {
        return IsValid() ? static_cast<int>(m_Layout->Slots.size()) : 0;
    }
    ScriptBehaviorSlot *At(int index) const {
        if (!IsValid() || index < 0 ||
            static_cast<std::size_t>(index) >= m_Layout->Slots.size()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Layout slot index is out of range.");
            return nullptr;
        }
        return Wrap(static_cast<std::size_t>(index));
    }
    ScriptBehaviorSlot *Find(int kind,
                             const ScriptBehaviorSelector &selector) const {
        if (!IsValid()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Layout is stale.");
            return nullptr;
        }
        if (kind < static_cast<int>(Authoring::SlotKind::In) ||
            kind > static_cast<int>(Authoring::SlotKind::Target)) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior slot kind is invalid.");
            return nullptr;
        }

        const BML_BehaviorSelector requested =
            Authoring::Detail::Wire::From(selector.Value);
        std::optional<std::size_t> found;
        for (std::size_t index = 0; index < m_Layout->Slots.size(); ++index) {
            const Authoring::Slot &slot = m_Layout->Slots[index];
            if (static_cast<int>(slot.Kind) != kind ||
                !Matches(requested, slot))
                continue;
            if (found && (requested.Kind == BML_BEHAVIOR_SELECTOR_ONLY ||
                          requested.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME)) {
                ScriptStringInterop::RaiseActiveException(
                    "Behavior slot selector is ambiguous in this Layout.");
                return nullptr;
            }
            found = index;
            if (requested.Kind == BML_BEHAVIOR_SELECTOR_INDEX ||
                requested.Kind == BML_BEHAVIOR_SELECTOR_NAME)
                break;
        }
        if (!found) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior slot selector matched nothing in this Layout.");
            return nullptr;
        }
        return Wrap(*found);
    }
    ScriptBehaviorSlot *FindNamed(int kind, const std::string &name) const {
        return Find(kind, ScriptBehaviorSelector(
            Authoring::Selector::Unique(name)));
    }

private:
    static bool Matches(const BML_BehaviorSelector &selector,
                        const Authoring::Slot &slot) noexcept {
        switch (selector.Kind) {
        case BML_BEHAVIOR_SELECTOR_ONLY:
            return true;
        case BML_BEHAVIOR_SELECTOR_INDEX:
            return selector.Index == slot.Index;
        case BML_BEHAVIOR_SELECTOR_NAME:
            return selector.Occurrence == slot.Occurrence &&
                std::string_view(selector.Name.Data, selector.Name.Length) ==
                    slot.Name;
        case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
            return std::string_view(selector.Name.Data, selector.Name.Length) ==
                slot.Name;
        default:
            return false;
        }
    }
    ScriptBehaviorSlot *Wrap(std::size_t index) const {
        return NewScriptObject<ScriptBehaviorSlot>(
            "Behavior Slot", m_State, m_Layout, index);
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<Authoring::Layout> m_Layout;
};

class ScriptBehaviorBlock final : public ScriptRef {
public:
    ScriptBehaviorBlock(std::shared_ptr<ScriptBehaviorState> state,
                        std::shared_ptr<BlockResource> block)
        : m_State(std::move(state)), m_Block(std::move(block)) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Block && m_Block->IsOpen();
    }

    ScriptBehaviorBlock *Clone() const {
        Authoring::Block *block = Resolve("Clone Behavior Block");
        if (!block)
            return nullptr;
        try {
            auto resource = std::make_shared<BlockResource>(*block);
            m_State->Keep(resource);
            return NewScriptObject<ScriptBehaviorBlock>(
                "Behavior Block", m_State, std::move(resource));
        } catch (const std::bad_alloc &) {
            ScriptStringInterop::RaiseActiveException(
                "Out of memory cloning Behavior Block.");
            return nullptr;
        }
    }

    ScriptBehaviorBlock *TargetOwner() {
        return Change("Set Behavior target", [](Authoring::Block &block,
                                                 Authoring::Session &) {
            block.TargetOwner();
            return true;
        });
    }

    ScriptBehaviorBlock *Target(const CKGUID &type, CKObject *object) {
        return Change("Set Behavior target", [&](Authoring::Block &block,
                                                  Authoring::Session &session) {
            if (!object) {
                block.NullTarget(type);
                return true;
            }
            auto reference = session.Reference(object);
            if (!Accept("Reference Behavior target", reference))
                return false;
            block.Target(type, reference.Value());
            return true;
        });
    }

    ScriptBehaviorBlock *NullTarget(const CKGUID &type) {
        return Change("Set null Behavior target",
                      [&](Authoring::Block &block, Authoring::Session &) {
            block.NullTarget(type);
            return true;
        });
    }

    ScriptBehaviorBlock *Settings(const void *values) {
        return SetValues(values, "Set Behavior Settings",
                         [](Authoring::Block &block,
                            std::vector<Authoring::SlotValue> copied) {
                             block.Settings(std::move(copied));
                         });
    }
    ScriptBehaviorBlock *Pins(const void *values) {
        return SetValues(values, "Set Behavior Pins",
                         [](Authoring::Block &block,
                            std::vector<Authoring::SlotValue> copied) {
                             block.Pins(std::move(copied));
                         });
    }
    ScriptBehaviorBlock *Locals(const void *values) {
        return SetValues(values, "Set Behavior Locals",
                         [](Authoring::Block &block,
                            std::vector<Authoring::SlotValue> copied) {
                             block.Locals(std::move(copied));
                         });
    }
    ScriptBehaviorBlock *SettingValue(
        const ScriptBehaviorSelector &slot,
        const ScriptBehaviorValue &value) {
        return SetSetting(slot.Value, value.Value);
    }
    ScriptBehaviorBlock *SettingNamedValue(
        const std::string &slot, const ScriptBehaviorValue &value) {
        return SetSetting(Authoring::Selector::Unique(slot), value.Value);
    }
    ScriptBehaviorBlock *PinValue(
        const ScriptBehaviorSelector &slot,
        const ScriptBehaviorValue &value) {
        return SetPin(slot.Value, value.Value);
    }
    ScriptBehaviorBlock *PinNamedValue(
        const std::string &slot, const ScriptBehaviorValue &value) {
        return SetPin(Authoring::Selector::Unique(slot), value.Value);
    }
    ScriptBehaviorBlock *LocalValue(
        const ScriptBehaviorSelector &slot,
        const ScriptBehaviorValue &value) {
        return SetLocal(slot.Value, value.Value);
    }
    ScriptBehaviorBlock *LocalNamedValue(
        const std::string &slot, const ScriptBehaviorValue &value) {
        return SetLocal(Authoring::Selector::Unique(slot), value.Value);
    }

#define BML_SCRIPT_BEHAVIOR_BLOCK_VALUE(Name)                                     \
    ScriptBehaviorBlock *Name##Bool(const std::string &slot, bool value) {        \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorBool(                                      \
        const ScriptBehaviorSelector &slot, bool value) {                         \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##Int(const std::string &slot, int value) {          \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorInt(                                       \
        const ScriptBehaviorSelector &slot, int value) {                          \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##Float(const std::string &slot, float value) {      \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorFloat(                                     \
        const ScriptBehaviorSelector &slot, float value) {                        \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##String(const std::string &slot,                    \
                                       const std::string &value) {                 \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorString(                                    \
        const ScriptBehaviorSelector &slot, const std::string &value) {           \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##Vec2(const std::string &slot,                      \
                                     const Vx2DVector &value) {                   \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorVec2(                                      \
        const ScriptBehaviorSelector &slot, const Vx2DVector &value) {            \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##Vec3(const std::string &slot,                      \
                                     const VxVector &value) {                     \
        return Set##Name(Authoring::Selector::Unique(slot),                       \
                         Authoring::Value(value));                                \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorVec3(                                      \
        const ScriptBehaviorSelector &slot, const VxVector &value) {              \
        return Set##Name(slot.Value, Authoring::Value(value));                    \
    }                                                                             \
    ScriptBehaviorBlock *Name##Object(const std::string &slot,                    \
                                       const CKGUID &type, CKObject *value) {      \
        return Name##SelectorObject(                                              \
            ScriptBehaviorSelector(Authoring::Selector::Unique(slot)),            \
            type, value);                                                         \
    }                                                                             \
    ScriptBehaviorBlock *Name##SelectorObject(                                    \
        const ScriptBehaviorSelector &slot, const CKGUID &type,                   \
        CKObject *value) {                                                        \
        Authoring::Session *session = SessionFor(m_State);                        \
        if (!session)                                                              \
            return nullptr;                                                       \
        Authoring::Value object = Authoring::Value::Null(type);                   \
        if (value) {                                                               \
            auto reference = session->Reference(value);                           \
            if (!Accept("Reference Behavior parameter", reference))             \
                return nullptr;                                                   \
            object = Authoring::Value::Object(type, reference.Value());           \
        }                                                                          \
        return Set##Name(slot.Value, std::move(object));                          \
    }

    BML_SCRIPT_BEHAVIOR_BLOCK_VALUE(Setting)
    BML_SCRIPT_BEHAVIOR_BLOCK_VALUE(Pin)
    BML_SCRIPT_BEHAVIOR_BLOCK_VALUE(Local)

#undef BML_SCRIPT_BEHAVIOR_BLOCK_VALUE

    ScriptBehaviorBlock *PinType(const std::string &slot,
                                  const CKGUID &type) {
        return PinTypeAt(
            ScriptBehaviorSelector(Authoring::Selector::Unique(slot)), type);
    }
    ScriptBehaviorBlock *PinTypeAt(const ScriptBehaviorSelector &slot,
                                    const CKGUID &type) {
        return Change("Select Behavior Pin type",
                       [&](Authoring::Block &block, Authoring::Session &) {
            block.PinType(slot.Value, type);
            return true;
        });
    }
    ScriptBehaviorBlock *PoutType(const std::string &slot,
                                   const CKGUID &type) {
        return PoutTypeAt(
            ScriptBehaviorSelector(Authoring::Selector::Unique(slot)), type);
    }
    ScriptBehaviorBlock *PoutTypeAt(const ScriptBehaviorSelector &slot,
                                     const CKGUID &type) {
        return Change("Select Behavior Pout type",
                       [&](Authoring::Block &block, Authoring::Session &) {
            block.PoutType(slot.Value, type);
            return true;
        });
    }

    bool Validate() {
        Authoring::Block *block = Resolve("Validate Behavior Block");
        if (!block)
            return false;
        auto result = block->Validate();
        return Accept("Validate Behavior Block", result);
    }

    ScriptBehaviorCall *Call(const ScriptBehaviorSelector &input);
    ScriptBehaviorCall *CallWith(const ScriptBehaviorSelector &input,
                                 const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorCall *CallOn(
        CKBeObject *owner, const ScriptBehaviorSelector &input,
        const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorTask *Start(const ScriptBehaviorSelector &input);
    ScriptBehaviorTask *StartWith(const ScriptBehaviorSelector &input,
                                  const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorTask *StartOn(
        CKBeObject *owner, const ScriptBehaviorSelector &input,
        const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorInstance *Spawn();
    ScriptBehaviorInstance *SpawnWith(
        const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorInstance *SpawnOn(
        CKBeObject *owner, const ScriptBehaviorFramePolicy &frames);
    ScriptBehaviorInstance *SpawnIn(CKBehavior *graph);
    ScriptBehaviorInstance *SpawnInWith(
        CKBehavior *graph, const ScriptBehaviorFramePolicy &frames);

    Authoring::Block *ForEdit() const { return Resolve("Add Behavior Block"); }

private:
    template <class F>
    ScriptBehaviorBlock *SetValues(const void *array, const char *operation,
                                   F &&apply) {
        std::vector<Authoring::SlotValue> values;
        if (!ReadSlotValues(m_State, array, values))
            return nullptr;
        return Change(operation, [&](Authoring::Block &block,
                                     Authoring::Session &) {
            apply(block, std::move(values));
            return true;
        });
    }
    Authoring::Block *Resolve(const char *operation) const {
        if (IsValid())
            return &*m_Block->Value;
        std::string message = operation;
        message += ": Block is stale or its Script Mod is unloading.";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return nullptr;
    }

    template <class F>
    ScriptBehaviorBlock *Change(const char *operation, F &&change) {
        Authoring::Block *block = Resolve(operation);
        Authoring::Session *session = SessionFor(m_State);
        if (!block || !session || !change(*block, *session))
            return nullptr;
        AddRef();
        return this;
    }

    ScriptBehaviorBlock *SetSetting(const Authoring::Selector &slot,
                                     Authoring::Value value) {
        return Change("Set Behavior Setting",
                      [&](Authoring::Block &block, Authoring::Session &) {
            block.Settings({Authoring::SlotValue(slot, std::move(value))});
            return true;
        });
    }
    ScriptBehaviorBlock *SetPin(const Authoring::Selector &slot,
                                 Authoring::Value value) {
        return Change("Set Behavior Pin",
                      [&](Authoring::Block &block, Authoring::Session &) {
            block.Pins({Authoring::SlotValue(slot, std::move(value))});
            return true;
        });
    }
    ScriptBehaviorBlock *SetLocal(const Authoring::Selector &slot,
                                   Authoring::Value value) {
        return Change("Set Behavior Local",
                      [&](Authoring::Block &block, Authoring::Session &) {
            block.Locals({Authoring::SlotValue(slot, std::move(value))});
            return true;
        });
    }

    template <class T, class Wrapper>
    Wrapper *KeepRun(Authoring::Result<T> result, RunResource::Kind kind,
                     const char *operation) {
        if (!Accept(operation, result))
            return nullptr;
        auto run = std::make_shared<RunResource>(kind, result.Take());
        m_State->Keep(run);
        return NewScriptObject<Wrapper>(operation, m_State, std::move(run));
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<BlockResource> m_Block;
};

class ScriptBehaviorFrames final : public ScriptRef {
public:
    ScriptBehaviorFrames(std::shared_ptr<ScriptBehaviorState> state,
                         Authoring::Frames frames)
        : m_State(std::move(state)),
          m_Frames(std::make_shared<Authoring::Frames>(std::move(frames))) {}

    int Count() const noexcept {
        return m_Frames ? static_cast<int>(m_Frames->Size()) : 0;
    }
    bool Empty() const noexcept { return Count() == 0; }
    ScriptBehaviorFrame *At(int index) const;

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<Authoring::Frames> m_Frames;
};

class ScriptBehaviorObjectRef final : public ScriptRef {
public:
    ScriptBehaviorObjectRef(std::shared_ptr<ScriptBehaviorState> state,
                            BML_ObjectRef reference)
        : m_State(std::move(state)), m_Reference(reference) {}

    bool IsNull() const noexcept { return m_Reference.Domain == 0; }
    bool IsValid() const noexcept { return Borrow() != nullptr; }
    bool IsStale() const noexcept { return !IsNull() && !IsValid(); }
    unsigned int Domain() const noexcept { return m_Reference.Domain; }
    unsigned int Slot() const noexcept { return m_Reference.Slot; }
    unsigned int Generation() const noexcept { return m_Reference.Generation; }
    CKObject *Borrow() const noexcept {
        return m_State && m_State->Active && m_State->Context &&
                m_Reference.Domain
            ? m_State->Context->ObjectRefs().Resolve(m_Reference)
            : nullptr;
    }

    const BML_ObjectRef &Value() const noexcept { return m_Reference; }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    BML_ObjectRef m_Reference{};
};

class ScriptBehaviorObjectList final : public ScriptRef {
public:
    ScriptBehaviorObjectList(std::shared_ptr<ScriptBehaviorState> state,
                             std::shared_ptr<Authoring::Frames> frames,
                             Authoring::ObjectList objects)
        : m_State(std::move(state)), m_Frames(std::move(frames)),
          m_Objects(std::move(objects)) {}

    int Count() const noexcept {
        return m_Frames ? static_cast<int>(m_Objects.Size()) : 0;
    }
    bool Empty() const noexcept { return Count() == 0; }
    ScriptBehaviorObjectRef *At(int index) const {
        if (!m_Frames || index < 0 ||
            static_cast<std::size_t>(index) >= m_Objects.Size()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior ObjectList index is out of range.");
            return nullptr;
        }
        return NewScriptObject<ScriptBehaviorObjectRef>(
            "Behavior ObjectRef", m_State,
            m_Objects[static_cast<std::size_t>(index)]);
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<Authoring::Frames> m_Frames;
    Authoring::ObjectList m_Objects;
};

class ScriptBehaviorFrame final : public ScriptRef {
public:
    ScriptBehaviorFrame(std::shared_ptr<ScriptBehaviorState> state,
                        std::shared_ptr<Authoring::Frames> frames,
                        std::size_t index)
        : m_State(std::move(state)), m_Frames(std::move(frames)),
          m_Index(index) {}

    std::uint64_t Sequence() const { return View().Sequence(); }
    std::uint64_t GameFrame() const { return View().GameFrame(); }
    int NativeResult() const { return View().NativeResult(); }
    int Continuation() const {
        return static_cast<int>(View().Continuation());
    }
    int Error() const { return static_cast<int>(View().Error()); }
    int OutCount() const { return static_cast<int>(View().OutCount()); }
    int PoutCount() const { return static_cast<int>(View().PoutCount()); }
    int StatusCount() const { return static_cast<int>(View().StatusCount()); }
    bool HasOut(const ScriptBehaviorSelector &selector) const {
        return View().HasOut(selector.Value);
    }
    std::string OutName(int index) const {
        return std::string(View().GetOut(CheckIndex(index, OutCount(), "Out")).Name());
    }
    std::string PoutName(int index) const {
        return std::string(View().GetPout(CheckIndex(index, PoutCount(), "Pout")).Name());
    }
    std::string Status(int index) const {
        return View().GetStatus(CheckIndex(index, StatusCount(), "Status")).Message;
    }

    bool ReadBool(const ScriptBehaviorSelector &selector, bool &out) const {
        return Read(selector, out, "Read Behavior bool Pout");
    }
    bool ReadInt(const ScriptBehaviorSelector &selector, int &out) const {
        return Read(selector, out, "Read Behavior int Pout");
    }
    bool ReadFloat(const ScriptBehaviorSelector &selector, float &out) const {
        return Read(selector, out, "Read Behavior float Pout");
    }
    bool ReadString(const ScriptBehaviorSelector &selector,
                    std::string &out) const {
        auto result = View().Pout<std::string_view>(selector.Value);
        if (!Accept("Read Behavior string Pout", result))
            return false;
        out.assign(result.Value());
        return true;
    }
    bool ReadVec2(const ScriptBehaviorSelector &selector,
                  Vx2DVector &out) const {
        return Read(selector, out, "Read Behavior Vec2 Pout");
    }
    bool ReadVec3(const ScriptBehaviorSelector &selector,
                  VxVector &out) const {
        return Read(selector, out, "Read Behavior Vec3 Pout");
    }
    bool ReadQuaternion(const ScriptBehaviorSelector &selector,
                        VxQuaternion &out) const {
        auto result = View().Pout<BML_Quaternion>(selector.Value);
        if (!Accept("Read Behavior Quaternion Pout", result))
            return false;
        out = ToQuaternion(result.Value());
        return true;
    }
    bool ReadEuler(const ScriptBehaviorSelector &selector,
                   VxVector &out) const {
        auto result = View().Pout<BML_Euler>(selector.Value);
        if (!Accept("Read Behavior Euler Pout", result))
            return false;
        out = VxVector(result->x, result->y, result->z);
        return true;
    }
    bool ReadRect(const ScriptBehaviorSelector &selector,
                  VxRect &out) const {
        return Read(selector, out, "Read Behavior Rect Pout");
    }
    bool ReadColor(const ScriptBehaviorSelector &selector,
                   VxColor &out) const {
        auto result = View().Pout<BML_Color>(selector.Value);
        if (!Accept("Read Behavior Color Pout", result))
            return false;
        out = ToColor(result.Value());
        return true;
    }
    bool ReadBox(const ScriptBehaviorSelector &selector,
                 VxBbox &out) const {
        auto result = View().Pout<BML_Box>(selector.Value);
        if (!Accept("Read Behavior Box Pout", result))
            return false;
        out = ToBox(result.Value());
        return true;
    }
    bool ReadMatrix(const ScriptBehaviorSelector &selector,
                    VxMatrix &out) const {
        return Read(selector, out, "Read Behavior Matrix Pout");
    }
    ScriptBehaviorObjectRef *ReadObject(
        const ScriptBehaviorSelector &selector) const {
        auto result = View().Pout<BML_ObjectRef>(selector.Value);
        if (!Accept("Read Behavior object Pout", result))
            return nullptr;
        return NewScriptObject<ScriptBehaviorObjectRef>(
            "Behavior ObjectRef", m_State, result.Value());
    }
    ScriptBehaviorObjectList *ReadObjects(
        const ScriptBehaviorSelector &selector) const {
        auto result = View().Pout<Authoring::ObjectList>(selector.Value);
        if (!Accept("Read Behavior object-list Pout", result))
            return nullptr;
        return NewScriptObject<ScriptBehaviorObjectList>(
            "Behavior ObjectList", m_State, m_Frames, result.Take());
    }

private:
    const Authoring::Frame View() const {
        if (!m_Frames || m_Index >= m_Frames->Size())
            throw std::out_of_range("Behavior Frame is stale.");
        return (*m_Frames)[m_Index];
    }
    static std::size_t CheckIndex(int index, int count, const char *kind) {
        if (index >= 0 && index < count)
            return static_cast<std::size_t>(index);
        std::string message = "Behavior ";
        message += kind;
        message += " index is out of range.";
        throw std::out_of_range(message);
    }
    template <class T>
    bool Read(const ScriptBehaviorSelector &selector, T &out,
              const char *operation) const {
        auto result = View().Pout<T>(selector.Value);
        if (!Accept(operation, result))
            return false;
        out = result.Value();
        return true;
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<Authoring::Frames> m_Frames;
    std::size_t m_Index = 0;
};

ScriptBehaviorFrame *ScriptBehaviorFrames::At(int index) const {
    if (!m_Frames || index < 0 || static_cast<std::size_t>(index) >= m_Frames->Size()) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior Frame index is out of range.");
        return nullptr;
    }
    return NewScriptObject<ScriptBehaviorFrame>(
        "Behavior Frame", m_State, m_Frames,
        static_cast<std::size_t>(index));
}

class ScriptBehaviorRun : public ScriptRef {
public:
    ScriptBehaviorRun(std::shared_ptr<ScriptBehaviorState> state,
                      std::shared_ptr<RunResource> run,
                      RunResource::Kind kind)
        : m_State(std::move(state)), m_Run(std::move(run)), m_Kind(kind) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Run && m_Run->IsOpen() &&
               m_Run->Type == m_Kind;
    }
    int State() const {
        auto info = Info();
        return info ? static_cast<int>(info->State) : -1;
    }
    std::string Error() const {
        auto info = Info();
        return info ? info->LastStatus.Message : std::string("Behavior Run is stale.");
    }
    ScriptBehaviorFrames *TakeFrames() {
        auto result = Visit<Authoring::Frames>([](auto &run) {
            return run.TakeFrames();
        });
        if (!Accept("Take Behavior Frames", result))
            return nullptr;
        return NewScriptObject<ScriptBehaviorFrames>(
            "Behavior Frames", m_State, result.Take());
    }
    ScriptBehaviorLayout *Layout() {
        auto result = Visit<Authoring::Layout>([](const auto &run) {
            return run.Layout();
        });
        if (!Accept("Read Behavior Layout", result))
            return nullptr;
        return NewScriptObject<ScriptBehaviorLayout>(
            "Behavior Layout", m_State, result.Take());
    }
    ScriptBehaviorGraph *Inspect(bool live);
    bool Close() noexcept {
        if (!m_Run)
            return true;
        m_Run->Retire();
        return true;
    }
    bool PinValue(const ScriptBehaviorSelector &slot,
                  const ScriptBehaviorValue &value) {
        return SetValue(Authoring::SlotKind::Pin, slot.Value, value.Value,
                        "Set Behavior Pin");
    }
    bool LocalValue(const ScriptBehaviorSelector &slot,
                    const ScriptBehaviorValue &value) {
        return SetValue(Authoring::SlotKind::Local, slot.Value, value.Value,
                        "Set Behavior Local");
    }
    bool SettingValue(const ScriptBehaviorSelector &slot,
                      const ScriptBehaviorValue &value) {
        return SetValue(Authoring::SlotKind::Setting, slot.Value, value.Value,
                        "Set Behavior Setting");
    }
    bool Settings(const void *array) {
        std::vector<Authoring::SlotValue> values;
        if (!ReadSlotValues(m_State, array, values))
            return false;
        auto result = Visit<std::uint64_t>([&](auto &run) {
            return run.Settings(values);
        });
        return Accept("Set Behavior Settings", result);
    }
    bool Set(ScriptBehaviorSlot *slot, const ScriptBehaviorValue &value) {
        if (!slot || !slot->IsValid()) {
            ScriptStringInterop::RaiseActiveException(
                "Set Behavior value requires a valid Layout Slot.");
            return false;
        }
        auto result = Visit<std::uint64_t>([&](auto &run) {
            return run.Set(slot->Value(), value.Value);
        });
        return Accept("Set Behavior value", result);
    }
    bool Bind(ScriptBehaviorSlot *slot, ScriptBehaviorPort *source,
              int relation);

#define BML_SCRIPT_BEHAVIOR_RUN_VALUE(Name)                                      \
    bool Name##Bool(const std::string &slot, bool value) {                       \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorBool(const ScriptBehaviorSelector &slot, bool value) {     \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##Int(const std::string &slot, int value) {                         \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorInt(const ScriptBehaviorSelector &slot, int value) {       \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##Float(const std::string &slot, float value) {                     \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorFloat(const ScriptBehaviorSelector &slot, float value) {   \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##String(const std::string &slot, const std::string &value) {        \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorString(const ScriptBehaviorSelector &slot,                 \
                              const std::string &value) {                         \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##Vec2(const std::string &slot, const Vx2DVector &value) {           \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorVec2(const ScriptBehaviorSelector &slot,                   \
                            const Vx2DVector &value) {                            \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##Vec3(const std::string &slot, const VxVector &value) {             \
        return SetValue(Authoring::SlotKind::Name,                               \
                        Authoring::Selector::Unique(slot),                        \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }                                                                             \
    bool Name##SelectorVec3(const ScriptBehaviorSelector &slot,                   \
                            const VxVector &value) {                              \
        return SetValue(Authoring::SlotKind::Name, slot.Value,                    \
                        Authoring::Value(value), "Set Behavior " #Name);         \
    }

    BML_SCRIPT_BEHAVIOR_RUN_VALUE(Pin)
    BML_SCRIPT_BEHAVIOR_RUN_VALUE(Local)
    BML_SCRIPT_BEHAVIOR_RUN_VALUE(Setting)

#undef BML_SCRIPT_BEHAVIOR_RUN_VALUE

    bool PinObject(const std::string &slot, const CKGUID &type,
                   CKObject *object) {
        return PinObjectAt(
            ScriptBehaviorSelector(Authoring::Selector::Unique(slot)),
            type, object);
    }
    bool PinObjectAt(const ScriptBehaviorSelector &slot, const CKGUID &type,
                     CKObject *object) {
        Authoring::Session *session = SessionFor(m_State);
        if (!session)
            return false;
        Authoring::Value value = Authoring::Value::Null(type);
        if (object) {
            auto reference = session->Reference(object);
            if (!Accept("Reference Behavior Pin object", reference))
                return false;
            value = Authoring::Value::Object(type, reference.Value());
        }
        return SetValue(Authoring::SlotKind::Pin, slot.Value, std::move(value),
                        "Set Behavior Pin");
    }

protected:
    Authoring::Result<Authoring::RunInfo> Info() const {
        return Visit<Authoring::RunInfo>([](const auto &run) {
            return run.Info();
        });
    }

    template <class T, class F>
    Authoring::Result<T> Visit(F &&call) const {
        if (!IsValid())
            return InvalidHandle<T>();
        return std::visit([&](auto &run) -> Authoring::Result<T> {
            using R = std::decay_t<decltype(run)>;
            if constexpr (std::is_same_v<R, std::monostate>)
                return InvalidHandle<T>();
            else
                return call(run);
        }, m_Run->Value);
    }

    bool SetValue(Authoring::SlotKind kind, const Authoring::Selector &slot,
                   Authoring::Value value, const char *operation) {
        auto result = Visit<std::uint64_t>([&](auto &run) {
            return run.Set(kind, slot, value);
        });
        return Accept(operation, result);
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<RunResource> m_Run;
    RunResource::Kind m_Kind;
};

class ScriptBehaviorCall final : public ScriptBehaviorRun {
public:
    ScriptBehaviorCall(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<RunResource> run)
        : ScriptBehaviorRun(std::move(state), std::move(run),
                            RunResource::Kind::Call) {}

    ScriptBehaviorTask *Continue();
};

class ScriptBehaviorTask final : public ScriptBehaviorRun {
public:
    ScriptBehaviorTask(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<RunResource> run)
        : ScriptBehaviorRun(std::move(state), std::move(run),
                            RunResource::Kind::Task) {}

    int Pulse(const ScriptBehaviorSelector &input) {
        if (!IsValid()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Task is stale.");
            return -1;
        }
        auto *task = std::get_if<Authoring::Task>(&m_Run->Value);
        if (!task)
            return -1;
        auto result = task->Pulse(input.Value);
        return Accept("Pulse Behavior Task", result)
            ? static_cast<int>(result.Value()) : -1;
    }
};

class ScriptBehaviorInstance final : public ScriptBehaviorRun {
public:
    ScriptBehaviorInstance(std::shared_ptr<ScriptBehaviorState> state,
                           std::shared_ptr<RunResource> run)
        : ScriptBehaviorRun(std::move(state), std::move(run),
                            RunResource::Kind::Instance) {}

    int Pulse(const ScriptBehaviorSelector &input) {
        if (!IsValid()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Instance is stale.");
            return -1;
        }
        auto *instance = std::get_if<Authoring::Instance>(&m_Run->Value);
        if (!instance)
            return -1;
        auto result = instance->Pulse(input.Value);
        return Accept("Pulse Behavior Instance", result)
            ? static_cast<int>(result.Value()) : -1;
    }
};

ScriptBehaviorTask *ScriptBehaviorCall::Continue() {
    if (!IsValid()) {
        ScriptStringInterop::RaiseActiveException("Behavior Call is stale.");
        return nullptr;
    }
    auto *call = std::get_if<Authoring::Call>(&m_Run->Value);
    if (!call)
        return nullptr;
    auto continued = call->Continue();
    if (!Accept("Continue Behavior Call", continued))
        return nullptr;
    m_Run->Value.emplace<Authoring::Task>(continued.Take());
    m_Run->Type = RunResource::Kind::Task;
    return NewScriptObject<ScriptBehaviorTask>(
        "continued Behavior Task", m_State, m_Run);
}

ScriptBehaviorCall *ScriptBehaviorBlock::Call(
    const ScriptBehaviorSelector &input) {
    return CallWith(input, {Authoring::Signals()});
}
ScriptBehaviorCall *ScriptBehaviorBlock::CallWith(
    const ScriptBehaviorSelector &input,
    const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Call Behavior Block");
    if (!block)
        return nullptr;
    return KeepRun<Authoring::Call, ScriptBehaviorCall>(
        block->Call(input.Value, frames.Value), RunResource::Kind::Call,
        "Call Behavior Block");
}
ScriptBehaviorCall *ScriptBehaviorBlock::CallOn(
    CKBeObject *owner, const ScriptBehaviorSelector &input,
    const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Call Behavior Block");
    Authoring::Session *session = SessionFor(m_State);
    if (!block || !session || !owner) {
        if (!owner)
            ScriptStringInterop::RaiseActiveException(
                "Call requires a live Behavior owner.");
        return nullptr;
    }
    auto reference = session->Reference(owner);
    if (!Accept("Reference Behavior owner", reference))
        return nullptr;
    return KeepRun<Authoring::Call, ScriptBehaviorCall>(
        block->Call(reference.Value(), input.Value, frames.Value),
        RunResource::Kind::Call, "Call Behavior Block");
}
ScriptBehaviorTask *ScriptBehaviorBlock::Start(
    const ScriptBehaviorSelector &input) {
    return StartWith(input, {Authoring::Signals()});
}
ScriptBehaviorTask *ScriptBehaviorBlock::StartWith(
    const ScriptBehaviorSelector &input,
    const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Start Behavior Block");
    if (!block)
        return nullptr;
    return KeepRun<Authoring::Task, ScriptBehaviorTask>(
        block->Start(input.Value, frames.Value), RunResource::Kind::Task,
        "Start Behavior Block");
}
ScriptBehaviorTask *ScriptBehaviorBlock::StartOn(
    CKBeObject *owner, const ScriptBehaviorSelector &input,
    const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Start Behavior Block");
    Authoring::Session *session = SessionFor(m_State);
    if (!block || !session || !owner) {
        if (!owner)
            ScriptStringInterop::RaiseActiveException(
                "Start requires a live Behavior owner.");
        return nullptr;
    }
    auto reference = session->Reference(owner);
    if (!Accept("Reference Behavior owner", reference))
        return nullptr;
    return KeepRun<Authoring::Task, ScriptBehaviorTask>(
        block->Start(reference.Value(), input.Value, frames.Value),
        RunResource::Kind::Task, "Start Behavior Block");
}
ScriptBehaviorInstance *ScriptBehaviorBlock::Spawn() {
    return SpawnWith({Authoring::Signals()});
}
ScriptBehaviorInstance *ScriptBehaviorBlock::SpawnWith(
    const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Spawn Behavior Block");
    if (!block)
        return nullptr;
    return KeepRun<Authoring::Instance, ScriptBehaviorInstance>(
        block->Spawn(frames.Value), RunResource::Kind::Instance,
        "Spawn Behavior Block");
}
ScriptBehaviorInstance *ScriptBehaviorBlock::SpawnOn(
    CKBeObject *owner, const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Spawn Behavior Block");
    Authoring::Session *session = SessionFor(m_State);
    if (!block || !session || !owner) {
        if (!owner)
            ScriptStringInterop::RaiseActiveException(
                "Spawn requires a live Behavior owner.");
        return nullptr;
    }
    auto reference = session->Reference(owner);
    if (!Accept("Reference Behavior owner", reference))
        return nullptr;
    return KeepRun<Authoring::Instance, ScriptBehaviorInstance>(
        block->Spawn(reference.Value(), frames.Value),
        RunResource::Kind::Instance, "Spawn Behavior Block");
}
ScriptBehaviorInstance *ScriptBehaviorBlock::SpawnIn(CKBehavior *graph) {
    return SpawnInWith(graph, {Authoring::Signals()});
}
ScriptBehaviorInstance *ScriptBehaviorBlock::SpawnInWith(
    CKBehavior *graph, const ScriptBehaviorFramePolicy &frames) {
    Authoring::Block *block = Resolve("Spawn Behavior Block in graph");
    if (!block)
        return nullptr;
    if (!graph) {
        ScriptStringInterop::RaiseActiveException(
            "SpawnIn requires a live graph-backed CKBehavior.");
        return nullptr;
    }
    Authoring::Session *session = SessionFor(m_State);
    if (!session)
        return nullptr;
    auto reference = session->Reference(graph);
    if (!Accept("Reference Behavior graph", reference))
        return nullptr;
    return KeepRun<Authoring::Instance, ScriptBehaviorInstance>(
        block->SpawnIn(reference.Value(), frames.Value), RunResource::Kind::Instance,
        "Spawn Behavior Block in graph");
}

class ScriptBehaviorPort final : public ScriptRef {
public:
    ScriptBehaviorPort(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<GraphResource> graph,
                       Authoring::Port port)
        : m_State(std::move(state)), m_Graph(std::move(graph)),
          m_Port(std::move(port)) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Graph && m_Graph->IsOpen() &&
               static_cast<bool>(m_Port);
    }
    int Kind() const noexcept {
        return IsValid() ? static_cast<int>(m_Port.Kind()) : -1;
    }
    int Index() const noexcept { return IsValid() ? m_Port.Index() : -1; }
    int Occurrence() const noexcept {
        return IsValid() ? m_Port.Occurrence() : -1;
    }
    std::uint64_t NodeId() const noexcept {
        return IsValid() ? m_Port.Node() : 0;
    }
    std::uint64_t LayoutGeneration() const noexcept {
        return IsValid() ? m_Port.LayoutGeneration() : 0;
    }
    CKGUID Type() const noexcept {
        return IsValid() ? m_Port.Type() : CKGUID(0, 0);
    }
    bool Dynamic() const noexcept { return IsValid() && m_Port.Dynamic(); }
    bool Active() const noexcept { return IsValid() && m_Port.Active(); }
    std::string Name() const {
        return IsValid() ? std::string(m_Port.Name()) : std::string{};
    }
    const Authoring::Port &Value() const noexcept { return m_Port; }
    bool BelongsTo(const std::shared_ptr<GraphResource> &graph) const noexcept {
        return m_Graph == graph;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<GraphResource> m_Graph;
    Authoring::Port m_Port;
};

class ScriptBehaviorNode final : public ScriptRef {
public:
    ScriptBehaviorNode(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<GraphResource> graph,
                       Authoring::Node node)
        : m_State(std::move(state)), m_Graph(std::move(graph)),
          m_Node(std::move(node)) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Graph && m_Graph->IsOpen() &&
               static_cast<bool>(m_Node);
    }
    std::uint64_t Id() const noexcept { return IsValid() ? m_Node.Id() : 0; }
    int Index() const noexcept { return IsValid() ? m_Node.Index() : -1; }
    int Occurrence() const noexcept {
        return IsValid() ? m_Node.Occurrence() : -1;
    }
    std::uint64_t LayoutGeneration() const noexcept {
        return IsValid() ? m_Node.LayoutGeneration() : 0;
    }
    int Kind() const noexcept {
        return IsValid() ? static_cast<int>(m_Node.Kind()) : -1;
    }
    bool IsGraph() const noexcept { return IsValid() && m_Node.IsGraph(); }
    CKGUID Prototype() const noexcept {
        return IsValid() ? m_Node.Prototype() : CKGUID(0, 0);
    }
    int Priority() const noexcept {
        return IsValid() ? m_Node.Priority() : 0;
    }
    bool Active() const noexcept { return IsValid() && m_Node.Active(); }
    std::string Name() const {
        return IsValid() ? std::string(m_Node.Name()) : std::string{};
    }
    int PortCount() const noexcept {
        return IsValid() ? static_cast<int>(m_Node.Ports().size()) : 0;
    }
    ScriptBehaviorPort *PortAt(int index) const {
        if (!IsValid() || index < 0 ||
            static_cast<std::size_t>(index) >= m_Node.Ports().size()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Node port index is out of range.");
            return nullptr;
        }
        return NewScriptObject<ScriptBehaviorPort>(
            "Behavior Port", m_State, m_Graph,
            m_Node.Ports()[static_cast<std::size_t>(index)]);
    }

#define BML_SCRIPT_GRAPH_PORT(Name)                                               \
    ScriptBehaviorPort *Name(const ScriptBehaviorSelector &slot) const {          \
        return WrapPort(m_Node.Name(slot.Value), "Behavior Node " #Name);        \
    }                                                                             \
    ScriptBehaviorPort *Name##ByName(const std::string &slot) const {             \
        return WrapPort(m_Node.Name(slot), "Behavior Node " #Name);              \
    }

    BML_SCRIPT_GRAPH_PORT(In)
    BML_SCRIPT_GRAPH_PORT(Out)
    BML_SCRIPT_GRAPH_PORT(Pin)
    BML_SCRIPT_GRAPH_PORT(Pout)
    BML_SCRIPT_GRAPH_PORT(Setting)
    BML_SCRIPT_GRAPH_PORT(Local)
#undef BML_SCRIPT_GRAPH_PORT

    ScriptBehaviorPort *Target() const {
        return WrapPort(m_Node.Target(), "Behavior Node Target");
    }
    const Authoring::Node &Value() const noexcept { return m_Node; }
    const std::shared_ptr<GraphResource> &GraphResourceValue() const noexcept {
        return m_Graph;
    }
    bool BelongsTo(const std::shared_ptr<GraphResource> &graph) const noexcept {
        return m_Graph == graph;
    }

private:
    ScriptBehaviorPort *WrapPort(Authoring::Port port,
                                 const char *description) const {
        if (!IsValid() || !port) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Node port selector matched nothing or was ambiguous.");
            return nullptr;
        }
        return NewScriptObject<ScriptBehaviorPort>(
            description, m_State, m_Graph, std::move(port));
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<GraphResource> m_Graph;
    Authoring::Node m_Node;
};

class ScriptBehaviorLink final : public ScriptRef {
public:
    ScriptBehaviorLink(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<GraphResource> graph,
                       Authoring::Link link)
        : m_State(std::move(state)), m_Graph(std::move(graph)),
          m_Link(std::move(link)) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Graph && m_Graph->IsOpen() &&
               static_cast<bool>(m_Link);
    }
    std::uint64_t Id() const noexcept { return IsValid() ? m_Link.Id() : 0; }
    int InitialDelay() const noexcept {
        return IsValid() ? m_Link.InitialDelay() : 0;
    }
    int RemainingDelay() const noexcept {
        return IsValid() ? m_Link.RemainingDelay() : 0;
    }
    int Pending() const noexcept {
        return IsValid() ? static_cast<int>(m_Link.Pending()) : -1;
    }
    ScriptBehaviorPort *Source() const {
        return Wrap(m_Link.Source(), "Behavior Link source");
    }
    ScriptBehaviorPort *Target() const {
        return Wrap(m_Link.Target(), "Behavior Link target");
    }
    const Authoring::Link &Value() const noexcept { return m_Link; }
    bool BelongsTo(const std::shared_ptr<GraphResource> &graph) const noexcept {
        return m_Graph == graph;
    }

private:
    ScriptBehaviorPort *Wrap(Authoring::Port port,
                             const char *description) const {
        if (!IsValid() || !port)
            return nullptr;
        return NewScriptObject<ScriptBehaviorPort>(
            description, m_State, m_Graph, std::move(port));
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<GraphResource> m_Graph;
    Authoring::Link m_Link;
};

class ScriptBehaviorObservedValue final : public ScriptRef {
public:
    ScriptBehaviorObservedValue(std::shared_ptr<ScriptBehaviorState> state,
                                Authoring::ObservedValue value)
        : m_State(std::move(state)), m_Value(std::move(value)) {}

    bool IsAvailable() const noexcept {
        return m_State && m_State->Active &&
            m_Value.State == Authoring::ObservationState::Available;
    }
    int State() const noexcept {
        return m_State && m_State->Active
            ? static_cast<int>(m_Value.State) : -1;
    }
    int Relation() const noexcept {
        return m_State && m_State->Active
            ? static_cast<int>(m_Value.Source) : -1;
    }
    int Kind() const noexcept {
        return m_State && m_State->Active && m_Value.Kind
            ? static_cast<int>(*m_Value.Kind) : -1;
    }
    CKGUID Type() const noexcept {
        return m_State && m_State->Active ? m_Value.Type : CKGUID(0, 0);
    }
    bool ReadBool(bool &out) const { return Read(out, "bool"); }
    bool ReadInt(int &out) const { return Read(out, "int"); }
    bool ReadFloat(float &out) const { return Read(out, "float"); }
    bool ReadString(std::string &out) const {
        return Read(out, "string");
    }
    bool ReadVec2(Vx2DVector &out) const {
        if (!Available("Vec2"))
            return false;
        const auto *value = std::get_if<BML_Vec2>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Vec2");
        out = Convert::ToVxVector(*value);
        return true;
    }
    bool ReadVec3(VxVector &out) const {
        if (!Available("Vec3"))
            return false;
        const auto *value = std::get_if<BML_Vec3>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Vec3");
        out = Convert::ToVxVector(*value);
        return true;
    }
    bool ReadQuaternion(VxQuaternion &out) const {
        if (!Available("Quaternion"))
            return false;
        const auto *value = std::get_if<BML_Quaternion>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Quaternion");
        out = ToQuaternion(*value);
        return true;
    }
    bool ReadEuler(VxVector &out) const {
        if (!Available("Euler"))
            return false;
        const auto *value = std::get_if<BML_Euler>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Euler");
        out = VxVector(value->x, value->y, value->z);
        return true;
    }
    bool ReadRect(VxRect &out) const {
        if (!Available("Rect"))
            return false;
        const auto *value = std::get_if<BML_Rect>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Rect");
        out = VxRect(value->left, value->top, value->right, value->bottom);
        return true;
    }
    bool ReadColor(VxColor &out) const {
        if (!Available("Color"))
            return false;
        const auto *value = std::get_if<BML_Color>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Color");
        out = ToColor(*value);
        return true;
    }
    bool ReadBox(VxBbox &out) const {
        if (!Available("Box"))
            return false;
        const auto *value = std::get_if<BML_Box>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Box");
        out = ToBox(*value);
        return true;
    }
    bool ReadMatrix(VxMatrix &out) const {
        if (!Available("Matrix"))
            return false;
        const auto *value = std::get_if<BML_Mat4>(&m_Value.Data);
        if (!value)
            return TypeMismatch("Matrix");
        out = Convert::ToVxMatrix(*value);
        return true;
    }
    ScriptBehaviorObjectRef *ReadObject() const {
        if (!Available("object"))
            return nullptr;
        const auto *value = std::get_if<BML_ObjectRef>(&m_Value.Data);
        if (!value)
            return TypeMismatch("object"), nullptr;
        return NewScriptObject<ScriptBehaviorObjectRef>(
            "Behavior ObjectRef", m_State, *value);
    }

private:
    bool Available(const char *type) const {
        if (IsAvailable())
            return true;
        std::string message = "Behavior value is not available as ";
        message += type;
        message += ".";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return false;
    }
    bool TypeMismatch(const char *type) const {
        std::string message = "Behavior value is not a ";
        message += type;
        message += ".";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return false;
    }
    template <class T>
    bool Read(T &out, const char *type) const {
        if (!Available(type))
            return false;
        const auto *value = std::get_if<T>(&m_Value.Data);
        if (!value)
            return TypeMismatch(type);
        out = *value;
        return true;
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    Authoring::ObservedValue m_Value;
};

class ScriptBehaviorHookEvent final {
public:
    ScriptBehaviorHookEvent() = default;
    ScriptBehaviorHookEvent(std::shared_ptr<ScriptBehaviorState> state,
                            Authoring::HookEvent event)
        : m_State(std::move(state)), m_Event(std::move(event)) {}

    float DeltaTime() const noexcept { return m_Event.DeltaTime; }
    CKBehavior *BorrowBlock() const {
        return static_cast<CKBehavior *>(Resolve(m_Event.Block,
                                                 CKCID_BEHAVIOR));
    }
    CKBehavior *BorrowScript() const {
        return static_cast<CKBehavior *>(Resolve(m_Event.Script,
                                                 CKCID_BEHAVIOR));
    }
    CKBeObject *BorrowOwner() const {
        return static_cast<CKBeObject *>(Resolve(m_Event.Owner,
                                                 CKCID_BEOBJECT));
    }

private:
    CKObject *Resolve(const BML_ObjectRef &reference,
                      CK_CLASSID classId) const {
        if (!m_State || !m_State->Active || !m_State->Context ||
            !reference.Domain)
            return nullptr;
        CKObject *object = m_State->Context->ObjectRefs().Resolve(reference);
        return object && CKIsChildClassOf(object, classId) ? object : nullptr;
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    Authoring::HookEvent m_Event;
};

class ScriptBehaviorChange final {
public:
    ScriptBehaviorChange() = default;
    ScriptBehaviorChange(std::shared_ptr<ScriptBehaviorState> state,
                         Authoring::Change change)
        : m_State(std::move(state)), m_Change(std::move(change)) {}

    int Kind() const noexcept { return static_cast<int>(m_Change.Kind); }
    std::uint64_t Sequence() const noexcept { return m_Change.Sequence; }
    std::uint64_t GameFrame() const noexcept { return m_Change.GameFrame; }
    std::uint64_t Before() const noexcept { return m_Change.Before; }
    std::uint64_t After() const noexcept { return m_Change.After; }
    ScriptBehaviorObservedValue *Previous() const {
        return NewScriptObject<ScriptBehaviorObservedValue>(
            "previous Behavior value", m_State, m_Change.PreviousValue);
    }
    ScriptBehaviorObservedValue *Current() const {
        return NewScriptObject<ScriptBehaviorObservedValue>(
            "current Behavior value", m_State, m_Change.CurrentValue);
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    Authoring::Change m_Change;
};

template <class T>
void ConstructBehaviorValue(T *self) {
    new (self) T();
}
template <class T>
void CopyBehaviorValue(const T &other, T *self) {
    new (self) T(other);
}
template <class T>
void DestructBehaviorValue(T *self) {
    self->~T();
}
template <class T>
T &AssignBehaviorValue(const T &other, T *self) {
    return *self = other;
}

struct ScriptBehaviorHookCall {
    ScriptModContextView *Context = nullptr;
    ScriptBehaviorHookEvent *Event = nullptr;
    int Result = static_cast<int>(Authoring::HookResult::Ok);
};

int WriteBehaviorHookArgs(asIScriptContext *context, void *userdata) {
    auto *call = static_cast<ScriptBehaviorHookCall *>(userdata);
    int code = context->SetArgObject(0, call ? call->Context : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, call ? call->Event : nullptr);
    return code;
}

int ReadBehaviorHookResult(asIScriptContext *context, void *userdata) {
    auto *call = static_cast<ScriptBehaviorHookCall *>(userdata);
    if (!context || !call)
        return asERROR;
    call->Result = static_cast<int>(context->GetReturnDWord());
    switch (static_cast<Authoring::HookResult>(call->Result)) {
    case Authoring::HookResult::Error:
    case Authoring::HookResult::Ok:
    case Authoring::HookResult::AgainNextFrame:
    case Authoring::HookResult::Fault:
        return asSUCCESS;
    default:
        return asINVALID_ARG;
    }
}

struct ScriptBehaviorWatchCall {
    ScriptModContextView *Context = nullptr;
    ScriptBehaviorChange *Change = nullptr;
};

int WriteBehaviorWatchArgs(asIScriptContext *context, void *userdata) {
    auto *call = static_cast<ScriptBehaviorWatchCall *>(userdata);
    int code = context->SetArgObject(0, call ? call->Context : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, call ? call->Change : nullptr);
    return code;
}

class ScriptBehaviorHookFunction final {
public:
    ScriptBehaviorHookFunction(std::shared_ptr<ScriptBehaviorState> state,
                               asIScriptFunction *function)
        : m_State(std::move(state)), m_Function(function) {
        if (m_State)
            m_State->CallbackHolders.fetch_add(1, std::memory_order_relaxed);
        if (m_Function)
            m_Function->AddRef();
    }
    ~ScriptBehaviorHookFunction() {
        if (m_Function)
            m_Function->Release();
        if (m_State)
            m_State->CallbackHolders.fetch_sub(1, std::memory_order_acq_rel);
    }

    static bool HasSignature(asIScriptFunction *function) {
        if (!function || !function->GetEngine())
            return false;
        const ScriptFunctionParam params[] = {
            {"BML::ModContext", asTM_INREF | asTM_CONST},
            {"BML::Behavior::HookEvent", asTM_INREF | asTM_CONST},
        };
        const int result = function->GetEngine()->GetTypeIdByDecl(
            "BML::Behavior::HookResult");
        return result >= 0 && ScriptFunctionHasSignature(
            function, result, params, 2);
    }

    Authoring::HookResult Invoke(const Authoring::HookEvent &source) {
        if (!m_State || !m_State->Active || !m_State->Owner ||
            !m_State->Owner->CanDispatchScriptServiceCallback())
            return Authoring::HookResult::Ok;

        ScriptBehaviorHookEvent event(m_State, source);
        ScriptBehaviorHookCall args{
            m_State->Owner->BorrowContextView(), &event,
            static_cast<int>(Authoring::HookResult::Ok)};
        ScriptFunctionCall call;
        call.Function = m_Function;
        call.Owner = m_State->Owner;
        call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = "Behavior Hook callback failed";
        call.InvalidStateMessage =
            "Behavior Hook callback has invalid runtime state.";
        call.ContextFailureMessage =
            "Unable to create AngelScript context for Behavior Hook callback.";
        call.SuspendedMessage = "Behavior Hook callback suspended";
        call.WriteArgs = WriteBehaviorHookArgs;
        call.ReadResult = ReadBehaviorHookResult;
        call.UserData = &args;

        ScriptDiagnostic diagnostic;
        if (!ExecuteScriptFunction(call, diagnostic)) {
            m_State->Owner->SetLoadFailure(diagnostic);
            return Authoring::HookResult::Fault;
        }
        return static_cast<Authoring::HookResult>(args.Result);
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    asIScriptFunction *m_Function = nullptr;
};

class ScriptBehaviorWatchFunction final {
public:
    ScriptBehaviorWatchFunction(std::shared_ptr<ScriptBehaviorState> state,
                                asIScriptFunction *function)
        : m_State(std::move(state)), m_Function(function) {
        if (m_State)
            m_State->CallbackHolders.fetch_add(1, std::memory_order_relaxed);
        if (m_Function)
            m_Function->AddRef();
    }
    ~ScriptBehaviorWatchFunction() {
        if (m_Function)
            m_Function->Release();
        if (m_State)
            m_State->CallbackHolders.fetch_sub(1, std::memory_order_acq_rel);
    }

    static bool HasSignature(asIScriptFunction *function) {
        const ScriptFunctionParam params[] = {
            {"BML::ModContext", asTM_INREF | asTM_CONST},
            {"BML::Behavior::Change", asTM_INREF | asTM_CONST},
        };
        return ScriptFunctionHasSignature(function, asTYPEID_VOID, params, 2);
    }

    void Invoke(const Authoring::Change &source) {
        if (!m_State || !m_State->Active || !m_State->Owner ||
            !m_State->Owner->CanDispatchScriptServiceCallback())
            return;

        ScriptBehaviorChange change(m_State, source);
        ScriptBehaviorWatchCall args{
            m_State->Owner->BorrowContextView(), &change};
        ScriptFunctionCall call;
        call.Function = m_Function;
        call.Owner = m_State->Owner;
        call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = "Behavior Watch callback failed";
        call.InvalidStateMessage =
            "Behavior Watch callback has invalid runtime state.";
        call.ContextFailureMessage =
            "Unable to create AngelScript context for Behavior Watch callback.";
        call.SuspendedMessage = "Behavior Watch callback suspended";
        call.WriteArgs = WriteBehaviorWatchArgs;
        call.UserData = &args;

        ScriptDiagnostic diagnostic;
        if (!ExecuteScriptFunction(call, diagnostic)) {
            m_State->Owner->SetLoadFailure(diagnostic);
            throw std::runtime_error(diagnostic.Message);
        }
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    asIScriptFunction *m_Function = nullptr;
};

class ScriptBehaviorWatch final : public ScriptRef {
public:
    ScriptBehaviorWatch(std::shared_ptr<ScriptBehaviorState> state,
                        std::shared_ptr<WatchResource> watch)
        : m_State(std::move(state)), m_Watch(std::move(watch)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Watch && m_Watch->IsOpen();
    }
    int State() const { return Read().first; }
    std::string Error() const { return Read().second; }
    int Close() noexcept {
        if (!m_Watch || !m_Watch->Value)
            return static_cast<int>(Authoring::CloseState::Closed);
        auto result = m_Watch->Value->Close();
        if (!result)
            return -1;
        if (result.Value() == Authoring::CloseState::Closed)
            m_Watch->Value.reset();
        return static_cast<int>(result.Value());
    }

private:
    std::pair<int, std::string> Read() const {
        if (!IsValid())
            return {-1, "Behavior Watch is stale."};
        auto info = m_Watch->Value->Info();
        return info ? std::pair<int, std::string>{
            static_cast<int>(info->State), info->LastStatus.Message}
            : std::pair<int, std::string>{-1, info.GetStatus().Message};
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<WatchResource> m_Watch;
};

class ScriptBehaviorGraph final : public ScriptRef {
public:
    ScriptBehaviorGraph(std::shared_ptr<ScriptBehaviorState> state,
                        std::shared_ptr<GraphResource> graph)
        : m_State(std::move(state)), m_Graph(std::move(graph)) {}

    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Graph && m_Graph->IsOpen();
    }
    int View() const noexcept {
        return IsValid() ? static_cast<int>(m_Graph->Value->Mode()) : -1;
    }
    std::uint64_t Generation() const noexcept {
        return IsValid() ? m_Graph->Value->Generation() : 0;
    }
    std::uint64_t Fingerprint() const noexcept {
        return IsValid() ? m_Graph->Value->Fingerprint() : 0;
    }
    int NodeCount() const noexcept {
        return IsValid()
            ? static_cast<int>(m_Graph->Value->Nodes().size()) : 0;
    }
    int LinkCount() const noexcept {
        return IsValid()
            ? static_cast<int>(m_Graph->Value->Links().size()) : 0;
    }
    ScriptBehaviorNode *Root() const {
        return WrapNode(IsValid() ? m_Graph->Value->Root() : Authoring::Node{});
    }
    ScriptBehaviorNode *NodeAt(int index) const {
        if (!IsValid() || index < 0 ||
            static_cast<std::size_t>(index) >= m_Graph->Value->Nodes().size()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Graph node index is out of range.");
            return nullptr;
        }
        return WrapNode(m_Graph->Value->Nodes()[static_cast<std::size_t>(index)]);
    }
    ScriptBehaviorLink *LinkAt(int index) const {
        if (!IsValid() || index < 0 ||
            static_cast<std::size_t>(index) >= m_Graph->Value->Links().size()) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Graph link index is out of range.");
            return nullptr;
        }
        return WrapLink(m_Graph->Value->Links()[static_cast<std::size_t>(index)]);
    }
    ScriptBehaviorNode *Find(const ScriptBehaviorSelector &selector,
                             const CKGUID &prototype) const {
        if (!IsValid())
            return nullptr;
        auto found = m_Graph->Value->Find(selector.Value, prototype);
        if (!Accept("Find Behavior Node", found))
            return nullptr;
        return WrapNode(found.Take());
    }
    int IncomingNodeCount(ScriptBehaviorNode *node) const {
        return node && node->IsValid() && node->BelongsTo(m_Graph)
            ? static_cast<int>(m_Graph->Value->Incoming(node->Value()).size())
            : InvalidRange("Read incoming Behavior Links");
    }
    int IncomingPortCount(ScriptBehaviorPort *port) const {
        return port && port->IsValid() && port->BelongsTo(m_Graph)
            ? static_cast<int>(m_Graph->Value->Incoming(port->Value()).size())
            : InvalidRange("Read incoming Behavior Links");
    }
    int OutgoingNodeCount(ScriptBehaviorNode *node) const {
        return node && node->IsValid() && node->BelongsTo(m_Graph)
            ? static_cast<int>(m_Graph->Value->Outgoing(node->Value()).size())
            : InvalidRange("Read outgoing Behavior Links");
    }
    int OutgoingPortCount(ScriptBehaviorPort *port) const {
        return port && port->IsValid() && port->BelongsTo(m_Graph)
            ? static_cast<int>(m_Graph->Value->Outgoing(port->Value()).size())
            : InvalidRange("Read outgoing Behavior Links");
    }
    ScriptBehaviorLink *IncomingNode(ScriptBehaviorNode *node, int index) const {
        if (!node || !node->IsValid() || !node->BelongsTo(m_Graph))
            return InvalidLink("Read incoming Behavior Link");
        return LinkFrom(m_Graph->Value->Incoming(node->Value()), index,
                        "incoming");
    }
    ScriptBehaviorLink *IncomingPort(ScriptBehaviorPort *port, int index) const {
        if (!port || !port->IsValid() || !port->BelongsTo(m_Graph))
            return InvalidLink("Read incoming Behavior Link");
        return LinkFrom(m_Graph->Value->Incoming(port->Value()), index,
                        "incoming");
    }
    ScriptBehaviorLink *OutgoingNode(ScriptBehaviorNode *node, int index) const {
        if (!node || !node->IsValid() || !node->BelongsTo(m_Graph))
            return InvalidLink("Read outgoing Behavior Link");
        return LinkFrom(m_Graph->Value->Outgoing(node->Value()), index,
                        "outgoing");
    }
    ScriptBehaviorLink *OutgoingPort(ScriptBehaviorPort *port, int index) const {
        if (!port || !port->IsValid() || !port->BelongsTo(m_Graph))
            return InvalidLink("Read outgoing Behavior Link");
        return LinkFrom(m_Graph->Value->Outgoing(port->Value()), index,
                        "outgoing");
    }
    ScriptBehaviorLink *EnteringNode(ScriptBehaviorNode *node) const {
        return UniqueLink(node, true);
    }
    ScriptBehaviorLink *EnteringPort(ScriptBehaviorPort *port) const {
        return UniqueLink(port, true);
    }
    ScriptBehaviorLink *LeavingNode(ScriptBehaviorNode *node) const {
        return UniqueLink(node, false);
    }
    ScriptBehaviorLink *LeavingPort(ScriptBehaviorPort *port) const {
        return UniqueLink(port, false);
    }
    ScriptBehaviorNode *PreviousNode(ScriptBehaviorNode *node) const {
        return Adjacent(node, true);
    }
    ScriptBehaviorNode *PreviousPort(ScriptBehaviorPort *port) const {
        return Adjacent(port, true);
    }
    ScriptBehaviorNode *NextNode(ScriptBehaviorNode *node) const {
        return Adjacent(node, false);
    }
    ScriptBehaviorNode *NextPort(ScriptBehaviorPort *port) const {
        return Adjacent(port, false);
    }
    ScriptBehaviorObservedValue *Read(ScriptBehaviorPort *port) const {
        if (!IsValid() || !port || !port->IsValid() ||
            !port->BelongsTo(m_Graph)) {
            InvalidRange("Read Behavior parameter");
            return nullptr;
        }
        auto value = m_Graph->Value->Read(port->Value());
        if (!Accept("Read Behavior parameter", value))
            return nullptr;
        return NewScriptObject<ScriptBehaviorObservedValue>(
            "observed Behavior value", m_State, value.Take());
    }
    ScriptBehaviorGraph *Inspect(ScriptBehaviorNode *node) const;
    ScriptBehaviorGraph *Logical() const;
    ScriptBehaviorGraph *Live() const;
    ScriptBehaviorPatch *Apply(const std::string &name,
                               ScriptBehaviorEdit *edit) const;
    ScriptBehaviorWatch *WatchGraph(asIScriptFunction *callback) const;
    ScriptBehaviorWatch *WatchLayout(ScriptBehaviorNode *node,
                                     asIScriptFunction *callback) const;
    ScriptBehaviorWatch *WatchValue(ScriptBehaviorPort *port,
                                    asIScriptFunction *callback) const;

    Authoring::Graph *Value() const noexcept {
        return IsValid() ? &*m_Graph->Value : nullptr;
    }

private:
    int InvalidRange(const char *operation) const {
        std::string message = operation;
        message += ": the Node or Port belongs to another Graph snapshot.";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return -1;
    }
    ScriptBehaviorLink *InvalidLink(const char *operation) const {
        InvalidRange(operation);
        return nullptr;
    }
    ScriptBehaviorLink *LinkFrom(Authoring::LinkRange links, int index,
                                 const char *direction) const {
        if (index < 0 || static_cast<std::size_t>(index) >= links.size()) {
            std::string message = "Behavior Graph ";
            message += direction;
            message += " Link index is out of range.";
            ScriptStringInterop::RaiseActiveException(message.c_str());
            return nullptr;
        }
        auto current = links.begin();
        for (int position = 0; position < index; ++position)
            ++current;
        return WrapLink(*current);
    }
    template <class T>
    ScriptBehaviorLink *UniqueLink(T *value, bool incoming) const {
        if (!value || !value->IsValid() || !value->BelongsTo(m_Graph))
            return InvalidLink(incoming ? "Find entering Behavior Link"
                                        : "Find leaving Behavior Link");
        auto found = incoming
            ? m_Graph->Value->Entering(value->Value())
            : m_Graph->Value->Leaving(value->Value());
        if (!Accept(incoming ? "Find entering Behavior Link"
                             : "Find leaving Behavior Link", found))
            return nullptr;
        return WrapLink(found.Take());
    }
    template <class T>
    ScriptBehaviorNode *Adjacent(T *value, bool previous) const {
        if (!value || !value->IsValid() || !value->BelongsTo(m_Graph)) {
            InvalidRange(previous ? "Find previous Behavior Node"
                                  : "Find next Behavior Node");
            return nullptr;
        }
        auto found = previous
            ? m_Graph->Value->Previous(value->Value())
            : m_Graph->Value->Next(value->Value());
        if (!Accept(previous ? "Find previous Behavior Node"
                             : "Find next Behavior Node", found))
            return nullptr;
        return WrapNode(found.Take());
    }
    ScriptBehaviorNode *WrapNode(Authoring::Node node) const {
        if (!IsValid() || !node)
            return nullptr;
        return NewScriptObject<ScriptBehaviorNode>(
            "Behavior Node", m_State, m_Graph, std::move(node));
    }
    ScriptBehaviorLink *WrapLink(Authoring::Link link) const {
        if (!IsValid() || !link)
            return nullptr;
        return NewScriptObject<ScriptBehaviorLink>(
            "Behavior Link", m_State, m_Graph, std::move(link));
    }
    ScriptBehaviorGraph *Keep(Authoring::Result<Authoring::Graph> result,
                              const char *operation) const {
        if (!Accept(operation, result))
            return nullptr;
        auto resource = std::make_shared<GraphResource>(result.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorGraph>(
            "Behavior Graph", m_State, std::move(resource));
    }
    ScriptBehaviorWatch *KeepWatch(
        Authoring::Result<Authoring::Watch> result) const {
        if (!Accept("Watch Behavior Graph", result))
            return nullptr;
        auto resource = std::make_shared<WatchResource>(result.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorWatch>(
            "Behavior Watch", m_State, std::move(resource));
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<GraphResource> m_Graph;
};

ScriptBehaviorGraph *ScriptBehaviorRun::Inspect(bool live) {
    auto result = Visit<Authoring::Graph>([&](const auto &run) {
        return run.Inspect(live ? Authoring::View::Live
                                : Authoring::View::Logical);
    });
    if (!Accept("Inspect Behavior Run", result))
        return nullptr;
    try {
        auto resource = std::make_shared<GraphResource>(result.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorGraph>(
            "Behavior Graph", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory inspecting Behavior Run.");
        return nullptr;
    }
}

bool ScriptBehaviorRun::Bind(ScriptBehaviorSlot *slot,
                             ScriptBehaviorPort *source, int relation) {
    if (!slot || !slot->IsValid() || !source || !source->IsValid()) {
        ScriptStringInterop::RaiseActiveException(
            "Bind requires a valid Layout Slot and Graph Port.");
        return false;
    }
    if (relation < static_cast<int>(Authoring::Relation::Stored) ||
        relation > static_cast<int>(Authoring::Relation::Operation)) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior value relation is invalid.");
        return false;
    }
    auto result = Visit<std::uint64_t>([&](auto &run) {
        return run.Bind(slot->Value(), source->Value(),
                        static_cast<Authoring::Relation>(relation));
    });
    return Accept("Bind Behavior value", result);
}

class ScriptBehaviorEditPort final : public ScriptRef {
public:
    ScriptBehaviorEditPort(std::shared_ptr<ScriptBehaviorState> state,
                           std::shared_ptr<EditResource> edit,
                           Authoring::Edit::Port port)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Port(std::move(port)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    const Authoring::Edit::Port &Value() const noexcept { return m_Port; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Port m_Port;
};

class ScriptBehaviorEditPorts final : public ScriptRef {
public:
    ScriptBehaviorEditPorts(std::shared_ptr<ScriptBehaviorState> state,
                            std::shared_ptr<EditResource> edit,
                            Authoring::Edit::Ports ports)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Ports(std::move(ports)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    const Authoring::Edit::Ports &Value() const noexcept { return m_Ports; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Ports m_Ports;
};

class ScriptBehaviorEditNodes final : public ScriptRef {
public:
    ScriptBehaviorEditNodes(std::shared_ptr<ScriptBehaviorState> state,
                            std::shared_ptr<EditResource> edit,
                            Authoring::Edit::Nodes nodes)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Nodes(std::move(nodes)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
#define BML_SCRIPT_EDIT_PORTS(Name)                                               \
    ScriptBehaviorEditPorts *Name(const ScriptBehaviorSelector &slot) const {     \
        return Wrap(m_Nodes.Name(slot.Value));                                   \
    }
    BML_SCRIPT_EDIT_PORTS(In)
    BML_SCRIPT_EDIT_PORTS(Out)
    BML_SCRIPT_EDIT_PORTS(Pin)
    BML_SCRIPT_EDIT_PORTS(Pout)
    BML_SCRIPT_EDIT_PORTS(Local)
#undef BML_SCRIPT_EDIT_PORTS
    ScriptBehaviorEditPorts *Target() const { return Wrap(m_Nodes.Target()); }
    const Authoring::Edit::Nodes &Value() const noexcept { return m_Nodes; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    ScriptBehaviorEditPorts *Wrap(Authoring::Edit::Ports ports) const {
        if (!IsValid())
            return nullptr;
        return NewScriptObject<ScriptBehaviorEditPorts>(
            "Behavior Edit Ports", m_State, m_Edit, std::move(ports));
    }
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Nodes m_Nodes;
};

class ScriptBehaviorEditNode final : public ScriptRef {
public:
    ScriptBehaviorEditNode(std::shared_ptr<ScriptBehaviorState> state,
                           std::shared_ptr<EditResource> edit,
                           Authoring::Edit::Node node)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Node(std::move(node)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
#define BML_SCRIPT_EDIT_PORT(Name)                                                \
    ScriptBehaviorEditPort *Name(const ScriptBehaviorSelector &slot) const {      \
        return NewScriptObject<ScriptBehaviorEditPort>(                          \
            "Behavior Edit Port", m_State, m_Edit, m_Node.Name(slot.Value));    \
    }
    BML_SCRIPT_EDIT_PORT(In)
    BML_SCRIPT_EDIT_PORT(Out)
    BML_SCRIPT_EDIT_PORT(Pin)
    BML_SCRIPT_EDIT_PORT(Pout)
    BML_SCRIPT_EDIT_PORT(Local)
#undef BML_SCRIPT_EDIT_PORT
    ScriptBehaviorEditPort *Target() const {
        return NewScriptObject<ScriptBehaviorEditPort>(
            "Behavior Edit Target", m_State, m_Edit, m_Node.Target());
    }
    ScriptBehaviorEditGraph *Graph() const;
    const Authoring::Edit::Node &Value() const noexcept { return m_Node; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Node m_Node;
};

class ScriptBehaviorEditLink final : public ScriptRef {
public:
    ScriptBehaviorEditLink(std::shared_ptr<ScriptBehaviorState> state,
                           std::shared_ptr<EditResource> edit,
                           Authoring::Edit::Link link)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Link(std::move(link)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    const Authoring::Edit::Link &Value() const noexcept { return m_Link; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Link m_Link;
};

class ScriptBehaviorEditPath final : public ScriptRef {
public:
    ScriptBehaviorEditPath(std::shared_ptr<ScriptBehaviorState> state,
                           std::shared_ptr<EditResource> edit,
                           Authoring::Edit::Path path)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Path(std::move(path)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    const Authoring::Edit::Path &Value() const noexcept { return m_Path; }
    bool BelongsTo(const std::shared_ptr<EditResource> &edit) const noexcept {
        return m_Edit == edit;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Path m_Path;
};

class ScriptBehaviorEditOperation final : public ScriptRef {
public:
    ScriptBehaviorEditOperation(std::shared_ptr<ScriptBehaviorState> state,
                                std::shared_ptr<EditResource> edit,
                                Authoring::Edit::Operation operation)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Operation(std::move(operation)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    ScriptBehaviorEditPort *Input(int index) const {
        if (!IsValid() || index < 0 || index > 1) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Operation input index is out of range.");
            return nullptr;
        }
        return NewScriptObject<ScriptBehaviorEditPort>(
            "Behavior Operation input", m_State, m_Edit,
            m_Operation.Input(index));
    }
    ScriptBehaviorEditPort *Result() const {
        if (!IsValid())
            return nullptr;
        return NewScriptObject<ScriptBehaviorEditPort>(
            "Behavior Operation result", m_State, m_Edit,
            m_Operation.Result());
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Operation m_Operation;
};

class ScriptBehaviorEditGraph final : public ScriptRef {
public:
    ScriptBehaviorEditGraph(std::shared_ptr<ScriptBehaviorState> state,
                            std::shared_ptr<EditResource> edit,
                            Authoring::Edit::Graph graph)
        : m_State(std::move(state)), m_Edit(std::move(edit)),
          m_Graph(std::move(graph)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    ScriptBehaviorEditNode *Root() const { return Wrap(m_Graph.Root()); }
    ScriptBehaviorEditNode *Require(const ScriptBehaviorSelector &selector,
                                    const CKGUID &prototype) const {
        return Wrap(m_Graph.Require(selector.Value, prototype));
    }
    ScriptBehaviorEditNode *RequirePattern(
        const ScriptBehaviorNodePattern &pattern) const {
        return Wrap(m_Graph.Require(pattern.Value));
    }
    ScriptBehaviorEditNodes *Each(
        const ScriptBehaviorNodePattern &pattern) const {
        return Wrap(m_Graph.Each(pattern.Value));
    }
    ScriptBehaviorEditNode *RequireNode(ScriptBehaviorNode *node) const {
        if (!IsValid() || !node || !node->IsValid())
            return InvalidNode("Require Behavior snapshot Node");
        return Wrap(m_Graph.Require(node->Value()));
    }
    ScriptBehaviorEditLink *RequireLink(ScriptBehaviorLink *link) const {
        if (!IsValid() || !link || !link->IsValid())
            return InvalidLink("Require Behavior snapshot Link");
        return Wrap(m_Graph.Require(link->Value()));
    }
    ScriptBehaviorEditNode *Use(ScriptBehaviorNode *node) const {
        if (!IsValid() || !node || !node->IsValid())
            return InvalidNode("Use Behavior snapshot Node");
        return Wrap(m_Graph.Use(node->Value()));
    }
    ScriptBehaviorEditLink *UseLink(ScriptBehaviorLink *link) const {
        if (!IsValid() || !link || !link->IsValid())
            return InvalidLink("Use Behavior snapshot Link");
        return Wrap(m_Graph.Use(link->Value()));
    }
    ScriptBehaviorEditLink *Between(ScriptBehaviorEditPort *source,
                                    ScriptBehaviorEditPort *sink,
                                    int delay) const {
        if (!Ports(source, sink, "Find Behavior Link"))
            return nullptr;
        return Wrap(m_Graph.Between(source->Value(), sink->Value(), delay));
    }
    ScriptBehaviorEditNode *NextPort(ScriptBehaviorEditPort *source) const {
        if (!Port(source, "Find next Behavior Node"))
            return nullptr;
        return Wrap(m_Graph.Next(source->Value()));
    }
    ScriptBehaviorEditNode *NextNode(ScriptBehaviorEditNode *source) const {
        if (!Node(source, "Find next Behavior Node"))
            return nullptr;
        return Wrap(m_Graph.Next(source->Value()));
    }
    ScriptBehaviorEditNode *PreviousPort(ScriptBehaviorEditPort *sink) const {
        if (!Port(sink, "Find previous Behavior Node"))
            return nullptr;
        return Wrap(m_Graph.Previous(sink->Value()));
    }
    ScriptBehaviorEditNode *PreviousNode(ScriptBehaviorEditNode *sink) const {
        if (!Node(sink, "Find previous Behavior Node"))
            return nullptr;
        return Wrap(m_Graph.Previous(sink->Value()));
    }
    ScriptBehaviorEditLink *LeavingPort(ScriptBehaviorEditPort *source) const {
        if (!Port(source, "Find leaving Behavior Link"))
            return nullptr;
        return Wrap(m_Graph.Leaving(source->Value()));
    }
    ScriptBehaviorEditLink *LeavingNode(ScriptBehaviorEditNode *source) const {
        if (!Node(source, "Find leaving Behavior Link"))
            return nullptr;
        return Wrap(m_Graph.Leaving(source->Value()));
    }
    ScriptBehaviorEditLink *EnteringPort(ScriptBehaviorEditPort *sink) const {
        if (!Port(sink, "Find entering Behavior Link"))
            return nullptr;
        return Wrap(m_Graph.Entering(sink->Value()));
    }
    ScriptBehaviorEditLink *EnteringNode(ScriptBehaviorEditNode *sink) const {
        if (!Node(sink, "Find entering Behavior Link"))
            return nullptr;
        return Wrap(m_Graph.Entering(sink->Value()));
    }
    ScriptBehaviorEditLink *To(ScriptBehaviorEditPort *source,
                               ScriptBehaviorEditNode *target) const {
        if (!Port(source, "Find Behavior Link to Node") ||
            !Node(target, "Find Behavior Link to Node"))
            return nullptr;
        return Wrap(m_Graph.To(source->Value(), target->Value()));
    }
    ScriptBehaviorEditPath *Follow(ScriptBehaviorEditPort *source) const {
        if (!Port(source, "Follow Behavior path"))
            return nullptr;
        return Wrap(m_Graph.Follow(source->Value()));
    }
    ScriptBehaviorEditNode *Add(ScriptBehaviorBlock *block) const {
        Authoring::Block *value = block ? block->ForEdit() : nullptr;
        if (!IsValid() || !value)
            return InvalidNode("Add Behavior Block");
        return Wrap(m_Graph.Add(*value));
    }
    ScriptBehaviorEditNode *AddGraph(const std::string &name,
                                     int priority) const {
        return Wrap(m_Graph.AddGraph(name, priority));
    }
    ScriptBehaviorEditNode *Replace(ScriptBehaviorEditNode *target,
                                    ScriptBehaviorBlock *block) const {
        Authoring::Block *value = block ? block->ForEdit() : nullptr;
        if (!IsValid() || !target || !target->IsValid() || !value)
            return InvalidNode("Replace Behavior Node");
        return Wrap(m_Graph.Replace(target->Value(), *value));
    }
    bool Remove(ScriptBehaviorEditNode *node) const {
        if (!IsValid() || !node || !node->IsValid())
            return Invalid("Remove Behavior Node");
        m_Graph.Remove(node->Value());
        return true;
    }
    bool Flow(ScriptBehaviorEditPort *source, ScriptBehaviorEditPort *sink,
              int delay) const {
        if (!Ports(source, sink, "Connect Behavior flow"))
            return false;
        m_Graph.Flow(source->Value(), sink->Value(), delay);
        return true;
    }
    bool FlowCycle(ScriptBehaviorEditPort *source,
                   ScriptBehaviorEditPort *sink, int delay) const {
        if (!Ports(source, sink, "Connect Behavior cycle"))
            return false;
        m_Graph.FlowCycle(source->Value(), sink->Value(), delay);
        return true;
    }
    bool FlowFrom(ScriptBehaviorEditPorts *sources,
                  ScriptBehaviorEditPort *sink, int delay) const {
        if (!PortSetAndPort(sources, sink, "Connect Behavior flow"))
            return false;
        m_Graph.Flow(sources->Value(), sink->Value(), delay);
        return true;
    }
    bool FlowTo(ScriptBehaviorEditPort *source,
                ScriptBehaviorEditPorts *sinks, int delay) const {
        if (!PortAndPortSet(source, sinks, "Connect Behavior flow"))
            return false;
        m_Graph.Flow(source->Value(), sinks->Value(), delay);
        return true;
    }
    bool FlowCycleFrom(ScriptBehaviorEditPorts *sources,
                       ScriptBehaviorEditPort *sink, int delay) const {
        if (!PortSetAndPort(sources, sink, "Connect Behavior cycle"))
            return false;
        m_Graph.FlowCycle(sources->Value(), sink->Value(), delay);
        return true;
    }
    bool FlowCycleTo(ScriptBehaviorEditPort *source,
                     ScriptBehaviorEditPorts *sinks, int delay) const {
        if (!PortAndPortSet(source, sinks, "Connect Behavior cycle"))
            return false;
        m_Graph.FlowCycle(source->Value(), sinks->Value(), delay);
        return true;
    }
    bool BindPort(ScriptBehaviorEditPort *sink,
                  ScriptBehaviorEditPort *source) const {
        if (!Ports(sink, source, "Bind Behavior ports"))
            return false;
        m_Graph.Bind(sink->Value(), source->Value());
        return true;
    }
    bool Share(ScriptBehaviorEditPort *sink,
               ScriptBehaviorEditPort *source) const {
        if (!Ports(sink, source, "Share Behavior ports"))
            return false;
        m_Graph.Share(sink->Value(), source->Value());
        return true;
    }
    bool BindPorts(ScriptBehaviorEditPorts *sinks,
                   ScriptBehaviorEditPort *source) const {
        if (!PortSetAndPort(sinks, source, "Bind Behavior ports"))
            return false;
        m_Graph.Bind(sinks->Value(), source->Value());
        return true;
    }
    bool SharePorts(ScriptBehaviorEditPorts *sinks,
                    ScriptBehaviorEditPort *source) const {
        if (!PortSetAndPort(sinks, source, "Share Behavior ports"))
            return false;
        m_Graph.Share(sinks->Value(), source->Value());
        return true;
    }
    bool Push(ScriptBehaviorEditPort *source,
              ScriptBehaviorEditPort *sink) const {
        if (!Ports(source, sink, "Push Behavior value"))
            return false;
        m_Graph.Push(source->Value(), sink->Value());
        return true;
    }
    bool PushFrom(ScriptBehaviorEditPorts *sources,
                  ScriptBehaviorEditPort *sink) const {
        if (!PortSetAndPort(sources, sink, "Push Behavior values"))
            return false;
        m_Graph.Push(sources->Value(), sink->Value());
        return true;
    }
    bool PushTo(ScriptBehaviorEditPort *source,
                ScriptBehaviorEditPorts *sinks) const {
        if (!PortAndPortSet(source, sinks, "Push Behavior value"))
            return false;
        m_Graph.Push(source->Value(), sinks->Value());
        return true;
    }
    ScriptBehaviorEditOperation *AddOperation(
        const CKGUID &operation, const CKGUID &result,
        const CKGUID &input1, const CKGUID &input2) const {
        if (!IsValid())
            return nullptr;
        return Wrap(m_Graph.AddOperation(operation, result, input1, input2));
    }
#define BML_SCRIPT_EDIT_BIND(Name, Type)                                          \
    bool Name(ScriptBehaviorEditPort *sink, Type value) const {                   \
        return BindValue(sink, Authoring::Value(value));                          \
    }
    BML_SCRIPT_EDIT_BIND(BindBool, bool)
    BML_SCRIPT_EDIT_BIND(BindInt, int)
    BML_SCRIPT_EDIT_BIND(BindFloat, float)
#undef BML_SCRIPT_EDIT_BIND
    bool BindString(ScriptBehaviorEditPort *sink,
                    const std::string &value) const {
        return BindValue(sink, Authoring::Value(value));
    }
    bool BindVec2(ScriptBehaviorEditPort *sink,
                  const Vx2DVector &value) const {
        return BindValue(sink, Authoring::Value(value));
    }
    bool BindVec3(ScriptBehaviorEditPort *sink,
                  const VxVector &value) const {
        return BindValue(sink, Authoring::Value(value));
    }
    bool BindAny(ScriptBehaviorEditPort *sink,
                 const ScriptBehaviorValue &value) const {
        return BindValue(sink, value.Value);
    }
    bool BindMany(ScriptBehaviorEditPorts *sinks,
                  const ScriptBehaviorValue &value) const {
        if (!PortSet(sinks, "Bind Behavior values"))
            return false;
        m_Graph.Bind(sinks->Value(), value.Value);
        return true;
    }
    ScriptBehaviorEditPort *AppendIn(const std::string &name) const {
        return Wrap(m_Graph.AppendIn(name));
    }
    ScriptBehaviorEditPort *AppendOut(const std::string &name) const {
        return Wrap(m_Graph.AppendOut(name));
    }
    ScriptBehaviorEditPort *AppendPin(const std::string &name,
                                      const CKGUID &type) const {
        return Wrap(m_Graph.AppendPin(name, type));
    }
    ScriptBehaviorEditPort *AppendPout(const std::string &name,
                                       const CKGUID &type) const {
        return Wrap(m_Graph.AppendPout(name, type));
    }
    ScriptBehaviorEditPort *AppendLocal(const std::string &name,
                                        const CKGUID &type) const {
        return Wrap(m_Graph.AppendLocal(name, type));
    }
    ScriptBehaviorEditPort *AppendInOn(ScriptBehaviorEditNode *owner,
                                       const std::string &name) const {
        return Node(owner, "Append Behavior In")
            ? Wrap(m_Graph.AppendIn(owner->Value(), name)) : nullptr;
    }
    ScriptBehaviorEditPort *AppendOutOn(ScriptBehaviorEditNode *owner,
                                        const std::string &name) const {
        return Node(owner, "Append Behavior Out")
            ? Wrap(m_Graph.AppendOut(owner->Value(), name)) : nullptr;
    }
    ScriptBehaviorEditPort *AppendPinOn(ScriptBehaviorEditNode *owner,
                                        const std::string &name,
                                        const CKGUID &type) const {
        return Node(owner, "Append Behavior Pin")
            ? Wrap(m_Graph.AppendPin(owner->Value(), name, type)) : nullptr;
    }
    ScriptBehaviorEditPort *AppendPoutOn(ScriptBehaviorEditNode *owner,
                                         const std::string &name,
                                         const CKGUID &type) const {
        return Node(owner, "Append Behavior Pout")
            ? Wrap(m_Graph.AppendPout(owner->Value(), name, type)) : nullptr;
    }
    ScriptBehaviorEditPort *AppendLocalOn(ScriptBehaviorEditNode *owner,
                                          const std::string &name,
                                          const CKGUID &type) const {
        return Node(owner, "Append Behavior Local")
            ? Wrap(m_Graph.AppendLocal(owner->Value(), name, type)) : nullptr;
    }
    bool SpliceNode(ScriptBehaviorEditLink *link,
                    ScriptBehaviorEditNode *through) const {
        if (!Link(link, "Splice Behavior Link") ||
            !Node(through, "Splice Behavior Link"))
            return false;
        m_Graph.Splice(link->Value(), through->Value());
        return true;
    }
    bool SplicePorts(ScriptBehaviorEditLink *link,
                     ScriptBehaviorEditPort *sink,
                     ScriptBehaviorEditPort *source) const {
        if (!Link(link, "Splice Behavior Link") ||
            !Ports(sink, source, "Splice Behavior Link"))
            return false;
        m_Graph.Splice(link->Value(), sink->Value(), source->Value());
        return true;
    }
    bool RedirectPort(ScriptBehaviorEditLink *link,
                      ScriptBehaviorEditPort *sink) const {
        if (!Link(link, "Redirect Behavior Link") ||
            !Port(sink, "Redirect Behavior Link"))
            return false;
        m_Graph.Redirect(link->Value(), sink->Value());
        return true;
    }
    bool RedirectLink(ScriptBehaviorEditLink *link,
                      ScriptBehaviorEditLink *destination) const {
        if (!Link(link, "Redirect Behavior Link") ||
            !Link(destination, "Redirect Behavior Link"))
            return false;
        m_Graph.Redirect(link->Value(), destination->Value());
        return true;
    }
    bool Reconnect(ScriptBehaviorEditLink *link,
                   ScriptBehaviorEditPort *source,
                   ScriptBehaviorEditPort *sink) const {
        if (!Link(link, "Reconnect Behavior Link") ||
            !Ports(source, sink, "Reconnect Behavior Link"))
            return false;
        m_Graph.Reconnect(link->Value(), source->Value(), sink->Value());
        return true;
    }
    bool ReconnectCycle(ScriptBehaviorEditLink *link,
                        ScriptBehaviorEditPort *source,
                        ScriptBehaviorEditPort *sink) const {
        if (!Link(link, "Reconnect Behavior cycle") ||
            !Ports(source, sink, "Reconnect Behavior cycle"))
            return false;
        m_Graph.ReconnectCycle(link->Value(), source->Value(), sink->Value());
        return true;
    }
    bool Tap(ScriptBehaviorEditPort *source,
             asIScriptFunction *callback) const {
        if (!Port(source, "Tap Behavior flow"))
            return false;
        auto hook = MakeHook(callback);
        if (!hook)
            return false;
        m_Graph.Tap(source->Value(), std::move(*hook));
        return true;
    }
    bool TapMany(ScriptBehaviorEditPorts *sources,
                 asIScriptFunction *callback) const {
        if (!PortSet(sources, "Tap Behavior flows"))
            return false;
        auto hook = MakeHook(callback);
        if (!hook)
            return false;
        m_Graph.Tap(sources->Value(), std::move(*hook));
        return true;
    }
    bool Before(ScriptBehaviorEditLink *link,
                asIScriptFunction *callback) const {
        if (!Link(link, "Insert Behavior Hook before Link"))
            return false;
        auto hook = MakeHook(callback);
        if (!hook)
            return false;
        m_Graph.Before(link->Value(), std::move(*hook));
        return true;
    }
    bool AfterPath(ScriptBehaviorEditPath *path,
                   asIScriptFunction *callback) const {
        if (!Path(path, "Insert Behavior Hook after path"))
            return false;
        auto hook = MakeHook(callback);
        if (!hook)
            return false;
        m_Graph.After(path->Value(), std::move(*hook));
        return true;
    }
    bool AfterPort(ScriptBehaviorEditPort *source,
                   asIScriptFunction *callback) const {
        if (!Port(source, "Insert Behavior Hook after flow"))
            return false;
        auto hook = MakeHook(callback);
        if (!hook)
            return false;
        m_Graph.After(source->Value(), std::move(*hook));
        return true;
    }

    const Authoring::Edit::Graph &Value() const noexcept { return m_Graph; }

private:
    bool Invalid(const char *operation) const {
        std::string message = operation;
        message += ": an Edit value is stale or belongs to another Edit.";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return false;
    }
    ScriptBehaviorEditNode *InvalidNode(const char *operation) const {
        Invalid(operation);
        return nullptr;
    }
    ScriptBehaviorEditLink *InvalidLink(const char *operation) const {
        Invalid(operation);
        return nullptr;
    }
    bool Node(ScriptBehaviorEditNode *node, const char *operation) const {
        return IsValid() && node && node->IsValid() &&
            node->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool Port(ScriptBehaviorEditPort *port, const char *operation) const {
        return IsValid() && port && port->IsValid() &&
            port->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool Link(ScriptBehaviorEditLink *link, const char *operation) const {
        return IsValid() && link && link->IsValid() &&
            link->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool Path(ScriptBehaviorEditPath *path, const char *operation) const {
        return IsValid() && path && path->IsValid() &&
            path->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool Ports(ScriptBehaviorEditPort *first,
               ScriptBehaviorEditPort *second,
               const char *operation) const {
        return IsValid() && first && second && first->IsValid() &&
            second->IsValid() && first->BelongsTo(m_Edit) &&
            second->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool PortSet(ScriptBehaviorEditPorts *ports,
                 const char *operation) const {
        return IsValid() && ports && ports->IsValid() &&
            ports->BelongsTo(m_Edit) ? true : Invalid(operation);
    }
    bool PortSetAndPort(ScriptBehaviorEditPorts *ports,
                        ScriptBehaviorEditPort *port,
                        const char *operation) const {
        return PortSet(ports, operation) && Port(port, operation);
    }
    bool PortAndPortSet(ScriptBehaviorEditPort *port,
                        ScriptBehaviorEditPorts *ports,
                        const char *operation) const {
        return Port(port, operation) && PortSet(ports, operation);
    }
    bool BindValue(ScriptBehaviorEditPort *sink,
                   Authoring::Value value) const {
        if (!IsValid() || !sink || !sink->IsValid() ||
            !sink->BelongsTo(m_Edit))
            return Invalid("Bind Behavior value");
        m_Graph.Bind(sink->Value(), std::move(value));
        return true;
    }
    std::optional<Authoring::Hook> MakeHook(
        asIScriptFunction *callback) const {
        if (!IsValid() || !ScriptBehaviorHookFunction::HasSignature(callback)) {
            ScriptStringInterop::RaiseActiveException(
                "Behavior Hook requires a HookCallback function.");
            return std::nullopt;
        }
        try {
            auto function = std::make_shared<ScriptBehaviorHookFunction>(
                m_State, callback);
            return Authoring::Hook(
                [function](const Authoring::HookEvent &event) {
                    return function->Invoke(event);
                });
        } catch (const std::bad_alloc &) {
            ScriptStringInterop::RaiseActiveException(
                "Out of memory retaining Behavior Hook callback.");
            return std::nullopt;
        }
    }
    ScriptBehaviorEditNode *Wrap(Authoring::Edit::Node node) const {
        return NewScriptObject<ScriptBehaviorEditNode>(
            "Behavior Edit Node", m_State, m_Edit, std::move(node));
    }
    ScriptBehaviorEditPort *Wrap(Authoring::Edit::Port port) const {
        return NewScriptObject<ScriptBehaviorEditPort>(
            "Behavior Edit Port", m_State, m_Edit, std::move(port));
    }
    ScriptBehaviorEditNodes *Wrap(Authoring::Edit::Nodes nodes) const {
        return NewScriptObject<ScriptBehaviorEditNodes>(
            "Behavior Edit Nodes", m_State, m_Edit, std::move(nodes));
    }
    ScriptBehaviorEditLink *Wrap(Authoring::Edit::Link link) const {
        return NewScriptObject<ScriptBehaviorEditLink>(
            "Behavior Edit Link", m_State, m_Edit, std::move(link));
    }
    ScriptBehaviorEditPath *Wrap(Authoring::Edit::Path path) const {
        return NewScriptObject<ScriptBehaviorEditPath>(
            "Behavior Edit Path", m_State, m_Edit, std::move(path));
    }
    ScriptBehaviorEditOperation *Wrap(
        Authoring::Edit::Operation operation) const {
        return NewScriptObject<ScriptBehaviorEditOperation>(
            "Behavior Operation", m_State, m_Edit, std::move(operation));
    }

    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
    Authoring::Edit::Graph m_Graph;
};

ScriptBehaviorEditGraph *ScriptBehaviorEditNode::Graph() const {
    if (!IsValid())
        return nullptr;
    return NewScriptObject<ScriptBehaviorEditGraph>(
        "nested Behavior Edit Graph", m_State, m_Edit, m_Node.Graph());
}

class ScriptBehaviorEdit final : public ScriptRef {
public:
    ScriptBehaviorEdit(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<EditResource> edit)
        : m_State(std::move(state)), m_Edit(std::move(edit)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Edit && m_Edit->IsOpen();
    }
    ScriptBehaviorEditGraph *Root() const {
        if (!IsValid())
            return nullptr;
        return NewScriptObject<ScriptBehaviorEditGraph>(
            "Behavior Edit Graph", m_State, m_Edit, m_Edit->Value->Root());
    }
    ScriptBehaviorPlan *Plan(const std::string &name,
                             const std::string &script, bool each);
    ScriptBehaviorScript *CreateScript(CKBeObject *owner,
                                       const std::string &name,
                                       int priority);
    Authoring::Edit *Value() const noexcept {
        return IsValid() ? &*m_Edit->Value : nullptr;
    }

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<EditResource> m_Edit;
};

class ScriptBehaviorPatch final : public ScriptRef {
public:
    ScriptBehaviorPatch(std::shared_ptr<ScriptBehaviorState> state,
                        std::shared_ptr<PatchResource> patch)
        : m_State(std::move(state)), m_Patch(std::move(patch)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Patch && m_Patch->IsOpen();
    }
    int State() const { return Read().first; }
    std::string Error() const { return Read().second; }
    bool Enable() { return Set(true); }
    bool Disable() { return Set(false); }
    bool Replace(ScriptBehaviorGraph *graph, ScriptBehaviorEdit *edit) {
        Authoring::Graph *target = graph ? graph->Value() : nullptr;
        Authoring::Edit *body = edit ? edit->Value() : nullptr;
        if (!IsValid() || !target || !body) {
            ScriptStringInterop::RaiseActiveException(
                "Replace Behavior Patch requires a live Graph and Edit.");
            return false;
        }
        auto replaced = m_Patch->Value->Replace(Authoring::On(*target, *body));
        return Accept("Replace Behavior Patch", replaced);
    }
    ScriptBehaviorObjectRef *Resolve(ScriptBehaviorEditNode *node) const {
        if (!IsValid() || !node || !node->IsValid()) {
            ScriptStringInterop::RaiseActiveException(
                "Resolve Behavior Node requires a live Patch symbol.");
            return nullptr;
        }
        auto resolved = m_Patch->Value->Resolve(node->Value());
        if (!Accept("Resolve patched Behavior Node", resolved))
            return nullptr;
        return NewScriptObject<ScriptBehaviorObjectRef>(
            "resolved Behavior ObjectRef", m_State, resolved.Value());
    }
    int Close() noexcept {
        if (!m_Patch || !m_Patch->Value)
            return static_cast<int>(Authoring::CloseState::Closed);
        auto result = m_Patch->Value->Close();
        if (!result)
            return -1;
        if (result.Value() == Authoring::CloseState::Closed)
            m_Patch->Value.reset();
        return static_cast<int>(result.Value());
    }

private:
    std::pair<int, std::string> Read() const {
        if (!IsValid())
            return {-1, "Behavior Patch is stale."};
        auto info = m_Patch->Value->Info();
        return info ? std::pair<int, std::string>{
            static_cast<int>(info->State), info->LastStatus.Message}
            : std::pair<int, std::string>{-1, info.GetStatus().Message};
    }
    bool Set(bool enabled) {
        if (!IsValid())
            return false;
        auto result = enabled ? m_Patch->Value->Enable()
                              : m_Patch->Value->Disable();
        return Accept(enabled ? "Enable Behavior Patch"
                              : "Disable Behavior Patch", result);
    }
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<PatchResource> m_Patch;
};

class ScriptBehaviorPlan final : public ScriptRef {
public:
    ScriptBehaviorPlan(std::shared_ptr<ScriptBehaviorState> state,
                       std::shared_ptr<PlanResource> plan)
        : m_State(std::move(state)), m_Plan(std::move(plan)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Plan && m_Plan->IsOpen();
    }
    int State() const { return Read().first; }
    std::string Error() const { return Read().second; }
    bool Enable() { return Set(true); }
    bool Disable() { return Set(false); }
    bool Replace(const std::string &script, ScriptBehaviorEdit *edit,
                 bool each) {
        Authoring::Edit *body = edit ? edit->Value() : nullptr;
        if (!IsValid() || !body) {
            ScriptStringInterop::RaiseActiveException(
                "Replace Behavior Plan requires a live Edit.");
            return false;
        }
        const Authoring::Scripts target = each
            ? Authoring::Scripts::Each(script) : Authoring::Scripts::One(script);
        auto replaced = m_Plan->Value->Replace(Authoring::On(target, *body));
        return Accept("Replace Behavior Plan", replaced);
    }
    int Close() noexcept {
        if (!m_Plan || !m_Plan->Value)
            return static_cast<int>(Authoring::CloseState::Closed);
        auto result = m_Plan->Value->Close();
        if (!result)
            return -1;
        if (result.Value() == Authoring::CloseState::Closed)
            m_Plan->Value.reset();
        return static_cast<int>(result.Value());
    }

private:
    std::pair<int, std::string> Read() const {
        if (!IsValid())
            return {-1, "Behavior Plan is stale."};
        auto info = m_Plan->Value->Info();
        return info ? std::pair<int, std::string>{
            static_cast<int>(info->State), info->LastStatus.Message}
            : std::pair<int, std::string>{-1, info.GetStatus().Message};
    }
    bool Set(bool enabled) {
        if (!IsValid())
            return false;
        auto result = enabled ? m_Plan->Value->Enable()
                              : m_Plan->Value->Disable();
        return Accept(enabled ? "Enable Behavior Plan"
                              : "Disable Behavior Plan", result);
    }
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<PlanResource> m_Plan;
};

class ScriptBehaviorScript final : public ScriptRef {
public:
    ScriptBehaviorScript(std::shared_ptr<ScriptBehaviorState> state,
                         std::shared_ptr<ScriptResource> script)
        : m_State(std::move(state)), m_Script(std::move(script)) {}
    bool IsValid() const noexcept {
        return m_State && m_State->Active && m_Script && m_Script->IsOpen();
    }
    int State() const { return Read().first; }
    std::string Error() const { return Read().second; }
    bool Activate() { return Set(0); }
    bool Restart() { return Set(1); }
    bool Deactivate() { return Set(2); }
    ScriptBehaviorGraph *Inspect(bool live) const {
        if (!IsValid())
            return nullptr;
        auto graph = m_Script->Value->Inspect(
            live ? Authoring::View::Live : Authoring::View::Logical);
        if (!Accept("Inspect authored Behavior Script", graph))
            return nullptr;
        auto resource = std::make_shared<GraphResource>(graph.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorGraph>(
            "authored Behavior Graph", m_State, std::move(resource));
    }
    ScriptBehaviorPatch *Apply(const std::string &name,
                               ScriptBehaviorEdit *edit) const {
        Authoring::Edit *body = edit ? edit->Value() : nullptr;
        if (!IsValid() || !body) {
            ScriptStringInterop::RaiseActiveException(
                "Apply Behavior Script edit requires a live Edit.");
            return nullptr;
        }
        auto applied = m_Script->Value->Apply(name, *body);
        if (!Accept("Apply authored Behavior Script edit", applied))
            return nullptr;
        auto resource = std::make_shared<PatchResource>(applied.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorPatch>(
            "Behavior Patch", m_State, std::move(resource));
    }
    int Close() noexcept {
        if (!m_Script || !m_Script->Value)
            return static_cast<int>(Authoring::CloseState::Closed);
        auto result = m_Script->Value->Close();
        if (!result)
            return -1;
        if (result.Value() == Authoring::CloseState::Closed)
            m_Script->Value.reset();
        return static_cast<int>(result.Value());
    }

private:
    std::pair<int, std::string> Read() const {
        if (!IsValid())
            return {-1, "Behavior Script is stale."};
        auto info = m_Script->Value->Info();
        return info ? std::pair<int, std::string>{
            static_cast<int>(info->State), info->LastStatus.Message}
            : std::pair<int, std::string>{-1, info.GetStatus().Message};
    }
    bool Set(int action) {
        if (!IsValid())
            return false;
        auto result = action == 0 ? m_Script->Value->Activate()
            : action == 1 ? m_Script->Value->Restart()
                          : m_Script->Value->Deactivate();
        return Accept("Set authored Behavior Script activity", result);
    }
    std::shared_ptr<ScriptBehaviorState> m_State;
    std::shared_ptr<ScriptResource> m_Script;
};

ScriptBehaviorGraph *ScriptBehaviorGraph::Inspect(
    ScriptBehaviorNode *node) const {
    if (!IsValid() || !node || !node->IsValid())
        return nullptr;
    return Keep(m_Graph->Value->Inspect(node->Value()),
                "Inspect nested Behavior Graph");
}
ScriptBehaviorGraph *ScriptBehaviorGraph::Logical() const {
    return IsValid() ? Keep(m_Graph->Value->Logical(),
                            "Read logical Behavior Graph") : nullptr;
}
ScriptBehaviorGraph *ScriptBehaviorGraph::Live() const {
    return IsValid() ? Keep(m_Graph->Value->Live(),
                            "Read live Behavior Graph") : nullptr;
}
ScriptBehaviorPatch *ScriptBehaviorGraph::Apply(
    const std::string &name, ScriptBehaviorEdit *edit) const {
    Authoring::Edit *body = edit ? edit->Value() : nullptr;
    if (!IsValid() || !body)
        return nullptr;
    auto applied = m_Graph->Value->Apply(name, *body);
    if (!Accept("Apply Behavior Edit", applied))
        return nullptr;
    auto resource = std::make_shared<PatchResource>(applied.Take());
    m_State->Keep(resource);
    return NewScriptObject<ScriptBehaviorPatch>(
        "Behavior Patch", m_State, std::move(resource));
}

ScriptBehaviorWatch *ScriptBehaviorGraph::WatchGraph(
    asIScriptFunction *callback) const {
    if (!IsValid() || !ScriptBehaviorWatchFunction::HasSignature(callback)) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior Graph Watch requires a WatchCallback function.");
        return nullptr;
    }
    try {
        auto function = std::make_shared<ScriptBehaviorWatchFunction>(
            m_State, callback);
        return KeepWatch(m_Graph->Value->Watch(
            Authoring::GraphChanged{},
            [function](const Authoring::Change &change) {
                function->Invoke(change);
            }));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory retaining Behavior Watch callback.");
        return nullptr;
    }
}

ScriptBehaviorWatch *ScriptBehaviorGraph::WatchLayout(
    ScriptBehaviorNode *node, asIScriptFunction *callback) const {
    if (!IsValid() || !node || !node->IsValid() ||
        !node->BelongsTo(m_Graph)) {
        InvalidRange("Watch Behavior layout");
        return nullptr;
    }
    if (!ScriptBehaviorWatchFunction::HasSignature(callback)) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior layout Watch requires a WatchCallback function.");
        return nullptr;
    }
    try {
        auto function = std::make_shared<ScriptBehaviorWatchFunction>(
            m_State, callback);
        return KeepWatch(m_Graph->Value->Watch(
            Authoring::LayoutChanged(node->Value()),
            [function](const Authoring::Change &change) {
                function->Invoke(change);
            }));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory retaining Behavior Watch callback.");
        return nullptr;
    }
}

ScriptBehaviorWatch *ScriptBehaviorGraph::WatchValue(
    ScriptBehaviorPort *port, asIScriptFunction *callback) const {
    if (!IsValid() || !port || !port->IsValid() ||
        !port->BelongsTo(m_Graph)) {
        InvalidRange("Watch Behavior value");
        return nullptr;
    }
    if (!ScriptBehaviorWatchFunction::HasSignature(callback)) {
        ScriptStringInterop::RaiseActiveException(
            "Behavior value Watch requires a WatchCallback function.");
        return nullptr;
    }
    try {
        auto function = std::make_shared<ScriptBehaviorWatchFunction>(
            m_State, callback);
        return KeepWatch(m_Graph->Value->Watch(
            Authoring::Sampled(port->Value()),
            [function](const Authoring::Change &change) {
                function->Invoke(change);
            }));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory retaining Behavior Watch callback.");
        return nullptr;
    }
}

ScriptBehaviorBlock *CurrentUse(const CKGUID &prototype) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Use must be called by an active Script Mod.");
        return nullptr;
    }
    return mod->UseBehavior(prototype);
}

ScriptBehaviorBlock *CurrentFind(const std::string &name,
                                 const std::string &category,
                                 const std::string &provider) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Find must be called by an active Script Mod.");
        return nullptr;
    }
    return mod->FindBehavior(name, category, provider);
}

ScriptBehaviorLayout *CurrentDescription(CKGUID prototype) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Describe must be called by an active Script Mod.");
        return nullptr;
    }
    return mod->DescribeBehavior(prototype);
}

ScriptBehaviorEdit *CurrentEdit() {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Edit must be created by an active Script Mod.");
        return nullptr;
    }
    return mod->CreateBehaviorEdit();
}

ScriptBehaviorGraph *CurrentInspect(CKBehavior *graph, bool live) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Inspect must be called by an active Script Mod.");
        return nullptr;
    }
    return mod->InspectBehavior(graph, live);
}

ScriptBehaviorPlan *CurrentPlan(const std::string &name,
                                const std::string &script,
                                ScriptBehaviorEdit *edit, bool each) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::Plan must be created by an active Script Mod.");
        return nullptr;
    }
    return mod->PlanBehavior(name, script, each, edit);
}

ScriptBehaviorScript *CurrentCreateScript(CKBeObject *owner,
                                           const std::string &name,
                                           ScriptBehaviorEdit *body,
                                           int priority) {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    if (!mod) {
        ScriptStringInterop::RaiseActiveException(
            "BML::Behavior::CreateScript must be called by an active Script Mod.");
        return nullptr;
    }
    return mod->CreateBehaviorScript(owner, name, body, priority);
}

ScriptBehaviorPlan *ScriptBehaviorEdit::Plan(
    const std::string &name, const std::string &script, bool each) {
    return IsValid() ? CurrentPlan(name, script, this, each) : nullptr;
}

ScriptBehaviorScript *ScriptBehaviorEdit::CreateScript(
    CKBeObject *owner, const std::string &name, int priority) {
    return IsValid()
        ? CurrentCreateScript(owner, name, this, priority) : nullptr;
}

ScriptBehaviorService::ScriptBehaviorService()
    : m_State(std::make_shared<ScriptBehaviorState>()) {}

ScriptBehaviorService::~ScriptBehaviorService() { Release(); }

bool ScriptBehaviorService::Bind(ModContext *context, ScriptMod *owner) {
    ScriptDiagnostic released;
    Release(&released);
    if (!released.Message.empty())
        return false;
    try {
        m_State = std::make_shared<ScriptBehaviorState>();
        m_State->Context = context;
        m_State->Owner = owner;
        m_State->OwnerId = owner && owner->GetID() ? owner->GetID() : "";
        m_State->Active = context && owner && !m_State->OwnerId.empty() &&
            context->BehaviorSessions().RegisterOwner(m_State->OwnerId) != 0;
        return m_State->Active;
    } catch (...) {
        return false;
    }
}

void ScriptBehaviorService::Release(ScriptDiagnostic *diagnostic) {
    if (!m_State)
        return;
    try {
        const BML::Behavior::Internal::Status status = m_State->Retire();
        if (!status && diagnostic) {
            *diagnostic = MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime,
                status.Message.empty()
                    ? "Failed to retire Script Behavior resources."
                    : status.Message);
        }
    } catch (const std::exception &error) {
        if (diagnostic)
            *diagnostic = MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime,
                std::string("Failed to retire Script Behavior resources: ") +
                    error.what());
    } catch (...) {
        if (diagnostic)
            *diagnostic = MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime,
                "Failed to retire Script Behavior resources.");
    }
}

std::size_t ScriptBehaviorService::GetActiveCount() const {
    if (!m_State)
        return 0;
    std::size_t count = 0;
    for (const auto &weak : m_State->Resources) {
        if (auto resource = weak.lock(); resource && resource->IsOpen())
            ++count;
    }
    return count;
}

ScriptBehaviorBlock *ScriptBehaviorService::Use(CKGUID prototype) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    if (!session)
        return nullptr;
    try {
        auto resource = std::make_shared<BlockResource>(session->Use(prototype));
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorBlock>(
            "Behavior Block", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Block.");
        return nullptr;
    }
}

ScriptBehaviorBlock *ScriptBehaviorService::Find(
    const std::string &name, const std::string &category,
    const std::string &provider) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    if (!session)
        return nullptr;
    Authoring::PrototypeQuery query;
    query.Name = name;
    if (!category.empty())
        query.Category = category;
    if (!provider.empty())
        query.Provider = provider;
    auto found = session->Prototypes(query);
    if (!Accept("Find Behavior Prototype", found))
        return nullptr;
    if (found->size() != 1) {
        std::string message = found->empty()
            ? "No Behavior Prototype matches the query."
            : "More than one Behavior Prototype matches the query.";
        ScriptStringInterop::RaiseActiveException(message.c_str());
        return nullptr;
    }
    try {
        auto resource = std::make_shared<BlockResource>(
            session->Use(found->front().Ref));
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorBlock>(
            "Behavior Block", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Block.");
        return nullptr;
    }
}

ScriptBehaviorLayout *ScriptBehaviorService::Layout(CKGUID prototype) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    if (!session)
        return nullptr;
    auto layout = session->Layout(prototype);
    if (!Accept("Read declared Behavior Layout", layout))
        return nullptr;
    return NewScriptObject<ScriptBehaviorLayout>(
        "Behavior Layout", m_State, layout.Take());
}

ScriptBehaviorEdit *ScriptBehaviorService::Edit() {
    if (!m_State || !m_State->GetSession())
        return nullptr;
    try {
        auto resource = std::make_shared<EditResource>(Authoring::Edit{});
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorEdit>(
            "Behavior Edit", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Edit.");
        return nullptr;
    }
}

ScriptBehaviorGraph *ScriptBehaviorService::Inspect(CKBehavior *graph,
                                                     bool live) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    if (!session || !graph) {
        if (!graph)
            ScriptStringInterop::RaiseActiveException(
                "Inspect requires a graph-backed CKBehavior.");
        return nullptr;
    }
    auto inspected = session->Inspect(
        graph, live ? Authoring::View::Live : Authoring::View::Logical);
    if (!Accept("Inspect Behavior Graph", inspected))
        return nullptr;
    try {
        auto resource = std::make_shared<GraphResource>(inspected.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorGraph>(
            "Behavior Graph", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Graph.");
        return nullptr;
    }
}

ScriptBehaviorPlan *ScriptBehaviorService::Plan(
    const std::string &name, const std::string &script, bool each,
    ScriptBehaviorEdit *edit) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    Authoring::Edit *body = edit ? edit->Value() : nullptr;
    if (!session || !body)
        return nullptr;
    auto planned = session->Plan(
        name, each ? Authoring::Scripts::Each(script)
                   : Authoring::Scripts::One(script), *body);
    if (!Accept("Create Behavior Plan", planned))
        return nullptr;
    try {
        auto resource = std::make_shared<PlanResource>(planned.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorPlan>(
            "Behavior Plan", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Plan.");
        return nullptr;
    }
}

ScriptBehaviorScript *ScriptBehaviorService::CreateScript(
    CKBeObject *owner, const std::string &name,
    ScriptBehaviorEdit *body, int priority) {
    Authoring::Session *session = m_State ? m_State->GetSession() : nullptr;
    Authoring::Edit *edit = body ? body->Value() : nullptr;
    if (!session || !owner || !edit) {
        if (!owner)
            ScriptStringInterop::RaiseActiveException(
                "CreateScript requires a live owner.");
        return nullptr;
    }
    auto created = session->CreateScript(owner, name, *edit, priority);
    if (!Accept("Create Behavior Script", created))
        return nullptr;
    try {
        auto resource = std::make_shared<ScriptResource>(created.Take());
        m_State->Keep(resource);
        return NewScriptObject<ScriptBehaviorScript>(
            "Behavior Script", m_State, std::move(resource));
    } catch (const std::bad_alloc &) {
        ScriptStringInterop::RaiseActiveException(
            "Out of memory creating Behavior Script.");
        return nullptr;
    }
}

#include "ScriptBehaviorBindings.inc"

} // namespace BML
