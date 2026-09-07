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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "BML/ImcWire.hpp"
#include "BML/TypeConvert.h"
#include "Behavior/HookBlock.h"
#include "Behavior/Patches.h"
#include "Behavior/Script.h"
#include "Behavior/Sessions.h"
#include "Behavior/FrameStore.h"
#include "Loader/ModContext.h"

namespace {

using BML::Behavior::Internal::AdmissionState;
using BML::Behavior::Internal::Sessions;
using BML::Behavior::Internal::Error;
using BML::Behavior::Internal::ExecutionError;
using BML::Behavior::Internal::RunFrame;
using BML::Behavior::Internal::FrameRetention;
using BML::Behavior::Internal::Phase;
using BML::Behavior::Internal::Pout;
using BML::Behavior::Internal::PoutKind;
using BML::Behavior::Internal::PrototypeInfo;
using BML::Behavior::Internal::PrototypeQuery;
using BML::Behavior::Internal::PrototypeRef;
using BML::Behavior::Internal::RunInfo;
using BML::Behavior::Internal::RunKind;
using BML::Behavior::Internal::RunResult;
using BML::Behavior::Internal::RunState;
using BML::Behavior::Internal::Slot;
using BML::Behavior::Internal::SlotKind;
using BML::Behavior::Internal::BlockSpec;
using BML::Behavior::Internal::Status;
using BML::Behavior::Internal::Value;
using BML::Behavior::Internal::Layout;
using BML::Behavior::Internal::ManagerRequirement;
using BML::Behavior::Internal::GraphModel;
using BML::Behavior::Internal::BehaviorKind;
using BML::Behavior::Internal::GraphValue;
using BML::Behavior::Internal::GraphView;
using BML::Behavior::Internal::ReadMode;
using BML::Behavior::Internal::Truth;
using BML::Behavior::Internal::ValueRelation;
using BML::Behavior::Internal::ValueState;
using BML::Behavior::Internal::WatchEvent;
using BML::Behavior::Internal::WatchKind;
using BML::Behavior::Internal::WatchSpec;
using BML::Behavior::Internal::Cycle;
using BML::Behavior::Internal::DetachedCompatibility;
using BML::Behavior::Internal::GraphEdit;
using BML::Behavior::Internal::Link;
using BML::Behavior::Internal::Node;
using BML::Behavior::Internal::NodePattern;
using BML::Behavior::Internal::Order;
using BML::Behavior::Internal::OrderKind;
using BML::Behavior::Internal::PatchId;
using BML::Behavior::Internal::PatchInfo;
using BML::Behavior::Internal::PatchKey;
using BML::Behavior::Internal::PatchState;
using BML::Behavior::Internal::ParameterOperation;
using BML::Behavior::Internal::PathRef;
using BML::Behavior::Internal::PlanCallbackState;
using BML::Behavior::Internal::PlanId;
using BML::Behavior::Internal::PlanInfo;
using BML::Behavior::Internal::PlanState;
using BML::Behavior::Internal::Port;
using BML::Behavior::Internal::ScriptSelection;
using BML::Behavior::Internal::SessionOwner;
using BML::Behavior::Internal::TargetSet;
using BML::Behavior::Internal::ScriptResult;
using BML::Behavior::Internal::ScriptId;
using BML::Behavior::Internal::ScriptInfo;
using BML::Behavior::Internal::ScriptState;
namespace HookBlock = BML::Behavior::Internal::HookBlock;
namespace Parameter = BML::Behavior::Internal::Parameter;

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
    case Error::InvalidArgument:
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
    case Error::RedirectConflict:
        return BML_BEHAVIOR_ERROR_REDIRECT_CONFLICT;
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
    return {Error::InvalidArgument, CKERR_INVALIDPARAMETER,
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
    const BML::Behavior::Internal::Parameter::Type parameterType =
        BML::Behavior::Internal::Parameter::Describe(parameters, type);
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
        using Form = BML::Behavior::Internal::Parameter::Form;
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
                  SlotKind kind, ModContext &context, BlockSpec &block,
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
               BlockSpec &to, Status &status) {
    if (from.StructSize < sizeof(from) ||
        from.Target.StructSize < sizeof(from.Target)) {
        status = InvalidValue("The Behavior Block has an unsupported StructSize.");
        return false;
    }
    if ((from.SettingStageCount && !from.SettingStages) ||
        (from.PinCount && !from.Pins) ||
        (from.LocalCount && !from.Locals)) {
        status = InvalidValue("A Behavior Block array is missing.");
        return false;
    }

    to = BlockSpec(Guid(from.Prototype));
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
            to.NextSettingStage();
        if (!ReadBindings(settings.Settings, settings.SettingCount,
                          SlotKind::Setting, context, to, status))
            return false;
    }
    if (!ReadBindings(from.Pins, from.PinCount, SlotKind::InputParameter,
                      context, to, status) ||
        !ReadBindings(from.Locals, from.LocalCount, SlotKind::Local,
                      context, to, status))
        return false;

    return true;
}

bool ReadFrames(const BML_BehaviorFramePolicy &from,
                FrameRetention &retention, Status &status) {
    if (from.StructSize < sizeof(from)) {
        status = InvalidValue(
            "The Behavior Frame policy has an unsupported StructSize.");
        return false;
    }
    if (from.Flags & ~BML_BEHAVIOR_FRAME_POLICY_POUTS) {
        status = InvalidValue("The Behavior Frame policy has unknown flags.");
        return false;
    }
    switch (from.Kind) {
    case BML_BEHAVIOR_FRAMES_SIGNALS:
        if (!from.Limit) {
            status = InvalidValue("Signals(n) requires a nonzero RunFrame limit.");
            return false;
        }
        retention = FrameRetention::Signals(from.Limit);
        break;
    case BML_BEHAVIOR_FRAMES_EACH_FRAME:
        if (!from.Limit) {
            status = InvalidValue("EachFrame(n) requires a nonzero RunFrame limit.");
            return false;
        }
        retention = FrameRetention::EachFrame(from.Limit);
        break;
    case BML_BEHAVIOR_FRAMES_LATEST:
        retention = FrameRetention::Latest();
        break;
    case BML_BEHAVIOR_FRAMES_NONE:
        retention = FrameRetention::Ignore();
        break;
    default:
        status = InvalidValue("The Behavior RunFrame policy is unknown.");
        return false;
    }
    if (from.Flags & BML_BEHAVIOR_FRAME_POLICY_POUTS)
        retention = retention.Pouts();
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
    out->Prototype.StructSize = sizeof(out->Prototype);
    out->Prototype.Prototype = Guid(info.Prototype.Guid);
    out->Prototype.Generation = info.Prototype.Generation;
    out->Status.StructSize = sizeof(out->Status);
    WriteStatus(&out->Status, info.LastStatus);
}

std::uint32_t PublicScriptState(ScriptState state) noexcept {
    switch (state) {
    case ScriptState::Ready: return BML_BEHAVIOR_SCRIPT_READY;
    case ScriptState::Closing: return BML_BEHAVIOR_SCRIPT_CLOSING;
    case ScriptState::Failed: return BML_BEHAVIOR_SCRIPT_FAILED;
    }
    return BML_BEHAVIOR_SCRIPT_FAILED;
}

