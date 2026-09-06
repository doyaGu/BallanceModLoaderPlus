#include "Behavior/Runtime.h"

#include <algorithm>
#include <sstream>

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message, CKERROR ckError = CK_OK,
                       int behaviorResult = CKBR_OK,
                       Phase phase = Phase::None,
                       CKGUID prototype = CKGUID()) {
    Status status{error, ckError, behaviorResult, std::move(message)};
    status.Details.Stage = phase;
    status.Details.Prototype = prototype;
    return status;
}

Status SlotFailure(Error error, std::string message,
                           const Slot &selector, CKGUID actualType = CKGUID(),
                           CKERROR ckError = CK_OK) {
    Status status = Failure(error, std::move(message), ckError, CKBR_OK,
                                    Phase::ParameterBinding);
    status.Details.Selector = selector;
    status.Details.ActualType = actualType;
    return status;
}

Status Annotate(Status status, Phase phase,
                        CKGUID prototype, const Slot *selector = nullptr) {
    if (!status) {
        status.Details.Stage = phase;
        status.Details.Prototype = prototype;
        if (selector)
            status.Details.Selector = *selector;
    }
    return status;
}

const char *SafeName(CKObject *object) {
    return object && object->GetName() ? object->GetName() : "<unnamed>";
}

bool IsExecutionError(int result) {
    return result == CKBR_ATTACHFAILED || result == CKBR_DETACHFAILED ||
           result == CKBR_LOCKED || result == CKBR_INFINITELOOP ||
           result >= CKBR_GENERICERROR;
}

bool HasContinuation(int result) {
    return (result & CKBR_ACTIVATENEXTFRAME) != 0;
}

Status ExecutionFailure(const ExecutionFault &fault,
                        CKGUID prototype = CKGUID()) {
    Error error = Error::ExecutionFailed;
    switch (fault.Code) {
    case ExecutionError::SelectorNotFound:
        error = Error::SlotNotFound;
        break;
    case ExecutionError::SelectorAmbiguous:
        error = Error::AmbiguousSlot;
        break;
    case ExecutionError::LayoutStale:
        error = Error::StaleLayout;
        break;
    case ExecutionError::UnsupportedBreak:
        error = Error::UnsupportedBreak;
        break;
    case ExecutionError::UnsupportedPout:
        error = Error::UnsupportedPout;
        break;
    case ExecutionError::PoutReadFailed:
        error = Error::PoutUnavailable;
        break;
    case ExecutionError::OutUnavailable:
        error = Error::ExecutionFailed;
        break;
    case ExecutionError::FrameQueueFull:
        error = Error::FrameQueueFull;
        break;
    case ExecutionError::Cancelled:
        error = Error::ExecutionCancelled;
        break;
    case ExecutionError::InvalidState:
        error = Error::InvalidState;
        break;
    default:
        break;
    }
    return Failure(error, fault.Message, CK_OK, fault.NativeCode,
                   Phase::Execution, prototype);
}

bool ContainsId(const CK_ID *ids, int count, CK_ID reference) {
    if (!ids || reference == 0)
        return false;
    for (int i = 0; i < count; ++i) {
        if (ids[i] == reference)
            return true;
    }
    return false;
}

Slot LiveSelector(const SlotInfo &slot) {
    if (!slot.Name.empty())
        return Slot::OccurrenceOf(slot.Kind, slot.Name, slot.Occurrence,
                                  slot.Type);
    if (slot.Kind == SlotKind::Target)
        return Slot::Only(slot.Kind, slot.Type);
    return Slot::At(slot.Kind, slot.Index, slot.Type);
}

std::string GuidText(CKGUID guid) {
    std::ostringstream stream;
    stream << std::hex << "0x" << guid.d1 << ":0x" << guid.d2;
    return stream.str();
}

class BehaviorContextScope final {
public:
    BehaviorContextScope(CKContext *context, CKBehavior *behavior,
                         const CKBehaviorContext *frame)
        : m_Context(context), m_SavedContext(context->m_BehaviorContext),
          m_Manager(context->GetBehaviorManager()),
          m_SavedCurrent(m_Manager ? m_Manager->m_CurrentBehavior : nullptr) {
        if (frame)
            m_Context->m_BehaviorContext = *frame;
        m_Context->m_BehaviorContext.Context = m_Context;
        m_Context->m_BehaviorContext.Behavior = behavior;
        if (m_Manager)
            m_Manager->m_CurrentBehavior = behavior;
    }

    ~BehaviorContextScope() {
        m_Context->m_BehaviorContext = m_SavedContext;
        if (m_Manager)
            m_Manager->m_CurrentBehavior = m_SavedCurrent;
    }

private:
    CKContext *m_Context;
    CKBehaviorContext m_SavedContext;
    CKBehaviorManager *m_Manager;
    CKBehavior *m_SavedCurrent;
};

class BehaviorExecutionScope final {
public:
    BehaviorExecutionScope(CKContext *context, CKBehavior *behavior,
                           const CKBehaviorContext *frame)
        : m_BehaviorContext(context, behavior, frame), m_Context(context),
          m_SavedDeferDestroy(context->m_DeferDestroyObjects) {
        m_Context->m_DeferDestroyObjects = TRUE;
    }

    ~BehaviorExecutionScope() {
        m_Context->m_DeferDestroyObjects = m_SavedDeferDestroy;
    }

private:
    BehaviorContextScope m_BehaviorContext;
    CKContext *m_Context;
    CKDWORD m_SavedDeferDestroy;
};

class FlagScope final {
public:
    explicit FlagScope(bool &flag) : m_Flag(flag) { m_Flag = true; }
    ~FlagScope() { m_Flag = false; }

private:
    bool &m_Flag;
};

class CKBehaviorAccess final : public CKBehavior {
public:
    static BehaviorBlockData *BlockData(CKBehavior *behavior) {
        BehaviorBlockData *CKBehavior::*member = &CKBehaviorAccess::m_BlockData;
        return behavior ? behavior->*member : nullptr;
    }
};

} // namespace

class Runtime::NativeAdapter final : public ExecutionAdapter {
public:
    NativeAdapter(Runtime &runtime, std::uint64_t instanceId,
                  const CKBehaviorContext *frame)
        : m_Runtime(runtime), m_InstanceId(instanceId), m_Frame(frame) {}

    bool Resolve(const ExecutionInput &input, ResolvedInput &resolved,
                 ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            fault = {ExecutionError::InvalidState, CKBR_BEHAVIORERROR,
                     "Behavior instance has expired."};
            return false;
        }

        if (input.Selector == InputSelector::Index) {
            if (input.LayoutGeneration != record->LayoutGeneration) {
                fault = {ExecutionError::LayoutStale, CKBR_PARAMETERERROR,
                         "Queued behavior input belongs to an older live layout."};
                return false;
            }
            if (input.Index < 0 || input.Index >= behavior->GetInputCount()) {
                fault = {ExecutionError::SelectorNotFound, CKBR_PARAMETERERROR,
                         "Behavior input index no longer exists."};
                return false;
            }
            resolved.Index = input.Index;
            return true;
        }

        Slot selector = input.RequireUnique
            ? Slot::Named(SlotKind::Input, input.Name)
            : Slot::OccurrenceOf(SlotKind::Input, input.Name, input.Occurrence);
        SlotInfo slot;
        Status status = m_Runtime.Resolve(behavior, selector, slot);
        if (!status) {
            fault = FromStatus(status);
            return false;
        }
        resolved.Index = slot.NativeIndex;
        return true;
    }

    bool Activate(const ResolvedInput &input, ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior || input.Index < 0 ||
            input.Index >= behavior->GetInputCount()) {
            // Earlier inputs of a queued batch may already be activated; a
            // partially activated OwnerDriven Block must not stay visible to
            // the parent-graph scheduler while the Run fails.
            if (behavior && record && record->GraphResident &&
                record->OwnerDriven)
                behavior->Activate(FALSE, FALSE);
            fault = {ExecutionError::ActivationFailed, CKBR_PARAMETERERROR,
                     "Resolved behavior input is no longer valid."};
            return false;
        }
        behavior->Activate(TRUE, FALSE);
        behavior->ActivateInput(input.Index, TRUE);
        return true;
    }

    NativeExecution Execute() override {
        NativeExecution result;
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            result.Fault = {ExecutionError::InvalidState, CKBR_BEHAVIORERROR,
                            "Behavior instance disappeared before execution."};
            return result;
        }

        // The host sets CKBEHAVIOR_ACTIVE when it schedules a Block: the
        // engine's graph loop does it in CheckIOsActivation and
        // FindNextBehaviorsToExecute, and ExecuteFunction only ever clears
        // the flag, never sets it.  The Run is an OwnerDriven Block's host,
        // so a continuation must set the flag before the native call or the
        // post-Execute state reads as finished.
        const bool hidden = record->GraphResident && record->OwnerDriven;
        if (hidden)
            behavior->Activate(TRUE, FALSE);

        const bool dynamicLayout = IsLayoutDynamic(behavior);
        const std::uint64_t layoutBefore =
            dynamicLayout ? LayoutIdentity(behavior) : 0;
        result.ReturnCode = m_Runtime.ExecuteNative(behavior, m_Frame);
        result.Retry = HasContinuation(result.ReturnCode);
        result.Error = IsExecutionError(result.ReturnCode);
        result.Break = result.ReturnCode == CKBR_BREAK;

        record = m_Runtime.FindRecord(m_InstanceId);
        if (!record || record->Expired ||
            m_Runtime.ResolveBehavior(*record) != behavior) {
            result.Fault = {ExecutionError::NativeFailed, result.ReturnCode,
                            "Building Block destroyed itself during execution."};
            return result;
        }
        if (dynamicLayout && LayoutIdentity(behavior) != layoutBefore)
            ++record->LayoutGeneration;
        // Ballanced's ExecuteFunction clears CKBEHAVIOR_ACTIVE unless the
        // Block asks for the next frame, while CheckBehaviorActivity keeps
        // the parent active from delayed links and active/waiting
        // sub-behaviors.  The post-Execute flag is therefore the native
        // continuation truth for both representations.
        result.Active = behavior->IsActive() != FALSE;
        // And the Run clears it again: Ballanced's CheckIOsActivation also
        // schedules every ACTIVE sub-behavior whether or not a Link reaches
        // it, so an OwnerDriven Block left ACTIVE would be executed by its
        // parent graph as well as by ProcessTasks.  The continuation truth
        // is captured above and the Run keeps the continuation, so the
        // Block stays hidden from the parent graph's scheduler.  The input
        // IOs stay as the Block left them: a waiting Block such as
        // WaitForAll keeps its reached inputs active across frames and
        // clears them itself when it completes.
        if (hidden)
            behavior->Activate(FALSE, FALSE);
        return result;
    }

    bool ReadOutputs(std::vector<ExecutionOutput> &activeOutputs,
                     ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            fault = {ExecutionError::OutUnavailable, CKBR_BEHAVIORERROR,
                     "Behavior disappeared before its outputs were read."};
            return false;
        }
        for (int index = 0; index < behavior->GetOutputCount(); ++index) {
            if (!behavior->IsOutputActive(index))
                continue;
            CKBehaviorIO *io = behavior->GetOutput(index);
            const std::string name = io && io->GetName() ? io->GetName() : "";
            activeOutputs.push_back(
                {index, name, Occurrence(behavior, index, name, false)});
        }
        return true;
    }

    bool ReadPouts(std::vector<Pout> &pouts,
                   ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            fault = {ExecutionError::PoutReadFailed, CKBR_BEHAVIORERROR,
                     "Behavior disappeared before its Pouts were read."};
            return false;
        }
        for (int index = 0; index < behavior->GetOutputParameterCount(); ++index) {
            CKParameterOut *parameter = behavior->GetOutputParameter(index);
            if (!parameter) {
                return Fail(
                    ExecutionError::PoutReadFailed, CKBR_PARAMETERERROR,
                    "A Pout disappeared before the Frame was read.",
                    fault);
            }

            PoutInfo info;
            if (!GetPoutInfo(parameter, info)) {
                return Fail(
                    ExecutionError::UnsupportedPout, CKBR_PARAMETERERROR,
                    std::string("Pout '") + SafeName(parameter) +
                        "' acquired an unsupported type during execution.",
                    fault);
            }

            Pout value;
            value.Index = index;
            value.Name = parameter->GetName() ? parameter->GetName() : "";
            value.Occurrence = PoutOccurrence(behavior, index, value.Name);
            const CKGUID type = parameter->GetGUID();
            value.TypeGuid1 = static_cast<std::uint32_t>(type.d1);
            value.TypeGuid2 = static_cast<std::uint32_t>(type.d2);
            value.Kind = info.Kind;
            if (info.Kind == PoutKind::Utf8) {
                const int size = parameter->GetStringValue(nullptr, FALSE);
                if (size < 0) {
                    return Fail(
                        ExecutionError::PoutReadFailed, size,
                        std::string("Pout '") + SafeName(parameter) +
                            "' could not be read.",
                        fault);
                }
                std::vector<char> text(
                    static_cast<std::size_t>(size) + 1u, '\0');
                if (size > 0 &&
                    parameter->GetStringValue(text.data(), FALSE) < 0) {
                    return Fail(
                        ExecutionError::PoutReadFailed, CKBR_PARAMETERERROR,
                        std::string("Pout '") + SafeName(parameter) +
                            "' changed while it was being read.",
                        fault);
                }
                if (size > 0)
                    value.Text.assign(text.data());
            } else if (info.Kind == PoutKind::Object) {
                CKObject *object = parameter->GetValueObject(FALSE);
                if (object) {
                    if (!m_Runtime.m_IssueObjectRef) {
                        return Fail(
                            ExecutionError::PoutReadFailed,
                            CKBR_PARAMETERERROR,
                            "An object Pout cannot be retained without ObjectRefs.",
                            fault);
                    }
                    const ObjectRef reference = m_Runtime.m_IssueObjectRef(object);
                    if (reference.IsNull()) {
                        return Fail(
                            ExecutionError::PoutReadFailed,
                            CKBR_PARAMETERERROR,
                            "ObjectRefs rejected an object Pout.",
                            fault);
                    }
                    value.ObjectDomain = reference.Domain;
                    value.ObjectSlot = reference.Slot;
                    value.ObjectGeneration = reference.Generation;
                }
            } else {
                const int size = parameter->GetDataSize();
                if (size < 0 || static_cast<std::size_t>(size) !=
                                    info.Size ||
                    info.Size > sizeof(value.Components)) {
                    return Fail(
                        ExecutionError::PoutReadFailed, CKBR_PARAMETERERROR,
                        std::string("Pout '") + SafeName(parameter) +
                            "' has an invalid value size.",
                        fault);
                }
                std::array<std::byte, sizeof(value.Components)> bytes{};
                const CKERROR error = parameter->GetValue(bytes.data(), FALSE);
                if (error != CK_OK) {
                    return Fail(
                        ExecutionError::PoutReadFailed, error,
                        std::string("Pout '") + SafeName(parameter) +
                            "' could not be read.",
                        fault);
                }
                if (info.Kind == PoutKind::Bool ||
                    info.Kind == PoutKind::Int32) {
                    std::memcpy(&value.Int32, bytes.data(), sizeof(value.Int32));
                    if (info.Kind == PoutKind::Bool)
                        value.Int32 = value.Int32 != 0 ? 1 : 0;
                } else if (info.Kind == PoutKind::Float32) {
                    std::memcpy(&value.Float32, bytes.data(), sizeof(value.Float32));
                } else {
                    switch (info.Kind) {
                    case PoutKind::Vec2: {
                        Vx2DVector native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.x;
                        value.Components[1] = native.y;
                        value.ComponentCount = 2;
                        break;
                    }
                    case PoutKind::Vec3: {
                        VxVector native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.x;
                        value.Components[1] = native.y;
                        value.Components[2] = native.z;
                        value.ComponentCount = 3;
                        break;
                    }
                    case PoutKind::Quaternion: {
                        VxQuaternion native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.x;
                        value.Components[1] = native.y;
                        value.Components[2] = native.z;
                        value.Components[3] = native.w;
                        value.ComponentCount = 4;
                        break;
                    }
                    case PoutKind::Euler: {
                        float native[3];
                        std::memcpy(native, bytes.data(), sizeof(native));
                        std::copy(std::begin(native), std::end(native),
                                  value.Components.begin());
                        value.ComponentCount = 3;
                        break;
                    }
                    case PoutKind::Rect: {
                        VxRect native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.left;
                        value.Components[1] = native.top;
                        value.Components[2] = native.right;
                        value.Components[3] = native.bottom;
                        value.ComponentCount = 4;
                        break;
                    }
                    case PoutKind::Color: {
                        VxColor native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.r;
                        value.Components[1] = native.g;
                        value.Components[2] = native.b;
                        value.Components[3] = native.a;
                        value.ComponentCount = 4;
                        break;
                    }
                    case PoutKind::Box: {
                        VxBbox native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        value.Components[0] = native.Min.x;
                        value.Components[1] = native.Min.y;
                        value.Components[2] = native.Min.z;
                        value.Components[3] = native.Max.x;
                        value.Components[4] = native.Max.y;
                        value.Components[5] = native.Max.z;
                        value.ComponentCount = 6;
                        break;
                    }
                    case PoutKind::Mat4: {
                        VxMatrix native;
                        std::memcpy(&native, bytes.data(), sizeof(native));
                        for (int row = 0; row < 4; ++row) {
                            for (int column = 0; column < 4; ++column) {
                                value.Components[static_cast<std::size_t>(
                                    row * 4 + column)] = native[row][column];
                            }
                        }
                        value.ComponentCount = 16;
                        break;
                    }
                    default:
                        return Fail(
                            ExecutionError::PoutReadFailed,
                            CKBR_PARAMETERERROR,
                            std::string("Pout '") + SafeName(parameter) +
                                "' has an invalid numeric value form.",
                            fault);
                    }
                }
            }
            pouts.push_back(std::move(value));
        }
        return true;
    }

    bool ClearOutputs(const std::vector<ExecutionOutput> &outputs,
                      ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            fault = {ExecutionError::OutUnavailable, CKBR_BEHAVIORERROR,
                     "Behavior disappeared before its active outputs were cleared."};
            return false;
        }
        for (const ExecutionOutput &output : outputs) {
            if (output.Index >= 0 && output.Index < behavior->GetOutputCount())
                behavior->ActivateOutput(output.Index, FALSE);
        }
        return true;
    }

