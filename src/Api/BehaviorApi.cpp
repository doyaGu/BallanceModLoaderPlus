#include "Api/BehaviorApi.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "BML/ImcWire.hpp"
#include "BML/TypeConvert.h"
#include "Behavior/Patches.h"
#include "Behavior/Sessions.h"
#include "Behavior/FrameStore.h"
#include "Loader/ModContext.h"

namespace {

using BML::Behavior::AdmissionState;
using BML::Behavior::Sessions;
using BML::Behavior::Error;
using BML::Behavior::ExecutionError;
using BML::Behavior::RunFrame;
using BML::Behavior::FrameRetention;
using BML::Behavior::Phase;
using BML::Behavior::Pout;
using BML::Behavior::PoutKind;
using BML::Behavior::PrototypeInfo;
using BML::Behavior::PrototypeQuery;
using BML::Behavior::PrototypeRef;
using BML::Behavior::RunInfo;
using BML::Behavior::RunKind;
using BML::Behavior::RunResult;
using BML::Behavior::RunState;
using BML::Behavior::Slot;
using BML::Behavior::SlotKind;
using BML::Behavior::Spec;
using BML::Behavior::Status;
using BML::Behavior::Value;
using BML::Behavior::Layout;
using BML::Behavior::ManagerRequirement;
using BML::Behavior::GraphModel;
using BML::Behavior::GraphValue;
using BML::Behavior::GraphView;
using BML::Behavior::ReadMode;
using BML::Behavior::Truth;
using BML::Behavior::ValueRelation;
using BML::Behavior::ValueState;
using BML::Behavior::WatchEvent;
using BML::Behavior::WatchKind;
using BML::Behavior::WatchSpec;
using BML::Behavior::Cycle;
using BML::Behavior::DetachedCompatibility;
using BML::Behavior::GraphEdit;
using BML::Behavior::Link;
using BML::Behavior::Node;
using BML::Behavior::NodeQuery;
using BML::Behavior::Order;
using BML::Behavior::OrderKind;
using BML::Behavior::PatchId;
using BML::Behavior::PatchInfo;
using BML::Behavior::PatchKey;
using BML::Behavior::PatchState;
using BML::Behavior::PathRef;
using BML::Behavior::PlanCallbackState;
using BML::Behavior::PlanId;
using BML::Behavior::PlanInfo;
using BML::Behavior::PlanState;
using BML::Behavior::Port;
using BML::Behavior::Script;
using BML::Behavior::SessionOwner;
using BML::Behavior::TargetSet;
namespace HookBlock = BML::Behavior::HookBlock;
namespace Parameter = BML::Behavior::Parameter;

template <typename T>
bool HasStructSize(const T *value) noexcept {
    return value && value->StructSize >= sizeof(T);
}

bool FitsStrided(std::size_t count, std::size_t stride,
                 std::size_t recordSize) noexcept {
    return count == 0 ||
        (stride != 0 && count - 1 <=
            ((std::numeric_limits<std::size_t>::max)() - recordSize) /
                stride);
}

bool IsUtf8(const char *data, std::size_t size) noexcept {
    if (!data && size)
        return false;
    for (std::size_t index = 0; index < size;) {
        const unsigned char first = static_cast<unsigned char>(data[index++]);
        if (first < 0x80)
            continue;
        unsigned continuation = 0;
        std::uint32_t codepoint = 0;
        if ((first & 0xe0u) == 0xc0u) {
            continuation = 1;
            codepoint = first & 0x1fu;
            if (codepoint < 2)
                return false;
        } else if ((first & 0xf0u) == 0xe0u) {
            continuation = 2;
            codepoint = first & 0x0fu;
        } else if ((first & 0xf8u) == 0xf0u) {
            continuation = 3;
            codepoint = first & 0x07u;
            if (codepoint > 4)
                return false;
        } else {
            return false;
        }
        if (continuation > size - index)
            return false;
        for (unsigned part = 0; part < continuation; ++part) {
            const unsigned char byte = static_cast<unsigned char>(data[index++]);
            if ((byte & 0xc0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6) | (byte & 0x3fu);
        }
        if ((continuation == 2 && codepoint < 0x800u) ||
            (continuation == 3 && codepoint < 0x10000u) ||
            codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return false;
    }
    return true;
}

bool ReadString(BML_BehaviorString value, std::string &out,
                bool allowNul = false) {
    if ((!value.Data && value.Length) ||
        !IsUtf8(value.Data, value.Length))
        return false;
    if (!allowNul && value.Length &&
        std::memchr(value.Data, '\0', value.Length))
        return false;
    out.assign(value.Data ? value.Data : "", value.Length);
    return true;
}

CKGUID Guid(BML_BehaviorGuid value) noexcept {
    return CKGUID(value.Data1, value.Data2);
}

BML_BehaviorGuid Guid(CKGUID value) noexcept {
    return {static_cast<std::uint32_t>(value.d1),
            static_cast<std::uint32_t>(value.d2)};
}

std::uint32_t PublicError(Error error) noexcept {
    switch (error) {
    case Error::None: return BML_BEHAVIOR_ERROR_NONE;
    case Error::ContextExpired:
    case Error::OwnerInvalid: return BML_BEHAVIOR_ERROR_OWNER_UNAVAILABLE;
    case Error::PrototypeNotFound: return BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND;
    case Error::PrototypeChanged: return BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED;
    case Error::PrototypeLoadFailed: return BML_BEHAVIOR_ERROR_PROTOTYPE_LOAD_FAILED;
    case Error::RequiredManagerMissing: return BML_BEHAVIOR_ERROR_REQUIRED_MANAGER_MISSING;
    case Error::CreateFailed: return BML_BEHAVIOR_ERROR_CREATION_FAILED;
    case Error::InitFailed: return BML_BEHAVIOR_ERROR_INITIALIZATION_FAILED;
    case Error::TargetInvalid: return BML_BEHAVIOR_ERROR_TARGET_INVALID;
    case Error::CallbackFailed: return BML_BEHAVIOR_ERROR_CALLBACK_FAILED;
    case Error::SlotNotFound: return BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND;
    case Error::AmbiguousSlot: return BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS;
    case Error::StaleLayout: return BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
    case Error::LayoutUnavailable: return BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE;
    case Error::TypeMismatch: return BML_BEHAVIOR_ERROR_TYPE_MISMATCH;
    case Error::ParameterTypeUnavailable: return BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNAVAILABLE;
    case Error::ParameterTypeUnsupported: return BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED;
    case Error::ValueWriteFailed: return BML_BEHAVIOR_ERROR_VALUE_INVALID;
    case Error::SourceInvalid: return BML_BEHAVIOR_ERROR_SOURCE_INVALID;
    case Error::OperationInvalid: return BML_BEHAVIOR_ERROR_OPERATION_INVALID;
    case Error::InvalidState: return BML_BEHAVIOR_ERROR_STATE_INVALID;
    case Error::Busy: return BML_BEHAVIOR_ERROR_BUSY;
    case Error::Unavailable: return BML_BEHAVIOR_ERROR_UNAVAILABLE;
    case Error::UnsupportedBreak: return BML_BEHAVIOR_ERROR_BREAK_UNSUPPORTED;
    case Error::UnsupportedPout: return BML_BEHAVIOR_ERROR_POUT_UNSUPPORTED;
    case Error::PoutUnavailable: return BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE;
    case Error::FrameQueueFull: return BML_BEHAVIOR_ERROR_FRAME_QUEUE_FULL;
    case Error::ExecutionCancelled: return BML_BEHAVIOR_ERROR_CANCELLED;
    case Error::DetachedUnsupported: return BML_BEHAVIOR_ERROR_DETACHED_UNSUPPORTED;
    case Error::ObserverUnavailable: return BML_BEHAVIOR_ERROR_OBSERVER_UNAVAILABLE;
    case Error::GraphChanged: return BML_BEHAVIOR_ERROR_GRAPH_CHANGED;
    case Error::InvalidGraphLocality:
        return BML_BEHAVIOR_ERROR_GRAPH_LOCALITY_INVALID;
    case Error::InvalidDelay: return BML_BEHAVIOR_ERROR_DELAY_INVALID;
    case Error::UnconfirmedSameFrameCycle:
        return BML_BEHAVIOR_ERROR_SAME_FRAME_CYCLE;
    case Error::SharedSourceCycle:
        return BML_BEHAVIOR_ERROR_SHARED_SOURCE_CYCLE;
    case Error::PushCycle: return BML_BEHAVIOR_ERROR_PUSH_CYCLE;
    case Error::InterfaceUnsupported:
        return BML_BEHAVIOR_ERROR_INTERFACE_UNSUPPORTED;
    case Error::SourceConflict: return BML_BEHAVIOR_ERROR_SOURCE_CONFLICT;
    case Error::SourceOrderCycle:
        return BML_BEHAVIOR_ERROR_SOURCE_ORDER_CYCLE;
    case Error::OrderingTargetMismatch:
        return BML_BEHAVIOR_ERROR_ORDERING_TARGET_MISMATCH;
    case Error::OverlayOrderCycle:
        return BML_BEHAVIOR_ERROR_OVERLAY_ORDER_CYCLE;
    case Error::LinkNotFound: return BML_BEHAVIOR_ERROR_LINK_NOT_FOUND;
    case Error::PathAmbiguous: return BML_BEHAVIOR_ERROR_PATH_AMBIGUOUS;
    case Error::PathCycle: return BML_BEHAVIOR_ERROR_PATH_CYCLE;
    case Error::QueryNotFound: return BML_BEHAVIOR_ERROR_QUERY_NOT_FOUND;
    case Error::QueryAmbiguous: return BML_BEHAVIOR_ERROR_QUERY_AMBIGUOUS;
    case Error::WorldBoundValue:
        return BML_BEHAVIOR_ERROR_WORLD_BOUND_VALUE;
    case Error::RevertConflict: return BML_BEHAVIOR_ERROR_REVERT_CONFLICT;
    case Error::TargetCardinality:
        return BML_BEHAVIOR_ERROR_TARGET_CARDINALITY;
    case Error::WrongThread: return BML_BEHAVIOR_ERROR_WRONG_THREAD;
    case Error::ExecutionFailed: return BML_BEHAVIOR_ERROR_NATIVE_ERROR;
    }
    return BML_BEHAVIOR_ERROR_NATIVE_ERROR;
}

std::uint32_t PublicError(ExecutionError error) noexcept {
    switch (error) {
    case ExecutionError::None: return BML_BEHAVIOR_ERROR_NONE;
    case ExecutionError::SelectorNotFound: return BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND;
    case ExecutionError::SelectorAmbiguous: return BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS;
    case ExecutionError::LayoutStale: return BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
    case ExecutionError::UnsupportedBreak: return BML_BEHAVIOR_ERROR_BREAK_UNSUPPORTED;
    case ExecutionError::UnsupportedPout: return BML_BEHAVIOR_ERROR_POUT_UNSUPPORTED;
    case ExecutionError::OutUnavailable:
        return BML_BEHAVIOR_ERROR_NATIVE_ERROR;
    case ExecutionError::PoutReadFailed: return BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE;
    case ExecutionError::FrameQueueFull: return BML_BEHAVIOR_ERROR_FRAME_QUEUE_FULL;
    case ExecutionError::Cancelled: return BML_BEHAVIOR_ERROR_CANCELLED;
    case ExecutionError::InvalidState:
    case ExecutionError::ActivationFailed: return BML_BEHAVIOR_ERROR_STATE_INVALID;
    case ExecutionError::NativeFailed: return BML_BEHAVIOR_ERROR_NATIVE_ERROR;
    }
    return BML_BEHAVIOR_ERROR_NATIVE_ERROR;
}

std::uint32_t PublicPhase(Phase phase) noexcept {
    switch (phase) {
    case Phase::None: return BML_BEHAVIOR_PHASE_NONE;
    case Phase::PrototypeResolution: return BML_BEHAVIOR_PHASE_PROTOTYPE;
    case Phase::ManagerValidation: return BML_BEHAVIOR_PHASE_MANAGER;
    case Phase::Creation: return BML_BEHAVIOR_PHASE_CREATION;
    case Phase::Initialization: return BML_BEHAVIOR_PHASE_INITIALIZATION;
    case Phase::StaticLayout: return BML_BEHAVIOR_PHASE_LAYOUT;
    case Phase::OwnerBinding: return BML_BEHAVIOR_PHASE_OWNER;
    case Phase::TargetBinding: return BML_BEHAVIOR_PHASE_TARGET;
    case Phase::Settings: return BML_BEHAVIOR_PHASE_SETTINGS;
    case Phase::LifecycleCallback: return BML_BEHAVIOR_PHASE_CALLBACK;
    case Phase::ParameterBinding: return BML_BEHAVIOR_PHASE_BINDING;
    case Phase::Execution: return BML_BEHAVIOR_PHASE_EXECUTION;
    case Phase::Edit: return BML_BEHAVIOR_PHASE_EDIT;
    case Phase::Teardown: return BML_BEHAVIOR_PHASE_TEARDOWN;
    }
    return BML_BEHAVIOR_PHASE_NONE;
}

void WriteMessage(BML_BehaviorStatus &out, const std::string &message) noexcept {
    out.MessageLength = message.size() > UINT32_MAX
        ? UINT32_MAX : static_cast<std::uint32_t>(message.size());
    const std::size_t count = (std::min)(
        message.size(), static_cast<std::size_t>(BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY - 1));
    if (count)
        std::memcpy(out.Message, message.data(), count);
    out.Message[count] = '\0';
}

void WriteStatus(BML_BehaviorStatus *out, const Status &status) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->Error = PublicError(status.Code);
    out->Phase = PublicPhase(status.Details.Stage);
    out->CkError = status.CkError;
    out->NativeResult = status.BehaviorResult;
    out->Prototype = Guid(status.Details.Prototype);
    out->Type = Guid(status.Details.ActualType);
    WriteMessage(*out, status.Message);
}

bool PrepareStatus(BML_BehaviorStatus *out) noexcept {
    if (!out)
        return true;
    if (!HasStructSize(out))
        return false;
    WriteStatus(out, {});
    return true;
}

Status InvalidValue(std::string message) {
    return {Error::ValueWriteFailed, CKERR_INVALIDPARAMETER,
            CKBR_PARAMETERERROR, std::move(message)};
}

bool ReadSelector(const BML_BehaviorSelector &from, SlotKind slotKind,
                  CKGUID type, Slot &to, Status &status) {
    if (from.StructSize < sizeof(from)) {
        status = InvalidValue("A Behavior selector has an unsupported StructSize.");
        return false;
    }
    switch (from.Kind) {
    case BML_BEHAVIOR_SELECTOR_INDEX:
        if (from.Index < 0) {
            status = InvalidValue("A Behavior slot index cannot be negative.");
            return false;
        }
        to = Slot::At(slotKind, from.Index, type);
        return true;
    case BML_BEHAVIOR_SELECTOR_NAME:
    case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME: {
        std::string name;
        if (!ReadString(from.Name, name) || name.empty() || from.Occurrence < 0) {
            status = InvalidValue("A Behavior slot name or occurrence is invalid.");
            return false;
        }
        to = from.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME
            ? Slot::Named(slotKind, std::move(name), type)
            : Slot::OccurrenceOf(slotKind, std::move(name), from.Occurrence, type);
        return true;
    }
    case BML_BEHAVIOR_SELECTOR_ONLY:
        if (from.Index != 0 || from.Occurrence != 0 || from.Name.Length != 0) {
            status = InvalidValue(
                "An only-slot Behavior selector cannot carry an index, occurrence, or name.");
            return false;
        }
        to = Slot::Only(slotKind, type);
        return true;
    default:
        status = InvalidValue("A Behavior selector kind is unknown.");
        return false;
    }
}

template <typename T>
Value RawValue(CKGUID type, const T &value) {
    return Value::Raw(type, &value, sizeof(value));
}

bool ReadValue(const BML_BehaviorValue &from, ModContext &context,
               Parameter::Binding &to, Status &status) {
    if (from.StructSize < sizeof(from)) {
        status = InvalidValue("A Behavior value has an unsupported StructSize.");
        return false;
    }
    const CKGUID type = Guid(from.Type);
    if (type == CKGUID()) {
        status = InvalidValue("A Behavior value requires its Virtools parameter type.");
        return false;
    }
    CKParameterManager *parameters = context.GetParameterManager();
    const BML::Behavior::Parameter::Type parameterType =
        BML::Behavior::Parameter::Describe(parameters, type);
    if (!parameterType.Valid) {
        status = {Error::ParameterTypeUnavailable,
                  CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                  "The Virtools parameter type is not registered."};
        status.Details.Stage = Phase::ParameterBinding;
        status.Details.ActualType = type;
        return false;
    }
    if (!parameterType.Supported()) {
        status = {Error::ParameterTypeUnsupported,
                  CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                  "The Virtools parameter type has no supported author-facing value form."};
        status.Details.Stage = Phase::ParameterBinding;
        status.Details.ActualType = type;
        return false;
    }
    const auto typeMatchesKind = [&] {
        using Form = BML::Behavior::Parameter::Form;
        switch (from.Kind) {
        case BML_BEHAVIOR_VALUE_BOOL: return parameterType.ValueForm == Form::Bool;
        case BML_BEHAVIOR_VALUE_INT32: return parameterType.ValueForm == Form::Int32;
        case BML_BEHAVIOR_VALUE_FLOAT32: return parameterType.ValueForm == Form::Float32;
        case BML_BEHAVIOR_VALUE_UTF8: return parameterType.ValueForm == Form::Utf8;
        case BML_BEHAVIOR_VALUE_VEC2: return parameterType.ValueForm == Form::Vec2;
        case BML_BEHAVIOR_VALUE_VEC3: return parameterType.ValueForm == Form::Vec3;
        case BML_BEHAVIOR_VALUE_QUATERNION: return parameterType.ValueForm == Form::Quaternion;
        case BML_BEHAVIOR_VALUE_EULER: return parameterType.ValueForm == Form::Euler;
        case BML_BEHAVIOR_VALUE_RECT: return parameterType.ValueForm == Form::Rect;
        case BML_BEHAVIOR_VALUE_COLOR: return parameterType.ValueForm == Form::Color;
        case BML_BEHAVIOR_VALUE_BOX: return parameterType.ValueForm == Form::Box;
        case BML_BEHAVIOR_VALUE_MAT4: return parameterType.ValueForm == Form::Mat4;
        case BML_BEHAVIOR_VALUE_OBJECT: return parameterType.ValueForm == Form::Object;
        default: return false;
        }
    };
    if (!typeMatchesKind()) {
        status = {Error::TypeMismatch, CKERR_INVALIDPARAMETERTYPE,
                  CKBR_PARAMETERERROR,
                  "The Behavior value kind does not describe this Virtools parameter type."};
        status.Details.ActualType = type;
        return false;
    }
    switch (from.Kind) {
    case BML_BEHAVIOR_VALUE_BOOL: {
        const CKBOOL value = from.Data.Bool ? TRUE : FALSE;
        to = RawValue(type, value);
        return true;
    }
    case BML_BEHAVIOR_VALUE_INT32:
        to = RawValue(type, from.Data.Int32);
        return true;
    case BML_BEHAVIOR_VALUE_FLOAT32:
        to = RawValue(type, from.Data.Float32);
        return true;
    case BML_BEHAVIOR_VALUE_UTF8: {
        std::string value;
        if (!ReadString(from.Data.Utf8, value)) {
            status = InvalidValue("A Behavior string value is not valid UTF-8 text.");
            return false;
        }
        to = Value::Text(type, std::move(value));
        return true;
    }
    case BML_BEHAVIOR_VALUE_VEC2:
        to = RawValue(type, BML::Convert::ToVxVector(from.Data.Vec2));
        return true;
    case BML_BEHAVIOR_VALUE_VEC3:
        to = RawValue(type, BML::Convert::ToVxVector(from.Data.Vec3));
        return true;
    case BML_BEHAVIOR_VALUE_QUATERNION: {
        const BML_Quaternion &value = from.Data.Quaternion;
        to = RawValue(type, VxQuaternion(value.x, value.y, value.z, value.w));
        return true;
    }
    case BML_BEHAVIOR_VALUE_EULER: {
        const float value[3] = {from.Data.Euler.x, from.Data.Euler.y,
                                from.Data.Euler.z};
        to = Value::Raw(type, value, sizeof(value));
        return true;
    }
    case BML_BEHAVIOR_VALUE_RECT: {
        const BML_Rect &value = from.Data.Rect;
        to = RawValue(type,
                      VxRect(value.left, value.top, value.right, value.bottom));
        return true;
    }
    case BML_BEHAVIOR_VALUE_COLOR: {
        const BML_Color &value = from.Data.Color;
        to = RawValue(type, VxColor(value.r, value.g, value.b, value.a));
        return true;
    }
    case BML_BEHAVIOR_VALUE_BOX: {
        const BML_Box &value = from.Data.Box;
        to = RawValue(type,
                      VxBbox(BML::Convert::ToVxVector(value.Min),
                             BML::Convert::ToVxVector(value.Max)));
        return true;
    }
    case BML_BEHAVIOR_VALUE_MAT4:
        to = RawValue(type, BML::Convert::ToVxMatrix(from.Data.Mat4));
        return true;
    case BML_BEHAVIOR_VALUE_OBJECT: {
        CKObject *object = context.ObjectRefs().Resolve(from.Data.Object);
        if (from.Data.Object.Domain && !object) {
            status = {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                      CKBR_PARAMETERERROR,
                      "A Behavior object value is stale."};
            return false;
        }
        to = Parameter::Binding::Object(type, object);
        return true;
    }
    default:
        status = InvalidValue("A Behavior value kind is unknown.");
        return false;
    }
}

bool ReadBindings(const BML_BehaviorBinding *bindings, std::uint32_t count,
                  SlotKind kind, ModContext &context, Spec &block,
                  Status &status) {
    if (count && !bindings) {
        status = InvalidValue("A Behavior binding array is missing.");
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const BML_BehaviorBinding &binding = bindings[index];
        if (binding.StructSize < sizeof(binding)) {
            status = InvalidValue("A Behavior binding has an unsupported StructSize.");
            return false;
        }
        Parameter::Binding value;
        if (!ReadValue(binding.Value, context, value, status))
            return false;
        Slot slot;
        if (!ReadSelector(binding.Slot, kind, Guid(binding.Value.Type), slot, status))
            return false;
        if (kind == SlotKind::Setting)
            block.Setting(std::move(slot), std::move(value));
        else if (kind == SlotKind::InputParameter)
            block.Input(std::move(slot), std::move(value));
        else
            block.Local(std::move(slot), std::move(value));
    }
    return true;
}

bool ReadBlock(const BML_BehaviorBlock &from, ModContext &context,
               Spec &to, Status &status) {
    if (from.StructSize < sizeof(from) ||
        from.Target.StructSize < sizeof(from.Target) ||
        from.Frames.StructSize < sizeof(from.Frames)) {
        status = InvalidValue("The Behavior Block has an unsupported StructSize.");
        return false;
    }
    if ((from.SettingStageCount && !from.SettingStages) ||
        (from.PinCount && !from.Pins) ||
        (from.LocalCount && !from.Locals)) {
        status = InvalidValue("A Behavior Block array is missing.");
        return false;
    }

    to = Spec(Guid(from.Prototype));
    to.PrototypeGeneration(from.PrototypeGeneration);
    switch (from.Target.Kind) {
    case BML_BEHAVIOR_TARGET_OWNER:
        to.TargetOwner();
        break;
    case BML_BEHAVIOR_TARGET_OBJECT: {
        CKObject *target = context.ObjectRefs().Resolve(from.Target.Object);
        if (!target) {
            status = {Error::TargetInvalid, CKERR_INVALIDOBJECT,
                      CKBR_PARAMETERERROR,
                      "The Behavior Target is stale or null."};
            return false;
        }
        to.Target(Guid(from.Target.Type), target);
        break;
    }
    case BML_BEHAVIOR_TARGET_NULL:
        to.NullTarget(Guid(from.Target.Type));
        break;
    default:
        status = InvalidValue("The Behavior Target kind is unknown.");
        return false;
    }

    for (std::uint32_t stage = 0; stage < from.SettingStageCount; ++stage) {
        const BML_BehaviorSettingStage &settings = from.SettingStages[stage];
        if (settings.StructSize < sizeof(settings)) {
            status = InvalidValue("A Behavior Setting stage has an unsupported StructSize.");
            return false;
        }
        if (stage)
            to.RefreshLayout();
        if (!ReadBindings(settings.Settings, settings.SettingCount,
                          SlotKind::Setting, context, to, status))
            return false;
    }
    if (!ReadBindings(from.Pins, from.PinCount, SlotKind::InputParameter,
                      context, to, status) ||
        !ReadBindings(from.Locals, from.LocalCount, SlotKind::Local,
                      context, to, status))
        return false;

    switch (from.Frames.Kind) {
    case BML_BEHAVIOR_FRAMES_SIGNALS:
        if (!from.Frames.Limit) {
            status = InvalidValue("signals(n) requires a nonzero RunFrame limit.");
            return false;
        }
        to.Frames(FrameRetention::Signals(from.Frames.Limit));
        break;
    case BML_BEHAVIOR_FRAMES_EACH_FRAME:
        if (!from.Frames.Limit) {
            status = InvalidValue("eachFrame(n) requires a nonzero RunFrame limit.");
            return false;
        }
        to.Frames(FrameRetention::EachFrame(from.Frames.Limit));
        break;
    case BML_BEHAVIOR_FRAMES_LATEST:
        to.Frames(FrameRetention::Latest());
        break;
    case BML_BEHAVIOR_FRAMES_NONE:
        to.Frames(FrameRetention::Ignore());
        break;
    default:
        status = InvalidValue("The Behavior RunFrame policy is unknown.");
        return false;
    }
    return true;
}

std::uint32_t PublicRunKind(RunKind kind) noexcept {
    switch (kind) {
    case RunKind::Call: return BML_BEHAVIOR_RUN_CALL;
    case RunKind::Task: return BML_BEHAVIOR_RUN_TASK;
    case RunKind::Instance: return BML_BEHAVIOR_RUN_INSTANCE;
    }
    return BML_BEHAVIOR_RUN_INSTANCE;
}

std::uint32_t PublicRunState(RunState state) noexcept {
    switch (state) {
    case RunState::Ready: return BML_BEHAVIOR_RUN_READY;
    case RunState::Pending: return BML_BEHAVIOR_RUN_PENDING;
    case RunState::Failed: return BML_BEHAVIOR_RUN_FAILED;
    }
    return BML_BEHAVIOR_RUN_FAILED;
}

void WriteRunInfo(BML_BehaviorRunInfo *out, const RunInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->Kind = PublicRunKind(info.Kind);
    out->State = PublicRunState(info.State);
    out->Flags = info.Detached == DetachedCompatibility::Unverified
        ? BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED : 0;
    out->Status.StructSize = sizeof(out->Status);
    WriteStatus(&out->Status, info.LastStatus);
}

bool ValidOutputs(BML_BehaviorRunInfo *info,
                  BML_BehaviorStatus *status) noexcept {
    const bool statusValid = PrepareStatus(status);
    return statusValid && (!info || HasStructSize(info));
}

std::uintptr_t SessionId(BML_BehaviorSession session) noexcept {
    return reinterpret_cast<std::uintptr_t>(session);
}

std::uintptr_t RunId(BML_BehaviorRun run) noexcept {
    return reinterpret_cast<std::uintptr_t>(run);
}

std::uintptr_t WatchId(BML_BehaviorWatch watch) noexcept {
    return reinterpret_cast<std::uintptr_t>(watch);
}

BML_BehaviorSession SessionHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorSession>(id);
}

BML_BehaviorRun RunHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorRun>(id);
}