void WriteScriptInfo(BML_BehaviorScriptInfo *out,
                     const ScriptInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicScriptState(info.State);
    out->Active = info.Active ? 1u : 0u;
    out->RequestedActive = info.RequestedActive ? 1u : 0u;
    out->Root = {info.Identity.Root.Reference.Domain,
                 info.Identity.Root.Reference.Slot,
                 info.Identity.Root.Reference.Generation};
    out->Owner = {info.Identity.Owner.Reference.Domain,
                  info.Identity.Owner.Reference.Slot,
                  info.Identity.Owner.Reference.Generation};
    out->Scene = {info.Identity.Scene.Reference.Domain,
                  info.Identity.Scene.Reference.Slot,
                  info.Identity.Scene.Reference.Generation};
    out->Priority = info.Priority;
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

ScriptId ScriptIdOf(BML_BehaviorScript script) noexcept {
    return static_cast<ScriptId>(reinterpret_cast<std::uintptr_t>(script));
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

BML_BehaviorScript ScriptHandle(ScriptId id) noexcept {
    return reinterpret_cast<BML_BehaviorScript>(
        static_cast<std::uintptr_t>(id));
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
    if (status.Code == Error::InvalidArgument)
        return BML_ERROR_INVALID_PARAMETER;
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

int OpenRunResult(const BML::Behavior::Internal::OpenRun &opened,
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
        const std::uintptr_t id = SessionId(session);
        context->BehaviorSessions().CloseSession(id);
        context->BehaviorScripts().CloseSession(id);
        return BML_OK;
    });
}

enum class OpenKind { Call, Start, Spawn };

int OpenRunEntry(OpenKind kind, BML_BehaviorSession session,
                 BML_ObjectRef ownerReference,
                 const BML_BehaviorBlock *block,
                 const BML_BehaviorFramePolicy *frames,
                 const BML_BehaviorSelector *input,
                 BML_BehaviorRun *outRun,
                 BML_BehaviorRunInfo *info,
                 BML_BehaviorStatus *status) {
    if (!ValidOutputs(info, status) || !session || !block || !frames || !outRun ||
        (kind != OpenKind::Spawn && !input))
        return BML_ERROR_INVALID_PARAMETER;
    *outRun = nullptr;
    ModContext *context = BML_GetModContext();
    if (!context)
        return BML_ERROR_FROZEN;
    if (!context->IsMainThread())
        return BML_ERROR_WRONG_THREAD;

    Status readStatus;
    BlockSpec spec;
    if (!ReadBlock(*block, *context, spec, readStatus)) {
        WriteStatus(status, readStatus);
        return BML_ERROR_INVALID_PARAMETER;
    }
    FrameRetention retention;
    if (!ReadFrames(*frames, retention, readStatus)) {
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
        ? sessions.Call(SessionId(session), owner, spec, in, retention)
        : kind == OpenKind::Start
            ? sessions.Start(SessionId(session), owner, spec, in, retention)
            : sessions.Spawn(SessionId(session), owner, spec, retention);
    return OpenRunResult(opened, outRun, info, status);
}

int BML_BEHAVIOR_CALL Call(BML_BehaviorSession session, BML_ObjectRef owner,
                           const BML_BehaviorBlock *block,
                           const BML_BehaviorFramePolicy *frames,
                           const BML_BehaviorSelector *input,
                           BML_BehaviorRun *outRun,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Call, session, owner, block, frames, input,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL Start(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            const BML_BehaviorFramePolicy *frames,
                            const BML_BehaviorSelector *input,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Start, session, owner, block, frames, input,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL Spawn(BML_BehaviorSession session, BML_ObjectRef owner,
                            const BML_BehaviorBlock *block,
                            const BML_BehaviorFramePolicy *frames,
                            BML_BehaviorRun *outRun,
                            BML_BehaviorRunInfo *info,
                            BML_BehaviorStatus *status) {
    return Guard([&] {
        return OpenRunEntry(OpenKind::Spawn, session, owner, block, frames, nullptr,
                            outRun, info, status);
    });
}

int BML_BEHAVIOR_CALL AttachBlock(BML_BehaviorSession session,
                                  BML_ObjectRef graph,
                                  const BML_BehaviorBlock *block,
                                  const BML_BehaviorFramePolicy *frames,
                                  BML_BehaviorRun *outRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!ValidOutputs(info, status) || !session || !block || !frames ||
            !outRun)
            return BML_ERROR_INVALID_PARAMETER;
        *outRun = nullptr;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;

        Status readStatus;
        BlockSpec spec;
        if (!ReadBlock(*block, *context, spec, readStatus)) {
            WriteStatus(status, readStatus);
            return BML_ERROR_INVALID_PARAMETER;
        }
        FrameRetention retention;
        if (!ReadFrames(*frames, retention, readStatus)) {
            WriteStatus(status, readStatus);
            return BML_ERROR_INVALID_PARAMETER;
        }
        CKBehavior *parent = ReadBehavior(graph, *context, readStatus);
        if (!parent) {
            WriteStatus(status, readStatus);
            return ResultCode(readStatus);
        }
        return OpenRunResult(
            context->BehaviorSessions().Attach(
                SessionId(session), parent, spec, retention),
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

class WireFrames final : public BML::Behavior::Internal::FrameBatch {
public:
    WireFrames(BML_BehaviorRunFrame *headers,
               std::uint32_t headerCapacity,
               std::uint32_t headerStride,
               void *payload,
               std::uint32_t payloadCapacity,
               std::uint32_t *headerCount,
               std::uint32_t *payloadSize) noexcept
        : m_Headers(reinterpret_cast<std::uint8_t *>(headers)),
          m_HeaderCapacity(headerCapacity), m_HeaderStride(headerStride),
          m_Payload(static_cast<std::uint8_t *>(payload)),
          m_PayloadCapacity(payloadCapacity), m_OutHeaderCount(headerCount),
          m_OutPayloadSize(payloadSize) {}

    bool Measure(const RunFrame &frame) override {
        return !m_Writing && Add(frame);
    }

    BML::Behavior::Internal::FrameBatchResult Ready() override {
        if (m_HeaderCount > UINT32_MAX || m_PayloadSize > UINT32_MAX ||
            !FitsStrided(m_HeaderCount, m_HeaderStride,
                         sizeof(BML_BehaviorRunFrame)))
            return BML::Behavior::Internal::FrameBatchResult::Failed;
        *m_OutHeaderCount = static_cast<std::uint32_t>(m_HeaderCount);
        *m_OutPayloadSize = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_HeaderCapacity < m_HeaderCount ||
            m_PayloadCapacity < m_PayloadSize)
            return BML::Behavior::Internal::FrameBatchResult::Insufficient;
        m_ExpectedHeaders = m_HeaderCount;
        m_ExpectedPayload = m_PayloadSize;
        m_HeaderCount = 0;
        m_PayloadSize = 0;
        m_Writing = true;
        return BML::Behavior::Internal::FrameBatchResult::Complete;
    }

    bool Write(const RunFrame &frame) override {
        return m_Writing && Add(frame) &&
            m_HeaderCount <= m_ExpectedHeaders &&
            m_PayloadSize <= m_ExpectedPayload;
    }

private:
    bool Add(const RunFrame &frame) {
        if (m_Writing &&
            (m_HeaderCount >= m_ExpectedHeaders ||
             m_HeaderCount >= m_HeaderCapacity || !m_Headers))
            return false;
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
        if (m_Writing)
            std::memcpy(m_Headers + m_HeaderCount * m_HeaderStride,
                        &header, sizeof(header));
        ++m_HeaderCount;
        return true;
    }
    bool Align(std::size_t alignment) {
        const std::size_t remainder = m_PayloadSize % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (m_PayloadSize > UINT32_MAX ||
            padding > UINT32_MAX - m_PayloadSize)
            return false;
        if (!CanWrite(padding))
            return false;
        if (m_Writing)
            std::memset(m_Payload + m_PayloadSize, 0, padding);
        m_PayloadSize += padding;
        return true;
    }

    bool Append(const void *data, std::size_t size, std::uint32_t &offset) {
        if ((!data && size) || m_PayloadSize > UINT32_MAX ||
            size > UINT32_MAX || size > UINT32_MAX - m_PayloadSize)
            return false;
        if (!CanWrite(size))
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Writing && size)
            std::memcpy(m_Payload + m_PayloadSize, data, size);
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
        if (!CanWrite(bytes))
            return false;
        offset = static_cast<std::uint32_t>(m_PayloadSize);
        if (m_Writing)
            std::memset(m_Payload + m_PayloadSize, 0, bytes);
        m_PayloadSize += bytes;
        return true;
    }

    template <typename T>
    bool StoreRecord(std::uint32_t base, std::size_t index,
                     const T &record) {
        if (!m_Writing)
            return true;
        const std::size_t at = static_cast<std::size_t>(base) +
            index * sizeof(T);
        if (!m_Payload || at > m_ExpectedPayload ||
            sizeof(T) > m_ExpectedPayload - at ||
            at > m_PayloadCapacity ||
            sizeof(T) > m_PayloadCapacity - at)
            return false;
        std::memcpy(m_Payload + at, &record, sizeof(record));
        return true;
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
            if (!StoreRecord(header.OutOffset, index, record))
                return false;
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
            if (!StoreRecord(header.PoutOffset, index, record))
                return false;
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
        return StoreRecord(header.DiagnosticOffset, 0, record);
    }

    [[nodiscard]] bool CanWrite(std::size_t size) const noexcept {
        if (!m_Writing)
            return true;
        return m_Payload && m_PayloadSize <= m_ExpectedPayload &&
            size <= m_ExpectedPayload - m_PayloadSize &&
            m_PayloadSize <= m_PayloadCapacity &&
            size <= m_PayloadCapacity - m_PayloadSize;
    }

    std::uint8_t *m_Headers = nullptr;
    std::size_t m_HeaderCapacity = 0;
    std::size_t m_HeaderStride = 0;
    std::uint8_t *m_Payload = nullptr;
    std::size_t m_PayloadCapacity = 0;
    std::uint32_t *m_OutHeaderCount = nullptr;
    std::uint32_t *m_OutPayloadSize = nullptr;
    bool m_Writing = false;
    std::size_t m_HeaderCount = 0;
    std::size_t m_PayloadSize = 0;
    std::size_t m_ExpectedHeaders = 0;
    std::size_t m_ExpectedPayload = 0;
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
        std::shared_ptr<BML::Behavior::Internal::FrameStore> store =
            context->BehaviorSessions().Frames(RunId(run));
        if (!store)
            return BML_ERROR_INVALID_HANDLE;

        WireFrames batch(headers, headerCapacity, headerStride,
                         payload, payloadCapacity,
                         outHeaderCount, outPayloadSize);
        switch (store->Take(batch)) {
        case BML::Behavior::Internal::FrameBatchResult::Complete:
            return BML_OK;
        case BML::Behavior::Internal::FrameBatchResult::Insufficient:
            return BML_ERROR_BUFFER_TOO_SMALL;
        case BML::Behavior::Internal::FrameBatchResult::Failed:
            return BML_ERROR_OUT_OF_MEMORY;
        }
        return BML_ERROR_FAIL;
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
    record.Origin = from.Origin == BML::Behavior::Internal::LayoutOrigin::Declared
        ? BML_BEHAVIOR_LAYOUT_DECLARED : BML_BEHAVIOR_LAYOUT_LIVE;
    record.Prototype.StructSize = sizeof(record.Prototype);
    record.Prototype.Prototype = Guid(from.Prototype);
    record.Prototype.Generation = from.ProviderGeneration;
    record.LayoutGeneration = from.Generation;
    switch (from.Kind) {
    case BML::Behavior::Internal::BehaviorKind::Function:
        record.Kind = BML_BEHAVIOR_KIND_FUNCTION;
        break;
    case BML::Behavior::Internal::BehaviorKind::Callback:
        record.Kind = BML_BEHAVIOR_KIND_CALLBACK;
        break;
    case BML::Behavior::Internal::BehaviorKind::Graph:
        record.Kind = BML_BEHAVIOR_KIND_GRAPH;
        break;
    }
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
        const BML::Behavior::Internal::SlotInfo &slot = from.Slots[index];
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

std::uint32_t PublicWatchState(BML::Behavior::Internal::WatchState state) noexcept {
    switch (state) {
    case BML::Behavior::Internal::WatchState::Active:
        return BML_BEHAVIOR_WATCH_ACTIVE;
    case BML::Behavior::Internal::WatchState::Failed:
        return BML_BEHAVIOR_WATCH_FAILED;
    }
    return BML_BEHAVIOR_WATCH_FAILED;
}

void WriteWatchInfo(BML_BehaviorWatchInfo *out,
                    const BML::Behavior::Internal::WatchInfo &info) noexcept {
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
    if (source.Nodes.size() > UINT32_MAX || source.Links.size() > UINT32_MAX ||
        source.Operations.size() > UINT32_MAX)
        return false;
    graph.NodeCount = static_cast<std::uint32_t>(source.Nodes.size());
    graph.LinkCount = static_cast<std::uint32_t>(source.Links.size());
    graph.OperationCount = static_cast<std::uint32_t>(source.Operations.size());
    if (!source.Nodes.empty() &&
        !payload.Reserve<BML_BehaviorGraphNode>(
            source.Nodes.size(), graph.NodeOffset))
        return false;
    if (!source.Links.empty() &&
        !payload.Reserve<BML_BehaviorGraphLink>(
            source.Links.size(), graph.LinkOffset))
        return false;
    if (!source.Operations.empty() &&
        !payload.Reserve<BML_BehaviorGraphOperation>(
            source.Operations.size(), graph.OperationOffset))
        return false;

    for (std::size_t index = 0; index < source.Nodes.size(); ++index) {
        const auto &node = source.Nodes[index];
        BML_BehaviorGraphNode record{};
        record.StructSize = sizeof(record);
        record.Id = node.Id;
        record.Object = {node.Object.Domain, node.Object.Slot,
                         node.Object.Generation};
        record.Parent = node.Parent;
        record.Index = node.Index;
        record.Occurrence = node.Occurrence;
        switch (node.Kind) {
        case BML::Behavior::Internal::BehaviorKind::Function:
            record.Kind = BML_BEHAVIOR_KIND_FUNCTION;
            break;
        case BML::Behavior::Internal::BehaviorKind::Callback:
            record.Kind = BML_BEHAVIOR_KIND_CALLBACK;
            break;
        case BML::Behavior::Internal::BehaviorKind::Graph:
            record.Kind = BML_BEHAVIOR_KIND_GRAPH;
            break;
        }
        record.LayoutGeneration = node.LayoutGeneration;
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
            portRecord.LayoutGeneration = port.LayoutGeneration;
            portRecord.Kind = PublicSlotKind(port.Kind);
            if (port.Dynamic)
                portRecord.Flags |= BML_BEHAVIOR_SLOT_DYNAMIC;
            portRecord.Index = port.Index;
            portRecord.Occurrence = port.Occurrence;
            portRecord.Type = Guid(port.Type);
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
    for (std::size_t index = 0; index < source.Operations.size(); ++index) {
        const auto &operation = source.Operations[index];
        BML_BehaviorGraphOperation record{};
        record.StructSize = sizeof(record);
        record.Id = operation.Id;
        record.Object = {operation.Object.Domain, operation.Object.Slot,
                         operation.Object.Generation};
        record.Owner = operation.Owner;
        record.Function = Guid(operation.Function);
        record.Result = Guid(operation.Result);
        record.Input1 = Guid(operation.Input1);
        record.Input2 = Guid(operation.Input2);
        if (!payload.Text(operation.Name, record.Name))
            return false;
        payload.Store(graph.OperationOffset, index, record);
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
        const auto *value = std::get_if<BML::Behavior::Internal::ObjectRef>(&source.Data);
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
    std::uint64_t layoutGeneration,
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
        if (slot->Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
            layoutGeneration == 0) {
            result = InvalidValue(
                "An indexed graph port requires its Layout generation.");
            WriteStatus(status, result);
            return BML_ERROR_INVALID_PARAMETER;
        }
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
            SessionId(session), native, layoutGeneration, selector,
            ReadMode::NonForcing, source);
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
        const auto *value = std::get_if<BML::Behavior::Internal::ObjectRef>(&source.Data);
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
            spec.LayoutGeneration = source->LayoutGeneration;
            SlotKind kind;
            switch (source->SlotKind) {
            case BML_BEHAVIOR_SLOT_PIN: kind = SlotKind::InputParameter; break;
            case BML_BEHAVIOR_SLOT_POUT: kind = SlotKind::OutputParameter; break;
            case BML_BEHAVIOR_SLOT_SETTING: kind = SlotKind::Setting; break;
            case BML_BEHAVIOR_SLOT_LOCAL: kind = SlotKind::Local; break;
            case BML_BEHAVIOR_SLOT_TARGET: kind = SlotKind::Target; break;
            default: return BML_ERROR_INVALID_PARAMETER;
            }
            if ((source->Slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
                 source->LayoutGeneration == 0) ||
                !ReadSelector(source->Slot, kind, CKGUID(),
                              spec.ValueSlot, result)) {
                if (result)
                    result = InvalidValue(
                        "An indexed watched port requires its Layout generation.");
                WriteStatus(status, result);
                return BML_ERROR_INVALID_PARAMETER;
            }
        } else if (spec.Kind == WatchKind::LayoutChanged) {
            if (!source->LayoutGeneration)
                return BML_ERROR_INVALID_PARAMETER;
            spec.LayoutGeneration = source->LayoutGeneration;
        }

        const BML_BehaviorWatchFunction function = *callback;
        BML::Behavior::Internal::PlanCallbackState state = callback->Retain
            ? BML::Behavior::Internal::PlanCallbackState::Retained(
                  callback->State, callback->Retain, callback->Release)
            : BML::Behavior::Internal::PlanCallbackState::Static(callback->State);
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
        BML::Behavior::Internal::WatchInfo current;
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
    case PlanState::Partial: return BML_BEHAVIOR_PLAN_PARTIAL;
    case PlanState::Unsatisfied: return BML_BEHAVIOR_PLAN_UNSATISFIED;
    case PlanState::Disabled: return BML_BEHAVIOR_PLAN_DISABLED;
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
    case PatchState::Disabled: return BML_BEHAVIOR_PATCH_DISABLED;
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
            (source->Slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
             source->LayoutGeneration == 0) ||
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
            source->LayoutGeneration, sourceSlot, nativeRelation,
            *outLayoutGeneration);
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
        BlockSpec settings;
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
                settings.NextSettingStage();
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
    ~HookThunk() noexcept {
        if (!Retained || !Function.Release)
            return;
        try {
            Function.Release(Function.State);
        } catch (...) {
            // A foreign release callback must not cross the Loader boundary.
        }
    }

    BML_BehaviorHookFunction Function{};
    bool Retained = false;
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
    case BML_BEHAVIOR_HOOK_FAULT: return HookBlock::CallbackFaulted;
    default: return CKBR_BEHAVIORERROR;
    }
}

enum class EditHandleKind {
    Node,
    Operation,
    Link,
    Path,
    Port,
};

struct EditHandle {
    EditHandleKind Kind = EditHandleKind::Node;
    std::uint32_t Scope = BML_BEHAVIOR_EDIT_GRAPH;
    Node NodeValue;
    ParameterOperation OperationValue;
    Link LinkValue;
    PathRef PathValue;
    Port PortValue;
};

// Translates the shared wire edit program into symbolic graph intent. The same
// value can be applied once to a known graph or retained by a Plan.
class EditProgram final {
public:
    Status Build(const BML_BehaviorEditStep *steps, std::uint32_t count,
                 ModContext &context, GraphEdit &edit);
    // Reports the symbolic Node each caller handle defined, so an applied
    // Patch answers ResolvePatchNode in the caller's own handle space.
    [[nodiscard]] BML::Behavior::Internal::Patches::HandleMap Nodes() const;

private:
    static bool Defines(std::uint32_t kind) noexcept;

    Status Step(const BML_BehaviorEditStep &step, ModContext &context,
                GraphEdit &edit);
    Status Use(std::uint32_t scope, std::uint32_t id, EditHandleKind kind,
               const EditHandle *&out) const;
    Status ReadPort(const BML_BehaviorPortRef &from, Port &out) const;
    Status ReadHook(const BML_BehaviorHookFunction *from,
                    HookBlock::Hook &out) const;
    Status ReadOrdering(const BML_BehaviorEditStep &step,
                        std::vector<Order> &out) const;

    using HandleKey = std::pair<std::uint32_t, std::uint32_t>;
    std::map<HandleKey, EditHandle> m_Handles;
    std::map<std::uint32_t, GraphEdit *> m_Graphs;
};

bool EditProgram::Defines(std::uint32_t kind) noexcept {
    switch (kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE:
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK:
    case BML_BEHAVIOR_EDIT_FOLLOW:
    case BML_BEHAVIOR_EDIT_ADD_BLOCK:
    case BML_BEHAVIOR_EDIT_APPEND_SLOT:
    case BML_BEHAVIOR_EDIT_USE_NODE:
    case BML_BEHAVIOR_EDIT_USE_LINK:
    case BML_BEHAVIOR_EDIT_ADD_OPERATION:
    case BML_BEHAVIOR_EDIT_REPLACE_BLOCK:
    case BML_BEHAVIOR_EDIT_ADD_GRAPH:
    case BML_BEHAVIOR_EDIT_ENTER_GRAPH:
    case BML_BEHAVIOR_EDIT_NEXT_NODE:
    case BML_BEHAVIOR_EDIT_PREVIOUS_NODE:
    case BML_BEHAVIOR_EDIT_LEAVING_LINK:
    case BML_BEHAVIOR_EDIT_ENTERING_LINK:
    case BML_BEHAVIOR_EDIT_LINK_TO_NODE:
    case BML_BEHAVIOR_EDIT_EACH_NODE:
        return true;
    default:
        return false;
    }
}

Status EditProgram::Build(const BML_BehaviorEditStep *steps,
                          std::uint32_t count, ModContext &context,
                          GraphEdit &edit) {
    EditHandle graph;
    graph.Scope = BML_BEHAVIOR_EDIT_GRAPH;
    graph.NodeValue = edit.Graph();
    m_Handles.clear();
    m_Graphs.clear();
    m_Graphs.emplace(BML_BEHAVIOR_EDIT_GRAPH, &edit);
    m_Handles.emplace(HandleKey{BML_BEHAVIOR_EDIT_GRAPH,
                                BML_BEHAVIOR_EDIT_GRAPH}, graph);
    for (std::uint32_t index = 0; index < count; ++index) {
        const Status status = Step(steps[index], context, edit);
        if (!status)
            return status;
    }
    return {};
}

BML::Behavior::Internal::Patches::HandleMap EditProgram::Nodes() const {
    BML::Behavior::Internal::Patches::HandleMap nodes;
    for (const auto &entry : m_Handles) {
        if (entry.second.Kind == EditHandleKind::Node &&
            entry.first.second != BML_BEHAVIOR_EDIT_GRAPH) {
            nodes.emplace(
                entry.first.second,
                BML::Behavior::Internal::Patches::Symbol{
                    entry.first.first, entry.second.NodeValue.Value});
        }
    }
    return nodes;
}

Status ReadNodePattern(const BML_BehaviorEditStep &step,
                       NodePattern &out, bool required) {
    out = {};
    const CKGUID prototype = Guid(step.Prototype.Prototype);
    if ((prototype.IsValid() || step.Prototype.Generation != 0) &&
        step.Prototype.StructSize < sizeof(step.Prototype)) {
        return InvalidValue(
            "A Node Pattern has an unsupported Prototype reference.");
    }
    if (step.Prototype.Generation != 0) {
        return InvalidValue(
            "A Node Pattern matches a Prototype GUID, not a provider generation.");
    }

    switch (step.Selector.Kind) {
    case BML_BEHAVIOR_SELECTOR_INDEX:
        if (step.Selector.StructSize < sizeof(step.Selector) ||
            step.Selector.Index < 0)
            return InvalidValue("A Node Pattern index is invalid.");
        out.Selector = NodePattern::SelectorKind::Index;
        out.Index = step.Selector.Index;
        break;
    case BML_BEHAVIOR_SELECTOR_NAME:
    case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
        if (step.Selector.StructSize < sizeof(step.Selector) ||
            !ReadString(step.Selector.Name, out.Name) || out.Name.empty() ||
            step.Selector.Occurrence < 0)
            return InvalidValue("A Node Pattern name is invalid.");
        out.Selector = NodePattern::SelectorKind::Name;
        out.Occurrence = step.Selector.Occurrence;
        out.Unique = step.Selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
        break;
    case BML_BEHAVIOR_SELECTOR_ONLY:
        if (step.Selector.StructSize < sizeof(step.Selector))
            return InvalidValue("A Node Pattern selector is invalid.");
        out.Selector = NodePattern::SelectorKind::Only;
        break;
    default:
        return InvalidValue("A Node Pattern selector is unknown.");
    }

    out.Prototype = prototype;
    if (step.ExpectedKind) {
        switch (step.ExpectedKind) {
        case BML_BEHAVIOR_KIND_FUNCTION:
            out.ExpectedKind = BehaviorKind::Function;
            break;
        case BML_BEHAVIOR_KIND_CALLBACK:
            out.ExpectedKind = BehaviorKind::Callback;
            break;
        case BML_BEHAVIOR_KIND_GRAPH:
            out.ExpectedKind = BehaviorKind::Graph;
            break;
        default:
            return InvalidValue("A Node Pattern Behavior kind is unknown.");
        }
    }
    if (step.ReservedShape != 0)
        return InvalidValue("A Node Pattern has reserved shape data.");
    out.PortShape = step.PortShape;
    if (required && !out)
        return InvalidValue("A Node Pattern has no observable condition.");
    return {};
}

Status EditProgram::Step(const BML_BehaviorEditStep &step,
                         ModContext &context, GraphEdit &root) {
    if (step.StructSize < sizeof(step))
        return InvalidValue("A Behavior edit step has an unsupported StructSize.");
    const auto graph = m_Graphs.find(step.Graph);
    if (graph == m_Graphs.end() || !graph->second)
        return InvalidValue("A Behavior edit step names an unknown graph scope.");
    GraphEdit &edit = *graph->second;
    std::uint32_t allowedFlags = 0;
    if (step.Kind == BML_BEHAVIOR_EDIT_REQUIRE_LINK)
        allowedFlags = BML_BEHAVIOR_EDIT_HAS_DELAY;
    else if (step.Kind == BML_BEHAVIOR_EDIT_FLOW)
        allowedFlags = BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
    if (step.Flags & ~allowedFlags)
        return InvalidValue("A Behavior edit step contains an unsupported flag.");
    if (Defines(step.Kind)) {
        if (step.Result == 0 || step.Result == BML_BEHAVIOR_EDIT_GRAPH ||
            m_Handles.find({step.Graph, step.Result}) != m_Handles.end() ||
            m_Graphs.find(step.Result) != m_Graphs.end()) {
            return InvalidValue(
                "A Behavior edit step handle is missing or already defined.");
        }
    } else if (step.Result != 0) {
        return InvalidValue("This kind of Behavior edit step defines no handle.");
    }

    EditHandle defined;
    defined.Scope = step.Graph;
    Status status;
    Port source;
    Port sink;
    switch (step.Kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE:
    case BML_BEHAVIOR_EDIT_EACH_NODE: {
        NodePattern query;
        if (status = ReadNodePattern(step, query, true); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = step.Kind == BML_BEHAVIOR_EDIT_EACH_NODE
            ? edit.Each(std::move(query))
            : edit.RequireOne(std::move(query));
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
    case BML_BEHAVIOR_EDIT_NEXT_NODE: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        NodePattern expected;
        if (status = ReadNodePattern(step, expected, false); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = expected
            ? edit.Next(source, std::move(expected)) : edit.Next(source);
        break;
    }
    case BML_BEHAVIOR_EDIT_PREVIOUS_NODE: {
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        NodePattern expected;
        if (status = ReadNodePattern(step, expected, false); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = expected
            ? edit.Previous(sink, std::move(expected)) : edit.Previous(sink);
        break;
    }
    case BML_BEHAVIOR_EDIT_LEAVING_LINK:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Leaving(source);
        break;
    case BML_BEHAVIOR_EDIT_ENTERING_LINK:
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Entering(sink);
        break;
    case BML_BEHAVIOR_EDIT_LINK_TO_NODE: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.To(source, target->NodeValue);
        break;
    }
    case BML_BEHAVIOR_EDIT_FOLLOW:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Path;
        defined.PathValue = edit.Follow(source);
        break;
    case BML_BEHAVIOR_EDIT_USE_NODE: {
        if (!step.Object.Domain) {
            return InvalidValue(
                "A used Behavior node needs an object reference.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.UseNode(
            BML::Behavior::Internal::ObjectRef{step.Object.Domain, step.Object.Slot,
                                     step.Object.Generation});
        break;
    }
    case BML_BEHAVIOR_EDIT_USE_LINK: {
        if (!step.Object.Domain) {
            return InvalidValue(
                "A used Behavior link needs an object reference.");
        }
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.UseLink(
            BML::Behavior::Internal::ObjectRef{step.Object.Domain, step.Object.Slot,
                                     step.Object.Generation});
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_BLOCK: {
        if (!step.Block)
            return InvalidValue("An added Behavior Block is missing.");
        BlockSpec block;
        if (!ReadBlock(*step.Block, context, block, status))
            return status;
        if (!block.Prototype().IsValid())
            return InvalidValue("An added Behavior Block needs a Prototype.");
        if (!block.PrototypeGeneration()) {
            return InvalidValue(
                "An added Behavior Block needs a fixed Prototype provider generation.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Add(std::move(block));
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_GRAPH: {
        std::string name;
        if (!ReadString(step.Name, name) || name.empty())
            return InvalidValue("An added graph-backed Node needs a name.");
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.AddGraph(std::move(name), step.Priority);
        break;
    }
    case BML_BEHAVIOR_EDIT_ENTER_GRAPH: {
        if (!step.Result || step.Result == BML_BEHAVIOR_EDIT_GRAPH ||
            m_Graphs.find(step.Result) != m_Graphs.end())
            return InvalidValue("A nested graph scope handle is invalid.");
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        GraphEdit &nested = edit.Enter(target->NodeValue, step.Result);
        m_Graphs.emplace(step.Result, &nested);
        EditHandle nestedRoot;
        nestedRoot.Scope = step.Result;
        nestedRoot.NodeValue = nested.Graph();
        m_Handles.emplace(
            HandleKey{step.Result, BML_BEHAVIOR_EDIT_GRAPH}, nestedRoot);
        return {};
    }
    case BML_BEHAVIOR_EDIT_REPLACE_BLOCK: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, target); !status)
            return status;
        if (!step.Block)
            return InvalidValue("A replacement Behavior Block is missing.");
        BlockSpec block;
        if (!ReadBlock(*step.Block, context, block, status))
            return status;
        if (!block.Prototype().IsValid())
            return InvalidValue("A replacement Behavior Block needs a Prototype.");
        if (!block.PrototypeGeneration()) {
            return InvalidValue(
                "A replacement Behavior Block needs a fixed Prototype provider generation.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Replace(target->NodeValue, std::move(block));
        break;
    }
    case BML_BEHAVIOR_EDIT_REMOVE_NODE: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, target); !status)
            return status;
        edit.Remove(target->NodeValue);
        break;
    }
    case BML_BEHAVIOR_EDIT_PATTERN_PORT_COUNT: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        SlotKind kind;
        switch (step.SlotKind) {
        case BML_BEHAVIOR_SLOT_IN: kind = SlotKind::Input; break;
        case BML_BEHAVIOR_SLOT_OUT: kind = SlotKind::Output; break;
        case BML_BEHAVIOR_SLOT_PIN: kind = SlotKind::InputParameter; break;
        case BML_BEHAVIOR_SLOT_POUT: kind = SlotKind::OutputParameter; break;
        case BML_BEHAVIOR_SLOT_SETTING: kind = SlotKind::Setting; break;
        case BML_BEHAVIOR_SLOT_LOCAL: kind = SlotKind::Local; break;
        case BML_BEHAVIOR_SLOT_TARGET: kind = SlotKind::Target; break;
        default:
            return InvalidValue(
                "A Node Pattern port count names an unknown port kind.");
        }
        if (step.Delay < 0)
            return InvalidValue("A Node Pattern port count cannot be negative.");
        return edit.Count(target->NodeValue, kind, step.Delay);
    }
    case BML_BEHAVIOR_EDIT_PATTERN_PORT_VALUE: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        if (step.Sink.Graph != step.Graph ||
            step.Sink.Handle != step.Target || step.Sink.Kind == 0) {
            return InvalidValue(
                "A Node Pattern value must name a port of its target Node.");
        }
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (sink.Owner != target->NodeValue.Value)
            return InvalidValue(
                "A Node Pattern value resolved to a different Node.");
        Parameter::Binding binding;
        if (!ReadValue(step.Value, context, binding, status))
            return status;
        if (binding.Kind() != Parameter::BindingKind::Value) {
            return {Error::WorldBoundValue, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "A Node Pattern in a Plan cannot retain a live object."};
        }
        return edit.Observe(std::move(sink), binding.Literal());
    }
    case BML_BEHAVIOR_EDIT_ADD_OPERATION: {
        if (step.Operation.StructSize < sizeof(step.Operation)) {
            return InvalidValue(
                "A Behavior Parameter Operation has an unsupported StructSize.");
        }
        const CKGUID operation = Guid(step.Operation.Operation);
        const CKGUID result = Guid(step.Operation.Result);
        const CKGUID input1 = Guid(step.Operation.Input1);
        const CKGUID input2 = Guid(step.Operation.Input2);
        if (!operation.IsValid() || !result.IsValid() ||
            result == CKPGUID_NONE) {
            return InvalidValue(
                "A Behavior Parameter Operation needs an operation GUID and result type.");
        }
        if (input2.IsValid() && input2 != CKPGUID_NONE &&
            (!input1.IsValid() || input1 == CKPGUID_NONE)) {
            return InvalidValue(
                "A Behavior Parameter Operation cannot have a second input without its first input.");
        }
        defined.Kind = EditHandleKind::Operation;
        defined.OperationValue = edit.AddOperation(
            operation, result, input1, input2);
        break;
    }
    case BML_BEHAVIOR_EDIT_APPEND_SLOT: {
        const EditHandle *owner = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, owner); !status)
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
        if (status = Use(step.Graph, step.Target, EditHandleKind::Path, path); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.After(path->PathValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_BEFORE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.Before(link->LinkValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_SPLICE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        if (step.Node) {
            const EditHandle *block = nullptr;
            if (status = Use(step.Graph, step.Node, EditHandleKind::Node, block); !status)
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
    case BML_BEHAVIOR_EDIT_REDIRECT: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        edit.Redirect(link->LinkValue, sink, std::move(ordering));
        break;
    }
    case BML_BEHAVIOR_EDIT_REDIRECT_TO_LINK: {
        const EditHandle *link = nullptr;
        const EditHandle *destination = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link,
                         link); !status)
            return status;
        if (status = Use(step.Graph, step.Node, EditHandleKind::Link,
                         destination); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        edit.Redirect(link->LinkValue, destination->LinkValue,
                      std::move(ordering));
        break;
    }
    default:
        return InvalidValue("A Behavior edit step names an unknown operation.");
    }

    if (Defines(step.Kind))
        m_Handles.emplace(HandleKey{step.Graph, step.Result}, defined);
    return {};
}

Status EditProgram::Use(std::uint32_t scope, std::uint32_t id,
                        EditHandleKind kind,
                        const EditHandle *&out) const {
    const auto found = m_Handles.find({scope, id});
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
    if (from.StructSize < sizeof(from))
        return InvalidValue("A Behavior port has an unsupported StructSize.");
    const EditHandle *handle = nullptr;
    if (from.Kind == 0) {
        const Status status = Use(from.Graph, from.Handle,
                                  EditHandleKind::Port, handle);
        if (!status)
            return status;
        out = handle->PortValue;
        return {};
    }
    SlotKind kind;
    if (!ReadSlotKind(from.Kind, kind))
        return InvalidValue("A Behavior port names an unknown slot kind.");
    Status status = Use(from.Graph, from.Handle, EditHandleKind::Node, handle);
    if (!status) {
        status = Use(from.Graph, from.Handle, EditHandleKind::Operation, handle);
        if (!status)
            return InvalidValue(
                "A Behavior port owner is neither a Node nor a Parameter Operation.");
        if (kind != SlotKind::InputParameter &&
            kind != SlotKind::OutputParameter) {
            return InvalidValue(
                "A Parameter Operation exposes only Pin and Pout ports.");
        }
    }
    Slot slot;
    if (!ReadSelector(from.Slot, kind, Guid(from.Type), slot, status))
        return status;
    out = Port{handle->Kind == EditHandleKind::Node
                   ? handle->NodeValue.Value
                   : handle->OperationValue.Value,
               std::move(slot)};
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
    if (from->Retain) {
        try {
            from->Retain(from->State);
            thunk->Retained = true;
        } catch (const std::exception &exception) {
            Status failure{Error::CallbackFailed, CK_OK,
                           CKBR_BEHAVIORERROR, exception.what()};
            failure.Details.Stage = Phase::LifecycleCallback;
            return failure;
        } catch (...) {
            Status failure{
                Error::CallbackFailed, CK_OK, CKBR_BEHAVIORERROR,
                "A Behavior Hook Retain callback threw an exception."};
            failure.Details.Stage = Phase::LifecycleCallback;
            return failure;
        }
    }
    out = HookBlock::Hook(
        PlanCallbackState::Retained(thunk, from->State, nullptr, nullptr),
        &InvokeHook, thunk.get(),
        HookBlock::Hook::Identity{
            reinterpret_cast<std::uintptr_t>(from->State),
            reinterpret_cast<std::uintptr_t>(from->Retain),
            reinterpret_cast<std::uintptr_t>(from->Release),
            reinterpret_cast<std::uintptr_t>(from->Invoke)});
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
            (info && !HasStructSize(info)) || !spec->EditCount ||
            !spec->Edits)
            return BML_ERROR_INVALID_PARAMETER;
        *outPlan = nullptr;
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

        std::vector<BML::Behavior::Internal::Patches::Rule> rules;
        try {
            rules.reserve(spec->EditCount);
            for (std::uint32_t index = 0; index < spec->EditCount; ++index) {
                const BML_BehaviorScriptEdit &target = spec->Edits[index];
                if (!HasStructSize(&target) || target.Reserved != 0 ||
                    (target.StepCount && !target.Steps))
                    return BML_ERROR_INVALID_PARAMETER;
                TargetSet targets;
                switch (target.Targets) {
                case BML_BEHAVIOR_TARGETS_EACH:
                    targets = TargetSet::Each;
                    break;
                case BML_BEHAVIOR_TARGETS_ONE:
                    targets = TargetSet::One;
                    break;
                default: return BML_ERROR_INVALID_PARAMETER;
                }
                std::string script;
                if (!ReadString(target.Script, script) || script.empty())
                    return BML_ERROR_INVALID_PARAMETER;
                GraphEdit edit;
                EditProgram program;
                result = program.Build(target.Steps, target.StepCount,
                                       *context, edit);
                if (!result) {
                    WriteStatus(status, result);
                    return BML_ERROR_INVALID_PARAMETER;
                }
                rules.push_back({ScriptSelection{std::move(script), targets},
                                 std::make_shared<GraphEdit>(std::move(edit))});
            }
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }

        PlanId id = 0;
        result = context->BehaviorPatches().Submit(
            context->BehaviorPlans(), owner, std::move(name),
            std::move(rules), id);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outPlan = PlanHandleOf(id);
        PlanInfo read;
        if (info && context->BehaviorPatches().ReadPlan(
                        context->BehaviorPlans(), owner, id, read))
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
        result = context->BehaviorPatches().ReadPlan(
            context->BehaviorPlans(), owner, PlanIdOf(plan), read);
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
        return ResultCode(context->BehaviorPatches().ClosePlan(
            context->BehaviorPlans(), owner, PlanIdOf(plan)));
    });
}

Status ReadScriptEdits(
    const BML_BehaviorScriptEdit *edits, std::uint32_t count,
    ModContext &context,
    std::vector<BML::Behavior::Internal::Patches::Rule> &out) {
    out.clear();
    if (!edits || !count)
        return InvalidValue("A Behavior Plan requires at least one Script rule.");
    try {
        out.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            const BML_BehaviorScriptEdit &source = edits[index];
            if (!HasStructSize(&source) || source.Reserved != 0 ||
                (source.StepCount && !source.Steps))
                return InvalidValue("A Script Edit descriptor is malformed.");
            TargetSet targets;
            switch (source.Targets) {
            case BML_BEHAVIOR_TARGETS_EACH: targets = TargetSet::Each; break;
            case BML_BEHAVIOR_TARGETS_ONE: targets = TargetSet::One; break;
            default:
                return InvalidValue("A Script Edit has an unknown cardinality.");
            }
            std::string script;
            if (!ReadString(source.Script, script) || script.empty())
                return InvalidValue("A Script Edit requires an exact name.");
            GraphEdit edit;
            EditProgram program;
            Status status = program.Build(source.Steps, source.StepCount,
                                          context, edit);
            if (!status)
                return status;
            out.push_back({ScriptSelection{std::move(script), targets},
                           std::make_shared<GraphEdit>(std::move(edit))});
        }
    } catch (const std::bad_alloc &) {
        return InvalidValue("The Loader could not retain the Behavior Plan rules.");
    }
    return {};
}