private:
    struct PoutInfo {
        PoutKind Kind = PoutKind::Int32;
        std::size_t Size = 0;
    };

    bool GetPoutInfo(CKParameter *parameter, PoutInfo &info) const {
        if (!parameter)
            return false;
        CKParameterManager *manager = m_Runtime.m_Context
            ? m_Runtime.m_Context->GetParameterManager() : nullptr;
        const Parameter::Type type =
            Parameter::Describe(manager, parameter->GetGUID());
        switch (type.ValueForm) {
        case Parameter::Form::Object:
            info = {PoutKind::Object, 0};
            break;
        case Parameter::Form::Bool:
            info = {PoutKind::Bool, sizeof(CKBOOL)};
            break;
        case Parameter::Form::Int32:
            info = {PoutKind::Int32, sizeof(int)};
            break;
        case Parameter::Form::Float32:
            info = {PoutKind::Float32, sizeof(float)};
            break;
        case Parameter::Form::Utf8:
            info = {PoutKind::Utf8, 0};
            break;
        case Parameter::Form::Vec2:
            info = {PoutKind::Vec2, sizeof(Vx2DVector)};
            break;
        case Parameter::Form::Vec3:
            info = {PoutKind::Vec3, sizeof(VxVector)};
            break;
        case Parameter::Form::Quaternion:
            info = {PoutKind::Quaternion, sizeof(VxQuaternion)};
            break;
        case Parameter::Form::Euler:
            info = {PoutKind::Euler, sizeof(float) * 3};
            break;
        case Parameter::Form::Rect:
            info = {PoutKind::Rect, sizeof(VxRect)};
            break;
        case Parameter::Form::Color:
            info = {PoutKind::Color, sizeof(VxColor)};
            break;
        case Parameter::Form::Box:
            info = {PoutKind::Box, sizeof(VxBbox)};
            break;
        case Parameter::Form::Mat4:
            info = {PoutKind::Mat4, sizeof(VxMatrix)};
            break;
        case Parameter::Form::Unsupported:
            return false;
        }
        return true;
    }

    static bool Fail(ExecutionError code, int nativeCode,
                     std::string message, ExecutionFault &fault) {
        fault = {code, nativeCode, std::move(message)};
        return false;
    }

    static int Occurrence(CKBehavior *behavior, int index,
                          const std::string &name, bool input = true) {
        int occurrence = 0;
        for (int current = 0; current < index; ++current) {
            CKBehaviorIO *io = input
                ? behavior->GetInput(current) : behavior->GetOutput(current);
            const char *candidate = io ? io->GetName() : nullptr;
            if ((candidate ? candidate : "") == name)
                ++occurrence;
        }
        return occurrence;
    }

    static int PoutOccurrence(CKBehavior *behavior, int index,
                              const std::string &name) {
        int occurrence = 0;
        for (int current = 0; current < index; ++current) {
            CKParameterOut *parameter = behavior->GetOutputParameter(current);
            const char *candidate = parameter ? parameter->GetName() : nullptr;
            if ((candidate ? candidate : "") == name)
                ++occurrence;
        }
        return occurrence;
    }

    static ExecutionFault FromStatus(const Status &status) {
        ExecutionError code = ExecutionError::InvalidState;
        switch (status.Code) {
        case Error::SlotNotFound:
            code = ExecutionError::SelectorNotFound;
            break;
        case Error::AmbiguousSlot:
            code = ExecutionError::SelectorAmbiguous;
            break;
        case Error::StaleLayout:
            code = ExecutionError::LayoutStale;
            break;
        default:
            break;
        }
        return {code, status.BehaviorResult, status.Message};
    }

    Runtime &m_Runtime;
    std::uint64_t m_InstanceId;
    const CKBehaviorContext *m_Frame;
};

class Runtime::NativeLifecycleAdapter final : public LifecycleAdapter {
public:
    NativeLifecycleAdapter(Runtime &runtime, Record &record,
                           CKBeObject *owner, CKBehavior *parent,
                           const BlockSpec *spec,
                           const CKBehaviorContext *frame)
        : m_Runtime(runtime), m_Record(record),
          m_Owner(runtime.CaptureObject(owner)),
          m_Parent(runtime.CaptureObject(parent)),
          m_Spec(spec), m_Frame(frame) {}

    bool InitializeAndReflect(LifecycleLayout &layout,
                              LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior)
            return false;
        CKBehaviorPrototype *prototype = m_Record.Prototype;
        if (!prototype) {
            return Fail(Failure(Error::PrototypeNotFound,
                                "Building Block Prototype is unavailable.",
                                CK_OK, CKBR_OK, Phase::Initialization,
                                m_Record.PrototypeGuid),
                        LifecycleError::InitializationFailed, fault);
        }
        Status status = m_Runtime.EnsurePrototypeLayout(behavior, prototype, false);
        if (!status)
            return Fail(std::move(status), LifecycleError::LayoutFailed, fault);
        layout.Generation = m_Record.LayoutGeneration;
        return true;
    }

    bool WriteSettingStage(std::size_t stage,
                           const LifecycleLayout &,
                           LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior)
            return false;
        if (!m_Spec || stage >= m_Spec->m_SettingStages.size()) {
            return Fail(Failure(Error::InvalidState,
                                "Behavior Setting stage does not exist."),
                        LifecycleError::SettingFailed, fault);
        }
        for (const BlockSpec::Binding &binding : m_Spec->m_SettingStages[stage]) {
            SlotInfo slot;
            Status status = m_Runtime.Resolve(behavior, binding.Target, slot);
            if (!status) {
                return Fail(Annotate(std::move(status), Phase::Settings,
                                     m_Record.PrototypeGuid,
                                     &binding.Target),
                            LifecycleError::SettingFailed, fault);
            }
            status = Parameter::Write(
                m_Runtime.m_Context,
                m_Runtime.ResolveParameter(behavior, slot), binding.Source);
            if (!status) {
                return Fail(Annotate(std::move(status), Phase::Settings,
                                     m_Record.PrototypeGuid,
                                     &binding.Target),
                            LifecycleError::SettingFailed, fault);
            }
        }
        return true;
    }

    bool AlignRelationsAndPlace(LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior || !m_Spec)
            return false;
        CKBeObject *owner = Owner(fault);
        if (m_Owner.Id != 0 && !owner)
            return false;

        if (owner) {
            CKERROR ownerError = behavior->SetOwner(owner, FALSE);
            if (ownerError == CK_OK)
                ownerError = behavior->SetSubBehaviorOwner(owner, FALSE);
            if (ownerError != CK_OK) {
                return Fail(Failure(Error::OwnerInvalid,
                                    "Failed to align Building Block owner tree.",
                                    ownerError, CKBR_OK, Phase::OwnerBinding,
                                    m_Record.PrototypeGuid),
                            LifecycleError::RelationFailed, fault);
            }
        }

        Status status = m_Runtime.BindTarget(behavior, owner, *m_Spec, m_Record);
        if (!status)
            return Fail(std::move(status), LifecycleError::RelationFailed, fault);
        status = m_Runtime.EnsurePrototypeDefaults(
            behavior, m_Record.Prototype, m_Record);
        if (!status)
            return Fail(std::move(status), LifecycleError::RelationFailed, fault);

        CKBehavior *parent = Parent(fault);
        if (m_Parent.Id != 0 && !parent)
            return false;
        if (parent) {
            const CKERROR addError = parent->AddSubBehavior(behavior);
            if (addError != CK_OK) {
                return Fail(Failure(Error::OwnerInvalid,
                                    "Failed to add Building Block to parent graph.",
                                    addError, CKBR_OK, Phase::OwnerBinding,
                                    m_Record.PrototypeGuid),
                            LifecycleError::RelationFailed, fault);
            }
        }
        return true;
    }

    bool CaptureIdentity(LifecycleIdentity &identity,
                         LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior)
            return false;

        identity = {};
        identity.Behavior = Convert(m_Runtime.CaptureObject(behavior));
        const bool graph = !behavior->IsUsingFunction();
        CKBehaviorPrototype *prototype = graph
            ? m_Record.Prototype : behavior->GetPrototype();
        const CKGUID guid = graph
            ? m_Record.PrototypeGuid : behavior->GetPrototypeGuid();
        identity.Prototype = {
            (static_cast<std::uint64_t>(guid.d1) << 32u) ^
                static_cast<std::uint32_t>(guid.d2),
            reinterpret_cast<std::uintptr_t>(prototype)};
        identity.Owner = Convert(m_Runtime.CaptureObject(behavior->GetOwner()));
        identity.Parent = Convert(m_Runtime.CaptureObject(behavior->GetParent()));
        return true;
    }

    bool Invoke(LifecycleCallback callback, LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior)
            return false;
        Status status = m_Runtime.CallCallback(
            m_Record, Message(callback), m_Frame);
        if (!status)
            return Fail(std::move(status), LifecycleError::CallbackFailed, fault);
        return true;
    }

    bool Revalidate(const LifecycleIdentity &identity,
                    LifecycleFault &fault) override {
        LifecycleIdentity current;
        if (!CaptureIdentity(current, fault))
            return false;
        if (current == identity)
            return true;
        std::string changed;
        const auto recordChange = [&](const char *name, bool differs) {
            if (!differs)
                return;
            if (!changed.empty())
                changed += ", ";
            changed += name;
        };
        recordChange("Behavior", !(current.Behavior == identity.Behavior));
        recordChange("Prototype", !(current.Prototype == identity.Prototype));
        recordChange("owner", !(current.Owner == identity.Owner));
        recordChange("parent", !(current.Parent == identity.Parent));
        CKBehavior *behavior = m_Runtime.ResolveBehavior(m_Record);
        return Fail(Failure(
                        Error::InvalidState,
                        "A native callback changed protected lifecycle identity: " +
                            changed + ".",
                        CK_OK, CKBR_OK, Phase::LifecycleCallback,
                        behavior ? m_Record.PrototypeGuid : CKGUID()),
                    LifecycleError::IdentityChanged, fault);
    }

    bool Reflect(LifecycleLayout &layout, LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior)
            return false;
        CKBehaviorPrototype *prototype = m_Record.Prototype;
        if (!prototype) {
            return Fail(Failure(Error::PrototypeNotFound,
                                "Building Block Prototype disappeared after callback."),
                        LifecycleError::LayoutFailed, fault);
        }
        Status status = m_Runtime.EnsurePrototypeLayout(behavior, prototype, true);
        if (!status)
            return Fail(std::move(status), LifecycleError::LayoutFailed, fault);
        m_Runtime.m_SharedBindings->Sources.Update(behavior);
        layout.Generation = ++m_Record.LayoutGeneration;
        return true;
    }

    bool ApplyBindings(LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior || !m_Spec)
            return false;
        CKBeObject *owner = Owner(fault);
        if (m_Owner.Id != 0 && !owner)
            return false;
        Status status = m_Runtime.BindTarget(
            behavior, owner, *m_Spec, m_Record);
        if (status)
            status = m_Runtime.ApplyBindings(behavior, *m_Spec, m_Record);
        if (!status)
            return Fail(std::move(status), LifecycleError::BindingFailed, fault);
        return true;
    }

    bool ApplyInterface(LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior || !m_Spec)
            return false;
        // CKBehavior::CreateInput and CreateOutput never consult the
        // variable-interface flags, so neither does this. A block whose code
        // ignores the appended port is the author's problem, not an error the
        // Runtime can detect.
        for (const std::string &name : m_Spec->m_AddedInputs) {
            if (!behavior->CreateInput(const_cast<CKSTRING>(name.c_str()))) {
                return Fail(Failure(Error::CreateFailed,
                                    "Failed to add a Building Block input."),
                            LifecycleError::BindingFailed, fault);
            }
        }
        for (const std::string &name : m_Spec->m_AddedOutputs) {
            if (!behavior->CreateOutput(const_cast<CKSTRING>(name.c_str()))) {
                return Fail(Failure(Error::CreateFailed,
                                    "Failed to add a Building Block output."),
                LifecycleError::BindingFailed, fault);
            }
        }
        return true;
    }

    bool ReconcileBindings(const LifecycleLayout &,
                           LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior || !m_Spec)
            return false;
        auto validate = [&](const Slot &selector) {
            SlotInfo slot;
            Status status = m_Runtime.Resolve(behavior, selector, slot);
            if (!status) {
                Fail(Annotate(std::move(status), Phase::ParameterBinding,
                              m_Record.PrototypeGuid, &selector),
                     LifecycleError::BindingFailed, fault);
                return false;
            }
            return true;
        };
        for (const BlockSpec::Binding &binding : m_Spec->m_Inputs) {
            if (!validate(binding.Target))
                return false;
        }
        for (const BlockSpec::Binding &binding : m_Spec->m_Locals) {
            if (!validate(binding.Target))
                return false;
        }
        if (m_Spec->m_TargetMode != TargetMode::Owner &&
            !validate(Slot::At(SlotKind::Target, 0, m_Spec->m_TargetType)))
            return false;
        m_Runtime.m_SharedBindings->Sources.Update(behavior);
        m_Runtime.PruneOwnedSources(m_Record);
        return true;
    }

    bool Deactivate(LifecycleFault &) override {
        if (CKBehavior *behavior = m_Runtime.ResolveBehavior(m_Record))
            behavior->Activate(FALSE, FALSE);
        return true;
    }

    bool DisconnectAndDestroy(LifecycleFault &) override {
        Runtime::CloseCallbacks(m_Record);
        m_Runtime.QueueDestroy(m_Record);
        return true;
    }

    [[nodiscard]] const Status &LastStatus() const noexcept {
        return m_LastStatus;
    }