BML_BehaviorWatch WatchHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorWatch>(id);
}

CKBeObject *ReadOwner(BML_ObjectRef owner, ModContext &context,
                      Status &status) {
    if (!owner.Domain)
        return nullptr;
    CKObject *object = context.ObjectRefs().Resolve(owner);
    if (!object || !CKIsChildClassOf(object, CKCID_BEOBJECT)) {
        status = {Error::OwnerInvalid, CKERR_INVALIDOBJECT,
                  CKBR_PARAMETERERROR,
                  "The Behavior owner is stale or is not a CKBeObject."};
        return nullptr;
    }
    return static_cast<CKBeObject *>(object);
}

CKBehavior *ReadBehavior(BML_ObjectRef reference, ModContext &context,
                         Status &status) {
    CKObject *object = context.ObjectRefs().Resolve(reference);
    if (!object || !CKIsChildClassOf(object, CKCID_BEHAVIOR)) {
        status = {Error::InvalidState, CKERR_INVALIDOBJECT,
                  CKBR_PARAMETERERROR,
                  "The Behavior graph or node reference is stale."};
        return nullptr;
    }
    return static_cast<CKBehavior *>(object);
}

template <typename Function>
int Guard(Function &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ResultCode(const Status &status) noexcept {
    if (status.Code == Error::WrongThread)
        return BML_ERROR_WRONG_THREAD;
    if (status.Code == Error::InvalidState)
        return BML_ERROR_INVALID_HANDLE;
    if (status.Code == Error::Busy)
        return BML_ERROR_BUSY;
    if (status.Code == Error::Unavailable ||
        status.Code == Error::ObserverUnavailable)
        return BML_ERROR_UNAVAILABLE;
    if (status.Code == Error::OwnerInvalid || status.Code == Error::ContextExpired)
        return BML_ERROR_OBJECT_INVALID;
    return status ? BML_OK : BML_ERROR_FAIL;
}

int OpenRunResult(const BML::Behavior::OpenRun &opened,
                  BML_BehaviorRun *outRun,
                  BML_BehaviorRunInfo *info,
                  BML_BehaviorStatus *status) {
    WriteStatus(status, opened.Result);
    if (!opened) {
        *outRun = nullptr;
        return ResultCode(opened.Result);
    }
    *outRun = RunHandle(opened.Id);
    WriteRunInfo(info, opened.Info);
    return BML_OK;
}

int BML_BEHAVIOR_CALL OpenSession(BML_BehaviorString requestedOwner,
                                  BML_BehaviorSession *outSession,
                                  BML_BehaviorStatus *status) {
    const void *caller = _ReturnAddress();
    return Guard([&] {
        if (!PrepareStatus(status) || !outSession)
            return BML_ERROR_INVALID_PARAMETER;
        *outSession = nullptr;
        std::string requested;
        if (!ReadString(requestedOwner, requested))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context || !context->AreModsLoaded())
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        const std::string owner = context->GetNativeModOwnerId(
            caller, requested.empty() ? nullptr : requested.c_str());
        if (owner.empty() || (!requested.empty() && owner != requested))
            return BML_ERROR_ACCESS_DENIED;
        std::uintptr_t id = 0;
        Status result = context->BehaviorSessions().OpenSession(owner, id);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outSession = SessionHandle(id);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseSession(BML_BehaviorSession session) {
    return Guard([&] {
        if (!session)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        context->BehaviorSessions().CloseSession(SessionId(session));
        return BML_OK;
    });
}

enum class OpenKind { Call, Start, Spawn };

int OpenRunEntry(OpenKind kind, BML_BehaviorSession session,
                 BML_ObjectRef ownerReference,
                 const BML_BehaviorBlock *block,
                 const BML_BehaviorSelector *input,
                 BML_BehaviorRun *outRun,
                 BML_BehaviorRunInfo *info,
                 BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !session || !block || !outRun ||
        (kind != OpenKind::Spawn && !input))
        return BML_ERROR_INVALID_PARAMETER;
    *outRun = nullptr;
    ModContext *context = BML_GetModContext();
    if (!context)
        return BML_ERROR_FROZEN;
    if (!context->IsMainThread())
        return BML_ERROR_WRONG_THREAD;

    Status readStatus;
    Spec definition;
    if (!ReadBlock(*block, *context, definition, readStatus)) {
        WriteStatus(status, readStatus);
        return BML_ERROR_INVALID_PARAMETER;
    }
    CKBeObject *owner = ReadOwner(ownerReference, *context, readStatus);
    if (ownerReference.Domain && !owner) {
        WriteStatus(status, readStatus);
        return BML_ERROR_OBJECT_INVALID;
    }
    Slot in;
    if (kind != OpenKind::Spawn &&
        !ReadSelector(*input, SlotKind::Input, CKGUID(), in, readStatus)) {
        WriteStatus(status, readStatus);
        return BML_ERROR_INVALID_PARAMETER;
    }

    Sessions &sessions = context->BehaviorSessions();
    const auto opened = kind == OpenKind::Call
        ? sessions.Call(SessionId(session), owner, definition, in)
        : kind == OpenKind::Start
            ? sessions.Start(SessionId(session), owner, definition, in)
            : sessions.Spawn(SessionId(session), owner, definition);
    return OpenRunResult(opened, outRun, info, status);
}

int BML_BEHAVIOR_CALL Call(BML_BehaviorSession session, BML_ObjectRef owner,
                           const BML_BehaviorBlock *block,
                           const BML_BehaviorSelector *input,
                           BML_BehaviorRun *outRun,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Call, session, owner, block, input,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL Start(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            const BML_BehaviorSelector *input,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Start, session, owner, block, input,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL Spawn(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Spawn, session, owner, block, nullptr,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL Continue(BML_BehaviorRun run,
                              BML_BehaviorRunInfo *info,
                              BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!ValidOutputs(info, status) || !run)
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        RunResult result = context->BehaviorSessions().Continue(RunId(run));
        WriteStatus(status, result.Detail);
        RunInfo current;
        const Status read = context->BehaviorSessions().ReadRun(RunId(run), current);
        if (read)
            WriteRunInfo(info, current);
        return ResultCode(result.Detail);
    });
}

int BML_BEHAVIOR_CALL Pulse(BML_BehaviorRun run,
                           const BML_BehaviorSelector *input,
                           std::uint32_t *admission,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!ValidOutputs(info, status) || !run || !input || !admission)
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Status readStatus;
        Slot in;
        if (!ReadSelector(*input, SlotKind::Input, CKGUID(), in, readStatus)) {
            WriteStatus(status, readStatus);
            return BML_ERROR_INVALID_PARAMETER;
        }
        RunResult result = context->BehaviorSessions().Pulse(RunId(run), in);
        WriteStatus(status, result.Detail);
        if (result.Admission == AdmissionState::Failed)
            return ResultCode(result.Detail);
        *admission = result.Admission == AdmissionState::Queued
            ? BML_BEHAVIOR_ADMISSION_QUEUED
            : BML_BEHAVIOR_ADMISSION_EXECUTED;
        RunInfo current;
        if (context->BehaviorSessions().ReadRun(RunId(run), current))
            WriteRunInfo(info, current);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadRun(BML_BehaviorRun run,
                             BML_BehaviorRunInfo *info,
                             BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!ValidOutputs(info, status) || !run || !info)
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        RunInfo current;
        Status result = context->BehaviorSessions().ReadRun(RunId(run), current);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WriteRunInfo(info, current);
        return BML_OK;
    });
}

