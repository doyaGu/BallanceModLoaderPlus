#include "Api/BehaviorApi.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "BML/ImcWire.hpp"
#include "Behavior/Sessions.h"
#include "Behavior/OutcomeStore.h"
#include "Loader/ModContext.h"

namespace {

using BML::Behavior::AdmissionState;
using BML::Behavior::Sessions;
using BML::Behavior::Error;
using BML::Behavior::ExecutionError;
using BML::Behavior::ExecutionOutcome;
using BML::Behavior::OutcomeRetention;
using BML::Behavior::Phase;
using BML::Behavior::Pout;
using BML::Behavior::PoutKind;
using BML::Behavior::RunInfo;
using BML::Behavior::RunKind;
using BML::Behavior::RunResult;
using BML::Behavior::RunState;
using BML::Behavior::Slot;
using BML::Behavior::SlotKind;
using BML::Behavior::Spec;
using BML::Behavior::Status;
using BML::Behavior::Value;

template <typename T>
bool HasStructSize(const T *value) noexcept {
    return value && value->StructSize >= sizeof(T);
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
    case Error::RequiredManagerMissing: return BML_BEHAVIOR_ERROR_REQUIRED_MANAGER_MISSING;
    case Error::CreateFailed: return BML_BEHAVIOR_ERROR_CREATION_FAILED;
    case Error::InitFailed: return BML_BEHAVIOR_ERROR_INITIALIZATION_FAILED;
    case Error::TargetInvalid: return BML_BEHAVIOR_ERROR_TARGET_INVALID;
    case Error::CallbackFailed: return BML_BEHAVIOR_ERROR_CALLBACK_FAILED;
    case Error::SlotNotFound: return BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND;
    case Error::AmbiguousSlot: return BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS;
    case Error::StaleLayout: return BML_BEHAVIOR_ERROR_LAYOUT_CHANGED;
    case Error::TypeMismatch: return BML_BEHAVIOR_ERROR_TYPE_MISMATCH;
    case Error::ValueWriteFailed:
    case Error::SourceInvalid:
    case Error::OperationInvalid: return BML_BEHAVIOR_ERROR_VALUE_INVALID;
    case Error::InvalidState: return BML_BEHAVIOR_ERROR_STATE_INVALID;
    case Error::UnsupportedBreak: return BML_BEHAVIOR_ERROR_BREAK_UNSUPPORTED;
    case Error::UnsupportedPout: return BML_BEHAVIOR_ERROR_POUT_UNSUPPORTED;
    case Error::PoutUnavailable: return BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE;
    case Error::OutcomeQueueFull: return BML_BEHAVIOR_ERROR_OUTCOME_LIMIT_REACHED;
    case Error::ExecutionCancelled: return BML_BEHAVIOR_ERROR_CANCELLED;
    case Error::WrongThread:
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
    case ExecutionError::OutputUnavailable:
    case ExecutionError::PoutReadFailed: return BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE;
    case ExecutionError::OutcomeQueueFull: return BML_BEHAVIOR_ERROR_OUTCOME_LIMIT_REACHED;
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
               Value &to, Status &status) {
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
    const auto plainValueType = [&](CKGUID base, std::size_t size) {
        if (type == base)
            return true;
        if (!parameters || !parameters->IsDerivedFrom(type, base))
            return false;
        CKParameterTypeDesc *description =
            parameters->GetParameterTypeDescription(type);
        return description && description->DefaultSize == size &&
               !description->CreateDefaultFunction &&
               !description->DeleteFunction &&
               !description->CopyFunction &&
               !description->SaveLoadFunction &&
               !description->CheckFunction;
    };
    const auto textType = [&] {
        return type == CKPGUID_STRING ||
               (parameters && parameters->IsDerivedFrom(type, CKPGUID_STRING));
    };
    const auto typeMatchesKind = [&] {
        switch (from.Kind) {
        case BML_BEHAVIOR_VALUE_BOOL: return plainValueType(CKPGUID_BOOL, sizeof(CKBOOL));
        case BML_BEHAVIOR_VALUE_INT32: return plainValueType(CKPGUID_INT, sizeof(std::int32_t));
        case BML_BEHAVIOR_VALUE_FLOAT32: return plainValueType(CKPGUID_FLOAT, sizeof(float));
        case BML_BEHAVIOR_VALUE_UTF8: return textType();
        case BML_BEHAVIOR_VALUE_VEC2: return plainValueType(CKPGUID_2DVECTOR, sizeof(BML_Vec2));
        case BML_BEHAVIOR_VALUE_VEC3: return plainValueType(CKPGUID_VECTOR, sizeof(BML_Vec3));
        case BML_BEHAVIOR_VALUE_QUATERNION: return plainValueType(CKPGUID_QUATERNION, sizeof(BML_Quaternion));
        case BML_BEHAVIOR_VALUE_EULER: return plainValueType(CKPGUID_EULERANGLES, sizeof(BML_Euler));
        case BML_BEHAVIOR_VALUE_RECT: return plainValueType(CKPGUID_RECT, sizeof(BML_Rect));
        case BML_BEHAVIOR_VALUE_COLOR: return plainValueType(CKPGUID_COLOR, sizeof(BML_Color));
        case BML_BEHAVIOR_VALUE_BOX: return plainValueType(CKPGUID_BOX, sizeof(BML_Box));
        case BML_BEHAVIOR_VALUE_MAT4: return plainValueType(CKPGUID_MATRIX, sizeof(BML_Mat4));
        case BML_BEHAVIOR_VALUE_OBJECT:
            return type == CKPGUID_OBJECT ||
                   (parameters && parameters->IsDerivedFrom(type, CKPGUID_OBJECT));
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
        to = RawValue(type, from.Data.Vec2);
        return true;
    case BML_BEHAVIOR_VALUE_VEC3:
        to = RawValue(type, from.Data.Vec3);
        return true;
    case BML_BEHAVIOR_VALUE_QUATERNION:
        to = RawValue(type, from.Data.Quaternion);
        return true;
    case BML_BEHAVIOR_VALUE_EULER:
        to = RawValue(type, from.Data.Euler);
        return true;
    case BML_BEHAVIOR_VALUE_RECT:
        to = RawValue(type, from.Data.Rect);
        return true;
    case BML_BEHAVIOR_VALUE_COLOR:
        to = RawValue(type, from.Data.Color);
        return true;
    case BML_BEHAVIOR_VALUE_BOX:
        to = RawValue(type, from.Data.Box);
        return true;
    case BML_BEHAVIOR_VALUE_MAT4:
        to = RawValue(type, from.Data.Mat4);
        return true;
    case BML_BEHAVIOR_VALUE_OBJECT: {
        CKObject *object = context.ObjectRefs().Resolve(from.Data.Object);
        if (from.Data.Object.Domain && !object) {
            status = {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                      CKBR_PARAMETERERROR,
                      "A Behavior object value is stale."};
            return false;
        }
        to = Value::Object(type, object);
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
        Value value;
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
        from.Outcomes.StructSize < sizeof(from.Outcomes)) {
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

    switch (from.Outcomes.Kind) {
    case BML_BEHAVIOR_RETENTION_SIGNALS:
        if (!from.Outcomes.Limit) {
            status = InvalidValue("signals(n) requires a nonzero Outcome limit.");
            return false;
        }
        to.Outcomes(OutcomeRetention::Signals(from.Outcomes.Limit));
        break;
    case BML_BEHAVIOR_RETENTION_EACH_FRAME:
        if (!from.Outcomes.Limit) {
            status = InvalidValue("eachFrame(n) requires a nonzero Outcome limit.");
            return false;
        }
        to.Outcomes(OutcomeRetention::EachFrame(from.Outcomes.Limit));
        break;
    case BML_BEHAVIOR_RETENTION_LATEST:
        to.Outcomes(OutcomeRetention::Latest());
        break;
    case BML_BEHAVIOR_RETENTION_NONE:
        to.Outcomes(OutcomeRetention::Ignore());
        break;
    default:
        status = InvalidValue("The Behavior Outcome retention kind is unknown.");
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
    case RunState::Completed: return BML_BEHAVIOR_RUN_COMPLETED;
    case RunState::Pending: return BML_BEHAVIOR_RUN_PENDING;
    case RunState::Queued: return BML_BEHAVIOR_RUN_QUEUED;
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
    out->Status.StructSize = sizeof(out->Status);
    WriteStatus(&out->Status, info.LastStatus);
}

bool ValidOutputs(BML_BehaviorRunInfo *info,
                  BML_BehaviorStatus *status) noexcept {
    return (!info || HasStructSize(info)) &&
           (!status || HasStructSize(status));
}

std::uintptr_t SessionId(BML_BehaviorSession session) noexcept {
    return reinterpret_cast<std::uintptr_t>(session);
}

std::uintptr_t RunId(BML_BehaviorRun run) noexcept {
    return reinterpret_cast<std::uintptr_t>(run);
}

BML_BehaviorSession SessionHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorSession>(id);
}

BML_BehaviorRun RunHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorRun>(id);
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
        if (!outSession || (status && !HasStructSize(status)))
            return BML_ERROR_INVALID_PARAMETER;
        *outSession = nullptr;
        std::string requested;
        if (!ReadString(requestedOwner, requested))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context || !context->AreModsLoaded())
            return BML_ERROR_FROZEN;
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
    if (!session || !block || !outRun || !ValidOutputs(info, status) ||
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
        if (!run || !ValidOutputs(info, status))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        RunResult result = context->BehaviorSessions().Continue(RunId(run));
        WriteStatus(status, result.Outcome);
        RunInfo current;
        const Status read = context->BehaviorSessions().ReadRun(RunId(run), current);
        if (read)
            WriteRunInfo(info, current);
        return ResultCode(result.Outcome);
    });
}

int BML_BEHAVIOR_CALL Pulse(BML_BehaviorRun run,
                           const BML_BehaviorSelector *input,
                           std::uint32_t *admission,
                           BML_BehaviorRunInfo *info,
                           BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!run || !input || !admission || !ValidOutputs(info, status))
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
        WriteStatus(status, result.Outcome);
        if (result.Admission == AdmissionState::Failed)
            return ResultCode(result.Outcome);
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
        if (!run || !info || !ValidOutputs(info, status))
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

class OutcomeBatch final {
public:
    bool Add(const ExecutionOutcome &outcome) {
        BML_BehaviorOutcomeHeader header{};
        header.StructSize = sizeof(header);
        header.Sequence = outcome.Sequence;
        header.Frame = outcome.Frame;
        header.NativeResult = outcome.ReturnCode;
        if (outcome.NativeContinuation)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_NATIVE;
        if (outcome.GraphActive)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_GRAPH_ACTIVE;
        if (outcome.QueuedInput)
            header.Continuation |= BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT;
        header.Terminal = outcome.Terminal ? 1u : 0u;
        header.Error = PublicError(outcome.Fault.Code);

        if (!AddOuts(outcome, header) || !AddPouts(outcome, header) ||
            !AddDiagnostic(outcome, header))
            return false;
        Headers.push_back(header);
        Sequences.push_back(outcome.Sequence);
        return true;
    }