private:
    static LifecycleObject Convert(ObjectStamp stamp) {
        return {static_cast<std::uint64_t>(stamp.Id),
                reinterpret_cast<std::uintptr_t>(stamp.Address)};
    }

    static CKDWORD Message(LifecycleCallback callback) {
        switch (callback) {
        case LifecycleCallback::Create: return CKM_BEHAVIORCREATE;
        case LifecycleCallback::Attach: return CKM_BEHAVIORATTACH;
        case LifecycleCallback::SettingsEdited: return CKM_BEHAVIORSETTINGSEDITED;
        case LifecycleCallback::Edited: return CKM_BEHAVIOREDITED;
        case LifecycleCallback::Reset: return CKM_BEHAVIORRESET;
        case LifecycleCallback::Detach: return CKM_BEHAVIORDETACH;
        case LifecycleCallback::Delete: return CKM_BEHAVIORDELETE;
        }
        return 0;
    }

    bool Fail(Status status, LifecycleError code,
              LifecycleFault &fault) {
        if (m_LastStatus)
            m_LastStatus = status;
        fault = {code,
                 status.CkError != CK_OK
                     ? static_cast<int>(status.CkError)
                     : status.BehaviorResult,
                 status.Message};
        return false;
    }

    CKBehavior *Behavior(LifecycleFault &fault) {
        CKBehavior *behavior = m_Runtime.ResolveBehavior(m_Record);
        if (behavior)
            return behavior;
        Fail(Failure(Error::InvalidState,
                     "Building Block disappeared during native lifecycle processing.",
                     CK_OK, CKBR_BEHAVIORERROR, Phase::LifecycleCallback),
             LifecycleError::IdentityChanged, fault);
        return nullptr;
    }

    CKBeObject *Owner(LifecycleFault &fault) {
        if (m_Owner.Id == 0)
            return nullptr;
        CKObject *object = m_Runtime.ResolveObject(m_Owner);
        if (object && CKIsChildClassOf(object, CKCID_BEOBJECT))
            return static_cast<CKBeObject *>(object);
        Fail(Failure(Error::OwnerInvalid,
                     "Building Block owner disappeared before configuration."),
             LifecycleError::IdentityChanged, fault);
        return nullptr;
    }

    CKBehavior *Parent(LifecycleFault &fault) {
        if (m_Parent.Id == 0)
            return nullptr;
        CKObject *object = m_Runtime.ResolveObject(m_Parent);
        if (object && CKIsChildClassOf(object, CKCID_BEHAVIOR))
            return static_cast<CKBehavior *>(object);
        Fail(Failure(Error::OwnerInvalid,
                     "Parent graph disappeared before configuration."),
             LifecycleError::IdentityChanged, fault);
        return nullptr;
    }

    Runtime &m_Runtime;
    Record &m_Record;
    ObjectStamp m_Owner;
    ObjectStamp m_Parent;
    const BlockSpec *m_Spec;
    const CKBehaviorContext *m_Frame;
    Status m_LastStatus;
};

Instance::~Instance() {
    Reset();
}

Instance::Instance(Instance &&other) noexcept
    : m_Access(std::move(other.m_Access)),
      m_Id(std::exchange(other.m_Id, 0)) {}

Instance &Instance::operator=(Instance &&other) noexcept {
    if (this == &other)
        return *this;
    Reset();
    m_Access = std::move(other.m_Access);
    m_Id = std::exchange(other.m_Id, 0);
    return *this;
}

Instance::operator bool() const noexcept {
    std::shared_ptr<Access> access = m_Access.lock();
    if (!access || m_Id == 0)
        return false;
    std::lock_guard<std::mutex> lock(access->Mutex);
    return access->Owner != nullptr;
}

CKBehavior *Instance::Get() const {
    std::shared_ptr<Access> access = m_Access.lock();
    if (!access)
        return nullptr;
    std::lock_guard<std::mutex> lock(access->Mutex);
    Runtime *runtime = access->Owner;
    if (!runtime || !runtime->ReadyStatus())
        return nullptr;
    const Runtime::Record *record = runtime->FindRecord(*this);
    return record ? runtime->ResolveBehavior(*record) : nullptr;
}

std::uint64_t Instance::LayoutGeneration() const {
    std::shared_ptr<Access> access = m_Access.lock();
    if (!access)
        return 0;
    std::lock_guard<std::mutex> lock(access->Mutex);
    Runtime *runtime = access->Owner;
    if (!runtime || !runtime->ReadyStatus())
        return 0;
    const Runtime::Record *record = runtime->FindRecord(*this);
    return record ? record->LayoutGeneration : 0;
}

void Instance::Reset() {
    std::shared_ptr<Access> access = m_Access.lock();
    const std::uint64_t id = std::exchange(m_Id, 0);
    m_Access.reset();
    if (!access || !id)
        return;
    std::lock_guard<std::mutex> lock(access->Mutex);
    if (access->Owner)
        access->Owner->RequestRelease(id);
}

Runtime::Runtime(CKContext *context,
                 std::function<ObjectRef(const void *)> issueObjectRef,
                 PrototypeCatalog *catalog,
                 Runtime *sourceRuntime)
    : m_Context(context), m_IssueObjectRef(std::move(issueObjectRef)),
      m_Catalog(catalog),
      m_Thread(std::this_thread::get_id()),
      m_Access(std::make_shared<Instance::Access>()),
      m_SharedBindings(sourceRuntime && sourceRuntime->m_Context == context
            ? sourceRuntime->m_SharedBindings
            : std::make_shared<SharedBindings>(context)) {
    m_Access->Owner = this;
}

Runtime::~Runtime() {
    {
        std::lock_guard<std::mutex> lock(m_Access->Mutex);
        m_Access->Owner = nullptr;
    }
    Close();
}

Status Runtime::ReadyStatus() const {
    if (!m_Context)
        return Failure(Error::ContextExpired, "Virtools context is no longer available.");
    if (m_Thread != std::this_thread::get_id())
        return Failure(Error::WrongThread, "Runtime may only be used on the game thread.");
    return {};
}

Status Runtime::ResolvePrototype(PrototypeRef requested,
                                 PrototypeRef &selected) const {
    selected = {};
    const CKGUID guid = requested.Guid;
    if (m_Catalog && m_Catalog->TracksRetirement()) {
        Layout layout;
        Status status = m_Catalog->DeclaredLayout(requested, layout);
        if (!status)
            return status;
        for (const ManagerRequirement &manager : layout.Managers) {
            if (!manager.Available) {
                std::ostringstream message;
                message << "Required manager " << GuidText(manager.Guid)
                        << " is missing for Building Block '"
                        << layout.PrototypeName << "'.";
                Status missing = Failure(
                    Error::RequiredManagerMissing, message.str(), CK_OK,
                    CKBR_OK, Phase::ManagerValidation, guid);
                missing.Details.RequiredManager = manager.Guid;
                return missing;
            }
        }
        selected = {guid, layout.ProviderGeneration};
        return {};
    }
    if (requested.Generation) {
        return Failure(
            Error::Unavailable,
            "The requested Building Block provider generation cannot be verified because provider retirement is not observable.",
            CK_OK, CKBR_OK, Phase::PrototypeResolution, guid);
    }
    if (!CKGetPrototypeFromGuid(guid)) {
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype GUID " + GuidText(guid) +
                           " is not registered.",
                       CK_OK, CKBR_OK, Phase::PrototypeResolution, guid);
    }
    if (CKObjectDeclaration *declaration = CKGetObjectDeclarationFromGuid(guid)) {
        for (int i = 0; i < declaration->GetManagerNeededCount(); ++i) {
            const CKGUID managerGuid = declaration->GetManagerNeeded(i);
            if (!m_Context->GetManagerByGuid(managerGuid)) {
                std::ostringstream message;
                message << "Required manager " << GuidText(managerGuid)
                        << " is missing for Building Block '"
                        << (declaration->GetName() ? declaration->GetName() : "<unnamed>")
                        << "'.";
                Status status = Failure(
                    Error::RequiredManagerMissing, message.str(), CK_OK, CKBR_OK,
                    Phase::ManagerValidation, guid);
                status.Details.RequiredManager = managerGuid;
                return status;
            }
        }
    }
    selected = {guid, 0};
    return {};
}

Status Runtime::ValidateTarget(CKBeObject *owner, const BlockSpec &spec) const {
    if (!m_Catalog || !m_Catalog->TracksRetirement())
        return {};
    Layout declared;
    Status status = m_Catalog->DeclaredLayout(
        {spec.Prototype(), spec.PrototypeGeneration()}, declared);
    if (!status)
        return status;

    if (owner && !CKIsChildClassOf(owner, declared.CompatibleClass)) {
        return Failure(Error::OwnerInvalid,
                       "Behavior owner is incompatible with the Prototype.",
                       CKERR_INVALIDOBJECT, CKBR_OWNERERROR,
                       Phase::OwnerBinding, spec.Prototype());
    }
    if (spec.m_TargetMode == TargetMode::Owner)
        return {};
    if (!(declared.BehaviorFlags & CKBEHAVIOR_TARGETABLE)) {
        return Failure(Error::TargetInvalid,
                       "The Prototype does not accept an explicit Target.",
                       CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                       Phase::TargetBinding, spec.Prototype());
    }

    CKParameterManager *parameters = m_Context
        ? m_Context->GetParameterManager() : nullptr;
    const Parameter::Type targetType =
        Parameter::Describe(parameters, spec.m_TargetType);
    if (!targetType.Valid)
        return Failure(Error::ParameterTypeUnavailable,
                       "The Behavior Target parameter type is unavailable.",
                       CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                       Phase::TargetBinding, spec.Prototype());
    if (!targetType.ObjectDerived())
        return Failure(Error::TypeMismatch,
                       "The Behavior Target type is not object-derived.",
                       CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                       Phase::TargetBinding, spec.Prototype());
    if (targetType.ClassId &&
        !CKIsChildClassOf(targetType.ClassId, declared.CompatibleClass)) {
        return Failure(Error::TargetInvalid,
                       "The typed Target cannot satisfy the Prototype compatible class.",
                       CKERR_INVALIDPARAMETERTYPE, CKBR_PARAMETERERROR,
                       Phase::TargetBinding, spec.Prototype());
    }
    if (spec.m_TargetMode == TargetMode::Explicit) {
        if (spec.m_TargetValue.Kind() == Parameter::BindingKind::Object) {
            CKObject *object = spec.m_TargetValue.ObjectValue();
            CKObject *live = object && m_Context
                ? m_Context->GetObject(spec.m_TargetValue.ObjectId()) : nullptr;
            if (live != object || !live || live->IsToBeDeleted())
                return Failure(Error::TargetInvalid,
                               "The explicit Behavior Target has expired.",
                               CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                               Phase::TargetBinding, spec.Prototype());
            if ((targetType.ClassId &&
                 !CKIsChildClassOf(live, targetType.ClassId)) ||
                !CKIsChildClassOf(live, declared.CompatibleClass)) {
                return Failure(Error::TargetInvalid,
                               "The explicit Behavior Target has an incompatible class.",
                               CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                               Phase::TargetBinding, spec.Prototype());
            }
        } else if (spec.m_TargetValue.Kind() ==
                   Parameter::BindingKind::Direct) {
            CKParameter *source = spec.m_TargetValue.Source();
            CKObject *live = source && m_Context
                ? m_Context->GetObject(spec.m_TargetValue.SourceId()) : nullptr;
            if (live != source || !live || live->IsToBeDeleted() ||
                !CKIsChildClassOf(live, CKCID_PARAMETER)) {
                return Failure(Error::TargetInvalid,
                               "The direct Target source has expired.",
                               CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                               Phase::TargetBinding, spec.Prototype());
            }
        } else if (spec.m_TargetValue.Kind() ==
                   Parameter::BindingKind::Shared) {
            CKParameterIn *source = spec.m_TargetValue.SharedSource();
            CKObject *live = source && m_Context
                ? m_Context->GetObject(spec.m_TargetValue.SharedSourceId())
                : nullptr;
            if (live != source || !live || live->IsToBeDeleted() ||
                !CKIsChildClassOf(live, CKCID_PARAMETERIN)) {
                return Failure(Error::TargetInvalid,
                               "The shared Target source has expired.",
                               CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                               Phase::TargetBinding, spec.Prototype());
            }
        } else {
            return Failure(Error::TargetInvalid,
                           "An explicit Behavior Target requires an object or parameter source.",
                           CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                           Phase::TargetBinding, spec.Prototype());
        }
    }
    return {};
}