int BML_BEHAVIOR_CALL SetPlanActive(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    std::uint32_t active, BML_BehaviorPlanInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !plan || active > 1 ||
            (info && !HasStructSize(info)))
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
        result = context->BehaviorPatches().SetPlanActive(
            context->BehaviorPlans(), owner, PlanIdOf(plan), active != 0);
        WriteStatus(status, result);
        PlanInfo read;
        if (info && context->BehaviorPatches().ReadPlan(
                        context->BehaviorPlans(), owner,
                        PlanIdOf(plan), read))
            WritePlanInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ReplacePlan(
    BML_BehaviorSession session, BML_BehaviorPlan plan,
    const BML_BehaviorScriptEdit *edits, std::uint32_t editCount,
    BML_BehaviorPlanInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !plan ||
            (info && !HasStructSize(info)))
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
        std::vector<BML::Behavior::Internal::Patches::Rule> rules;
        result = ReadScriptEdits(edits, editCount, *context, rules);
        if (result)
            result = context->BehaviorPatches().ReplacePlan(
                context->BehaviorPlans(), owner, PlanIdOf(plan),
                std::move(rules));
        WriteStatus(status, result);
        PlanInfo read;
        if (info && context->BehaviorPatches().ReadPlan(
                        context->BehaviorPlans(), owner,
                        PlanIdOf(plan), read))
            WritePlanInfo(info, read);
        return ResultCode(result);
    });
}