std::uint32_t PublicPoutKind(PoutKind kind) noexcept {
    return static_cast<std::uint32_t>(kind) + 1u;
}

class FrameBatch final {
public:
    explicit FrameBatch(bool materialize) noexcept
        : m_Materialize(materialize) {}

    bool Add(const RunFrame &frame) {
        BML_BehaviorRunFrame header{};
        header.StructSize = sizeof(header);
        header.Sequence = frame.Sequence;
        header.Frame = frame.Frame;
        header.NativeResult = frame.ReturnCode;
        if (frame.NativeContinuation)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_NATIVE;
        if (frame.QueuedInput)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT;
        header.Error = PublicError(frame.Fault.Code);

        if (!AddOuts(frame, header) || !AddPouts(frame, header) ||
            !AddDiagnostic(frame, header))
            return false;
        if (m_Materialize) {
            m_Headers.push_back(header);
            m_Sequences.push_back(frame.Sequence);
        }
        ++m_HeaderCount;
        return true;
    }

    [[nodiscard]] std::size_t HeaderCount() const noexcept {
        return m_HeaderCount;
    }

    [[nodiscard]] std::size_t PayloadSize() const noexcept {
        return m_PayloadSize;
    }

    [[nodiscard]] const std::vector<BML_BehaviorRunFrame> &Headers() const
        noexcept {
        return m_Headers;
    }

    [[nodiscard]] const std::vector<std::uint8_t> &Payload() const noexcept {
        return m_Payload;
    }

    [[nodiscard]] const std::vector<std::uint64_t> &Sequences() const noexcept {
        return m_Sequences;
    }

private:
    bool Align(std::size_t alignment) {
        const std::size_t remainder = m_PayloadSize % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (m_PayloadSize > UINT32_MAX ||
            padding > UINT32_MAX - m_PayloadSize)
            return false;
        if (m_Materialize)
            m_Payload.insert(m_Payload.end(), padding, 0);
        m_PayloadSize += padding;
        return true;
    }

    bool Append(const void *data, std::size_t size, std::uint32_t &offset) {
        if ((!data && size) || m_PayloadSize > UINT32_MAX ||
            size > UINT32_MAX || size > UINT32_MAX - m_PayloadSize)
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Materialize && size) {
            const auto *bytes = static_cast<const std::uint8_t *>(data);
            m_Payload.insert(m_Payload.end(), bytes, bytes + size);
        }
        m_PayloadSize += size;
        return true;
    }

    template <typename T>
    bool ReserveRecords(std::size_t count, std::uint32_t &offset) {
        if (!Align(alignof(T)) || count > UINT32_MAX / sizeof(T))
            return false;
        const std::size_t bytes = count * sizeof(T);
        if (m_PayloadSize > UINT32_MAX ||
            bytes > UINT32_MAX - m_PayloadSize)
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Materialize)
            m_Payload.resize(m_Payload.size() + bytes, 0);
        m_PayloadSize += bytes;
        return true;
    }

    template <typename T>
    void StoreRecord(std::uint32_t base, std::size_t index,
                     const T &record) {
        if (m_Materialize) {
            std::memcpy(m_Payload.data() + base + index * sizeof(T),
                        &record, sizeof(record));
        }
    }

    bool AddOuts(const RunFrame &frame,
                 BML_BehaviorRunFrame &header) {
        if (frame.ActiveOutputs.empty())
            return true;
        header.OutCount = static_cast<std::uint32_t>(frame.ActiveOutputs.size());
        if (!ReserveRecords<BML_BehaviorOutRecord>(
                frame.ActiveOutputs.size(), header.OutOffset))
            return false;
        for (std::size_t index = 0; index < frame.ActiveOutputs.size(); ++index) {
            const auto &out = frame.ActiveOutputs[index];
            BML_BehaviorOutRecord record{};
            record.StructSize = sizeof(record);
            record.Index = out.Index;
            record.Occurrence = out.Occurrence;
            record.NameLength = static_cast<std::uint32_t>(out.Name.size());
            if (!Append(out.Name.data(), out.Name.size(), record.NameOffset))
                return false;
            StoreRecord(header.OutOffset, index, record);
        }
        return true;
    }

    bool AddPouts(const RunFrame &frame,
                  BML_BehaviorRunFrame &header) {
        if (frame.Pouts.empty())
            return true;
        header.PoutCount = static_cast<std::uint32_t>(frame.Pouts.size());
        if (!ReserveRecords<BML_BehaviorPoutRecord>(
                frame.Pouts.size(), header.PoutOffset))
            return false;
        for (std::size_t index = 0; index < frame.Pouts.size(); ++index) {
            const Pout &pout = frame.Pouts[index];
            BML_BehaviorPoutRecord record{};
            record.StructSize = sizeof(record);
            record.Index = pout.Index;
            record.Occurrence = pout.Occurrence;
            record.Type = {pout.TypeGuid1, pout.TypeGuid2};
            record.Kind = PublicPoutKind(pout.Kind);
            record.NameLength = static_cast<std::uint32_t>(pout.Name.size());
            if (!Append(pout.Name.data(), pout.Name.size(), record.NameOffset) ||
                !AddPoutValue(pout, record))
                return false;
            StoreRecord(header.PoutOffset, index, record);
        }
        return true;
    }

    bool AddPoutValue(const Pout &pout, BML_BehaviorPoutRecord &record) {
        if (!Align(BML_BEHAVIOR_VALUE_ALIGNMENT))
            return false;
        std::uint8_t bytes[64]{};
        std::size_t size = 0;
        switch (pout.Kind) {
        case PoutKind::Bool:
            BML::Imc::Wire::Detail::Store32(bytes, pout.Int32 ? 1u : 0u);
            size = 4;
            break;
        case PoutKind::Int32:
            BML::Imc::Wire::Detail::Store32(
                bytes, static_cast<std::uint32_t>(pout.Int32));
            size = 4;
            break;
        case PoutKind::Float32:
            BML::Imc::Wire::Detail::Store32(
                bytes, std::bit_cast<std::uint32_t>(pout.Float32));
            size = 4;
            break;
        case PoutKind::Utf8:
            record.ValueSize = static_cast<std::uint32_t>(pout.Text.size());
            return Append(pout.Text.data(), pout.Text.size(), record.ValueOffset);
        case PoutKind::Object:
            BML::Imc::Wire::Detail::Store32(bytes, pout.ObjectDomain);
            BML::Imc::Wire::Detail::Store32(bytes + 4, pout.ObjectSlot);
            BML::Imc::Wire::Detail::Store32(bytes + 8, pout.ObjectGeneration);
            size = 12;
            break;
        default:
            if (pout.ComponentCount > pout.Components.size())
                return false;
            size = static_cast<std::size_t>(pout.ComponentCount) * 4u;
            for (std::uint32_t index = 0; index < pout.ComponentCount; ++index) {
                BML::Imc::Wire::Detail::Store32(
                    bytes + index * 4,
                    std::bit_cast<std::uint32_t>(pout.Components[index]));
            }
            break;
        }
        record.ValueSize = static_cast<std::uint32_t>(size);
        return Append(bytes, size, record.ValueOffset);
    }

    bool AddDiagnostic(const RunFrame &frame,
                       BML_BehaviorRunFrame &header) {
        if (!frame.Fault)
            return true;
        header.DiagnosticCount = 1;
        if (!ReserveRecords<BML_BehaviorDiagnosticRecord>(
                1, header.DiagnosticOffset))
            return false;
        BML_BehaviorDiagnosticRecord record{};
        record.StructSize = sizeof(record);
        record.Error = PublicError(frame.Fault.Code);
        record.Phase = BML_BEHAVIOR_PHASE_EXECUTION;
        record.NativeResult = frame.Fault.NativeCode;
        record.MessageLength = static_cast<std::uint32_t>(
            frame.Fault.Message.size());
        if (!Append(frame.Fault.Message.data(), frame.Fault.Message.size(),
                    record.MessageOffset))
            return false;
        StoreRecord(header.DiagnosticOffset, 0, record);
        return true;
    }

    const bool m_Materialize;
    std::size_t m_HeaderCount = 0;
    std::size_t m_PayloadSize = 0;
    std::vector<BML_BehaviorRunFrame> m_Headers;
    std::vector<std::uint8_t> m_Payload;
    std::vector<std::uint64_t> m_Sequences;
};