Status Runtime::CreateBehavior(const BlockSpec &spec, CKBehavior *&behavior,
                               Record &record) const {
    behavior = nullptr;
    PrototypeRef selected;
    Status status = ResolvePrototype(
        {spec.Prototype(), spec.PrototypeGeneration()}, selected);
    if (!status)
        return status;

    CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(spec.Prototype());
    if (!prototype) {
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype disappeared before creation.",
                       CKERR_INVALIDOBJECT, CKBR_OK,
                       Phase::PrototypeResolution, spec.Prototype());
    }

    behavior = static_cast<CKBehavior *>(
        m_Context->CreateObject(CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
    if (!behavior) {
        return Failure(Error::CreateFailed, "Failed to create CKBehavior.",
                       CK_OK, CKBR_OK, Phase::Creation, spec.Prototype());
    }
    behavior->UseFunction();
    const CKERROR initError = behavior->InitFromGuid(spec.Prototype());
    if (initError != CK_OK) {
        m_Context->DestroyObject(behavior);
        behavior = nullptr;
        return Failure(Error::InitFailed,
                       "Failed to initialize Building Block from Prototype.",
                       initError, CKBR_OK, Phase::Initialization,
                       spec.Prototype());
    }

    record.PrototypeGuid = spec.Prototype();
    record.Prototype = prototype;
    record.ProviderGeneration = selected.Generation;
    return {};
}

Status Runtime::CheckDetached(
    const BlockSpec &spec, DetachedCompatibility &compatibility,
    bool graphResident) const {
    compatibility = DetachedCompatibility::Unverified;
    if (!m_Catalog || !m_Catalog->TracksRetirement())
        return {};

    Status status = m_Catalog->Detached(
        {spec.Prototype(), spec.PrototypeGeneration()}, compatibility);
    if (!status)
        return status;
    if (!graphResident &&
        compatibility == DetachedCompatibility::GraphOnly) {
        return Failure(
            Error::DetachedUnsupported,
            "This Building Block requires a parent graph and cannot be run detached.",
            CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
            Phase::PrototypeResolution, spec.Prototype());
    }
    return {};
}

CreateResult Runtime::Instantiate(CKBeObject *owner, const BlockSpec &spec,
                                  const CKBehaviorContext *frame,
                                  FrameRetention retention) {
    CreateResult result;
    result.Detail = ReadyStatus();
    if (!result.Detail)
        return result;
    result.Detail = CheckDetached(spec, result.Detached);
    if (!result.Detail)
        return result;
    if (owner && owner->GetCKContext() != m_Context) {
        result.Detail = Failure(Error::OwnerInvalid, "Behavior owner belongs to another CKContext.");
        return result;
    }
    // CaptureObject drops a to-be-deleted object silently. An owner in that
    // state must fail admission instead of degrading into an ownerless ATTACH.
    if (owner && (owner->IsToBeDeleted() ||
                  CKGetObject(m_Context, owner->GetID()) != owner)) {
        result.Detail = Failure(Error::OwnerInvalid,
                                "Behavior owner is being deleted.");
        return result;
    }
    result.Detail = ValidateTarget(owner, spec);
    if (!result.Detail)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.KeepAlive = spec.m_KeepAlive;
    record.Protocol = Execution(retention);
    CKBehavior *behavior = nullptr;
    result.Detail = CreateBehavior(spec, behavior, record);
    if (!result.Detail)
        return result;

    record.Behavior = CaptureObject(behavior);
    result.Detail = Configure(behavior, owner, nullptr, spec, frame, record);
    if (!result.Detail) {
        DestroyReady(DestroyMode::Ready);
        return result;
    }
    record.Desired = spec;
    record.Desired.m_SettingStages.clear();
    record.Desired.m_AddedInputs.clear();
    record.Desired.m_AddedOutputs.clear();
    record.Desired.m_KeepAlive.clear();

    const std::uint64_t instanceId = record.Id;
    m_Records.emplace(instanceId, std::move(record));
    result.Descriptor = Describe(behavior, m_Records.at(instanceId).LayoutGeneration);
    result.Handle = Instance(m_Access, instanceId);
    return result;
}

CallResult Runtime::Call(CKBeObject *owner, const BlockSpec &spec,
                         const Slot &input,
                         const CKBehaviorContext *frame,
                         FrameRetention retention) {
    CallResult result;
    CreateResult created = Instantiate(owner, spec, frame, retention);
    result.Detail = created.Detail;
    if (!created)
        return result;
    result.Descriptor = std::move(created.Descriptor);
    result.Handle = std::move(created.Handle);
    result.Detached = created.Detached;
    Slot entry = input;
    entry.Kind = SlotKind::Input;
    SlotRef slot;
    Status resolved = Resolve(result.Handle, entry, slot);
    Record *record = resolved ? FindRecord(result.Handle) : nullptr;
    if (!resolved || !record) {
        if (!resolved)
            resolved.Details.Stage = Phase::Execution;
        result.Run = {resolved ? Failure(Error::InvalidState,
                                         "Behavior instance has expired.")
                                      : std::move(resolved),
                      RunState::Failed, CKBR_PARAMETERERROR, {}};
    } else {
        const ExecutionInput activation = entry.UsesName()
            ? ExecutionInput::Named(entry.Name, entry.Occurrence,
                                    entry.RequireUnique)
            : ExecutionInput::At(slot.Slot.NativeIndex,
                                 record->LayoutGeneration);
        result.Run = Execute(record->Id, &activation, true, frame);
    }
    if (!result.Run)
        result.Detail = result.Run.Detail;
    return result;
}

AttachResult Runtime::AddToGraph(CKBehavior *parent, const BlockSpec &spec,
                                             const CKBehaviorContext *frame) {
    return Attach(parent, spec, frame, nullptr, nullptr,
                  FrameRetention::Ignore());
}

CreateResult Runtime::AttachToGraph(CKBehavior *parent, const BlockSpec &spec,
                                    const CKBehaviorContext *frame,
                                    FrameRetention retention) {
    CreateResult created;
    Instance handle;
    DetachedCompatibility detached = DetachedCompatibility::Unverified;
    AttachResult attached = Attach(parent, spec, frame, &handle, &detached,
                                   retention);
    created.Detail = std::move(attached.Detail);
    created.Descriptor = std::move(attached.Descriptor);
    created.Handle = std::move(handle);
    created.Detached = detached;
    return created;
}

AttachResult Runtime::CreateInGraph(CKBehavior *parent,
                                    const BlockSpec &spec,
                                    const CKBehaviorContext *frame) {
    AttachResult result;
    result.Detail = ReadyStatus();
    if (!result.Detail)
        return result;
    if (!parent || parent->GetCKContext() != m_Context ||
        parent->IsToBeDeleted() || parent->IsUsingFunction()) {
        result.Detail = Failure(
            Error::OwnerInvalid,
            "Parent graph is invalid, retiring, or belongs to another CKContext.");
        return result;
    }
    result.Detail = ValidateTarget(parent->GetOwner(), spec);
    if (!result.Detail)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.Parent = CaptureObject(parent);
    record.GraphResident = true;
    record.KeepAlive = spec.m_KeepAlive;
    record.Protocol = Execution(FrameRetention::Ignore());
    CKBehavior *behavior = nullptr;
    result.Detail = CreateBehavior(spec, behavior, record);
    if (!result.Detail)
        return result;

    record.Behavior = CaptureObject(behavior);
    result.Detail = CreateBlock(
        behavior, parent->GetOwner(), parent, spec, frame, record);
    if (!result.Detail) {
        // Lifecycle has already balanced every callback it admitted and
        // queued this unowned Block for destruction. This call runs at the
        // graph edit safe point, so leave no transient child in the graph.
        DestroyReady(DestroyMode::Ready);
        return result;
    }
    record.Desired = spec;

    result.Block = behavior;
    const std::uint64_t instanceId = record.Id;
    m_Records.emplace(instanceId, std::move(record));
    result.Descriptor = Describe(
        behavior, m_Records.at(instanceId).LayoutGeneration);
    return result;
}

Status Runtime::EditInGraph(CKBehavior *behavior, const BlockSpec &spec,
                            const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(behavior);
    CKObject *parentObject = record ? ResolveObject(record->Parent) : nullptr;
    CKBehavior *parent = parentObject &&
        CKIsChildClassOf(parentObject, CKCID_BEHAVIOR)
        ? static_cast<CKBehavior *>(parentObject) : nullptr;
    if (!record || !record->GraphResident || !parent ||
        record->NativeLifecycle.State() != LifecycleState::Configuring) {
        return Failure(
            Error::InvalidState,
            "Only a newly created graph Block can receive its final relations.");
    }

    record->Desired = spec;
    const std::uint64_t id = record->Id;
    Status status = EditBlock(
        behavior, parent->GetOwner(), parent, record->Desired, frame, *record);
    if (!status) {
        record = FindRecord(id);
        if (record) {
            record->Protocol.RequestClose();
            record->NativeLifecycle.RequestClose();
            CloseCallbacks(*record);
        }
        DrainCloseQueue();
        DestroyReady(DestroyMode::Ready);
        return status;
    }

    record = FindRecord(id);
    if (!record)
        return Failure(Error::InvalidState,
                       "The graph Block disappeared during EDITED.");
    record->Desired.m_SettingStages.clear();
    record->Desired.m_AddedInputs.clear();
    record->Desired.m_AddedOutputs.clear();
    record->Desired.m_KeepAlive.clear();
    return {};
}

AttachResult Runtime::Attach(CKBehavior *parent, const BlockSpec &spec,
                             const CKBehaviorContext *frame,
                             Instance *handle,
                             DetachedCompatibility *detached,
                             FrameRetention retention) {
    AttachResult result;
    result.Detail = ReadyStatus();
    if (!result.Detail)
        return result;
    if (!parent || parent->GetCKContext() != m_Context ||
        parent->IsToBeDeleted() || parent->IsUsingFunction()) {
        result.Detail = Failure(
            Error::OwnerInvalid,
            "Parent graph is invalid, retiring, or belongs to another CKContext.");
        return result;
    }
    // A graph-resident Block runs where a GraphOnly Block belongs, so the
    // catalog answer is reported rather than enforced here.  A Prototype the
    // catalog never heard of still parks fine; it just reports Unverified.
    DetachedCompatibility compatibility = DetachedCompatibility::Unverified;
    if (detached && !CheckDetached(spec, compatibility, true))
        compatibility = DetachedCompatibility::Unverified;
    result.Detail = ValidateTarget(parent->GetOwner(), spec);
    if (!result.Detail)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.Parent = CaptureObject(parent);
    record.GraphResident = true;
    record.OwnerDriven = handle != nullptr;
    record.KeepAlive = spec.m_KeepAlive;
    record.Protocol = Execution(handle ? retention : FrameRetention::Ignore());
    CKBehavior *behavior = nullptr;
    result.Detail = CreateBehavior(spec, behavior, record);
    if (!result.Detail)
        return result;

    record.Behavior = CaptureObject(behavior);
    result.Detail = Configure(behavior, parent->GetOwner(), parent, spec, frame, record);
    if (!result.Detail) {
        DestroyReady(DestroyMode::Ready);
        return result;
    }
    record.Desired = spec;
    record.Desired.m_SettingStages.clear();
    record.Desired.m_AddedInputs.clear();
    record.Desired.m_AddedOutputs.clear();
    record.Desired.m_KeepAlive.clear();

    result.Block = behavior;
    const std::uint64_t instanceId = record.Id;
    m_Records.emplace(instanceId, std::move(record));
    result.Descriptor = Describe(
        behavior, m_Records.at(instanceId).LayoutGeneration);
    if (handle)
        *handle = Instance(m_Access, instanceId);
    if (detached)
        *detached = compatibility;
    return result;
}

Layout Runtime::Describe(CKBehavior *behavior, std::uint64_t generation) const {
    if (!ReadyStatus() || !behavior)
        return {};
    if (!generation)
        generation = LayoutGeneration(behavior);
    Layout declared;
    const Layout *metadata = nullptr;
    if (m_Catalog && m_Catalog->TracksRetirement()) {
        if (m_Catalog->DeclaredLayout(
                {PrototypeGuid(behavior), 0}, declared))
            metadata = &declared;
    }
    Layout layout = LiveLayout(
        m_Context, behavior, PrototypeGuid(behavior), PrototypeOf(behavior))
        .Describe(generation, metadata);
    const Record *record = FindRecord(behavior);
    if (record && record->ProviderGeneration) {
        layout.ProviderGeneration = record->ProviderGeneration;
    } else if (m_Catalog && m_Catalog->TracksRetirement() &&
               !layout.ProviderGeneration) {
        PrototypeRef selected;
        if (m_Catalog->Resolve({layout.Prototype, 0}, selected))
            layout.ProviderGeneration = selected.Generation;
    }
    return layout;
}

std::uint64_t Runtime::LayoutGeneration(CKBehavior *behavior) const noexcept {
    const Record *record = FindRecord(behavior);
    return record ? record->LayoutGeneration : 0;
}

Status Runtime::Describe(const Instance &instance, Layout &layout) const {
    layout = {};
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    const Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::LayoutUnavailable,
                       "The Run no longer owns a native Behavior Layout.",
                       CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                       Phase::StaticLayout);
    layout = Describe(behavior, record->LayoutGeneration);
    return {};
}

Status Runtime::Resolve(CKBehavior *behavior, const Slot &selector,
                                        SlotInfo &slot) const {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    if (!behavior)
        return Failure(Error::InvalidState, "Behavior no longer exists.");
    return LiveLayout(m_Context, behavior, PrototypeGuid(behavior),
                      PrototypeOf(behavior)).Resolve(selector, slot);
}