Status ReadGraphEdits(
    const BML_BehaviorGraphEdit *edits, std::uint32_t count,
    ModContext &context,
    std::vector<BML::Behavior::Internal::Patches::Target> &out) {
    out.clear();
    if (!edits || !count)
        return InvalidValue("A Behavior Patch requires at least one Graph Edit.");
    try {
        out.reserve(count);
        std::set<std::uint32_t> publicHandles;
        for (std::uint32_t index = 0; index < count; ++index) {
            const BML_BehaviorGraphEdit &source = edits[index];
            if (!HasStructSize(&source) || source.Reserved != 0 ||
                !source.Graph.Domain ||
                (source.StepCount && !source.Steps))
                return InvalidValue("A Graph Edit descriptor is malformed.");
            GraphEdit edit;
            EditProgram program;
            Status status = program.Build(source.Steps, source.StepCount,
                                          context, edit);
            if (!status)
                return status;
            const auto localNodes = program.Nodes();
            BML::Behavior::Internal::Patches::HandleMap nodes;
            for (const auto &[handle, node] : localNodes) {
                if (handle > UINT32_MAX - source.HandleBase)
                    return InvalidValue("A composed Patch Node handle overflows.");
                const std::uint32_t publicHandle = handle + source.HandleBase;
                if (!publicHandles.insert(publicHandle).second)
                    return InvalidValue(
                        "Node handles must be unique across a composed Behavior Patch.");
                nodes.emplace(publicHandle, node);
            }
            BML::Behavior::Internal::Patches::Target target;
            target.Graph = {source.Graph.Domain, source.Graph.Slot,
                            source.Graph.Generation};
            target.Fingerprint = source.Fingerprint;
            target.Body = std::make_shared<GraphEdit>(std::move(edit));
            target.Handles = std::move(nodes);
            out.push_back(std::move(target));
        }
    } catch (const std::bad_alloc &) {
        return InvalidValue("The Loader could not retain the composed Behavior Patch.");
    }
    return {};
}