int BML_BEHAVIOR_CALL TakeFrames(
    BML_BehaviorRun run, BML_BehaviorRunFrame *headers,
    std::uint32_t headerCapacity, std::uint32_t headerStride,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outHeaderCount, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !outHeaderCount || !outPayloadSize ||
            (headerCapacity && (!headers || headerStride < sizeof(*headers))) ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        std::shared_ptr<BML::Behavior::FrameStore> store =
            context->BehaviorSessions().Frames(RunId(run));
        if (!store)
            return BML_ERROR_INVALID_HANDLE;

        const std::vector<RunFrame> frames = store->Read();
        FrameBatch measured(false);
        for (const RunFrame &frame : frames) {
            if (!measured.Add(frame))
                return BML_ERROR_OUT_OF_MEMORY;
        }
        if (measured.HeaderCount() > UINT32_MAX ||
            measured.PayloadSize() > UINT32_MAX)
            return BML_ERROR_OUT_OF_MEMORY;
        if (!FitsStrided(measured.HeaderCount(), headerStride,
                         sizeof(BML_BehaviorRunFrame)))
            return BML_ERROR_OUT_OF_MEMORY;
        *outHeaderCount = static_cast<std::uint32_t>(measured.HeaderCount());
        *outPayloadSize = static_cast<std::uint32_t>(measured.PayloadSize());
        if (headerCapacity < measured.HeaderCount() ||
            payloadCapacity < measured.PayloadSize())
            return BML_ERROR_BUFFER_TOO_SMALL;

        FrameBatch batch(true);
        for (const RunFrame &frame : frames) {
            if (!batch.Add(frame))
                return BML_ERROR_OUT_OF_MEMORY;
        }
        if (batch.HeaderCount() != measured.HeaderCount() ||
            batch.PayloadSize() != measured.PayloadSize())
            return BML_ERROR_BUSY;

        auto *headerBytes = reinterpret_cast<std::uint8_t *>(headers);
        for (std::size_t index = 0; index < batch.Headers().size(); ++index)
            std::memcpy(headerBytes + index * headerStride,
                        &batch.Headers()[index], sizeof(batch.Headers()[index]));
        if (!batch.Payload().empty())
            std::memcpy(payload, batch.Payload().data(), batch.Payload().size());
        if (!store->Consume(batch.Sequences()))
            return BML_ERROR_BUSY;
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseRun(BML_BehaviorRun run) {
    return Guard([&] {
        if (!run)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        context->BehaviorSessions().CloseRun(RunId(run));
        return BML_OK;
    });
}

class BehaviorPayload final {
public:
    template <typename T>
    bool Reserve(std::size_t count, std::uint32_t &offset) {
        if (!Align(alignof(T)) || count > UINT32_MAX / sizeof(T))
            return false;
        const std::size_t size = count * sizeof(T);
        if (m_Bytes.size() > UINT32_MAX ||
            size > UINT32_MAX - m_Bytes.size())
            return false;
        offset = static_cast<std::uint32_t>(m_Bytes.size());
        m_Bytes.resize(m_Bytes.size() + size, 0);
        return true;
    }

    template <typename T>
    void Store(std::uint32_t offset, std::size_t index, const T &record) {
        std::memcpy(m_Bytes.data() + offset + index * sizeof(T),
                    &record, sizeof(record));
    }

    bool Text(const std::string &value, BML_BehaviorText &text) {
        if (value.size() > UINT32_MAX || m_Bytes.size() > UINT32_MAX ||
            value.size() > UINT32_MAX - m_Bytes.size())
            return false;
        text.Offset = static_cast<std::uint32_t>(m_Bytes.size());
        text.Length = static_cast<std::uint32_t>(value.size());
        m_Bytes.insert(m_Bytes.end(), value.begin(), value.end());
        return true;
    }

    bool Bytes(const void *value, std::size_t size, std::uint32_t &offset) {
        if ((!value && size) || m_Bytes.size() > UINT32_MAX ||
            size > UINT32_MAX - m_Bytes.size())
            return false;
        offset = static_cast<std::uint32_t>(m_Bytes.size());
        const auto *bytes = static_cast<const std::uint8_t *>(value);
        if (size)
            m_Bytes.insert(m_Bytes.end(), bytes, bytes + size);
        return true;
    }

    bool Value(const void *value, std::size_t size, std::uint32_t &offset) {
        return Align(BML_BEHAVIOR_VALUE_ALIGNMENT) &&
            Bytes(value, size, offset);
    }

    [[nodiscard]] const std::vector<std::uint8_t> &Bytes() const noexcept {
        return m_Bytes;
    }

private:
    bool Align(std::size_t alignment) {
        const std::size_t remainder = m_Bytes.size() % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (m_Bytes.size() > UINT32_MAX ||
            padding > UINT32_MAX - m_Bytes.size())
            return false;
        m_Bytes.insert(m_Bytes.end(), padding, 0);
        return true;
    }

    std::vector<std::uint8_t> m_Bytes;
};

std::uint32_t PublicValueKind(Parameter::Form form) noexcept {
    switch (form) {
    case Parameter::Form::Bool: return BML_BEHAVIOR_VALUE_BOOL;
    case Parameter::Form::Int32: return BML_BEHAVIOR_VALUE_INT32;
    case Parameter::Form::Float32: return BML_BEHAVIOR_VALUE_FLOAT32;
    case Parameter::Form::Utf8: return BML_BEHAVIOR_VALUE_UTF8;
    case Parameter::Form::Vec2: return BML_BEHAVIOR_VALUE_VEC2;
    case Parameter::Form::Vec3: return BML_BEHAVIOR_VALUE_VEC3;
    case Parameter::Form::Quaternion: return BML_BEHAVIOR_VALUE_QUATERNION;
    case Parameter::Form::Euler: return BML_BEHAVIOR_VALUE_EULER;
    case Parameter::Form::Rect: return BML_BEHAVIOR_VALUE_RECT;
    case Parameter::Form::Color: return BML_BEHAVIOR_VALUE_COLOR;
    case Parameter::Form::Box: return BML_BEHAVIOR_VALUE_BOX;
    case Parameter::Form::Mat4: return BML_BEHAVIOR_VALUE_MAT4;
    case Parameter::Form::Object: return BML_BEHAVIOR_VALUE_OBJECT;
    case Parameter::Form::Unsupported: return 0;
    }
    return 0;
}

std::uint32_t PublicSlotKind(SlotKind kind) noexcept {
    switch (kind) {
    case SlotKind::Input: return BML_BEHAVIOR_SLOT_IN;
    case SlotKind::Output: return BML_BEHAVIOR_SLOT_OUT;
    case SlotKind::InputParameter: return BML_BEHAVIOR_SLOT_PIN;
    case SlotKind::OutputParameter: return BML_BEHAVIOR_SLOT_POUT;
    case SlotKind::Setting: return BML_BEHAVIOR_SLOT_SETTING;
    case SlotKind::Local: return BML_BEHAVIOR_SLOT_LOCAL;
    case SlotKind::Target: return BML_BEHAVIOR_SLOT_TARGET;
    }
    return 0;
}

bool AddManagers(const std::vector<ManagerRequirement> &managers,
                 BehaviorPayload &payload, std::uint32_t &offset,
                 std::uint32_t &count) {
    if (managers.size() > UINT32_MAX)
        return false;
    count = static_cast<std::uint32_t>(managers.size());
    if (managers.empty())
        return true;
    if (!payload.Reserve<BML_BehaviorManagerInfo>(managers.size(), offset))
        return false;
    for (std::size_t index = 0; index < managers.size(); ++index) {
        BML_BehaviorManagerInfo manager{};
        manager.StructSize = sizeof(manager);
        manager.Guid = Guid(managers[index].Guid);
        manager.Available = managers[index].Available ? 1u : 0u;
        payload.Store(offset, index, manager);
    }
    return true;
}

bool AddPrototype(const PrototypeInfo &prototype, BehaviorPayload &payload,
                  BML_BehaviorPrototypeInfo &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.Ref.StructSize = sizeof(record.Ref);
    record.Ref.Prototype = Guid(prototype.Ref.Guid);
    record.Ref.Generation = prototype.Ref.Generation;
    record.Provider = Guid(prototype.Provider.Guid);
    record.Version = prototype.Version;
    record.CompatibleClass = prototype.CompatibleClass;
    return payload.Text(prototype.Name, record.Name) &&
           payload.Text(prototype.Category, record.Category) &&
           payload.Text(prototype.Provider.Name, record.ProviderName) &&
           payload.Text(prototype.Author, record.Author) &&
           payload.Text(prototype.Description, record.Description) &&
           AddManagers(prototype.Managers, payload, record.ManagerOffset,
                       record.ManagerCount);
}

bool ReadPrototypeQuery(const BML_BehaviorPrototypeQuery &from,
                        PrototypeQuery &to, Status &status) {
    constexpr std::uint32_t kKnownMatch =
        BML_BEHAVIOR_MATCH_PROTOTYPE | BML_BEHAVIOR_MATCH_NAME |
        BML_BEHAVIOR_MATCH_CATEGORY | BML_BEHAVIOR_MATCH_PROVIDER |
        BML_BEHAVIOR_MATCH_PROVIDER_GUID |
        BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS |
        BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS;
    if (from.StructSize < sizeof(from) || (from.Match & ~kKnownMatch) ||
        from.Reserved != 0) {
        status = InvalidValue("The Behavior Prototype query is malformed.");
        return false;
    }
    to.MatchGuid = (from.Match & BML_BEHAVIOR_MATCH_PROTOTYPE) != 0;
    to.Guid = Guid(from.Prototype);
    to.MatchName = (from.Match & BML_BEHAVIOR_MATCH_NAME) != 0;
    to.MatchCategory = (from.Match & BML_BEHAVIOR_MATCH_CATEGORY) != 0;
    to.MatchProvider = (from.Match & BML_BEHAVIOR_MATCH_PROVIDER) != 0;
    to.MatchProviderGuid =
        (from.Match & BML_BEHAVIOR_MATCH_PROVIDER_GUID) != 0;
    to.ProviderGuid = Guid(from.ProviderGuid);
    to.MatchCompatibleClass =
        (from.Match & BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS) != 0;
    to.CompatibleClass = from.CompatibleClass;
    if ((to.MatchName && !ReadString(from.Name, to.Name)) ||
        (to.MatchCategory && !ReadString(from.Category, to.Category)) ||
        (to.MatchProvider && !ReadString(from.Provider, to.Provider)) ||
        (to.MatchCompatibleClass && from.CompatibleClass <= 0)) {
        status = InvalidValue("A Behavior Prototype query filter is invalid.");
        return false;
    }
    const bool matchManagers =
        (from.Match & BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS) != 0;
    if ((!matchManagers && from.RequiredManagerCount) ||
        (matchManagers && from.RequiredManagerCount &&
         !from.RequiredManagers)) {
        status = InvalidValue(
            "The Behavior Prototype manager filter is invalid.");
        return false;
    }
    if (matchManagers) {
        to.RequiredManagers.reserve(from.RequiredManagerCount);
        for (std::uint32_t index = 0; index < from.RequiredManagerCount; ++index)
            to.RequiredManagers.push_back(Guid(from.RequiredManagers[index]));
    }
    return true;
}

bool AddLayout(const Layout &from, BehaviorPayload &payload,
               BML_BehaviorLayout &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.Origin = from.Origin == BML::Behavior::LayoutOrigin::Declared
        ? BML_BEHAVIOR_LAYOUT_DECLARED : BML_BEHAVIOR_LAYOUT_LIVE;
    record.Prototype.StructSize = sizeof(record.Prototype);
    record.Prototype.Prototype = Guid(from.Prototype);
    record.Prototype.Generation = from.ProviderGeneration;
    record.LayoutGeneration = from.Generation;
    switch (from.Kind) {
    case BML::Behavior::BehaviorKind::Function:
        record.Kind = BML_BEHAVIOR_KIND_FUNCTION;
        break;
    case BML::Behavior::BehaviorKind::Callback:
        record.Kind = BML_BEHAVIOR_KIND_CALLBACK;
        break;
    case BML::Behavior::BehaviorKind::Graph:
        record.Kind = BML_BEHAVIOR_KIND_GRAPH;
        break;
    }
    if (from.MaterializedNow)
        record.Flags |= BML_BEHAVIOR_LAYOUT_MATERIALIZED_NOW;
    record.CompatibleClass = from.CompatibleClass;
    record.PrototypeFlags = from.PrototypeFlags;
    record.BehaviorFlags = from.BehaviorFlags;
    record.TargetType = Guid(from.TargetType);
    if (!payload.Text(from.PrototypeName, record.Name) ||
        !payload.Text(from.Category, record.Category) ||
        !payload.Text(from.ProviderName, record.ProviderName) ||
        !payload.Text(from.Author, record.Author) ||
        !payload.Text(from.Description, record.Description) ||
        !AddManagers(from.Managers, payload, record.ManagerOffset,
                     record.ManagerCount))
        return false;
    if (from.Slots.size() > UINT32_MAX)
        return false;
    record.SlotCount = static_cast<std::uint32_t>(from.Slots.size());
    if (from.Slots.empty())
        return true;
    if (!payload.Reserve<BML_BehaviorSlotRecord>(
            from.Slots.size(), record.SlotOffset))
        return false;
    for (std::size_t index = 0; index < from.Slots.size(); ++index) {
        const BML::Behavior::SlotInfo &slot = from.Slots[index];
        BML_BehaviorSlotRecord output{};
        output.StructSize = sizeof(output);
        output.Kind = PublicSlotKind(slot.Kind);
        if (slot.Dynamic)
            output.Flags |= BML_BEHAVIOR_SLOT_DYNAMIC;
        output.ValueKind = PublicValueKind(slot.ValueForm);
        if (output.ValueKind)
            output.Flags |= BML_BEHAVIOR_SLOT_VALUE_SUPPORTED;
        output.Index = slot.Index;
        output.Occurrence = slot.Occurrence;
        output.Type = Guid(slot.Type);
        if (!payload.Text(slot.Name, output.Name) ||
            !payload.Text(slot.TypeName, output.TypeName))
            return false;
        payload.Store(record.SlotOffset, index, output);
    }
    return true;
}

int BML_BEHAVIOR_CALL FindPrototypes(
    BML_BehaviorSession session, const BML_BehaviorPrototypeQuery *query,
    BML_BehaviorPrototypeInfo *prototypes, std::uint32_t prototypeCapacity,
    std::uint32_t prototypeStride, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPrototypeCount,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !query ||
            !outPrototypeCount || !outPayloadSize ||
            (prototypeCapacity &&
             (!prototypes || prototypeStride < sizeof(*prototypes))) ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        PrototypeQuery requested;
        Status result;
        if (!ReadPrototypeQuery(*query, requested, result)) {
            WriteStatus(status, result);
            return BML_ERROR_INVALID_PARAMETER;
        }
        std::vector<PrototypeInfo> found;
        result = context->BehaviorSessions().FindPrototypes(
            SessionId(session), requested, found);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);

        BehaviorPayload wire;
        std::vector<BML_BehaviorPrototypeInfo> records(found.size());
        for (std::size_t index = 0; index < found.size(); ++index) {
            if (!AddPrototype(found[index], wire, records[index]))
                return BML_ERROR_OUT_OF_MEMORY;
        }
        if (records.size() > UINT32_MAX || wire.Bytes().size() > UINT32_MAX)
            return BML_ERROR_OUT_OF_MEMORY;
        if (!FitsStrided(records.size(), prototypeStride,
                         sizeof(BML_BehaviorPrototypeInfo)))
            return BML_ERROR_OUT_OF_MEMORY;
        *outPrototypeCount = static_cast<std::uint32_t>(records.size());
        *outPayloadSize = static_cast<std::uint32_t>(wire.Bytes().size());
        if (prototypeCapacity < records.size() ||
            payloadCapacity < wire.Bytes().size())
            return BML_ERROR_BUFFER_TOO_SMALL;

        auto *recordBytes = reinterpret_cast<std::uint8_t *>(prototypes);
        for (std::size_t index = 0; index < records.size(); ++index)
            std::memcpy(recordBytes + index * prototypeStride,
                        &records[index], sizeof(records[index]));
        if (!wire.Bytes().empty())
            std::memcpy(payload, wire.Bytes().data(), wire.Bytes().size());
        return BML_OK;
    });
}