Status Runtime::Resolve(const Instance &instance,
                                        const Slot &selector,
                                        SlotRef &slot) const {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    const Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    Status status = Resolve(behavior, selector, slot.Slot);
    if (!status)
        return status;
    CKObject *object = ResolveSlotObject(behavior, slot.Slot);
    slot.InstanceId = record->Id;
    slot.Object = object;
    if (!object)
        return Failure(Error::StaleLayout, "Resolved slot object has expired.");
    slot.LayoutGeneration = record->LayoutGeneration;
    return {};
}

CKParameter *Runtime::ResolveParameter(CKBehavior *behavior, const SlotInfo &slot) const {
    return LiveLayout(m_Context, behavior, PrototypeGuid(behavior),
                      PrototypeOf(behavior)).Parameter(slot);
}

CKObject *Runtime::ResolveSlotObject(CKBehavior *behavior, const SlotInfo &slot) const {
    return LiveLayout(m_Context, behavior, PrototypeGuid(behavior),
                      PrototypeOf(behavior)).Object(slot);
}

Status Runtime::ValidateSlot(const Record &record,
                                             const SlotRef &slot) const {
    if (record.Id != slot.InstanceId ||
        record.LayoutGeneration != slot.LayoutGeneration) {
        return Failure(Error::StaleLayout,
                       "Resolved slot belongs to an older Building Block layout.");
    }
    CKBehavior *behavior = ResolveBehavior(record);
    CKObject *object = ResolveSlotObject(behavior, slot.Slot);
    if (!object || object != slot.Object) {
        return Failure(Error::StaleLayout,
                       "Resolved slot object has been replaced or deleted.");
    }
    return {};
}

CKParameter *Runtime::Parameter(const Instance &instance, const Slot &selector,
                                         Status *status) const {
    Status ready = ReadyStatus();
    if (!ready) {
        if (status)
            *status = ready;
        return nullptr;
    }
    const Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    SlotInfo slot;
    Status resolved;
    if (!record || !behavior) {
        resolved = Failure(Error::InvalidState, "Behavior instance has expired.");
    } else if (record->Failure.Code != Error::None) {
        resolved = record->Failure;
    } else {
        resolved = Resolve(behavior, selector, slot);
    }
    if (status)
        *status = resolved;
    return resolved ? ResolveParameter(behavior, slot) : nullptr;
}

CKParameter *Runtime::Parameter(const Instance &instance,
                                        const SlotRef &slot,
                                        Status *status) const {
    Status ready = ReadyStatus();
    const Record *record = ready ? FindRecord(instance) : nullptr;
    if (ready && !record)
        ready = Failure(Error::InvalidState, "Behavior instance has expired.");
    if (ready && record->Failure.Code != Error::None)
        ready = record->Failure;
    if (ready)
        ready = ValidateSlot(*record, slot);
    if (status)
        *status = ready;
    return ready ? ResolveParameter(ResolveBehavior(*record), slot.Slot) : nullptr;
}

Status Runtime::BindInput(CKBehavior *behavior, Record &record,
                                          const SlotInfo &slot,
                                          const Parameter::Binding &value) {
    if (!behavior)
        return Failure(Error::InvalidState, "Behavior is unavailable.");
    CKParameterIn *input = slot.Kind == SlotKind::Target
        ? behavior->GetTargetParameter()
        : behavior->GetInputParameter(slot.NativeIndex);
    if (!input)
        return Failure(Error::SlotNotFound, "Input parameter no longer exists.");
    m_SharedBindings->Sources.Update(input);

    CKParameter *oldDirect = input->GetDirectSource();
    const ObjectStamp oldRef = CaptureObject(oldDirect);
    // Only remember whether the previous source is Runtime-owned. The literal
    // branch below appends to OwnedSources, so an iterator taken here would
    // not survive; the entry is looked up again when it is released.
    const auto findOwned = [&] {
        return std::find_if(record.OwnedSources.begin(),
                            record.OwnedSources.end(),
                            [&](ObjectStamp candidate) {
                                return candidate == oldRef;
                            });
    };
    const bool oldOwned = findOwned() != record.OwnedSources.end();

    const bool storedValue = value.Kind() != Parameter::BindingKind::Direct &&
                             value.Kind() != Parameter::BindingKind::Shared;
    if (storedValue && oldOwned && oldDirect &&
        oldDirect->GetGUID() == input->GetGUID() &&
        m_SharedBindings->Sources.Count(oldDirect) == 1) {
        return Parameter::Write(m_Context, oldDirect, value);
    }

    if (value.Kind() == Parameter::BindingKind::Direct) {
        CKObject *sourceObject = value.SourceId()
            ? m_Context->GetObject(value.SourceId()) : nullptr;
        if (sourceObject != value.Source() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
            return Failure(Error::SourceInvalid, "Direct source is invalid.");
        }
        const CKERROR error = input->SetDirectSource(value.Source());
        if (error != CK_OK)
            return Failure(Error::TypeMismatch, "Input rejected its direct source.", error);
    } else if (value.Kind() == Parameter::BindingKind::Shared) {
        CKObject *sourceObject = value.SharedSourceId()
            ? m_Context->GetObject(value.SharedSourceId()) : nullptr;
        if (sourceObject != value.SharedSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETERIN)) {
            return Failure(Error::SourceInvalid, "Shared input source is invalid.");
        }
        const CKERROR error = input->ShareSourceWith(value.SharedSource());
        if (error != CK_OK)
            return Failure(Error::TypeMismatch, "Input rejected its shared source.", error);
    } else {
        std::ostringstream name;
        name << "__BML_BehaviorSource_" << behavior->GetID() << "_"
             << static_cast<int>(slot.Kind) << "_" << slot.NativeIndex;
        CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
            const_cast<CKSTRING>(name.str().c_str()), input->GetGUID(), TRUE);
        if (!literal)
            return Failure(Error::CreateFailed, "Failed to create an input source parameter.");
        Status status = Parameter::Write(m_Context, literal, value);
        if (!status) {
            m_Context->DestroyObject(literal);
            return status;
        }
        const CKERROR error = input->SetDirectSource(literal);
        if (error != CK_OK) {
            m_Context->DestroyObject(literal);
            return Failure(Error::TypeMismatch, "Input rejected its literal source.", error);
        }
        m_SharedBindings->Sources.Own(literal);
        record.OwnedSources.push_back(CaptureObject(literal));
    }

    m_SharedBindings->Sources.Update(input);
    if (oldOwned && m_SharedBindings->Sources.Count(oldDirect) == 0) {
        QueueSourceDestroy(oldRef);
        const auto owned = findOwned();
        if (owned != record.OwnedSources.end())
            record.OwnedSources.erase(owned);
    }
    return {};
}

Status Runtime::EnsurePrototypeLayout(CKBehavior *behavior,
                                                      CKBehaviorPrototype *prototype,
                                                      bool afterSettings) {
    if (!behavior || !prototype) {
        return Failure(Error::InvalidState,
                       "Behavior Prototype is unavailable while restoring its static layout.",
                       CK_OK, CKBR_OK, Phase::StaticLayout);
    }

    const CKDWORD flags = behavior->GetFlags();
    const bool repairInputs = !afterSettings ||
        (flags & CKBEHAVIOR_INTERNALLYCREATEDINPUTS) == 0;
    const bool repairOutputs = !afterSettings ||
        (flags & CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS) == 0;
    const bool repairInputParameters = !afterSettings ||
        (flags & CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS) == 0;
    const bool repairOutputParameters = !afterSettings ||
        (flags & CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS) == 0;

    if (repairInputs) {
        CKBEHAVIORIO_DESC **descriptions = prototype->GetInIOList();
        for (int i = behavior->GetInputCount(); i < prototype->GetInputCount(); ++i) {
            CKBEHAVIORIO_DESC *description = descriptions ? descriptions[i] : nullptr;
            CKSTRING name = description && description->Name
                ? description->Name : const_cast<CKSTRING>("In");
            if (!behavior->CreateInput(name)) {
                return Failure(Error::CreateFailed,
                               "Failed to restore static behavior input #" +
                                   std::to_string(i) + ".",
                               CK_OK, CKBR_OK, Phase::StaticLayout,
                               prototype->GetGuid());
            }
        }
    }

    if (repairOutputs) {
        CKBEHAVIORIO_DESC **descriptions = prototype->GetOutIOList();
        for (int i = behavior->GetOutputCount(); i < prototype->GetOutputCount(); ++i) {
            CKBEHAVIORIO_DESC *description = descriptions ? descriptions[i] : nullptr;
            CKSTRING name = description && description->Name
                ? description->Name : const_cast<CKSTRING>("Out");
            if (!behavior->CreateOutput(name)) {
                return Failure(Error::CreateFailed,
                               "Failed to restore static behavior output #" +
                                   std::to_string(i) + ".",
                               CK_OK, CKBR_OK, Phase::StaticLayout,
                               prototype->GetGuid());
            }
        }
    }

    CKParameterManager *manager = m_Context ? m_Context->GetParameterManager() : nullptr;
    if (repairInputParameters) {
        CKPARAMETER_DESC **descriptions = prototype->GetInParameterList();
        for (int i = 0; i < prototype->GetInParameterCount(); ++i) {
            CKPARAMETER_DESC *description = descriptions ? descriptions[i] : nullptr;
            if (!description) {
                return Failure(Error::InvalidState,
                               "Prototype input parameter metadata is missing.",
                               CK_OK, CKBR_OK, Phase::StaticLayout,
                               prototype->GetGuid());
            }
            CKParameterIn *input = behavior->GetInputParameter(i);
            const std::string expectedName = description->Name ? description->Name : "";
            if (input && input->GetGUID() == description->Guid &&
                std::string(input->GetName() ? input->GetName() : "") == expectedName) {
                continue;
            }

            CKSTRING name = description->Name
                ? description->Name : const_cast<CKSTRING>("");
            if (i < behavior->GetInputParameterCount()) {
                const CKParameterType type = manager
                    ? manager->ParameterGuidToType(description->Guid) : -1;
                input = type >= 0 ? behavior->InsertInputParameter(i, name, type) : nullptr;
                if (input && input->GetGUID() != description->Guid)
                    input->SetGUID(description->Guid, TRUE, name);
            } else {
                input = behavior->CreateInputParameter(name, description->Guid);
            }
            if (!input) {
                Status status = Failure(
                    Error::CreateFailed,
                    "Failed to restore static input parameter #" + std::to_string(i) +
                        " '" + expectedName + "'.",
                    CK_OK, CKBR_OK, Phase::StaticLayout, prototype->GetGuid());
                status.Details.ActualType = description->Guid;
                return status;
            }
        }
    }

    if (repairOutputParameters) {
        CKPARAMETER_DESC **descriptions = prototype->GetOutParameterList();
        for (int i = 0; i < prototype->GetOutParameterCount(); ++i) {
            CKPARAMETER_DESC *description = descriptions ? descriptions[i] : nullptr;
            if (!description) {
                return Failure(Error::InvalidState,
                               "Prototype output parameter metadata is missing.",
                               CK_OK, CKBR_OK, Phase::StaticLayout,
                               prototype->GetGuid());
            }
            CKParameterOut *output = behavior->GetOutputParameter(i);
            const std::string expectedName = description->Name ? description->Name : "";
            if (output && output->GetGUID() == description->Guid &&
                std::string(output->GetName() ? output->GetName() : "") == expectedName) {
                continue;
            }

            CKSTRING name = description->Name
                ? description->Name : const_cast<CKSTRING>("");
            if (i < behavior->GetOutputParameterCount()) {
                const CKParameterType type = manager
                    ? manager->ParameterGuidToType(description->Guid) : -1;
                output = type >= 0 ? behavior->InsertOutputParameter(i, name, type) : nullptr;
                if (output && output->GetGUID() != description->Guid)
                    output->SetGUID(description->Guid);
            } else {
                output = behavior->CreateOutputParameter(name, description->Guid);
            }
            if (!output) {
                Status status = Failure(
                    Error::CreateFailed,
                    "Failed to restore static output parameter #" + std::to_string(i) +
                        " '" + expectedName + "'.",
                    CK_OK, CKBR_OK, Phase::StaticLayout, prototype->GetGuid());
                status.Details.ActualType = description->Guid;
                return status;
            }
        }
    }
    return {};
}

Status Runtime::EnsurePrototypeDefaults(CKBehavior *behavior,
                                                        CKBehaviorPrototype *prototype,
                                                        Record &record) {
    if (!behavior || !prototype)
        return Failure(Error::InvalidState, "Behavior Prototype is unavailable.");
    CKPARAMETER_DESC **descriptions = prototype->GetInParameterList();
    const int count = (std::min)(behavior->GetInputParameterCount(),
                                 prototype->GetInParameterCount());
    for (int index = 0; index < count; ++index) {
        CKParameterIn *input = behavior->GetInputParameter(index);
        CKPARAMETER_DESC *description = descriptions ? descriptions[index] : nullptr;
        if (!input || !description || input->GetRealSource() ||
            input->GetGUID() != description->Guid ||
            std::string(input->GetName() ? input->GetName() : "") !=
                (description->Name ? description->Name : "")) {
            continue;
        }
        const bool hasRaw = description->DefaultValue && description->DefaultValueSize > 0;
        const bool hasText = description->DefaultValueString &&
                             description->DefaultValueString[0] != '\0';
        if (!hasRaw && !hasText)
            continue;
        const Value value = hasRaw
            ? Value::Raw(description->Guid, description->DefaultValue,
                                  static_cast<std::size_t>(description->DefaultValueSize))
            : Value::Text(description->Guid, description->DefaultValueString);
        SlotInfo slot{SlotKind::InputParameter, index, index,
                          input->GetName() ? input->GetName() : "", input->GetGUID(),
                          input->GetRealSource() ? input->GetRealSource()->GetDataSize() : 0};
        Status status = BindInput(behavior, record, slot, value);
        if (!status)
            return status;
        if (CKParameter *source = input->GetDirectSource())
            behavior->SetInputParameterDefaultValue(input, source);
    }
    return {};
}