int BML_BEHAVIOR_CALL ApplyPatch(
    BML_BehaviorSession session, const BML_BehaviorPatchSpec *spec,
    BML_BehaviorPatch *outPatch, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(spec) || !outPatch ||
            (info && !HasStructSize(info)) || !spec->EditCount ||
            !spec->Edits)
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

        std::vector<BML::Behavior::Internal::Patches::Target> targets;
        result = ReadGraphEdits(spec->Edits, spec->EditCount, *context,
                                targets);
        if (!result) {
            WriteStatus(status, result);
            return ResultCode(result);
        }

        PatchId id = 0;
        result = context->BehaviorPatches().Apply(
            owner, std::move(name), std::move(targets), id);
        WriteStatus(status, result);
        if (id)
            *outPatch = PatchHandleOf(id);
        if (!result)
            return ResultCode(result);
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

int BML_BEHAVIOR_CALL SetPatchActive(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    std::uint32_t active, BML_BehaviorPatchInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !patch || active > 1 ||
            (info && !HasStructSize(info)))
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
        result = context->BehaviorPatches().SetActive(
            owner, PatchIdOf(patch), active != 0);
        WriteStatus(status, result);
        PatchInfo read;
        if (info && context->BehaviorPatches().Read(
                        owner, PatchIdOf(patch), read))
            WritePatchInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL ReplacePatch(
    BML_BehaviorSession session, BML_BehaviorPatch patch,
    const BML_BehaviorGraphEdit *edits, std::uint32_t editCount,
    BML_BehaviorPatchInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !patch ||
            (info && !HasStructSize(info)))
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
        std::vector<BML::Behavior::Internal::Patches::Target> targets;
        result = ReadGraphEdits(edits, editCount, *context, targets);
        if (result)
            result = context->BehaviorPatches().Replace(
                owner, PatchIdOf(patch), std::move(targets));
        WriteStatus(status, result);
        PatchInfo read;
        if (info && context->BehaviorPatches().Read(
                        owner, PatchIdOf(patch), read))
            WritePatchInfo(info, read);
        return ResultCode(result);
    });
}