    std::vector<BML_BehaviorOutcomeHeader> Headers;
    std::vector<std::uint8_t> Payload;
    std::vector<std::uint64_t> Sequences;

private:
    bool Align(std::size_t alignment) {
        const std::size_t remainder = Payload.size() % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (Payload.size() > UINT32_MAX ||
            padding > UINT32_MAX - Payload.size())
            return false;
        Payload.insert(Payload.end(), padding, 0);
        return true;
    }

    bool Append(const void *data, std::size_t size, std::uint32_t &offset) {
        if (Payload.size() > UINT32_MAX || size > UINT32_MAX ||
            size > UINT32_MAX - Payload.size())
            return false;
        offset = static_cast<std::uint32_t>(Payload.size());
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        Payload.insert(Payload.end(), bytes, bytes + size);
        return true;
    }

    template <typename T>
    bool ReserveRecords(std::size_t count, std::uint32_t &offset) {
        if (!Align(alignof(T)) || count > UINT32_MAX / sizeof(T))
            return false;
        const std::size_t bytes = count * sizeof(T);
        if (Payload.size() > UINT32_MAX ||
            bytes > UINT32_MAX - Payload.size())
            return false;
        offset = static_cast<std::uint32_t>(Payload.size());
        Payload.resize(Payload.size() + bytes, 0);
        return true;
    }