Status Runtime::BindTarget(CKBehavior *behavior, CKBeObject *owner,
                                           const BlockSpec &spec, Record &record) {
    if (!behavior)
        return Failure(Error::InvalidState, "Behavior is unavailable.",
                       CK_OK, CKBR_OK, Phase::TargetBinding);
    if (spec.m_TargetMode == TargetMode::Owner) {
        const CKERROR error = behavior->UseTarget(FALSE);
        if (error != CK_OK)
            return Failure(Error::TargetInvalid,
                           "Failed to disable the explicit target.", error,
                           CKBR_OK, Phase::TargetBinding,
                           record.PrototypeGuid);
        if (owner && !CKIsChildClassOf(owner, behavior->GetCompatibleClassID())) {
            return Failure(Error::OwnerInvalid,
                           "Owner is incompatible with the Building Block Prototype.",
                           CK_OK, CKBR_OK, Phase::OwnerBinding,
                           record.PrototypeGuid);
        }
        return {};
    }

    const CKERROR useError = behavior->UseTarget(TRUE);
    if (useError != CK_OK)
        return Failure(Error::TargetInvalid,
                       "Failed to enable the explicit target.", useError,
                       CKBR_OK, Phase::TargetBinding,
                       record.PrototypeGuid);
    if (spec.m_TargetValue.Kind() == Parameter::BindingKind::Object &&
        spec.m_TargetValue.ObjectValue()) {
        CKObject *target = spec.m_TargetValue.ObjectId()
            ? m_Context->GetObject(spec.m_TargetValue.ObjectId()) : nullptr;
        if (target != spec.m_TargetValue.ObjectValue() || target->IsToBeDeleted())
            return Failure(Error::TargetInvalid, "Explicit target has expired.",
                           CK_OK, CKBR_OK, Phase::TargetBinding,
                           record.PrototypeGuid);
        if (!CKIsChildClassOf(target, behavior->GetCompatibleClassID())) {
            return Failure(Error::TargetInvalid,
                           "Explicit target is incompatible with the Building Block Prototype.",
                           CK_OK, CKBR_OK, Phase::TargetBinding,
                           record.PrototypeGuid);
        }
    }
    SlotInfo targetSlot;
    Status status = Resolve(
        behavior, Slot::At(SlotKind::Target, 0, spec.m_TargetType), targetSlot);
    if (status)
        status = BindInput(behavior, record, targetSlot, spec.m_TargetValue);
    return Annotate(std::move(status), Phase::TargetBinding,
                    record.PrototypeGuid);
}

Status Runtime::ApplyBindings(CKBehavior *behavior, const BlockSpec &spec,
                                              Record &record) {
    for (const BlockSpec::Binding &binding : spec.m_Locals) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            record.PrototypeGuid, &binding.Target);
        status = Parameter::Write(
            m_Context, ResolveParameter(behavior, slot), binding.Source);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            record.PrototypeGuid, &binding.Target);
    }
    for (const BlockSpec::Binding &binding : spec.m_Inputs) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            record.PrototypeGuid, &binding.Target);
        status = BindInput(behavior, record, slot, binding.Source);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            record.PrototypeGuid, &binding.Target);
    }
    return {};
}

void Runtime::PruneOwnedSources(Record &record) {
    for (auto it = record.OwnedSources.begin(); it != record.OwnedSources.end();) {
        CKObject *object = ResolveObject(*it);
        auto *source = object && CKIsChildClassOf(object, CKCID_PARAMETER)
            ? static_cast<CKParameter *>(object) : nullptr;
        const bool used = source &&
            m_SharedBindings->Sources.Count(source) != 0;
        if (used) {
            ++it;
        } else {
            QueueSourceDestroy(*it);
            it = record.OwnedSources.erase(it);
        }
    }
}

void Runtime::SweepRecords() {
    for (auto &[instanceId, record] : m_Records) {
        CKBehavior *behavior = ResolveBehavior(record);
        if (!behavior) {
            record.Expired = true;
            record.Protocol.RequestClose({
                ExecutionError::Cancelled, CKBR_BEHAVIORERROR,
                "Behavior object was deleted during execution."});
            record.NativeLifecycle.RequestClose();
            CloseCallbacks(record);
            continue;
        }
        m_SharedBindings->Sources.Update(behavior);
        PruneOwnedSources(record);
    }
}

Status Runtime::Configure(CKBehavior *behavior,
                          CKBeObject *owner, CKBehavior *parent,
                          const BlockSpec &spec,
                          const CKBehaviorContext *frame,
                          Record &record) {
    Status status = CreateBlock(behavior, owner, parent, spec, frame, record);
    return status ? EditBlock(behavior, owner, parent, spec, frame, record)
                  : status;
}

Status Runtime::CreateBlock(CKBehavior *behavior,
                            CKBeObject *owner, CKBehavior *parent,
                            const BlockSpec &spec,
                            const CKBehaviorContext *frame,
                            Record &record) {
    if (!behavior)
        return Failure(Error::InvalidState, "Behavior configuration target is invalid.");
    struct ConfiguringScope final {
        std::vector<Record *> &Stack;
        explicit ConfiguringScope(std::vector<Record *> &stack, Record &record)
            : Stack(stack) { Stack.push_back(&record); }
        ~ConfiguringScope() { Stack.pop_back(); }
    } configuring(m_ConfiguringRecords, record);
    LifecyclePlan plan;
    plan.HasOwner = owner != nullptr;
    plan.HasInterface = !spec.m_AddedInputs.empty() ||
                        !spec.m_AddedOutputs.empty();
    plan.SettingStages.reserve(spec.m_SettingStages.size());
    for (const std::vector<BlockSpec::Binding> &stage : spec.m_SettingStages)
        plan.SettingStages.push_back(!stage.empty());

    NativeLifecycleAdapter adapter(*this, record, owner, parent, &spec, frame);
    if (record.NativeLifecycle.Create(plan, adapter))
        return {};
    return LifecycleStatus(adapter, record);
}

Status Runtime::EditBlock(CKBehavior *behavior,
                          CKBeObject *owner, CKBehavior *parent,
                          const BlockSpec &spec,
                          const CKBehaviorContext *frame,
                          Record &record) {
    if (!behavior)
        return Failure(Error::InvalidState,
                       "Behavior configuration target is invalid.");
    struct ConfiguringScope final {
        std::vector<Record *> &Stack;
        explicit ConfiguringScope(std::vector<Record *> &stack, Record &record)
            : Stack(stack) { Stack.push_back(&record); }
        ~ConfiguringScope() { Stack.pop_back(); }
    } configuring(m_ConfiguringRecords, record);

    NativeLifecycleAdapter adapter(*this, record, owner, parent, &spec, frame);
    if (record.NativeLifecycle.Edit(adapter))
        return {};
    return LifecycleStatus(adapter, record);
}

Status Runtime::LifecycleStatus(const NativeLifecycleAdapter &adapter,
                                const Record &record) const {
    if (!adapter.LastStatus())
        return adapter.LastStatus();

    const LifecycleFault &fault = record.NativeLifecycle.Failure();
    Error error = Error::InvalidState;
    Phase phase = Phase::LifecycleCallback;
    switch (fault.Code) {
    case LifecycleError::InitializationFailed:
    case LifecycleError::LayoutFailed:
        error = Error::InitFailed;
        phase = Phase::Initialization;
        break;
    case LifecycleError::SettingFailed:
        error = Error::ValueWriteFailed;
        phase = Phase::Settings;
        break;
    case LifecycleError::RelationFailed:
        error = Error::OwnerInvalid;
        phase = Phase::OwnerBinding;
        break;
    case LifecycleError::CallbackFailed:
        error = Error::CallbackFailed;
        break;
    case LifecycleError::BindingFailed:
        error = Error::SourceInvalid;
        phase = Phase::ParameterBinding;
        break;
    default:
        break;
    }
    return Failure(error,
                   fault.Message.empty()
                       ? "Behavior native lifecycle configuration failed."
                       : fault.Message,
                   CK_OK, fault.NativeCode, phase,
                   record.PrototypeGuid);
}

Status Runtime::CallCallback(Record &record, CKDWORD message,
                             const CKBehaviorContext *frame) const {
    CKBehavior *behavior = ResolveBehavior(record);
    if (!behavior || !m_Context)
        return Failure(Error::InvalidState, "Cannot invoke callback on an expired behavior.");

    const CK_ID behaviorId = behavior->GetID();
    const CKGUID prototypeGuid = record.PrototypeGuid;
    int result = CK_OK;
    if (message == CKM_BEHAVIORCREATE) {
        // Ballance's retail CK2.dll maps CKM_BEHAVIORCREATE to a zero callback
        // mask inside CallCallbackFunction. Dispatch CREATE from the live block
        // data so the provider's current callback, mask, and argument are kept.
        BehaviorBlockData *block = CKBehaviorAccess::BlockData(behavior);
        if (block && block->m_Callback &&
            (block->m_CallbackMask & CKCB_BEHAVIORCREATE) != 0) {
            BehaviorContextScope scope(m_Context, behavior, frame);
            m_Context->m_BehaviorContext.CallbackMessage = message;
            m_Context->m_BehaviorContext.CallbackArg = block->m_CallbackArg;
            result = block->m_Callback(m_Context->m_BehaviorContext);
        }
    } else {
        BehaviorContextScope scope(m_Context, behavior, frame);
        result = behavior->CallCallbackFunction(message);
    }
    CKObject *current = m_Context->GetObject(behaviorId);
    if (current != behavior || (current && current->IsToBeDeleted())) {
        Status status = Failure(
            Error::InvalidState,
            "Building Block was destroyed by its lifecycle callback.", CK_OK,
            CKBR_OK, Phase::LifecycleCallback,
            prototypeGuid);
        status.Details.CallbackMessage = message;
        return status;
    }
    if (result == CK_OK)
        return {};
    Status status = Failure(
        Error::CallbackFailed, "Building Block lifecycle callback failed.",
        result, CKBR_OK, Phase::LifecycleCallback,
        prototypeGuid);
    status.Details.CallbackMessage = message;
    return status;
}

Status Runtime::SetInput(Instance &instance, const Slot &selector,
                                         const Parameter::Binding &value) {
    SlotRef slot;
    Status status = Resolve(instance, selector, slot);
    return status ? SetInput(instance, slot, value) : status;
}

Status Runtime::SetInput(Instance &instance,
                                         const SlotRef &slot,
                                         const Parameter::Binding &value) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Inputs can only be rebound while the instance is idle.");
    Status status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != SlotKind::InputParameter &&
        slot.Slot.Kind != SlotKind::Target) {
        return Failure(Error::InvalidState, "Resolved slot is not an input parameter.");
    }
    status = BindInput(behavior, *record, slot.Slot, value);
    PruneOwnedSources(*record);
    if (status) {
        const Slot selector = LiveSelector(slot.Slot);
        if (slot.Slot.Kind == SlotKind::Target) {
            record->Desired.m_TargetMode = TargetMode::Explicit;
            record->Desired.m_TargetType = slot.Slot.Type;
            record->Desired.m_TargetValue = value;
        } else {
            record->Desired.Input(selector, value);
        }
    }
    return status;
}

Status Runtime::SetLocal(Instance &instance, const Slot &selector,
                                         const Parameter::Binding &value) {
    SlotRef slot;
    Status status = Resolve(instance, selector, slot);
    return status ? SetLocal(instance, slot, value) : status;
}

Status Runtime::SetLocal(Instance &instance,
                                         const SlotRef &slot,
                                         const Parameter::Binding &value) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Locals can only be edited while the instance is idle.");
    Status status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != SlotKind::Local)
        return Failure(Error::InvalidState, "Resolved slot is not a local parameter.");
    Status written = Parameter::Write(
        m_Context, ResolveParameter(behavior, slot.Slot), value);
    if (written)
        record->Desired.Local(LiveSelector(slot.Slot), value);
    return written;
}

Status Runtime::Bind(Instance &instance, const SlotRef &slot,
                     CKBehavior *source, const Slot &sourceSlot,
                     Parameter::BindingKind relation) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    if (!source || source->GetCKContext() != m_Context ||
        source->IsToBeDeleted()) {
        return Failure(Error::SourceInvalid,
                       "Behavior parameter source is stale or belongs to another CKContext.");
    }
    SlotInfo resolved;
    Status status = Resolve(source, sourceSlot, resolved);
    if (!status)
        return status;
    if (relation == Parameter::BindingKind::Direct) {
        CKParameter *parameter = LiveLayout(
            m_Context, source, PrototypeGuid(source), PrototypeOf(source))
                .Parameter(resolved);
        if (!parameter)
            return Failure(Error::SourceInvalid,
                           "The source slot has no live parameter value.");
        return SetInput(instance, slot, Parameter::Binding::Direct(parameter));
    }
    if (relation == Parameter::BindingKind::Shared) {
        if (resolved.Kind != SlotKind::InputParameter &&
            resolved.Kind != SlotKind::Target) {
            return Failure(Error::SourceInvalid,
                           "Only a Pin or Target can share its Virtools source.");
        }
        CKObject *object = LiveLayout(
            m_Context, source, PrototypeGuid(source), PrototypeOf(source))
                .Object(resolved);
        auto *input = object && CKIsChildClassOf(object, CKCID_PARAMETERIN)
            ? static_cast<CKParameterIn *>(object) : nullptr;
        if (!input)
            return Failure(Error::SourceInvalid,
                           "The source Pin or Target is no longer live.");
        return SetInput(instance, slot, Parameter::Binding::Shared(input));
    }
    return Failure(Error::SourceInvalid,
                   "Behavior Bind accepts only direct or shared Virtools sources.");
}

Status Runtime::Configure(Instance &instance, const BlockSpec &settings,
                          const CKBehaviorContext *frame) {
    Record *record = FindRecord(instance);
    if (!record)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    // A Setting callback may create another managed Behavior and rehash the
    // record table. Keep the desired native relations stable across callbacks.
    const BlockSpec desired = record->Desired;
    return ApplySettings(instance, settings.m_SettingStages,
                         desired, frame);
}

Status Runtime::Reconfigure(Instance &instance, const BlockSpec &spec,
                                            const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Reconfiguration requires an idle behavior instance.");
    if (spec.Prototype() != record->PrototypeGuid)
        return Failure(Error::InvalidState,
                       "Reconfiguration spec names a different Building Block Prototype.");
    if (!spec.m_AddedInputs.empty() || !spec.m_AddedOutputs.empty())
        return Failure(Error::InvalidState,
                       "Reconfiguration cannot append duplicate behavior IOs.");

    Status status = ApplySettings(instance, spec.m_SettingStages, spec, frame);
    if (status) {
        record = FindRecord(instance);
        if (record) {
            record->Desired = spec;
            record->Desired.m_SettingStages.clear();
            record->Desired.m_AddedInputs.clear();
            record->Desired.m_AddedOutputs.clear();
            record->Desired.m_KeepAlive.clear();
        }
    }
    return status;
}