int WriteLayoutResult(const Layout &source, BML_BehaviorLayout *layout,
                      void *payload, std::uint32_t payloadCapacity,
                      std::uint32_t *outPayloadSize) {
    BehaviorPayload wire;
    BML_BehaviorLayout record{};
    if (!AddLayout(source, wire, record) || wire.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outPayloadSize = static_cast<std::uint32_t>(wire.Bytes().size());
    if (payloadCapacity < wire.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *layout = record;
    if (!wire.Bytes().empty())
        std::memcpy(payload, wire.Bytes().data(), wire.Bytes().size());
    return BML_OK;
}

int BML_BEHAVIOR_CALL ReadDeclaredLayout(
    BML_BehaviorSession session, const BML_BehaviorPrototypeRef *prototype,
    BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(prototype) ||
            !HasStructSize(layout) || !outPayloadSize ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Layout source;
        Status result = context->BehaviorSessions().ReadDeclaredLayout(
            SessionId(session),
            PrototypeRef{Guid(prototype->Prototype), prototype->Generation},
            source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadLiveLayout(
    BML_BehaviorRun run, BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !HasStructSize(layout) ||
            !outPayloadSize ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Layout source;
        Status result = context->BehaviorSessions().ReadLiveLayout(
            RunId(run), source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

std::uint32_t PublicTruth(Truth value) noexcept {
    switch (value) {
    case Truth::No: return BML_BEHAVIOR_FALSE;
    case Truth::Yes: return BML_BEHAVIOR_TRUE;
    case Truth::Unknown: return BML_BEHAVIOR_UNKNOWN;
    }
    return BML_BEHAVIOR_UNKNOWN;
}

std::uint32_t PublicValueState(ValueState state) noexcept {
    switch (state) {
    case ValueState::Available: return BML_BEHAVIOR_VALUE_AVAILABLE;
    case ValueState::Indeterminate: return BML_BEHAVIOR_VALUE_INDETERMINATE;
    case ValueState::Unsupported: return BML_BEHAVIOR_VALUE_UNSUPPORTED;
    }
    return BML_BEHAVIOR_VALUE_UNSUPPORTED;
}

std::uint32_t PublicRelation(ValueRelation relation) noexcept {
    switch (relation) {
    case ValueRelation::Stored: return BML_BEHAVIOR_VALUE_STORED;
    case ValueRelation::Direct: return BML_BEHAVIOR_VALUE_DIRECT;
    case ValueRelation::Shared: return BML_BEHAVIOR_VALUE_SHARED;
    case ValueRelation::Operation: return BML_BEHAVIOR_VALUE_OPERATION;
    }
    return BML_BEHAVIOR_VALUE_STORED;
}

std::uint32_t PublicWatchKind(WatchKind kind) noexcept {
    switch (kind) {
    case WatchKind::GraphChanged: return BML_BEHAVIOR_WATCH_GRAPH;
    case WatchKind::LayoutChanged: return BML_BEHAVIOR_WATCH_LAYOUT;
    case WatchKind::SampledValueChanged:
        return BML_BEHAVIOR_WATCH_SAMPLED_VALUE;
    }
    return 0;
}

std::uint32_t PublicWatchState(BML::Behavior::WatchState state) noexcept {
    switch (state) {
    case BML::Behavior::WatchState::Active:
        return BML_BEHAVIOR_WATCH_ACTIVE;
    case BML::Behavior::WatchState::Failed:
        return BML_BEHAVIOR_WATCH_FAILED;
    }
    return BML_BEHAVIOR_WATCH_FAILED;
}

void WriteWatchInfo(BML_BehaviorWatchInfo *out,
                    const BML::Behavior::WatchInfo &info) noexcept {
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicWatchState(info.State);
    out->Diagnostic.StructSize = sizeof(out->Diagnostic);
    WriteStatus(&out->Diagnostic, info.Diagnostic);
}

bool AddGraph(const GraphModel &source, BehaviorPayload &payload,
              BML_BehaviorGraph &graph) {
    graph = {};
    graph.StructSize = sizeof(graph);
    graph.View = source.View == GraphView::Logical
        ? BML_BEHAVIOR_GRAPH_LOGICAL : BML_BEHAVIOR_GRAPH_LIVE;
    graph.Root = {source.Root.Domain, source.Root.Slot, source.Root.Generation};
    graph.Generation = source.Generation;
    graph.Fingerprint = source.Fingerprint;
    if (source.Nodes.size() > UINT32_MAX || source.Links.size() > UINT32_MAX)
        return false;
    graph.NodeCount = static_cast<std::uint32_t>(source.Nodes.size());
    graph.LinkCount = static_cast<std::uint32_t>(source.Links.size());
    if (!source.Nodes.empty() &&
        !payload.Reserve<BML_BehaviorGraphNode>(
            source.Nodes.size(), graph.NodeOffset))
        return false;
    if (!source.Links.empty() &&
        !payload.Reserve<BML_BehaviorGraphLink>(
            source.Links.size(), graph.LinkOffset))
        return false;

    for (std::size_t index = 0; index < source.Nodes.size(); ++index) {
        const auto &node = source.Nodes[index];
        BML_BehaviorGraphNode record{};
        record.StructSize = sizeof(record);
        record.Id = node.Id;
        record.Object = {node.Object.Domain, node.Object.Slot,
                         node.Object.Generation};
        record.Parent = node.Parent;
        record.Prototype = Guid(node.Prototype);
        record.Priority = node.Priority;
        record.Active = node.Active ? 1u : 0u;
        if (!payload.Text(node.Name, record.Name) ||
            node.Ports.size() > UINT32_MAX)
            return false;
        record.PortCount = static_cast<std::uint32_t>(node.Ports.size());
        if (!node.Ports.empty() &&
            !payload.Reserve<BML_BehaviorGraphPort>(
                node.Ports.size(), record.PortOffset))
            return false;
        for (std::size_t portIndex = 0;
             portIndex < node.Ports.size(); ++portIndex) {
            const auto &port = node.Ports[portIndex];
            BML_BehaviorGraphPort portRecord{};
            portRecord.StructSize = sizeof(portRecord);
            portRecord.Node = node.Id;
            portRecord.Kind = PublicSlotKind(port.Kind);
            portRecord.Index = port.Index;
            portRecord.Occurrence = port.Occurrence;
            portRecord.Active = port.Active ? 1u : 0u;
            if (!payload.Text(port.Name, portRecord.Name))
                return false;
            payload.Store(record.PortOffset, portIndex, portRecord);
        }
        payload.Store(graph.NodeOffset, index, record);
    }

    for (std::size_t index = 0; index < source.Links.size(); ++index) {
        const auto &link = source.Links[index];
        BML_BehaviorGraphLink record{};
        record.StructSize = sizeof(record);
        record.Id = link.Id;
        record.Object = {link.Object.Domain, link.Object.Slot,
                         link.Object.Generation};
        record.SourceNode = link.Source.Node;
        record.SourceKind = PublicSlotKind(link.Source.Kind);
        record.SourceIndex = link.Source.Index;
        record.TargetNode = link.Target.Node;
        record.TargetKind = PublicSlotKind(link.Target.Kind);
        record.TargetIndex = link.Target.Index;
        record.InitialDelay = link.InitialDelay;
        record.RemainingDelay = link.RemainingDelay;
        record.Pending = PublicTruth(link.Pending);
        payload.Store(graph.LinkOffset, index, record);
    }
    return true;
}

int WriteGraphResult(const GraphModel &source, BML_BehaviorGraph *graph,
                     void *payload, std::uint32_t payloadCapacity,
                     std::uint32_t *outPayloadSize) {
    BehaviorPayload bytes;
    BML_BehaviorGraph wire{};
    if (!AddGraph(source, bytes, wire) || bytes.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outPayloadSize = static_cast<std::uint32_t>(bytes.Bytes().size());
    if (payloadCapacity < bytes.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *graph = wire;
    if (!bytes.Bytes().empty())
        std::memcpy(payload, bytes.Bytes().data(), bytes.Bytes().size());
    return BML_OK;
}

bool AddGraphValue(const GraphValue &source, BehaviorPayload &payload,
                   BML_BehaviorGraphValue &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.State = PublicValueState(source.State);
    record.Relation = PublicRelation(source.Relation);
    record.Type = Guid(source.Type);
    if (source.State != ValueState::Available)
        return true;
    record.Kind = PublicValueKind(source.Form);
    if (!record.Kind)
        return false;

    std::uint8_t bytes[64]{};
    std::size_t size = 0;
    const auto storeFloat = [&](std::size_t index, float value) {
        BML::Imc::Wire::Detail::Store32(
            bytes + index * 4, std::bit_cast<std::uint32_t>(value));
    };
    switch (source.Form) {
    case Parameter::Form::Bool: {
        const bool *value = std::get_if<bool>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(bytes, *value ? 1u : 0u);
        size = 4;
        break;
    }
    case Parameter::Form::Int32: {
        const auto *value = std::get_if<std::int32_t>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(
            bytes, static_cast<std::uint32_t>(*value));
        size = 4;
        break;
    }
    case Parameter::Form::Float32: {
        const float *value = std::get_if<float>(&source.Data);
        if (!value)
            return false;
        storeFloat(0, *value);
        size = 4;
        break;
    }
    case Parameter::Form::Utf8: {
        const std::string *value = std::get_if<std::string>(&source.Data);
        if (!value || value->size() > UINT32_MAX)
            return false;
        record.ValueSize = static_cast<std::uint32_t>(value->size());
        return payload.Value(value->data(), value->size(), record.ValueOffset);
    }
    case Parameter::Form::Object: {
        const auto *value = std::get_if<BML::Behavior::ObjectRef>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(bytes, value->Domain);
        BML::Imc::Wire::Detail::Store32(bytes + 4, value->Slot);
        BML::Imc::Wire::Detail::Store32(bytes + 8, value->Generation);
        size = 12;
        break;
    }
    case Parameter::Form::Vec2: {
        const auto *value = std::get_if<std::array<float, 2>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 8;
        break;
    }
    case Parameter::Form::Vec3:
    case Parameter::Form::Euler: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 12;
        break;
    }
    case Parameter::Form::Quaternion:
    case Parameter::Form::Rect:
    case Parameter::Form::Color: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 16;
        break;
    }
    case Parameter::Form::Box: {
        const auto *value = std::get_if<std::array<float, 6>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 24;
        break;
    }
    case Parameter::Form::Mat4: {
        const auto *value = std::get_if<std::array<float, 16>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 64;
        break;
    }
    case Parameter::Form::Unsupported:
        return false;
    }
    record.ValueSize = static_cast<std::uint32_t>(size);
    return payload.Value(bytes, size, record.ValueOffset);
}

int BML_BEHAVIOR_CALL Inspect(
    BML_BehaviorSession session, BML_ObjectRef root, std::uint32_t view,
    BML_BehaviorGraph *graph, void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(graph) ||
            !outPayloadSize ||
            (payloadCapacity && !payload) ||
            (view != BML_BEHAVIOR_GRAPH_LOGICAL &&
             view != BML_BEHAVIOR_GRAPH_LIVE))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Status result;
        CKBehavior *native = ReadBehavior(root, *context, result);
        if (!native) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        GraphModel source;
        result = context->BehaviorSessions().ReadGraph(
            SessionId(session), native,
            view == BML_BEHAVIOR_GRAPH_LOGICAL
                ? GraphView::Logical : GraphView::Live,
            source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        return WriteGraphResult(source, graph, payload, payloadCapacity,
                                outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL InspectRun(
    BML_BehaviorRun run, std::uint32_t view, BML_BehaviorGraph *graph,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outPayloadSize, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !HasStructSize(graph) ||
            !outPayloadSize ||
            (payloadCapacity && !payload) ||
            (view != BML_BEHAVIOR_GRAPH_LOGICAL &&
             view != BML_BEHAVIOR_GRAPH_LIVE))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        GraphModel source;
        Status result = context->BehaviorSessions().ReadGraph(
            RunId(run), view == BML_BEHAVIOR_GRAPH_LOGICAL
                ? GraphView::Logical : GraphView::Live,
            source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        return WriteGraphResult(source, graph, payload, payloadCapacity,
                                outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadNodeLayout(
    BML_BehaviorSession session, BML_ObjectRef node,
    BML_BehaviorLayout *layout, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(layout) ||
            !outPayloadSize ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Status result;
        CKBehavior *native = ReadBehavior(node, *context, result);
        if (!native) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        Layout source;
        result = context->BehaviorSessions().ReadNodeLayout(
            SessionId(session), native, source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        return WriteLayoutResult(source, layout, payload, payloadCapacity,
                                 outPayloadSize);
    });
}

int BML_BEHAVIOR_CALL ReadGraphValue(
    BML_BehaviorSession session, BML_ObjectRef node, std::uint32_t slotKind,
    const BML_BehaviorSelector *slot, std::uint32_t read,
    BML_BehaviorGraphValue *value, void *payload,
    std::uint32_t payloadCapacity, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !slot ||
            !HasStructSize(value) || !outPayloadSize ||
            (payloadCapacity && !payload) ||
            read != BML_BEHAVIOR_READ_NON_FORCING)
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        SlotKind nativeKind;
        switch (slotKind) {
        case BML_BEHAVIOR_SLOT_PIN: nativeKind = SlotKind::InputParameter; break;
        case BML_BEHAVIOR_SLOT_POUT: nativeKind = SlotKind::OutputParameter; break;
        case BML_BEHAVIOR_SLOT_SETTING: nativeKind = SlotKind::Setting; break;
        case BML_BEHAVIOR_SLOT_LOCAL: nativeKind = SlotKind::Local; break;
        case BML_BEHAVIOR_SLOT_TARGET: nativeKind = SlotKind::Target; break;
        default: return BML_ERROR_INVALID_PARAMETER;
        }
        Status result;
        Slot selector;
        if (!ReadSelector(*slot, nativeKind, CKGUID(), selector, result)) {
            WriteStatus(status, result);
            return BML_ERROR_INVALID_PARAMETER;
        }
        CKBehavior *native = ReadBehavior(node, *context, result);
        if (!native) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        GraphValue source;
        result = context->BehaviorSessions().ReadGraphValue(
            SessionId(session), native, selector, ReadMode::NonForcing,
            source);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        BehaviorPayload bytes;
        BML_BehaviorGraphValue wire{};
        if (!AddGraphValue(source, bytes, wire) ||
            bytes.Bytes().size() > UINT32_MAX)
            return BML_ERROR_OUT_OF_MEMORY;
        *outPayloadSize = static_cast<std::uint32_t>(bytes.Bytes().size());
        if (payloadCapacity < bytes.Bytes().size())
            return BML_ERROR_BUFFER_TOO_SMALL;
        *value = wire;
        if (!bytes.Bytes().empty())
            std::memcpy(payload, bytes.Bytes().data(), bytes.Bytes().size());
        return BML_OK;
    });
}

bool WriteWatchValue(const GraphValue &source,
                     BML_BehaviorWatchValue &out) {
    out = {};
    out.StructSize = sizeof(out);
    out.State = PublicValueState(source.State);
    out.Relation = PublicRelation(source.Relation);
    out.Value.StructSize = sizeof(out.Value);
    out.Value.Type = Guid(source.Type);
    if (source.State != ValueState::Available)
        return true;
    out.Value.Kind = PublicValueKind(source.Form);
    switch (source.Form) {
    case Parameter::Form::Bool: {
        const bool *value = std::get_if<bool>(&source.Data);
        if (value) out.Value.Data.Bool = *value ? 1u : 0u;
        return value != nullptr;
    }
    case Parameter::Form::Int32: {
        const auto *value = std::get_if<std::int32_t>(&source.Data);
        if (value) out.Value.Data.Int32 = *value;
        return value != nullptr;
    }
    case Parameter::Form::Float32: {
        const float *value = std::get_if<float>(&source.Data);
        if (value) out.Value.Data.Float32 = *value;
        return value != nullptr;
    }
    case Parameter::Form::Utf8: {
        const auto *value = std::get_if<std::string>(&source.Data);
        if (value) out.Value.Data.Utf8 = {value->data(),
            static_cast<std::uint32_t>(value->size())};
        return value != nullptr && value->size() <= UINT32_MAX;
    }
    case Parameter::Form::Object: {
        const auto *value = std::get_if<BML::Behavior::ObjectRef>(&source.Data);
        if (value) out.Value.Data.Object = {
            value->Domain, value->Slot, value->Generation};
        return value != nullptr;
    }
    case Parameter::Form::Vec2: {
        const auto *value = std::get_if<std::array<float, 2>>(&source.Data);
        if (value) out.Value.Data.Vec2 = {(*value)[0], (*value)[1]};
        return value != nullptr;
    }
    case Parameter::Form::Vec3: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (value) out.Value.Data.Vec3 = {(*value)[0], (*value)[1], (*value)[2]};
        return value != nullptr;
    }
    case Parameter::Form::Euler: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (value) out.Value.Data.Euler = {(*value)[0], (*value)[1], (*value)[2]};
        return value != nullptr;
    }
    case Parameter::Form::Quaternion: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Quaternion = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Rect: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Rect = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Color: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Color = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Box: {
        const auto *value = std::get_if<std::array<float, 6>>(&source.Data);
        if (value) out.Value.Data.Box = {
            {(*value)[0], (*value)[1], (*value)[2]},
            {(*value)[3], (*value)[4], (*value)[5]}};
        return value != nullptr;
    }
    case Parameter::Form::Mat4: {
        const auto *value = std::get_if<std::array<float, 16>>(&source.Data);
        if (value)
            std::memcpy(&out.Value.Data.Mat4, value->data(), sizeof(BML_Mat4));
        return value != nullptr;
    }
    case Parameter::Form::Unsupported:
        return false;
    }
    return false;
}

int BML_BEHAVIOR_CALL OpenWatch(
    BML_BehaviorSession session, const BML_BehaviorWatchSpec *source,
    const BML_BehaviorWatchFunction *callback,
    BML_BehaviorWatch *outWatch, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(source) ||
            !HasStructSize(callback) || !outWatch || !callback->Invoke ||
            (!!callback->Retain != !!callback->Release))
            return BML_ERROR_INVALID_PARAMETER;
        *outWatch = nullptr;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;

        WatchSpec spec;
        switch (source->Kind) {
        case BML_BEHAVIOR_WATCH_GRAPH:
            spec.Kind = WatchKind::GraphChanged;
            break;
        case BML_BEHAVIOR_WATCH_LAYOUT:
            spec.Kind = WatchKind::LayoutChanged;
            break;
        case BML_BEHAVIOR_WATCH_SAMPLED_VALUE:
            spec.Kind = WatchKind::SampledValueChanged;
            break;
        default:
            return BML_ERROR_INVALID_PARAMETER;
        }
        if (source->View == BML_BEHAVIOR_GRAPH_LOGICAL)
            spec.View = GraphView::Logical;
        else if (source->View == BML_BEHAVIOR_GRAPH_LIVE)
            spec.View = GraphView::Live;
        else
            return BML_ERROR_INVALID_PARAMETER;
        if (source->Read != BML_BEHAVIOR_READ_NON_FORCING)
            return BML_ERROR_INVALID_PARAMETER;

        Status result;
        CKBehavior *root = nullptr;
        CKBehavior *node = nullptr;
        if (spec.Kind == WatchKind::GraphChanged) {
            root = ReadBehavior(source->Root, *context, result);
            if (!root) {
                WriteStatus(status, result);
                return ResultCode(result);
            }
        } else {
            node = ReadBehavior(source->Node, *context, result);
            if (!node) {
                WriteStatus(status, result);
                return ResultCode(result);
            }
        }
        if (spec.Kind == WatchKind::SampledValueChanged) {
            SlotKind kind;
            switch (source->SlotKind) {
            case BML_BEHAVIOR_SLOT_PIN: kind = SlotKind::InputParameter; break;
            case BML_BEHAVIOR_SLOT_POUT: kind = SlotKind::OutputParameter; break;
            case BML_BEHAVIOR_SLOT_SETTING: kind = SlotKind::Setting; break;
            case BML_BEHAVIOR_SLOT_LOCAL: kind = SlotKind::Local; break;
            case BML_BEHAVIOR_SLOT_TARGET: kind = SlotKind::Target; break;
            default: return BML_ERROR_INVALID_PARAMETER;
            }
            if (!ReadSelector(source->Slot, kind, CKGUID(),
                              spec.ValueSlot, result)) {
                WriteStatus(status, result);
                return BML_ERROR_INVALID_PARAMETER;
            }
        }

        const BML_BehaviorWatchFunction function = *callback;
        BML::Behavior::PlanCallbackState state = callback->Retain
            ? BML::Behavior::PlanCallbackState::Retained(
                  callback->State, callback->Retain, callback->Release)
            : BML::Behavior::PlanCallbackState::Static(callback->State);
        std::uintptr_t id = 0;
        result = context->BehaviorSessions().OpenWatch(
            SessionId(session), root, node, std::move(spec), std::move(state),
            [context, function](const WatchEvent &event) {
                BML_BehaviorWatchEvent wire{};
                wire.StructSize = sizeof(wire);
                wire.Kind = PublicWatchKind(event.Kind);
                wire.Sequence = event.Sequence;
                wire.Frame = event.Frame;
                wire.Before = event.Before;
                wire.After = event.After;
                if (!WriteWatchValue(event.PreviousValue,
                                     wire.PreviousValue) ||
                    !WriteWatchValue(event.CurrentValue,
                                     wire.CurrentValue))
                    throw std::runtime_error(
                        "A Behavior Watch value could not be represented.");
                auto invocation = context->LockModInvocation();
                int callbackResult = BML_BEHAVIOR_WATCH_ERROR;
                try {
                    callbackResult = function.Invoke(function.State, &wire);
                } catch (...) {
                    throw std::runtime_error(
                        "A Behavior Watch callback threw an exception.");
                }
                if (callbackResult != BML_BEHAVIOR_WATCH_OK) {
                    throw std::runtime_error(
                        "A Behavior Watch callback reported an error.");
                }
            }, id);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outWatch = WatchHandle(id);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseWatch(BML_BehaviorWatch watch) {
    return Guard([&] {
        if (!watch)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        context->BehaviorSessions().CloseWatch(WatchId(watch));
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadWatch(BML_BehaviorWatch watch,
                                BML_BehaviorWatchInfo *info,
                                BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !watch || !HasStructSize(info))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        BML::Behavior::WatchInfo current;
        const Status result = context->BehaviorSessions().ReadWatch(
            WatchId(watch), current);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WriteWatchInfo(info, current);
        return BML_OK;
    });
}

std::uint32_t PublicPlanState(PlanState state) noexcept {
    switch (state) {
    case PlanState::Reconciling: return BML_BEHAVIOR_PLAN_RECONCILING;
    case PlanState::Active: return BML_BEHAVIOR_PLAN_ACTIVE;
    case PlanState::Unsatisfied: return BML_BEHAVIOR_PLAN_UNSATISFIED;
    case PlanState::Conflicted: return BML_BEHAVIOR_PLAN_CONFLICTED;
    case PlanState::Retiring: return BML_BEHAVIOR_PLAN_RETIRING;
    }
    return BML_BEHAVIOR_PLAN_UNSATISFIED;
}

std::uint32_t Count(std::size_t value) noexcept {
    return value > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(value);
}

void WritePlanInfo(BML_BehaviorPlanInfo *out, const PlanInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicPlanState(info.State);
    out->World = info.World;
    out->Matches = Count(info.Matches);
    out->Installations = Count(info.Installations);
    out->Diagnostic.StructSize = sizeof(out->Diagnostic);
    WriteStatus(&out->Diagnostic, info.Diagnostic);
}

std::uint32_t PublicPatchState(PatchState state) noexcept {
    switch (state) {
    case PatchState::Pending: return BML_BEHAVIOR_PATCH_PENDING;
    case PatchState::Active: return BML_BEHAVIOR_PATCH_ACTIVE;
    case PatchState::Closing: return BML_BEHAVIOR_PATCH_CLOSING;
    case PatchState::Conflicted: return BML_BEHAVIOR_PATCH_CONFLICTED;
    case PatchState::Closed: return BML_BEHAVIOR_PATCH_CLOSED;
    case PatchState::Failed: return BML_BEHAVIOR_PATCH_FAILED;
    }
    return BML_BEHAVIOR_PATCH_FAILED;
}

void WritePatchInfo(BML_BehaviorPatchInfo *out,
                    const PatchInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicPatchState(info.State);
    out->Conflicts = Count(info.Conflicts.size());
    out->Diagnostic.StructSize = sizeof(out->Diagnostic);
    WriteStatus(&out->Diagnostic, info.Diagnostic);
}

bool ReadSlotKind(std::uint32_t kind, SlotKind &out) noexcept {
    switch (kind) {
    case BML_BEHAVIOR_SLOT_IN: out = SlotKind::Input; return true;
    case BML_BEHAVIOR_SLOT_OUT: out = SlotKind::Output; return true;
    case BML_BEHAVIOR_SLOT_PIN: out = SlotKind::InputParameter; return true;
    case BML_BEHAVIOR_SLOT_POUT: out = SlotKind::OutputParameter; return true;
    case BML_BEHAVIOR_SLOT_SETTING: out = SlotKind::Setting; return true;
    case BML_BEHAVIOR_SLOT_LOCAL: out = SlotKind::Local; return true;
    case BML_BEHAVIOR_SLOT_TARGET: out = SlotKind::Target; return true;
    default: return false;
    }
}

bool ReadLiveSlot(const BML_BehaviorSlotRef &from, Slot &slot,
                  Status &status) {
    SlotKind kind;
    if (!HasStructSize(&from) || !ReadSlotKind(from.Kind, kind)) {
        status = InvalidValue("A live Behavior slot has an invalid kind or StructSize.");
        return false;
    }
    if (from.Slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
        from.LayoutGeneration == 0) {
        status = InvalidValue(
            "An indexed live Behavior slot requires its Layout generation.");
        return false;
    }
    return ReadSelector(from.Slot, kind, Guid(from.Type), slot, status);
}

int BML_BEHAVIOR_CALL Set(
    BML_BehaviorRun run, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValue *value, std::uint64_t *outLayoutGeneration,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !HasStructSize(slot) ||
            !HasStructSize(value) || !outLayoutGeneration)
            return BML_ERROR_INVALID_PARAMETER;
        *outLayoutGeneration = 0;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Slot target;
        Status result;
        Parameter::Binding binding;
        if (!ReadLiveSlot(*slot, target, result) ||
            !ReadValue(*value, *context, binding, result)) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        result = context->BehaviorSessions().Set(
            RunId(run), slot->LayoutGeneration, target, binding,
            *outLayoutGeneration);
        WriteStatus(status, result);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Bind(
    BML_BehaviorRun run, const BML_BehaviorSlotRef *slot,
    const BML_BehaviorValueRef *source, std::uint32_t relation,
    std::uint64_t *outLayoutGeneration, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !HasStructSize(slot) ||
            !HasStructSize(source) || !outLayoutGeneration)
            return BML_ERROR_INVALID_PARAMETER;
        *outLayoutGeneration = 0;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Slot targetSlot;
        Slot sourceSlot;
        SlotKind sourceKind;
        Status result;
        if (!ReadLiveSlot(*slot, targetSlot, result) ||
            !ReadSlotKind(source->Kind, sourceKind) ||
            !ReadSelector(source->Slot, sourceKind, CKGUID(),
                          sourceSlot, result)) {
            if (result)
                result = InvalidValue("A Behavior Bind source is invalid.");
            WriteStatus(status, result);
            return ResultCode(result);
        }
        Parameter::BindingKind nativeRelation;
        if (relation == BML_BEHAVIOR_VALUE_DIRECT)
            nativeRelation = Parameter::BindingKind::Direct;
        else if (relation == BML_BEHAVIOR_VALUE_SHARED)
            nativeRelation = Parameter::BindingKind::Shared;
        else {
            result = InvalidValue(
                "Behavior Bind accepts direct or shared source semantics.");
            WriteStatus(status, result);
            return ResultCode(result);
        }
        CKBehavior *native = ReadBehavior(source->Node, *context, result);
        if (!native) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        result = context->BehaviorSessions().Bind(
            RunId(run), slot->LayoutGeneration, targetSlot, native,
            sourceSlot, nativeRelation, *outLayoutGeneration);
        WriteStatus(status, result);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Configure(
    BML_BehaviorRun run, const BML_BehaviorSettingStage *stages,
    std::uint32_t stageCount, std::uint64_t *outLayoutGeneration,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !run || !stages || stageCount == 0 ||
            !outLayoutGeneration)
            return BML_ERROR_INVALID_PARAMETER;
        *outLayoutGeneration = 0;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        Spec settings;
        Status result;
        for (std::uint32_t index = 0; index < stageCount; ++index) {
            const BML_BehaviorSettingStage &stage = stages[index];
            if (!HasStructSize(&stage)) {
                result = InvalidValue(
                    "A Behavior Setting stage has an unsupported StructSize.");
                WriteStatus(status, result);
                return ResultCode(result);
            }
            if (index)
                settings.RefreshLayout();
            if (!ReadBindings(stage.Settings, stage.SettingCount,
                              SlotKind::Setting, *context, settings,
                              result)) {
                WriteStatus(status, result);
                return ResultCode(result);
            }
        }
        result = context->BehaviorSessions().Configure(
            RunId(run), settings, *outLayoutGeneration);
        WriteStatus(status, result);
        return ResultCode(result);
    });
}

// The record a Hook Block hands back to InvokeHook. The callback state owns it,
// so it outlives the Hook occurrence and every Binding taken from that
// occurrence, including Bindings a Conflicted Plan can no longer revert.
struct HookThunk {
    ~HookThunk() {
        if (Function.Release)
            Function.Release(Function.State);
    }

    BML_BehaviorHookFunction Function{};
};

int InvokeHook(const CKBehaviorContext *native, void *argument) {
    const auto *thunk = static_cast<const HookThunk *>(argument);
    if (!thunk || !thunk->Function.Invoke || !native || !native->Behavior)
        return CKBR_BEHAVIORERROR;
    ModContext *context = BML_GetModContext();
    if (!context)
        return CKBR_BEHAVIORERROR;

    CKBehavior *block = native->Behavior;
    BML_BehaviorHookContext wire{};
    wire.StructSize = sizeof(wire);
    wire.DeltaTime = native->DeltaTime;
    wire.Block = context->ObjectRefs().Issue(block);
    wire.Script = context->ObjectRefs().Issue(block->GetOwnerScript());
    wire.Owner = context->ObjectRefs().Issue(block->GetOwner());

    int result;
    {
        auto invocation = context->LockModInvocation();
        result = thunk->Function.Invoke(thunk->Function.State, &wire);
    }
    switch (result) {
    case BML_BEHAVIOR_HOOK_OK: return CKBR_OK;
    case BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME: return CKBR_ACTIVATENEXTFRAME;
    default: return CKBR_BEHAVIORERROR;
    }
}

enum class EditHandleKind {
    Node,
    Link,
    Path,
    Port,
};

struct EditHandle {
    EditHandleKind Kind = EditHandleKind::Node;
    Node NodeValue;
    Link LinkValue;
    PathRef PathValue;
    Port PortValue;
};

// Translates the shared wire edit program into symbolic graph intent. The same
// value can be applied once to a known graph or retained by a durable Plan.
class EditProgram final {
public:
    Status Build(const BML_BehaviorEditStep *steps, std::uint32_t count,
                 ModContext &context, GraphEdit &edit);

private:
    static bool Defines(std::uint32_t kind) noexcept;

    Status Step(const BML_BehaviorEditStep &step, ModContext &context,
                GraphEdit &edit);
    Status Use(std::uint32_t id, EditHandleKind kind,
               const EditHandle *&out) const;
    Status ReadPort(const BML_BehaviorPortRef &from, Port &out) const;
    Status ReadHook(const BML_BehaviorHookFunction *from,
                    HookBlock::Hook &out) const;
    Status ReadOrdering(const BML_BehaviorEditStep &step,
                        std::vector<Order> &out) const;

    std::map<std::uint32_t, EditHandle> m_Handles;
};

bool EditProgram::Defines(std::uint32_t kind) noexcept {
    switch (kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE:
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK:
    case BML_BEHAVIOR_EDIT_FOLLOW:
    case BML_BEHAVIOR_EDIT_ADD_BLOCK:
    case BML_BEHAVIOR_EDIT_APPEND_SLOT:
        return true;
    default:
        return false;
    }
}

Status EditProgram::Build(const BML_BehaviorEditStep *steps,
                          std::uint32_t count, ModContext &context,
                          GraphEdit &edit) {
    EditHandle graph;
    graph.NodeValue = edit.Graph();
    m_Handles.clear();
    m_Handles.emplace(BML_BEHAVIOR_EDIT_GRAPH, graph);
    for (std::uint32_t index = 0; index < count; ++index) {
        const Status status = Step(steps[index], context, edit);
        if (!status)
            return status;
    }
    return {};
}

Status EditProgram::Step(const BML_BehaviorEditStep &step,
                         ModContext &context, GraphEdit &edit) {
    if (step.StructSize < sizeof(step))
        return InvalidValue("A Behavior edit step has an unsupported StructSize.");
    if (Defines(step.Kind)) {
        if (step.Result == 0 || step.Result == BML_BEHAVIOR_EDIT_GRAPH ||
            m_Handles.find(step.Result) != m_Handles.end()) {
            return InvalidValue(
                "A Behavior edit step handle is missing or already defined.");
        }
    } else if (step.Result != 0) {
        return InvalidValue("This kind of Behavior edit step defines no handle.");
    }

    EditHandle defined;
    Status status;
    Port source;
    Port sink;
    switch (step.Kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE: {
        std::string name;
        if (!ReadString(step.Name, name))
            return InvalidValue("A Behavior node name is not valid UTF-8 text.");
        NodeQuery query{std::move(name), Guid(step.Prototype)};
        if (!query) {
            return InvalidValue(
                "A required Behavior node needs a name or a Prototype.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.RequireOne(std::move(query));
        break;
    }
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        std::optional<int> delay;
        if (step.Flags & BML_BEHAVIOR_EDIT_HAS_DELAY)
            delay = step.Delay;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.RequireOne(source, sink, delay);
        break;
    }
    case BML_BEHAVIOR_EDIT_FOLLOW:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Path;
        defined.PathValue = edit.Follow(source);
        break;
    case BML_BEHAVIOR_EDIT_ADD_BLOCK: {
        const CKGUID prototype = Guid(step.Prototype);
        if (!prototype.IsValid())
            return InvalidValue("An added Behavior Block needs a Prototype.");
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Add(prototype);
        break;
    }
    case BML_BEHAVIOR_EDIT_APPEND_SLOT: {
        const EditHandle *owner = nullptr;
        if (status = Use(step.Target, EditHandleKind::Node, owner); !status)
            return status;
        std::string name;
        if (!ReadString(step.Name, name) || name.empty())
            return InvalidValue("An appended Behavior slot needs a name.");
        const CKGUID type = Guid(step.Type);
        const bool typed = step.SlotKind == BML_BEHAVIOR_SLOT_PIN ||
                           step.SlotKind == BML_BEHAVIOR_SLOT_POUT ||
                           step.SlotKind == BML_BEHAVIOR_SLOT_LOCAL;
        if (typed && !type.IsValid()) {
            return InvalidValue(
                "An appended Behavior Pin, Pout, or Local needs a parameter type.");
        }
        defined.Kind = EditHandleKind::Port;
        switch (step.SlotKind) {
        case BML_BEHAVIOR_SLOT_IN:
            defined.PortValue = edit.AppendIn(owner->NodeValue, std::move(name));
            break;
        case BML_BEHAVIOR_SLOT_OUT:
            defined.PortValue = edit.AppendOut(owner->NodeValue, std::move(name));
            break;
        case BML_BEHAVIOR_SLOT_PIN:
            defined.PortValue =
                edit.AppendPin(owner->NodeValue, std::move(name), type);
            break;
        case BML_BEHAVIOR_SLOT_POUT:
            defined.PortValue =
                edit.AppendPout(owner->NodeValue, std::move(name), type);
            break;
        case BML_BEHAVIOR_SLOT_LOCAL:
            defined.PortValue =
                edit.AppendLocal(owner->NodeValue, std::move(name), type);
            break;
        default:
            return InvalidValue(
                "A Behavior edit can only append an In, Out, Pin, Pout, or Local.");
        }
        break;
    }
    case BML_BEHAVIOR_EDIT_FLOW:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        edit.Flow(source, sink, step.Delay,
                  (step.Flags & BML_BEHAVIOR_EDIT_CONFIRM_CYCLE)
                      ? Cycle::Confirmed : Cycle::Reject);
        break;
    case BML_BEHAVIOR_EDIT_BIND_VALUE: {
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        Parameter::Binding binding;
        if (!ReadValue(step.Value, context, binding, status))
            return status;
        if (binding.Kind() != Parameter::BindingKind::Value) {
            return {Error::WorldBoundValue, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "A symbolic Behavior edit cannot bind a live object."};
        }
        edit.Bind(sink, binding.Literal());
        break;
    }
    case BML_BEHAVIOR_EDIT_BIND_PORT:
    case BML_BEHAVIOR_EDIT_SHARE:
    case BML_BEHAVIOR_EDIT_PUSH:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (step.Kind == BML_BEHAVIOR_EDIT_BIND_PORT)
            edit.Bind(sink, source);
        else if (step.Kind == BML_BEHAVIOR_EDIT_SHARE)
            edit.Share(sink, source);
        else
            edit.Push(source, sink);
        break;
    case BML_BEHAVIOR_EDIT_TAP: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.Tap(source, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_AFTER: {
        const EditHandle *path = nullptr;
        if (status = Use(step.Target, EditHandleKind::Path, path); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.After(path->PathValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_SPLICE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        if (step.Node) {
            const EditHandle *block = nullptr;
            if (status = Use(step.Node, EditHandleKind::Node, block); !status)
                return status;
            edit.Splice(link->LinkValue, block->NodeValue, std::move(ordering));
            break;
        }
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (status = ReadPort(step.Source, source); !status)
            return status;
        edit.Splice(link->LinkValue, sink, source, std::move(ordering));
        break;
    }
    default:
        return InvalidValue("A Behavior edit step names an unknown operation.");
    }

    if (Defines(step.Kind))
        m_Handles.emplace(step.Result, defined);
    return {};
}

Status EditProgram::Use(std::uint32_t id, EditHandleKind kind,
                        const EditHandle *&out) const {
    const auto found = m_Handles.find(id);
    if (found == m_Handles.end()) {
        return InvalidValue(
            "A Behavior edit step read a handle no earlier step defined.");
    }
    if (found->second.Kind != kind)
        return InvalidValue("A Behavior edit step read a handle of another kind.");
    out = &found->second;
    return {};
}

Status EditProgram::ReadPort(const BML_BehaviorPortRef &from, Port &out) const {
    const EditHandle *handle = nullptr;
    if (from.Kind == 0) {
        const Status status = Use(from.Handle, EditHandleKind::Port, handle);
        if (!status)
            return status;
        out = handle->PortValue;
        return {};
    }
    if (from.StructSize < sizeof(from))
        return InvalidValue("A Behavior port has an unsupported StructSize.");
    SlotKind kind;
    if (!ReadSlotKind(from.Kind, kind))
        return InvalidValue("A Behavior port names an unknown slot kind.");
    Status status = Use(from.Handle, EditHandleKind::Node, handle);
    if (!status)
        return status;
    Slot slot;
    if (!ReadSelector(from.Slot, kind, Guid(from.Type), slot, status))
        return status;
    out = Port{handle->NodeValue.Value, std::move(slot)};
    return {};
}

Status EditProgram::ReadHook(const BML_BehaviorHookFunction *from,
                             HookBlock::Hook &out) const {
    if (!from || from->StructSize < sizeof(*from) || !from->Invoke)
        return InvalidValue("A Behavior Hook needs a callback.");
    if ((from->Retain == nullptr) != (from->Release == nullptr)) {
        return InvalidValue(
            "A Behavior Hook needs both Retain and Release, or neither.");
    }
    // The record takes the caller reference here, so the caller may drop its
    // own as soon as this edit is accepted. The matching Release runs when the
    // last holder of the record drops it, which is later than retirement for a
    // Conflicted Patch and earlier than any lease for an edit that never installs.
    auto thunk = std::make_shared<HookThunk>();
    thunk->Function = *from;
    if (from->Retain)
        from->Retain(from->State);
    out = HookBlock::Hook(
        PlanCallbackState::Retained(thunk, from->State, nullptr, nullptr),
        &InvokeHook, thunk.get());
    return {};
}

Status EditProgram::ReadOrdering(const BML_BehaviorEditStep &step,
                                 std::vector<Order> &out) const {
    if (step.OrderCount && !step.Ordering)
        return InvalidValue("A Behavior Patch ordering array is missing.");
    for (std::uint32_t index = 0; index < step.OrderCount; ++index) {
        const BML_BehaviorEditOrder &order = step.Ordering[index];
        if (order.StructSize < sizeof(order)) {
            return InvalidValue(
                "A Behavior Patch ordering entry has an unsupported StructSize.");
        }
        OrderKind kind;
        switch (order.Kind) {
        case BML_BEHAVIOR_ORDER_BEFORE: kind = OrderKind::Before; break;
        case BML_BEHAVIOR_ORDER_AFTER: kind = OrderKind::After; break;
        default:
            return InvalidValue(
                "A Behavior Patch ordering entry names an unknown order.");
        }
        std::string owner;
        std::string name;
        if (!ReadString(order.Owner, owner) || owner.empty() ||
            !ReadString(order.Name, name) || name.empty()) {
            return InvalidValue(
                "A Behavior Patch ordering entry needs an owner and name.");
        }
        out.push_back({kind, PatchKey{std::move(owner), std::move(name)}});
    }
    return {};
}

BML_BehaviorPlan PlanHandleOf(PlanId id) noexcept {
    return reinterpret_cast<BML_BehaviorPlan>(static_cast<std::uintptr_t>(id));
}

BML_BehaviorPatch PatchHandleOf(PatchId id) noexcept {
    return reinterpret_cast<BML_BehaviorPatch>(
        static_cast<std::uintptr_t>(id));
}

PatchId PatchIdOf(BML_BehaviorPatch patch) noexcept {
    return static_cast<PatchId>(reinterpret_cast<std::uintptr_t>(patch));
}

PlanId PlanIdOf(BML_BehaviorPlan plan) noexcept {
    return static_cast<PlanId>(reinterpret_cast<std::uintptr_t>(plan));
}

bool ReadSessionOwner(BML_BehaviorSession session, ModContext &context,
                      SessionOwner &out, Status &status) {
    status = context.BehaviorSessions().ReadOwner(SessionId(session), out);
    return static_cast<bool>(status);
}

int BML_BEHAVIOR_CALL SubmitPlan(
    BML_BehaviorSession session, const BML_BehaviorPlanSpec *spec,
    BML_BehaviorPlan *outPlan, BML_BehaviorPlanInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(spec) || !outPlan ||
            (info && !HasStructSize(info)) ||
            (spec->StepCount && !spec->Steps))
            return BML_ERROR_INVALID_PARAMETER;
        *outPlan = nullptr;
        TargetSet targets;
        switch (spec->Targets) {
        case BML_BEHAVIOR_TARGETS_EACH: targets = TargetSet::Each; break;
        case BML_BEHAVIOR_TARGETS_ONE: targets = TargetSet::One; break;
        default: return BML_ERROR_INVALID_PARAMETER;
        }
        std::string name;
        std::string script;
        if (!ReadString(spec->Name, name) || name.empty() ||
            !ReadString(spec->Script, script) || script.empty())
            return BML_ERROR_INVALID_PARAMETER;

        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result)) {
            WriteStatus(status, result);
            return ResultCode(result);
        }

        GraphEdit edit;
        EditProgram program;
        result = program.Build(spec->Steps, spec->StepCount, *context, edit);
        if (!result) {
            WriteStatus(status, result);
            return BML_ERROR_INVALID_PARAMETER;
        }

        PlanId id = 0;
        result = context->BehaviorPatches().Submit(
            context->BehaviorPlans(), owner,
            Script{std::move(script), targets}, std::move(name),
            std::move(edit), id);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outPlan = PlanHandleOf(id);
        PlanInfo read;
        if (info && context->BehaviorPlans().Read(owner.Id, owner.Generation,
                                                  id, read))
            WritePlanInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPlan(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    BML_BehaviorPlanInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !plan || !HasStructSize(info))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result)) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        PlanInfo read;
        result = context->BehaviorPlans().Read(owner.Id, owner.Generation,
                                               PlanIdOf(plan), read);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WritePlanInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ClosePlan(BML_BehaviorSession session,
                                BML_BehaviorPlan plan) {
    return Guard([&] {
        if (!session || !plan)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result))
            return ResultCode(result);
        return ResultCode(context->BehaviorPlans().Close(
            owner.Id, owner.Generation, PlanIdOf(plan)));
    });
}