    template <typename T>
    void StoreRecord(std::uint32_t base, std::size_t index,
                     const T &record) {
        std::memcpy(Payload.data() + base + index * sizeof(T),
                    &record, sizeof(record));
    }

    bool AddOuts(const ExecutionOutcome &outcome,
                 BML_BehaviorOutcomeHeader &header) {
        if (outcome.ActiveOutputs.empty())
            return true;
        header.OutCount = static_cast<std::uint32_t>(outcome.ActiveOutputs.size());
        if (!ReserveRecords<BML_BehaviorOutRecord>(
                outcome.ActiveOutputs.size(), header.OutOffset))
            return false;
        for (std::size_t index = 0; index < outcome.ActiveOutputs.size(); ++index) {
            const auto &out = outcome.ActiveOutputs[index];
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

    bool AddPouts(const ExecutionOutcome &outcome,
                  BML_BehaviorOutcomeHeader &header) {
        if (outcome.Pouts.empty())
            return true;
        header.PoutCount = static_cast<std::uint32_t>(outcome.Pouts.size());
        if (!ReserveRecords<BML_BehaviorPoutRecord>(
                outcome.Pouts.size(), header.PoutOffset))
            return false;
        for (std::size_t index = 0; index < outcome.Pouts.size(); ++index) {
            const Pout &pout = outcome.Pouts[index];
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
        std::uint8_t bytes[64]{};
        std::size_t size = 0;
        switch (pout.Kind) {
        case PoutKind::Bool:
            bytes[0] = pout.Int32 ? 1 : 0;
            size = 1;
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

    bool AddDiagnostic(const ExecutionOutcome &outcome,
                       BML_BehaviorOutcomeHeader &header) {
        if (!outcome.Fault)
            return true;
        header.DiagnosticCount = 1;
        if (!ReserveRecords<BML_BehaviorDiagnosticRecord>(
                1, header.DiagnosticOffset))
            return false;
        BML_BehaviorDiagnosticRecord record{};
        record.StructSize = sizeof(record);
        record.Error = PublicError(outcome.Fault.Code);
        record.Phase = BML_BEHAVIOR_PHASE_EXECUTION;
        record.NativeResult = outcome.Fault.NativeCode;
        record.MessageLength = static_cast<std::uint32_t>(
            outcome.Fault.Message.size());
        if (!Append(outcome.Fault.Message.data(), outcome.Fault.Message.size(),
                    record.MessageOffset))
            return false;
        StoreRecord(header.DiagnosticOffset, 0, record);
        return true;
    }
};

int BML_BEHAVIOR_CALL DrainOutcomes(
    BML_BehaviorRun run, BML_BehaviorOutcomeHeader *headers,
    std::uint32_t headerCapacity, std::uint32_t headerStride,
    void *payload, std::uint32_t payloadCapacity,
    std::uint32_t *outHeaderCount, std::uint32_t *outPayloadSize,
    BML_BehaviorStatus *status) {
    return Guard([&] {
        if (!run || !outHeaderCount || !outPayloadSize ||
            (status && !HasStructSize(status)) ||
            (headerCapacity && (!headers || headerStride < sizeof(*headers))) ||
            (payloadCapacity && !payload))
            return BML_ERROR_INVALID_PARAMETER;
        ModContext *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        std::shared_ptr<BML::Behavior::OutcomeStore> store =
            context->BehaviorSessions().Outcomes(RunId(run));
        if (!store)
            return BML_ERROR_INVALID_HANDLE;

        OutcomeBatch batch;
        for (const ExecutionOutcome &outcome : store->Read()) {
            if (!batch.Add(outcome))
                return BML_ERROR_OUT_OF_MEMORY;
        }
        if (batch.Headers.size() > UINT32_MAX || batch.Payload.size() > UINT32_MAX)
            return BML_ERROR_OUT_OF_MEMORY;
        *outHeaderCount = static_cast<std::uint32_t>(batch.Headers.size());
        *outPayloadSize = static_cast<std::uint32_t>(batch.Payload.size());
        WriteStatus(status, {});
        if (headerCapacity < batch.Headers.size() ||
            payloadCapacity < batch.Payload.size())
            return BML_ERROR_BUFFER_TOO_SMALL;

        auto *headerBytes = reinterpret_cast<std::uint8_t *>(headers);
        for (std::size_t index = 0; index < batch.Headers.size(); ++index)
            std::memcpy(headerBytes + index * headerStride,
                        &batch.Headers[index], sizeof(batch.Headers[index]));
        if (!batch.Payload.empty())
            std::memcpy(payload, batch.Payload.data(), batch.Payload.size());
        if (!store->Consume(batch.Sequences))
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
    &DrainOutcomes,
    &CloseRun,
};

} // namespace

namespace BML::Api {

const BML_BehaviorInterface &BehaviorInterface() noexcept {
    return kBehaviorInterface;
}

} // namespace BML::Api