Status Runtime::ApplySettings(
    Instance &instance,
    const std::vector<std::vector<BlockSpec::Binding>> &settings,
    const BlockSpec &desired, const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Configuration requires an idle behavior instance.");

    CKBehaviorPrototype *prototype = record->Prototype;
    if (!prototype)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype is unavailable.");
    CKBeObject *owner = behavior->GetOwner();
    const std::uint64_t instanceId = record->Id;
    const auto fail = [this, instanceId](Status failure) {
        Record *current = FindRecord(instanceId);
        if (current && current->Failure.Code == Error::None)
            current->Failure = failure;
        return failure;
    };
    ++record->LayoutGeneration;
    Status status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return fail(std::move(status));
    status = BindTarget(behavior, owner, desired, *record);
    if (!status)
        return fail(std::move(status));
    for (const auto &stage : settings) {
        for (const BlockSpec::Binding &binding : stage) {
            SlotInfo setting;
            status = Resolve(behavior, binding.Target, setting);
            if (!status) {
                status = Annotate(std::move(status), Phase::Settings,
                                  record->PrototypeGuid, &binding.Target);
                return fail(std::move(status));
            }
            status = Parameter::Write(
                m_Context, ResolveParameter(behavior, setting), binding.Source);
            if (!status) {
                status = Annotate(std::move(status), Phase::Settings,
                                  record->PrototypeGuid, &binding.Target);
                return fail(std::move(status));
            }
        }
        if (stage.empty())
            continue;
        ++record->LayoutGeneration;
        status = CallCallback(*record, CKM_BEHAVIORSETTINGSEDITED, frame);
        if (!status)
            return fail(std::move(status));
        status = Reacquire(instanceId, behavior, record);
        if (!status)
            return fail(std::move(status));
        owner = behavior->GetOwner();
        prototype = record->Prototype;
        status = EnsurePrototypeLayout(behavior, prototype, true);
        if (!status)
            return fail(std::move(status));
        status = BindTarget(behavior, owner, desired, *record);
        if (!status)
            return fail(std::move(status));
        status = EnsurePrototypeDefaults(behavior, prototype, *record);
        if (!status)
            return fail(std::move(status));
    }
    status = ApplyBindings(behavior, desired, *record);
    if (!status)
        return fail(std::move(status));
    ++record->LayoutGeneration;
    status = CallCallback(*record, CKM_BEHAVIOREDITED, frame);
    if (!status)
        return fail(std::move(status));
    status = Reacquire(instanceId, behavior, record);
    if (!status)
        return fail(std::move(status));
    owner = behavior->GetOwner();
    prototype = record->Prototype;
    status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return fail(std::move(status));
    status = BindTarget(behavior, owner, desired, *record);
    if (!status)
        return fail(std::move(status));
    status = EnsurePrototypeDefaults(behavior, prototype, *record);
    if (!status)
        return fail(std::move(status));
    status = ApplyBindings(behavior, desired, *record);
    PruneOwnedSources(*record);
    return status ? status : fail(std::move(status));
}

RunResult Runtime::Pulse(Instance &instance, const Slot &input,
                                       const CKBehaviorContext *frame) {
    Slot entry = input;
    entry.Kind = SlotKind::Input;
    SlotRef slot;
    Status status = Resolve(instance, entry, slot);
    if (!status)
        return {std::move(status), RunState::Failed, CKBR_PARAMETERERROR, {}};
    Record *record = FindRecord(instance);
    if (!record)
        return {Failure(Error::InvalidState, "Behavior instance has expired."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    const ExecutionInput activation = entry.UsesName()
        ? ExecutionInput::Named(entry.Name, entry.Occurrence, entry.RequireUnique)
        : ExecutionInput::At(slot.Slot.NativeIndex, record->LayoutGeneration);
    return Execute(record->Id, &activation, false, frame);
}

RunResult Runtime::Pulse(Instance &instance,
                                       const SlotRef &input,
                                       const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return {std::move(ready), RunState::Failed, CKBR_BEHAVIORERROR, {}};
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return {Failure(Error::InvalidState, "Behavior instance has expired."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Failure.Code != Error::None)
        return {record->Failure, RunState::Failed, CKBR_BEHAVIORERROR, {}};
    Status status = ValidateSlot(*record, input);
    if (!status)
        return {std::move(status), RunState::Failed, CKBR_PARAMETERERROR, {}};
    if (input.Slot.Kind != SlotKind::Input)
        return {Failure(Error::InvalidState, "Resolved slot is not a behavior input."),
                RunState::Failed, CKBR_PARAMETERERROR, {}};
    const ExecutionInput activation = ExecutionInput::At(
        input.Slot.NativeIndex, input.LayoutGeneration);
    return Execute(record->Id, &activation, false, frame);
}

RunResult Runtime::Step(Instance &instance, const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return {std::move(ready), RunState::Failed, CKBR_BEHAVIORERROR, {}};
    Record *record = FindRecord(instance);
    if (!record)
        return {Failure(Error::InvalidState, "Behavior instance has expired."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Failure.Code != Error::None)
        return {record->Failure, RunState::Failed, CKBR_BEHAVIORERROR, {}};
    return Execute(record->Id, nullptr, false, frame);
}

RunResult Runtime::StartTask(Instance &instance, const Slot &input,
                                           const CKBehaviorContext *frame) {
    return Pulse(instance, input, frame);
}

Status Runtime::Continue(Instance &instance) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    if (!record)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    if (record->Protocol.State() != ExecutionState::Pending)
        return Failure(Error::InvalidState,
                       "Only a pending behavior instance can be continued.");
    record->Protocol.Continue();
    QueueFrame(*record);
    return {};
}

bool Runtime::IsTaskActive(const Instance &instance) const {
    if (!ReadyStatus())
        return false;
    const Record *record = FindRecord(instance);
    return record && record->Protocol.NeedsFrame();
}

ExecutionState Runtime::State(const Instance &instance) const {
    if (!ReadyStatus())
        return ExecutionState::Closed;
    const Record *record = FindRecord(instance);
    if (!record)
        return ExecutionState::Closed;
    return record->Failure.Code == Error::None
        ? record->Protocol.State() : ExecutionState::Failed;
}

std::vector<RunFrame> Runtime::Take(Instance &instance) {
    if (!ReadyStatus())
        return {};
    Record *record = FindRecord(instance);
    return record ? record->Protocol.Take() : std::vector<RunFrame>{};
}

std::shared_ptr<FrameStore> Runtime::Frames(const Instance &instance) const {
    const Record *record = FindRecord(instance);
    return record ? record->Protocol.Frames() : nullptr;
}

Status Runtime::InstanceFailure(const Instance &instance) const {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    const Record *record = FindRecord(instance);
    if (!record)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Failure.Code != Error::None)
        return record->Failure;
    const ExecutionFault &fault = record->Protocol.Failure();
    if (!fault)
        return {};
    return ExecutionFailure(fault, record->PrototypeGuid);
}

bool Runtime::ProcessTasks(const CKBehaviorContext *frame) {
    if (!ReadyStatus())
        return false;
    if (m_ProcessingTasks)
        return false;
    FlagScope processing(m_ProcessingTasks);
    bool executed = false;
    m_FrameRecords.assign(m_FrameQueue.begin(), m_FrameQueue.end());
    m_FrameQueue.clear();
    for (std::uint64_t instanceId : m_FrameRecords) {
        auto it = m_Records.find(instanceId);
        if (it == m_Records.end())
            continue;
        it->second.QueuedForFrame = false;
        if (!it->second.Protocol.NeedsFrame())
            continue;
        executed = true;
        (void) Execute(instanceId, nullptr, false, frame);
    }
    m_FrameRecords.clear();
    return executed;
}

void Runtime::ProcessFrame() {
    if (!ReadyStatus())
        return;
    if (m_ProcessingFrame)
        return;
    FlagScope processing(m_ProcessingFrame);
    ++m_Frame;
    DrainDeferredReleases();
    SweepRecords();
    const bool lifecycleRan = DrainCloseQueue();
    const bool behaviorRan = ProcessTasks(&m_Context->m_BehaviorContext);
    // Native execution and lifecycle callbacks can delete or replace any CK
    // object. With neither, the records validated above cannot change during
    // this Runtime safe point, so repeating the full CK identity walk adds no
    // information.
    if (lifecycleRan || behaviorRan) {
        SweepRecords();
        DrainCloseQueue();
    }
    AdoptSharedBindings();
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (pending.Frames > 0)
            --pending.Frames;
    }
    DestroyReady(DestroyMode::Ready);
}

void Runtime::ClosePending() {
    if (!ReadyStatus())
        return;
    DrainDeferredReleases();
    SweepRecords();
    DrainCloseQueue();
    AdoptSharedBindings();
    DestroyReady(DestroyMode::Ready);
}

RunResult Runtime::Execute(std::uint64_t instanceId,
                           const ExecutionInput *input, bool once,
                           const CKBehaviorContext *frame) {
    Record *record = FindRecord(instanceId);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return {Failure(Error::InvalidState, "Behavior instance has expired."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    const CKGUID prototypeGuid = record->PrototypeGuid;
    if (record->GraphResident && !record->OwnerDriven)
        return {Failure(Error::InvalidState,
                        "Graph-resident Building Blocks must be executed by their parent graph.",
                        CK_OK, CKBR_OK, Phase::Execution,
                        prototypeGuid),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    NativeAdapter adapter(*this, instanceId, frame);
    ExecutionResult executed = input
        ? (once ? record->Protocol.Call(*input, m_Frame, adapter)
                : record->Protocol.Pulse(*input, m_Frame, adapter))
        : record->Protocol.Step(m_Frame, adapter);

    RunResult result;
    result.Admission = executed.State;
    if (executed.State == AdmissionState::Queued) {
        result.State = RunState::Pending;
    } else if (executed.State == AdmissionState::Failed) {
        result.State = RunState::Failed;
    }

    if (executed.Frame) {
        result.ReturnCode = executed.Frame->ReturnCode;
        result.Detail.BehaviorResult = executed.Frame->ReturnCode;
        result.ActiveOutputs = std::move(executed.Frame->ActiveOutputs);
    }

    if (executed.Fault) {
        result.Detail = ExecutionFailure(executed.Fault, prototypeGuid);
    }

    record = FindRecord(instanceId);
    if (!record)
        return result;

    const ExecutionState state = record->Protocol.State();
    if (executed.State == AdmissionState::Executed) {
        if (state == ExecutionState::Pending)
            result.State = RunState::Pending;
        else if (state == ExecutionState::Failed ||
                 state == ExecutionState::Closing ||
                 state == ExecutionState::Closed)
            result.State = RunState::Failed;
        else
            result.State = RunState::Ready;
    }
    QueueFrame(*record);

    if (record->Expired || ResolveBehavior(*record) != behavior) {
        for (ObjectStamp source : record->OwnedSources)
            QueueSourceDestroy(source);
        m_Records.erase(instanceId);
        return result;
    }

    return result;
}

int Runtime::ExecuteNative(CKBehavior *behavior, const CKBehaviorContext *frame) const {
    if (!behavior || !m_Context)
        return CKBR_BEHAVIORERROR;
    BehaviorExecutionScope scope(m_Context, behavior, frame);
    return behavior->Execute(m_Context->m_BehaviorContext.DeltaTime);
}

Status Runtime::Reacquire(std::uint64_t instanceId, CKBehavior *behavior,
                                          Record *&record) {
    record = FindRecord(instanceId);
    if (!record || !behavior || ResolveBehavior(*record) != behavior ||
        behavior->IsToBeDeleted()) {
        return Failure(Error::InvalidState,
                       "Building Block instance changed or disappeared during a callback.",
                       CK_OK, CKBR_OK, Phase::LifecycleCallback,
                       behavior ? PrototypeGuid(behavior) : CKGUID());
    }
    return {};
}

Runtime::Record *Runtime::FindRecord(const Instance &instance) {
    if (instance.m_Access.lock() != m_Access || !instance.m_Id)
        return nullptr;
    return FindRecord(instance.m_Id);
}

const Runtime::Record *Runtime::FindRecord(const Instance &instance) const {
    if (instance.m_Access.lock() != m_Access || !instance.m_Id)
        return nullptr;
    return FindRecord(instance.m_Id);
}

Runtime::Record *Runtime::FindRecord(std::uint64_t instanceId) {
    auto it = m_Records.find(instanceId);
    return it == m_Records.end() ? nullptr : &it->second;
}

const Runtime::Record *Runtime::FindRecord(std::uint64_t instanceId) const {
    auto it = m_Records.find(instanceId);
    return it == m_Records.end() ? nullptr : &it->second;
}

Runtime::Record *Runtime::FindRecord(CKBehavior *behavior) {
    return const_cast<Record *>(
        static_cast<const Runtime *>(this)->FindRecord(behavior));
}

const Runtime::Record *Runtime::FindRecord(CKBehavior *behavior) const {
    if (!behavior)
        return nullptr;
    for (auto it = m_ConfiguringRecords.rbegin();
         it != m_ConfiguringRecords.rend(); ++it) {
        const Record *record = *it;
        if (record && record->Behavior.Address == behavior &&
            ResolveBehavior(*record) == behavior)
            return record;
    }
    for (const auto &[id, record] : m_Records) {
        (void) id;
        if (record.Behavior.Address == behavior &&
            ResolveBehavior(record) == behavior)
            return &record;
    }
    return nullptr;
}

CKGUID Runtime::PrototypeGuid(CKBehavior *behavior) const {
    const Record *record = FindRecord(behavior);
    return record ? record->PrototypeGuid
                  : (behavior ? behavior->GetPrototypeGuid() : CKGUID());
}

CKBehaviorPrototype *Runtime::PrototypeOf(CKBehavior *behavior) const {
    const Record *record = FindRecord(behavior);
    return record ? record->Prototype
                  : (behavior ? behavior->GetPrototype() : nullptr);
}

Runtime::ObjectStamp Runtime::CaptureObject(CKObject *object) const {
    if (!object || !m_Context || object->GetCKContext() != m_Context ||
        object->IsToBeDeleted()) {
        return {};
    }
    const CK_ID id = object->GetID();
    return id != 0 && CKGetObject(m_Context, id) == object
        ? ObjectStamp{id, object} : ObjectStamp{};
}

CKObject *Runtime::ResolveObject(ObjectStamp object) const {
    if (!m_Context || object.Id == 0 || !object.Address)
        return nullptr;
    CKObject *current = CKGetObject(m_Context, object.Id);
    return current == object.Address && !current->IsToBeDeleted() ? current : nullptr;
}

CKBehavior *Runtime::ResolveBehavior(const Record &record) const {
    CKObject *object = ResolveObject(record.Behavior);
    return object && CKIsChildClassOf(object, CKCID_BEHAVIOR)
        ? static_cast<CKBehavior *>(object) : nullptr;
}

void Runtime::RequestRelease(std::uint64_t instanceId) {
    if (!instanceId)
        return;
    if (m_Thread == std::this_thread::get_id()) {
        Release(instanceId);
        return;
    }
    std::lock_guard<std::mutex> lock(m_DeferredMutex);
    m_DeferredReleases.push_back(instanceId);
}

void Runtime::DrainDeferredReleases() {
    std::vector<std::uint64_t> releases;
    {
        std::lock_guard<std::mutex> lock(m_DeferredMutex);
        releases.swap(m_DeferredReleases);
    }
    for (std::uint64_t instanceId : releases)
        Release(instanceId);
}

void Runtime::QueueFrame(Record &record) {
    if (!record.Protocol.NeedsFrame() || record.QueuedForFrame)
        return;
    record.QueuedForFrame = true;
    m_FrameQueue.push_back(record.Id);
}

void Runtime::Release(std::uint64_t instanceId) {
    auto it = m_Records.find(instanceId);
    if (it == m_Records.end())
        return;
    it->second.Protocol.RequestClose();
    it->second.NativeLifecycle.RequestClose();
    CloseCallbacks(it->second);
}

void Runtime::QueueDestroy(Record &record) {
    PendingDestroy pending;
    pending.Behavior = record.Behavior;
    pending.Parent = record.Parent;
    pending.Sources = std::move(record.OwnedSources);
    pending.KeepAlive = std::move(record.KeepAlive);
    pending.Frames = 0;
    pending.DestroyBehavior = true;
    pending.GraphResident = record.NativeLifecycle.Ledger().Placed;
    m_PendingDestroy.push_back(std::move(pending));
}

void Runtime::CloseCallbacks(Record &record) noexcept {
    for (const std::shared_ptr<CallbackResource> &resource : record.KeepAlive) {
        if (resource)
            resource->CloseAdmission();
    }
}

bool Runtime::DrainCloseQueue(bool force) {
    bool lifecycleRan = false;
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        Record &record = it->second;
        if (!record.NativeLifecycle.CloseRequested() ||
            record.Protocol.State() == ExecutionState::Running) {
            ++it;
            continue;
        }
        lifecycleRan = true;
        NativeLifecycleAdapter adapter(*this, record, nullptr, nullptr,
                                       nullptr, nullptr);
        (void) record.NativeLifecycle.Drain(adapter);
        record.Protocol.MarkClosed();
        it = m_Records.erase(it);
    }
    if (force)
        DestroyReady(DestroyMode::Reset);
    return lifecycleRan;
}

void Runtime::QueueSourceDestroy(ObjectStamp source, int frames) {
    if (source.Id == 0)
        return;
    PendingDestroy pending;
    pending.Sources.push_back(source);
    pending.Frames = frames;
    m_PendingDestroy.push_back(std::move(pending));
}

void Runtime::DestroyConnectedLinks(CKBehavior *parent, CKBehavior *behavior) {
    if (!parent || !behavior)
        return;
    for (int i = parent->GetSubBehaviorLinkCount() - 1; i >= 0; --i) {
        CKBehaviorLink *link = parent->GetSubBehaviorLink(i);
        if (!link)
            continue;
        CKBehaviorIO *source = link->GetInBehaviorIO();
        CKBehaviorIO *destination = link->GetOutBehaviorIO();
        if ((!source || source->GetOwner() != behavior) &&
            (!destination || destination->GetOwner() != behavior)) {
            continue;
        }
        link = parent->RemoveSubBehaviorLink(i);
        if (link)
            m_Context->DestroyObject(link);
    }
}

void Runtime::AdoptSharedBindings() {
    if (!m_SharedBindings || m_SharedBindings->Pending.empty())
        return;
    m_PendingDestroy.splice(m_PendingDestroy.end(), m_SharedBindings->Pending);
}

void Runtime::Close() {
    if (!m_Context || m_Thread != std::this_thread::get_id())
        return;
    DrainDeferredReleases();
    for (auto &[instanceId, record] : m_Records) {
        record.Protocol.RequestClose();
        record.NativeLifecycle.RequestClose();
        CloseCallbacks(record);
    }
    DrainCloseQueue();
    DestroyReady(DestroyMode::Close);
}

void Runtime::DestroyReady(DestroyMode mode) {
    const bool requestedForce = mode == DestroyMode::Reset;
    if (m_Destroying) {
        m_ForceDestroyPending = m_ForceDestroyPending || requestedForce;
        return;
    }
    m_Destroying = true;
    const bool force = requestedForce || std::exchange(m_ForceDestroyPending, false);
    const bool closing = mode == DestroyMode::Close;
    // Entries retained by this pass are collected separately and appended
    // once the walk is over. Appending to m_PendingDestroy while walking it
    // would revisit them in the same pass, and in force mode nothing changes
    // between visits: a callback still on the stack keeps RetireAtSafePoint
    // false, so the walk would never terminate.
    std::list<PendingDestroy> deferred;
    auto it = m_PendingDestroy.begin();
    while (it != m_PendingDestroy.end()) {
        if (!force && !closing && it->Frames > 0) {
            ++it;
            continue;
        }
        auto resolveBehavior = [&]() -> CKBehavior * {
            CKObject *object = ResolveObject(it->Behavior);
            return object && CKIsChildClassOf(object, CKCID_BEHAVIOR)
                ? static_cast<CKBehavior *>(object) : nullptr;
        };
        CKBehavior *behavior = resolveBehavior();
        if (behavior && it->DestroyBehavior)
            m_SharedBindings->Sources.Remove(behavior);
        behavior = resolveBehavior();

        PendingDestroy retained;
        retained.Frames = 1;
        if (!force) {
            for (auto source = it->Sources.begin(); source != it->Sources.end();) {
                CKObject *object = ResolveObject(*source);
                auto *parameter = object && CKIsChildClassOf(object, CKCID_PARAMETER)
                    ? static_cast<CKParameter *>(object) : nullptr;
                if (parameter &&
                    m_SharedBindings->Sources.Count(parameter) != 0) {
                    retained.Sources.push_back(*source);
                    source = it->Sources.erase(source);
                } else {
                    ++source;
                }
            }
        }

        std::vector<ObjectStamp> sources = std::move(it->Sources);
        it->Sources.clear();
        if (it->DestroyBehavior) {
            behavior = resolveBehavior();
            if (behavior && it->GraphResident) {
                CKObject *parentObject = ResolveObject(it->Parent);
                if (parentObject && CKIsChildClassOf(parentObject, CKCID_BEHAVIOR)) {
                    auto *parent = static_cast<CKBehavior *>(parentObject);
                    DestroyConnectedLinks(parent, behavior);
                    parent->RemoveSubBehavior(behavior);
                }
            }
            behavior = resolveBehavior();
            if (behavior) {
                behavior->SetOwner(nullptr, FALSE);
                m_Context->DestroyObject(behavior);
            }
        }
        for (ObjectStamp source : sources) {
            if (CKObject *object = ResolveObject(source)) {
                if (CKIsChildClassOf(object, CKCID_PARAMETER))
                    m_SharedBindings->Sources.Remove(
                        static_cast<CKParameter *>(object));
                m_Context->DestroyObject(object);
            }
        }
        for (std::shared_ptr<CallbackResource> &resource : it->KeepAlive) {
            if (resource && !resource->RetireAtSafePoint())
                retained.KeepAlive.push_back(std::move(resource));
        }
        it = m_PendingDestroy.erase(it);
        if (!retained.Sources.empty() || !retained.KeepAlive.empty()) {
            if (closing && m_SharedBindings)
                m_SharedBindings->Pending.push_back(std::move(retained));
            else
                deferred.push_back(std::move(retained));
        }
    }
    m_PendingDestroy.splice(m_PendingDestroy.end(), deferred);
    m_Destroying = false;
    if (m_ForceDestroyPending) {
        AdoptSharedBindings();
        DestroyReady(DestroyMode::Reset);
    }
}

void Runtime::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0)
        return;
    if (!ReadyStatus())
        return;
    AdoptSharedBindings();
    m_SharedBindings->Sources.Remove(ids, count);
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (ContainsId(ids, count, pending.Behavior.Id)) {
            pending.Behavior = {};
            pending.GraphResident = false;
        }
        pending.Sources.erase(
            std::remove_if(pending.Sources.begin(), pending.Sources.end(),
                           [&](ObjectStamp source) { return ContainsId(ids, count, source.Id); }),
            pending.Sources.end());
    }
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        const bool deleting = ContainsId(ids, count, it->second.Behavior.Id) ||
                              ContainsId(ids, count, it->second.Parent.Id);
        if (!deleting) {
            it->second.OwnedSources.erase(
                std::remove_if(it->second.OwnedSources.begin(), it->second.OwnedSources.end(),
                               [&](ObjectStamp source) { return ContainsId(ids, count, source.Id); }),
                it->second.OwnedSources.end());
            ++it;
            continue;
        }
        it->second.Expired = true;
        it->second.Protocol.RequestClose({
            ExecutionError::Cancelled, CKBR_BEHAVIORERROR,
            "Behavior object or parent graph was deleted."});
        it->second.NativeLifecycle.RequestClose();
        CloseCallbacks(it->second);
        ++it;
    }
}