int BML_BEHAVIOR_CALL Reference(BML_BehaviorSession session,
                                std::uint32_t object,
                                BML_ObjectRef *outReference,
                                BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !outReference)
            return BML_ERROR_INVALID_PARAMETER;
        *outReference = BML_ObjectRef{};
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
        CKContext *ck = context->GetCKContext();
        CKObject *found = ck && object
            ? ck->GetObject(static_cast<CK_ID>(object)) : nullptr;
        if (!found || found->IsToBeDeleted())
            return BML_ERROR_NOT_FOUND;
        const BML_ObjectRef issued = context->ObjectRefs().Issue(found);
        if (!issued.Domain)
            return BML_ERROR_FAIL;
        *outReference = issued;
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ResolvePatchNode(BML_BehaviorSession session,
                                      BML_BehaviorPatch patch,
                                      std::uint32_t handle,
                                      BML_ObjectRef *outNode,
                                      BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !patch || !outNode || !handle)
            return BML_ERROR_INVALID_PARAMETER;
        *outNode = BML_ObjectRef{};
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
        BML::Behavior::Internal::ObjectRef node;
        result = context->BehaviorPatches().ResolveNode(
            owner, PatchIdOf(patch), handle, node);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        *outNode = BML_ObjectRef{node.Domain, node.Slot, node.Generation};
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CreateScript(
    BML_BehaviorSession session, const BML_BehaviorScriptSpec *spec,
    BML_BehaviorScript *outScript, BML_BehaviorScriptInfo *info,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !HasStructSize(spec) ||
            !outScript || (info && !HasStructSize(info)) ||
            (spec->StepCount && !spec->Steps) || !spec->Owner.Domain)
            return BML_ERROR_INVALID_PARAMETER;
        *outScript = nullptr;
        std::string name;
        if (!ReadString(spec->Name, name))
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
        GraphEdit body;
        EditProgram program;
        result = program.Build(
            spec->Steps, spec->StepCount, *context, body);
        if (!result) {
            WriteStatus(status, result);
            return ResultCode(result);
        }
        CKBeObject *nativeOwner = ReadOwner(spec->Owner, *context, result);
        if (!nativeOwner) {
            WriteStatus(status, result);
            return BML_ERROR_OBJECT_INVALID;
        }
        ScriptResult opened = context->BehaviorScripts().Create(
            owner, SessionId(session), nativeOwner, std::move(name),
            spec->Priority, std::move(body));
        WriteStatus(status, opened.Result);
        if (!opened)
            return ResultCode(opened.Result);
        *outScript = ScriptHandle(opened.Id);
        WriteScriptInfo(info, opened.Info);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL ReadScript(
    BML_BehaviorSession session, BML_BehaviorScript script,
    BML_BehaviorScriptInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !script ||
            !HasStructSize(info))
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
        ScriptInfo read;
        result = context->BehaviorScripts().Read(
            owner, ScriptIdOf(script), read);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WriteScriptInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL SetScriptActive(
    BML_BehaviorSession session, BML_BehaviorScript script,
    std::uint32_t active, std::uint32_t reset,
    BML_BehaviorScriptInfo *info, BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!PrepareStatus(status) || !session || !script ||
            (info && !HasStructSize(info)) || active > 1 || reset > 1 ||
            (!active && reset))
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
        ScriptInfo read;
        result = context->BehaviorScripts().SetActive(
            owner, ScriptIdOf(script), active != 0, reset != 0, read);
        WriteStatus(status, result);
        if (!result)
            return ResultCode(result);
        WriteScriptInfo(info, read);
        return BML_OK;
    });
}

int BML_BEHAVIOR_CALL CloseScript(BML_BehaviorSession session,
                                  BML_BehaviorScript script) {
    return Guard([&] {
        if (!session || !script)
            return BML_ERROR_INVALID_HANDLE;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        SessionOwner owner;
        Status result;
        if (!ReadSessionOwner(session, *context, owner, result))
            return ResultCode(result);
        return ResultCode(context->BehaviorScripts().Close(
            owner, ScriptIdOf(script)));
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
    &Reference,
    &ResolvePatchNode,
    &AttachBlock,
    &CreateScript,
    &ReadScript,
    &SetScriptActive,
    &CloseScript,
    &SetPatchActive,
    &ReplacePatch,
    &SetPlanActive,
    &ReplacePlan,
};

} // namespace

namespace BML::Api {

const BML_BehaviorInterface &BehaviorInterface() noexcept {
    return kBehaviorInterface;
}

} // namespace BML::Api
