#include "Api/Behavior/Codec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "BML/TypeConvert.h"
#include "Loader/ModContext.h"

namespace BML::Api::Behavior {
namespace {

using BML::Behavior::Internal::DetachedCompatibility;
using BML::Behavior::Internal::PatchState;
using BML::Behavior::Internal::Phase;
using BML::Behavior::Internal::PlanState;
using BML::Behavior::Internal::RunKind;
using BML::Behavior::Internal::RunState;
using BML::Behavior::Internal::ScriptState;
using BML::Behavior::Internal::Value;
using BML::Behavior::Internal::WatchKind;

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
    std::string storage;
    std::string_view text = message;
    try {
        text = Utf8Text(message, storage);
    } catch (...) {
    }
    out.MessageLength = text.size() > UINT32_MAX
        ? UINT32_MAX : static_cast<std::uint32_t>(text.size());
    std::size_t count = (std::min)(
        text.size(), static_cast<std::size_t>(BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY - 1));
    // A truncated message ends on a whole UTF-8 sequence.
    if (count < text.size()) {
        while (count && (static_cast<unsigned char>(text[count]) & 0xc0u) == 0x80u)
            --count;
    }
    if (count)
        std::memcpy(out.Message, text.data(), count);
    out.Message[count] = '\0';
}

template <typename T>
Value RawValue(CKGUID type, const T &value) {
    return Value::Raw(type, &value, sizeof(value));
}

bool ReadParameterTypes(const BML_BehaviorParameterType *types,
                        std::uint32_t count, SlotKind kind,
                        ModContext &context, BlockSpec &block,
                        Status &status) {
    if (count && !types) {
        status = InvalidValue("A Behavior parameter type array is missing.");
        return false;
    }
    CKParameterManager *parameters = context.GetParameterManager();
    for (std::uint32_t index = 0; index < count; ++index) {
        const BML_BehaviorParameterType &parameter = types[index];
        if (parameter.StructSize < sizeof(parameter)) {
            status = InvalidValue(
                "A Behavior parameter type has an unsupported StructSize.");
            return false;
        }
        const CKGUID type = Guid(parameter.Type);
        if (!type.IsValid() || !parameters ||
            parameters->ParameterGuidToType(type) < 0) {
            status = {Error::ParameterTypeUnavailable,
                      CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                      "The selected Virtools parameter type is not registered."};
            status.Details.Stage = Phase::ParameterBinding;
            status.Details.ActualType = type;
            return false;
        }
        Slot slot;
        if (!ReadSelector(parameter.Slot, kind, CKGUID(), slot, status))
            return false;
        if (kind == SlotKind::InputParameter)
            block.PinType(std::move(slot), type);
        else
            block.PoutType(std::move(slot), type);
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

std::uint32_t PublicScriptState(ScriptState state) noexcept {
    switch (state) {
    case ScriptState::Ready: return BML_BEHAVIOR_SCRIPT_READY;
    case ScriptState::Closing: return BML_BEHAVIOR_SCRIPT_CLOSING;
    case ScriptState::Failed: return BML_BEHAVIOR_SCRIPT_FAILED;
    }
    return BML_BEHAVIOR_SCRIPT_FAILED;
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

} // namespace

bool FitsStrided(std::size_t count, std::size_t stride,
                 std::size_t recordSize) noexcept {
    return count == 0 ||
        (stride != 0 && count - 1 <=
            ((std::numeric_limits<std::size_t>::max)() - recordSize) /
                stride);
}

bool ReadString(BML_BehaviorString value, std::string &out,
                bool allowNul) {
    if ((!value.Data && value.Length) ||
        !IsUtf8(value.Data, value.Length))
        return false;
    if (!allowNul && value.Length &&
        std::memchr(value.Data, '\0', value.Length))
        return false;
    out.assign(value.Data ? value.Data : "", value.Length);
    return true;
}

bool ReadNativeString(BML_BehaviorString value, std::string &out) {
    if (!ReadString(value, out))
        return false;
    out = NativeText(out);
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
    return PublicError(ToError(error));
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
        if (!ReadNativeString(from.Name, name) || name.empty() || from.Occurrence < 0) {
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
        if (!ReadNativeString(from.Data.Utf8, value)) {
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
        (from.LocalCount && !from.Locals) ||
        (from.PinTypeCount && !from.PinTypes) ||
        (from.PoutTypeCount && !from.PoutTypes)) {
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
                      context, to, status) ||
        !ReadParameterTypes(from.PinTypes, from.PinTypeCount,
                            SlotKind::InputParameter, context, to, status) ||
        !ReadParameterTypes(from.PoutTypes, from.PoutTypeCount,
                            SlotKind::OutputParameter, context, to, status))
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
    if ((to.MatchName && !ReadNativeString(from.Name, to.Name)) ||
        (to.MatchCategory && !ReadNativeString(from.Category, to.Category)) ||
        (to.MatchProvider && !ReadNativeString(from.Provider, to.Provider)) ||
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

void WritePlanInfo(BML_BehaviorPlanInfo *out, const PlanInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicPlanState(info.State);
    out->World = info.World;
    out->Matches = Count(info.Matches);
    out->Instances = Count(info.Instances);
    out->LastStatus.StructSize = sizeof(out->LastStatus);
    WriteStatus(&out->LastStatus, info.LastStatus);
}

void WriteFailures(BML_BehaviorFailures *out, const Status &apply,
                   const Status &restore) noexcept {
    *out = {};
    out->StructSize = sizeof(*out);
    out->Apply.StructSize = sizeof(out->Apply);
    out->Restore.StructSize = sizeof(out->Restore);
    WriteStatus(&out->Apply, apply);
    WriteStatus(&out->Restore, restore);
}

void WritePatchInfo(BML_BehaviorPatchInfo *out,
                    const PatchInfo &info) noexcept {
    if (!out)
        return;
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicPatchState(info.State);
    out->Conflicts = Count(info.Conflicts.size());
    out->LastStatus.StructSize = sizeof(out->LastStatus);
    WriteStatus(&out->LastStatus, info.LastStatus);
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

bool ReadDataSlotKind(std::uint32_t kind, SlotKind &out) noexcept {
    return ReadSlotKind(kind, out) && out != SlotKind::Input &&
           out != SlotKind::Output;
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

bool ReadPortQuery(
    const BML_BehaviorPortRef &from,
    Installations::PortQuery &out,
    Status &status) {
    out = Installations::PortQuery();
    if (from.StructSize < sizeof(from) || !from.Binding || !from.Graph ||
        !from.Handle) {
        status = InvalidValue("A Plan instance Port reference is malformed.");
        return false;
    }
    out.Binding = from.Binding;
    out.Scope = from.Graph;
    out.Handle = from.Handle;
    if (from.Kind == 0)
        return true;

    SlotKind kind;
    if (!ReadDataSlotKind(from.Kind, kind)) {
        status = InvalidValue(
            "A Plan instance value requires a Target, Pin, Pout, Setting, or Local Port.");
        return false;
    }
    Slot selector;
    if (!ReadSelector(from.Slot, kind, Guid(from.Type), selector, status))
        return false;
    out.Selector = std::move(selector);
    return true;
}

bool ReadNodeRef(const BML_BehaviorNodeRef &from,
                 Installations::SymbolRef &out,
                 Status &status) {
    if (from.StructSize < sizeof(from) || !from.Binding || !from.Graph ||
        !from.Handle) {
        status = InvalidValue("A Plan instance Node reference is malformed.");
        return false;
    }
    out.Binding = from.Binding;
    out.Scope = from.Graph;
    out.Handle = from.Handle;
    return true;
}

bool ReadPlanInstance(
    const BML_BehaviorPlanInstance &from,
    Installations::PlanInstance &out,
    Status &status) {
    if (from.StructSize < sizeof(from) || !from.Identity || !from.Binding ||
        !from.PlanRevision || !from.Revision || !from.World ||
        !from.Script.Domain) {
        status = InvalidValue(
            "A Behavior Plan instance snapshot is malformed.");
        return false;
    }
    out.Rule = from.Rule;
    out.PlanRevision = from.PlanRevision;
    out.Binding = from.Binding;
    out.Value.Target = {from.Script.Domain, from.Script.Slot,
                        from.Script.Generation};
    out.Value.Id = from.Identity;
    out.Value.World = from.World;
    out.Value.Revision = from.Revision;
    return true;
}

BML_BehaviorPlanInstance WritePlanInstance(
    const Installations::PlanInstance &from) noexcept {
    BML_BehaviorPlanInstance out{};
    out.StructSize = sizeof(out);
    out.Rule = from.Rule;
    out.PlanRevision = from.PlanRevision;
    out.Binding = from.Binding;
    out.World = from.Value.World;
    out.Revision = from.Value.Revision;
    out.Identity = from.Value.Id;
    out.Script = {from.Value.Target.Domain, from.Value.Target.Slot,
                  from.Value.Target.Generation};
    return out;
}

int WritePlanInstances(const std::vector<Installations::PlanInstance> &found,
                       BML_BehaviorPlanInstance *instances,
                       std::uint32_t instanceCapacity,
                       std::uint32_t instanceStride,
                       std::uint32_t *outInstanceCount) noexcept {
    if (found.size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outInstanceCount = static_cast<std::uint32_t>(found.size());
    if (instanceCapacity < found.size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    if (!FitsStrided(found.size(), instanceStride,
                     sizeof(BML_BehaviorPlanInstance)))
        return BML_ERROR_OUT_OF_MEMORY;

    auto *bytes = reinterpret_cast<std::uint8_t *>(instances);
    for (std::size_t index = 0; index < found.size(); ++index) {
        const BML_BehaviorPlanInstance record =
            WritePlanInstance(found[index]);
        std::memcpy(bytes + index * instanceStride,
                    &record, sizeof(record));
    }
    return BML_OK;
}

int ReadWatchSpec(const BML_BehaviorWatchSpec &from, ModContext &context,
                  WatchSpec &spec, CKBehavior *&root, CKBehavior *&node,
                  Status &status) {
    switch (from.Kind) {
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
    if (from.View == BML_BEHAVIOR_GRAPH_LOGICAL)
        spec.View = GraphView::Logical;
    else if (from.View == BML_BEHAVIOR_GRAPH_LIVE)
        spec.View = GraphView::Live;
    else
        return BML_ERROR_INVALID_PARAMETER;
    if (from.Read != BML_BEHAVIOR_READ_NON_FORCING)
        return BML_ERROR_INVALID_PARAMETER;

    root = nullptr;
    node = nullptr;
    if (spec.Kind == WatchKind::GraphChanged) {
        root = ReadBehavior(from.Root, context, status);
        if (!root)
            return ResultCode(status);
    } else {
        node = ReadBehavior(from.Node, context, status);
        if (!node)
            return ResultCode(status);
    }
    if (spec.Kind == WatchKind::SampledValueChanged) {
        spec.LayoutGeneration = from.LayoutGeneration;
        SlotKind kind;
        if (!ReadDataSlotKind(from.SlotKind, kind))
            return BML_ERROR_INVALID_PARAMETER;
        if ((from.Slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
             from.LayoutGeneration == 0) ||
            !ReadSelector(from.Slot, kind, CKGUID(), spec.ValueSlot,
                          status)) {
            if (status)
                status = InvalidValue(
                    "An indexed watched port requires its Layout generation.");
            return BML_ERROR_INVALID_PARAMETER;
        }
    } else if (spec.Kind == WatchKind::LayoutChanged) {
        if (!from.LayoutGeneration)
            return BML_ERROR_INVALID_PARAMETER;
        spec.LayoutGeneration = from.LayoutGeneration;
    }
    return BML_OK;
}

} // namespace BML::Api::Behavior