int BML_BEHAVIOR_CALL ApplyPatch(
    BML_BehaviorSession session, const BML_BehaviorPatchSpec *spec,
    BML_BehaviorPatch *outPatch, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(spec) || !outPatch ||
            (info && !HasStructSize(info)) ||
            (spec->StepCount && !spec->Steps) || !spec->Graph.Domain)
            return BML_ERROR_INVALID_PARAMETER;
        *outPatch = nullptr;
        std::string name;
        if (!ReadString(spec->Name, name) || name.empty())
            return BML_ERROR_INVALID_PARAMETER;

        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result)) {
            WriteStatus(status, result);
            return ResultCode(result);
        }

        GraphEdit edit;
        EditProgram program;
        result = program.Build(spec->Steps, spec->StepCount, *context, edit);
        if (!result) {
            WriteStatus(status, result);
            return BML_ERROR_INVALID_PARAMETER;
        }

        PatchId id = 0;
        result = context->BehaviorPatches().Apply(
            owner,
            BML::Behavior::ObjectRef{spec->Graph.Domain, spec->Graph.Slot,
                                     spec->Graph.Generation},
            std::move(name), std::move(edit), id);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outPatch = PatchHandleOf(id);
        PatchInfo read;
        if (info && context->BehaviorPatches().Read(owner, id, read))
            WritePatchInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadPatch(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !patch || !HasStructSize(info))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result)) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        PatchInfo read;
        result = context->BehaviorPatches().Read(owner, PatchIdOf(patch), read);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WritePatchInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ClosePatch(BML_BehaviorSession session,
                                 BML_BehaviorPatch patch) {
    return Guard([&] {
        if (!session || !patch)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result))
            return ResultCode(result);
        return ResultCode(
            context->BehaviorPatches().Close(owner, PatchIdOf(patch)));
    });
}

const BML_BehaviorInterface kBehaviorInterface = {
    BML_IFACE_HEADER(BML_BehaviorInterface, BML_BEHAVIOR_INTERFACE_ID,
                     BML_BEHAVIOR_INTERFACE_MAJOR,
                     BML_BEHAVIOR_INTERFACE_MINOR),
    &OpenSession,
    &CloseSession,
    &Call,
    &Start,
    &Spawn,
    &Continue,
    &Pulse,
    &ReadRun,
    &TakeFrames,
    &CloseRun,
    &FindPrototypes,
    &ReadDeclaredLayout,
    &ReadLiveLayout,
    &Inspect,
    &ReadNodeLayout,
    &ReadGraphValue,
    &OpenWatch,
    &CloseWatch,
    &SubmitPlan,
    &ReadPlan,
    &ClosePlan,
    &InspectRun,
    &Set,
    &Bind,
    &Configure,
    &ApplyPatch,
    &ReadPatch,
    &ClosePatch,
    &ReadWatch,
};

} // namespace

namespace BML::Api {

const BML_BehaviorInterface &BehaviorInterface() noexcept {
    return kBehaviorInterface;
}

} // namespace BML::Api