void Runtime::ResetWorld() {
    if (!m_Context || m_Thread != std::this_thread::get_id())
        return;
    DrainDeferredReleases();
    AdoptSharedBindings();
    for (auto &[instanceId, record] : m_Records) {
        record.Protocol.RequestClose({
            ExecutionError::Cancelled, CKBR_BEHAVIORERROR,
            "World reset cancelled behavior execution."});
        record.NativeLifecycle.RequestClose(true);
        CloseCallbacks(record);
    }
    DrainCloseQueue(true);
}

Status Runtime::Close(CKBehavior *behavior) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    if (!behavior)
        return Failure(Error::InvalidState,
                       "Cannot close a null graph-resident Building Block.");

    for (auto it = m_ConfiguringRecords.rbegin();
         it != m_ConfiguringRecords.rend(); ++it) {
        Record *record = *it;
        if (!record || ResolveBehavior(*record) != behavior)
            continue;
        record->Protocol.RequestClose();
        record->NativeLifecycle.RequestClose();
        CloseCallbacks(*record);
        return {};
    }

    for (auto &[instanceId, record] : m_Records) {
        if (ResolveBehavior(record) != behavior)
            continue;
        if (!record.GraphResident) {
            return Failure(
                Error::InvalidState,
                "Runtime::Close only accepts Runtime-owned graph nodes.");
        }
        record.Protocol.RequestClose();
        record.NativeLifecycle.RequestClose();
        CloseCallbacks(record);
        DrainCloseQueue();
        DestroyReady(DestroyMode::Ready);
        return {};
    }
    return Failure(Error::InvalidState,
                   "Building Block is not owned by this Behavior Runtime.");
}

const char *DescribeError(Error error) {
    switch (error) {
    case Error::None: return "none";
    case Error::WrongThread: return "wrong thread";
    case Error::ContextExpired: return "context expired";
    case Error::PrototypeNotFound: return "prototype not found";
    case Error::RequiredManagerMissing: return "required manager missing";
    case Error::CreateFailed: return "creation failed";
    case Error::InitFailed: return "initialization failed";
    case Error::OwnerInvalid: return "invalid owner";
    case Error::TargetInvalid: return "invalid target";
    case Error::CallbackFailed: return "callback failed";
    case Error::SlotNotFound: return "slot not found";
    case Error::AmbiguousSlot: return "ambiguous slot";
    case Error::StaleLayout: return "stale layout";
    case Error::TypeMismatch: return "type mismatch";
    case Error::ValueWriteFailed: return "value write failed";
    case Error::SourceInvalid: return "invalid source";
    case Error::InvalidState: return "invalid state";
    case Error::Busy: return "busy";
    case Error::ExecutionFailed: return "execution failed";
    case Error::OperationInvalid: return "invalid parameter operation";
    case Error::UnsupportedBreak: return "unsupported break";
    case Error::FrameQueueFull: return "frame queue full";
    case Error::ExecutionCancelled: return "execution cancelled";
    case Error::DetachedUnsupported: return "detached execution unsupported";
    case Error::ObserverUnavailable: return "observer unavailable";
    case Error::GraphChanged: return "graph changed";
    case Error::InvalidGraphLocality: return "invalid graph locality";
    case Error::InvalidDelay: return "invalid Link delay";
    case Error::UnconfirmedSameFrameCycle:
        return "unconfirmed same-frame cycle";
    case Error::SharedSourceCycle: return "shared-source cycle";
    case Error::PushCycle: return "Pout destination cycle";
    case Error::InterfaceUnsupported: return "interface change unsupported";
    case Error::SourceConflict: return "Pin source conflict";
    case Error::SourceOrderCycle: return "Pin source order cycle";
    case Error::OrderingTargetMismatch: return "ordering target mismatch";
    case Error::OverlayOrderCycle: return "overlay order cycle";
    case Error::LinkNotFound: return "Link not found";
    case Error::PathAmbiguous: return "ambiguous Path";
    case Error::PathCycle: return "cyclic Path";
    case Error::QueryNotFound: return "query not found";
    case Error::QueryAmbiguous: return "ambiguous query";
    case Error::WorldBoundValue: return "world-bound value";
    case Error::RevertConflict: return "Patch revert conflict";
    case Error::TargetCardinality: return "target cardinality mismatch";
    case Error::RedirectConflict: return "Link Redirect conflict";
    }
    return "unknown";
}

const char *DescribePhase(Phase phase) {
    switch (phase) {
    case Phase::None: return "none";
    case Phase::PrototypeResolution: return "prototype resolution";
    case Phase::ManagerValidation: return "manager validation";
    case Phase::Creation: return "creation";
    case Phase::Initialization: return "initialization";
    case Phase::StaticLayout: return "static layout";
    case Phase::OwnerBinding: return "owner binding";
    case Phase::TargetBinding: return "target binding";
    case Phase::Settings: return "settings";
    case Phase::LifecycleCallback: return "lifecycle callback";
    case Phase::ParameterBinding: return "parameter binding";
    case Phase::Execution: return "execution";
    case Phase::Edit: return "edit";
    case Phase::Teardown: return "teardown";
    }
    return "unknown";
}

} // namespace BML::Behavior::Internal
