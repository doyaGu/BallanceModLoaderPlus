#include "Behavior/Runtime.h"

#include <algorithm>
#include <sstream>

namespace BML::Behavior {
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

bool ContainsId(const CK_ID *ids, int count, CK_ID reference) {
    if (!ids || reference == 0)
        return false;
    for (int i = 0; i < count; ++i) {
        if (ids[i] == reference)
            return true;
    }
    return false;
}

bool SameSelector(const Slot &left, const Slot &right) {
    if (left.Kind != right.Kind || left.UsesName() != right.UsesName())
        return false;
    return left.UsesName()
        ? left.Name == right.Name && left.Occurrence == right.Occurrence
        : left.Index == right.Index;
}

const char *SlotKindName(SlotKind kind) {
    switch (kind) {
    case SlotKind::Input: return "input";
    case SlotKind::Output: return "output";
    case SlotKind::InputParameter: return "input parameter";
    case SlotKind::OutputParameter: return "output parameter";
    case SlotKind::Setting: return "setting";
    case SlotKind::Local: return "local parameter";
    case SlotKind::Target: return "target";
    }
    return "slot";
}

std::string GuidText(CKGUID guid) {
    std::ostringstream stream;
    stream << std::hex << "0x" << guid.d1 << ":0x" << guid.d2;
    return stream.str();
}

std::string CandidateList(const Layout &layout, SlotKind kind) {
    std::ostringstream stream;
    bool first = true;
    for (const SlotInfo &candidate : layout.Slots) {
        if (candidate.Kind != kind)
            continue;
        stream << (first ? " Available: " : ", ") << "[" << candidate.Index << "] '"
               << candidate.Name << "'";
        if (candidate.Type.IsValid())
            stream << " " << GuidText(candidate.Type);
        first = false;
    }
    if (first)
        stream << " No slots of this kind are present.";
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

class BehaviorInternals final : public CKBehavior {
public:
    static BehaviorBlockData *BlockData(CKBehavior *behavior) {
        BehaviorBlockData *CKBehavior::*member = &BehaviorInternals::m_BlockData;
        return behavior ? behavior->*member : nullptr;
    }
};

CKDWORD CallbackMaskForMessage(CKDWORD message) {
    switch (message) {
    case CKM_BEHAVIORDELETE: return CKCB_BEHAVIORDELETE;
    case CKM_BEHAVIORATTACH: return CKCB_BEHAVIORATTACH;
    case CKM_BEHAVIORDETACH: return CKCB_BEHAVIORDETACH;
    case CKM_BEHAVIORCREATE: return CKCB_BEHAVIORCREATE;
    case CKM_BEHAVIORRESET: return CKCB_BEHAVIORRESET;
    case CKM_BEHAVIOREDITED: return CKCB_BEHAVIOREDITED;
    case CKM_BEHAVIORSETTINGSEDITED: return CKCB_BEHAVIORSETTINGSEDITED;
    default: return 0;
    }
}

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
            CKBehaviorIO *io = behavior->GetInput(input.Index);
            resolved.Name = io && io->GetName() ? io->GetName() : "";
            resolved.Occurrence = Occurrence(behavior, input.Index, resolved.Name);
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
        resolved.Name = slot.Name;
        resolved.Occurrence = input.Occurrence;
        return true;
    }

    bool Activate(const ResolvedInput &input, ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior || input.Index < 0 ||
            input.Index >= behavior->GetInputCount()) {
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

        for (int index = 0; index < behavior->GetOutputParameterCount(); ++index) {
            CKParameterOut *parameter = behavior->GetOutputParameter(index);
            PoutInfo info;
            if (!parameter || !GetPoutInfo(parameter, info)) {
                result.Executed = false;
                result.Fault = {
                    ExecutionError::UnsupportedPout, CKBR_PARAMETERERROR,
                    parameter
                        ? std::string("Pout '") + SafeName(parameter) +
                              "' has an unsupported type."
                        : "A Pout is unavailable before Behavior execution."};
                return result;
            }
        }

        ++record->LayoutGeneration;
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
        // Ballanced's CKBehavior::ExecuteFunction updates CKBEHAVIOR_ACTIVE
        // from CKBR_ACTIVATENEXTFRAME, while CheckBehaviorActivity updates the
        // same flag from delayed links and active/waiting sub-behaviors.  The
        // post-Execute flag is therefore the native continuation truth for
        // both representations.
        result.Active = behavior->IsActive() != FALSE;
        return result;
    }

    bool ReadOutputs(std::vector<ExecutionOutput> &activeOutputs,
                     std::vector<Pout> &pouts,
                     ExecutionFault &fault) override {
        Record *record = m_Runtime.FindRecord(m_InstanceId);
        CKBehavior *behavior = record ? m_Runtime.ResolveBehavior(*record) : nullptr;
        if (!record || !behavior) {
            fault = {ExecutionError::OutputUnavailable, CKBR_BEHAVIORERROR,
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
            fault = {ExecutionError::OutputUnavailable, CKBR_BEHAVIORERROR,
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
                           const Spec *spec,
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
        for (const Spec::Binding &binding : m_Spec->m_SettingStages[stage]) {
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
        CKBehaviorPrototype *prototype = m_Record.Prototype;
        const CKGUID guid = m_Record.PrototypeGuid;
        identity.Prototype = {
            (static_cast<std::uint64_t>(guid.d1) << 32u) ^
                static_cast<std::uint32_t>(guid.d2),
            reinterpret_cast<std::uintptr_t>(prototype)};
        identity.Owner = Convert(m_Runtime.CaptureObject(behavior->GetOwner()));
        identity.Parent = Convert(m_Runtime.CaptureObject(behavior->GetParent()));
        identity.Target = Convert(
            m_Runtime.CaptureObject(behavior->GetTargetParameter()));

        CKParameterIn *target = behavior->GetTargetParameter();
        identity.Sources.push_back(Convert(m_Runtime.CaptureObject(
            target ? target->GetRealSource() : nullptr)));
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
        recordChange("target", !(current.Target == identity.Target));
        recordChange("parameter source", current.Sources != identity.Sources);
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
        layout.Generation = ++m_Record.LayoutGeneration;
        return true;
    }

    bool ApplyBindings(LifecycleFault &fault) override {
        CKBehavior *behavior = Behavior(fault);
        if (!behavior || !m_Spec)
            return false;
        const CKDWORD flags = behavior->GetFlags();
        if (!m_Spec->m_AddedInputs.empty() &&
            (flags & CKBEHAVIOR_VARIABLEINPUTS) == 0) {
            return Fail(Failure(
                            Error::InterfaceUnsupported,
                            "Building Block does not permit dynamic inputs."),
                        LifecycleError::BindingFailed, fault);
        }
        if (!m_Spec->m_AddedOutputs.empty() &&
            (flags & CKBEHAVIOR_VARIABLEOUTPUTS) == 0) {
            return Fail(Failure(
                            Error::InterfaceUnsupported,
                            "Building Block does not permit dynamic outputs."),
                        LifecycleError::BindingFailed, fault);
        }
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
        Status status = m_Runtime.ApplyBindings(behavior, *m_Spec, m_Record);
        if (!status)
            return Fail(std::move(status), LifecycleError::BindingFailed, fault);
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
        for (const Spec::Binding &binding : m_Spec->m_Inputs) {
            if (!validate(binding.Target))
                return false;
        }
        for (const Spec::OperationBinding &binding : m_Spec->m_Operations) {
            if (!validate(binding.Target))
                return false;
        }
        for (const Spec::Binding &binding : m_Spec->m_Locals) {
            if (!validate(binding.Target))
                return false;
        }
        m_Runtime.PruneOwnedSources(behavior, m_Record);
        m_Runtime.PruneOwnedOperations(behavior, m_Record);
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
    const Spec *m_Spec;
    const CKBehaviorContext *m_Frame;
    Status m_LastStatus;
};

Operation &Operation::Result(CKGUID type) {
    m_ResultType = type;
    return *this;
}

Operation &Operation::Input1(Parameter::Binding value) {
    m_Input1 = std::move(value);
    m_HasInput1 = true;
    return *this;
}

Operation &Operation::Input2(Parameter::Binding value) {
    m_Input2 = std::move(value);
    m_HasInput2 = true;
    return *this;
}

Spec &Spec::TargetOwner() {
    m_TargetMode = TargetMode::Owner;
    m_TargetType = CKGUID();
    m_TargetValue = Parameter::Binding();
    return *this;
}

Spec &Spec::Target(CKGUID type, CKObject *object) {
    m_TargetMode = object ? TargetMode::Explicit : TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Object(type, object);
    return *this;
}

Spec &Spec::NullTarget(CKGUID type) {
    m_TargetMode = TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = Value::Null(type);
    return *this;
}

Spec &Spec::TargetSource(CKGUID type, CKParameter *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Direct(source);
    return *this;
}

Spec &Spec::TargetShared(CKGUID type, CKParameterIn *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Shared(source);
    return *this;
}

Spec &Spec::Setting(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::Setting;
    m_SettingStages.back().push_back({std::move(slot), std::move(value)});
    return *this;
}

Spec &Spec::RefreshLayout() {
    if (!m_SettingStages.back().empty())
        m_SettingStages.emplace_back();
    return *this;
}

Spec &Spec::Input(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::InputParameter;
    m_Operations.erase(
        std::remove_if(m_Operations.begin(), m_Operations.end(),
                       [&](const OperationBinding &binding) {
                           return SameSelector(binding.Target, slot);
                       }),
        m_Operations.end());
    for (Binding &binding : m_Inputs) {
        if (SameSelector(binding.Target, slot)) {
            binding = {std::move(slot), std::move(value)};
            return *this;
        }
    }
    m_Inputs.push_back({std::move(slot), std::move(value)});
    return *this;
}

Spec &Spec::Input(Slot slot, Operation operation) {
    slot.Kind = SlotKind::InputParameter;
    m_Inputs.erase(
        std::remove_if(m_Inputs.begin(), m_Inputs.end(),
                       [&](const Binding &binding) { return SameSelector(binding.Target, slot); }),
        m_Inputs.end());
    for (OperationBinding &binding : m_Operations) {
        if (SameSelector(binding.Target, slot)) {
            binding = {std::move(slot), std::move(operation)};
            return *this;
        }
    }
    m_Operations.push_back({std::move(slot), std::move(operation)});
    return *this;
}

Spec &Spec::Local(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::Local;
    m_Locals.push_back({std::move(slot), std::move(value)});
    return *this;
}

Spec &Spec::AddInput(std::string name) {
    m_AddedInputs.push_back(std::move(name));
    return *this;
}

Spec &Spec::AddOutput(std::string name) {
    m_AddedOutputs.push_back(std::move(name));
    return *this;
}

Spec &Spec::Frames(FrameRetention retention) {
    m_FrameRetention = retention;
    return *this;
}

Spec &Spec::KeepAlive(std::shared_ptr<CallbackResource> resource) {
    if (resource)
        m_KeepAlive.push_back(std::move(resource));
    return *this;
}

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
                 PrototypeCatalog *catalog)
    : m_Context(context), m_IssueObjectRef(std::move(issueObjectRef)),
      m_Catalog(catalog),
      m_Thread(std::this_thread::get_id()),
      m_Access(std::make_shared<Instance::Access>()),
      m_SharedBindings(AcquireSharedBindings(context)) {
    m_Access->Owner = this;
}

Runtime::~Runtime() {
    {
        std::lock_guard<std::mutex> lock(m_Access->Mutex);
        m_Access->Owner = nullptr;
    }
    Close();
}

std::shared_ptr<Runtime::SharedBindings>
Runtime::AcquireSharedBindings(CKContext *context) {
    if (!context)
        return {};
    static std::mutex registryMutex;
    // A live Runtime anchors the state. Weak registry entries cannot retain a
    // dead CKContext or leak its object stamps into a later context at the
    // same address.
    static std::unordered_map<CKContext *, std::weak_ptr<SharedBindings>> registry;
    std::lock_guard<std::mutex> lock(registryMutex);
    std::shared_ptr<SharedBindings> bindings = registry[context].lock();
    if (!bindings)
        registry[context] = bindings = std::make_shared<SharedBindings>();
    return bindings;
}

Status Runtime::ReadyStatus() const {
    if (!m_Context)
        return Failure(Error::ContextExpired, "Virtools context is no longer available.");
    if (m_Thread != std::this_thread::get_id())
        return Failure(Error::WrongThread, "Runtime may only be used on the game thread.");
    return {};
}

Status Runtime::ResolvePrototype(CKGUID guid, std::uint64_t generation) const {
    if (m_Catalog) {
        Layout layout;
        Status status = m_Catalog->DeclaredLayout({guid, generation}, layout);
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
        return {};
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
    return {};
}

Status Runtime::ValidateTarget(CKBeObject *owner, const Spec &spec) const {
    if (!m_Catalog)
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

Status Runtime::CreateBehavior(const Spec &spec, CKBehavior *&behavior,
                               Record &record) const {
    behavior = nullptr;
    Status status = ResolvePrototype(spec.Prototype(),
                                     spec.PrototypeGeneration());
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
    if (BehaviorBlockData *block = BehaviorInternals::BlockData(behavior)) {
        record.Callback = block->m_Callback;
        record.CallbackMask = block->m_CallbackMask;
        record.CallbackArgument = block->m_CallbackArg;
    }
    if (!prototype->GetFunction())
        behavior->UseGraph();
    return {};
}

Status Runtime::CheckDetached(const Spec &spec, bool &unverified) const {
    unverified = true;
    if (!m_Catalog)
        return {};

    DetachedCompatibility compatibility = DetachedCompatibility::Unverified;
    Status status = m_Catalog->Detached(
        {spec.Prototype(), spec.PrototypeGeneration()}, compatibility);
    if (!status)
        return status;
    if (compatibility == DetachedCompatibility::GraphOnly) {
        return Failure(
            Error::DetachedUnsupported,
            "This Building Block requires a parent graph and cannot be run detached.",
            CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
            Phase::PrototypeResolution, spec.Prototype());
    }
    unverified = compatibility == DetachedCompatibility::Unverified;
    return {};
}

CreateResult Runtime::Instantiate(CKBeObject *owner, const Spec &spec,
                                            const CKBehaviorContext *frame) {
    CreateResult result;
    result.Detail = ReadyStatus();
    if (!result.Detail)
        return result;
    result.Detail = CheckDetached(spec, result.UnverifiedDetached);
    if (!result.Detail)
        return result;
    if (owner && owner->GetCKContext() != m_Context) {
        result.Detail = Failure(Error::OwnerInvalid, "Behavior owner belongs to another CKContext.");
        return result;
    }
    result.Detail = ValidateTarget(owner, spec);
    if (!result.Detail)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.KeepAlive = spec.m_KeepAlive;
    record.Protocol = Execution(spec.m_FrameRetention);
    CKBehavior *behavior = nullptr;
    result.Detail = CreateBehavior(spec, behavior, record);
    if (!result.Detail)
        return result;

    record.Behavior = CaptureObject(behavior);
    result.Detail = Configure(behavior, owner, nullptr, spec, frame, record);
    if (!result.Detail)
        return result;

    const std::uint64_t instanceId = record.Id;
    m_Records.emplace(instanceId, std::move(record));
    result.Descriptor = Describe(behavior, m_Records.at(instanceId).LayoutGeneration);
    result.Handle = Instance(m_Access, instanceId);
    return result;
}

CallResult Runtime::Call(CKBeObject *owner, const Spec &spec,
                                 const Slot &input,
                                 const CKBehaviorContext *frame) {
    CallResult result;
    CreateResult created = Instantiate(owner, spec, frame);
    result.Detail = created.Detail;
    if (!created)
        return result;
    result.Descriptor = std::move(created.Descriptor);
    result.Handle = std::move(created.Handle);
    result.UnverifiedDetached = created.UnverifiedDetached;
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

AttachResult Runtime::AddToGraph(CKBehavior *parent, const Spec &spec,
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
    record.Protocol = Execution(spec.m_FrameRetention);
    CKBehavior *behavior = nullptr;
    result.Detail = CreateBehavior(spec, behavior, record);
    if (!result.Detail)
        return result;

    record.Behavior = CaptureObject(behavior);
    result.Detail = Configure(behavior, parent->GetOwner(), parent, spec, frame, record);
    if (!result.Detail)
        return result;

    result.Block = behavior;
    const std::uint64_t instanceId = record.Id;
    m_Records.emplace(instanceId, std::move(record));
    result.Descriptor = Describe(
        behavior, m_Records.at(instanceId).LayoutGeneration);
    return result;
}

Layout Runtime::Describe(CKBehavior *behavior, std::uint64_t generation) const {
    Layout layout;
    if (!ReadyStatus() || !behavior)
        return layout;
    layout.Prototype = PrototypeGuid(behavior);
    layout.Origin = LayoutOrigin::Live;
    layout.Kind = behavior->IsUsingFunction()
        ? BehaviorKind::Function : BehaviorKind::Graph;
    CKBehaviorPrototype *prototype = PrototypeOf(behavior);
    if (prototype) {
        layout.PrototypeName = prototype->GetName() ? prototype->GetName() : "";
        layout.PrototypeFlags = prototype->GetFlags();
    }
    if (CKObjectDeclaration *declaration =
            CKGetObjectDeclarationFromGuid(layout.Prototype)) {
        layout.Category = declaration->GetCategory() ? declaration->GetCategory() : "";
        for (int i = 0; i < declaration->GetManagerNeededCount(); ++i)
            layout.RequiredManagers.push_back(declaration->GetManagerNeeded(i));
    }
    layout.CompatibleClass = behavior->GetCompatibleClassID();
    layout.BehaviorFlags = behavior->GetFlags();
    layout.Generation = generation;

    for (int i = 0; i < behavior->GetInputCount(); ++i) {
        CKBehaviorIO *io = behavior->GetInput(i);
        layout.Slots.push_back({SlotKind::Input, i, i, io && io->GetName() ? io->GetName() : "", CKGUID(), 0});
    }
    for (int i = 0; i < behavior->GetOutputCount(); ++i) {
        CKBehaviorIO *io = behavior->GetOutput(i);
        layout.Slots.push_back({SlotKind::Output, i, i, io && io->GetName() ? io->GetName() : "", CKGUID(), 0});
    }
    for (int i = 0; i < behavior->GetInputParameterCount(); ++i) {
        CKParameterIn *parameter = behavior->GetInputParameter(i);
        CKParameterTypeDesc *description = parameter && m_Context
            ? m_Context->GetParameterManager()->GetParameterTypeDescription(parameter->GetGUID())
            : nullptr;
        layout.Slots.push_back({SlotKind::InputParameter, i, i,
                                parameter && parameter->GetName() ? parameter->GetName() : "",
                                parameter ? parameter->GetGUID() : CKGUID(),
                                parameter && parameter->GetRealSource()
                                    ? parameter->GetRealSource()->GetDataSize()
                                    : (description ? description->DefaultSize : 0)});
    }
    for (int i = 0; i < behavior->GetOutputParameterCount(); ++i) {
        CKParameterOut *parameter = behavior->GetOutputParameter(i);
        layout.Slots.push_back({SlotKind::OutputParameter, i, i,
                                parameter && parameter->GetName() ? parameter->GetName() : "",
                                parameter ? parameter->GetGUID() : CKGUID(),
                                parameter ? parameter->GetDataSize() : 0});
    }

    int settingIndex = 0;
    for (int nativeIndex = 0; nativeIndex < behavior->GetLocalParameterCount(); ++nativeIndex) {
        CKParameterLocal *parameter = behavior->GetLocalParameter(nativeIndex);
        bool setting = behavior->IsLocalParameterSetting(nativeIndex) != FALSE;
        if (prototype && nativeIndex < prototype->GetLocalParameterCount()) {
            CKPARAMETER_DESC **locals = prototype->GetLocalParameterList();
            setting = locals && locals[nativeIndex] &&
                locals[nativeIndex]->Type == 3;
        }
        if (setting) {
            layout.Slots.push_back({SlotKind::Setting, settingIndex++, nativeIndex,
                                    parameter && parameter->GetName() ? parameter->GetName() : "",
                                    parameter ? parameter->GetGUID() : CKGUID(),
                                    parameter ? parameter->GetDataSize() : 0});
        } else {
            // A setting lives in CKBehavior's native local array, but it is not
            // an ordinary local: changing it requires SETTINGSEDITED and may
            // rebuild the rest of the layout. Exposing it twice lets callers
            // bypass that lifecycle through SetLocal.
            layout.Slots.push_back({SlotKind::Local, nativeIndex, nativeIndex,
                                    parameter && parameter->GetName() ? parameter->GetName() : "",
                                    parameter ? parameter->GetGUID() : CKGUID(),
                                    parameter ? parameter->GetDataSize() : 0});
        }
    }
    if (CKParameterIn *target = behavior->GetTargetParameter()) {
        layout.TargetType = target->GetGUID();
        CKParameterTypeDesc *description = m_Context
            ? m_Context->GetParameterManager()->GetParameterTypeDescription(target->GetGUID())
            : nullptr;
        layout.Slots.push_back({SlotKind::Target, 0, 0,
                                target->GetName() ? target->GetName() : "Target",
                                target->GetGUID(), target->GetRealSource()
                                    ? target->GetRealSource()->GetDataSize()
                                    : (description ? description->DefaultSize : 0)});
    }
    CKParameterManager *parameters = m_Context
        ? m_Context->GetParameterManager() : nullptr;
    for (std::size_t index = 0; index < layout.Slots.size(); ++index) {
        SlotInfo &slot = layout.Slots[index];
        slot.Occurrence = 0;
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (layout.Slots[previous].Kind == slot.Kind &&
                layout.Slots[previous].Name == slot.Name)
                ++slot.Occurrence;
        }
        if (slot.Type.IsValid()) {
            const Parameter::Type type =
                Parameter::Describe(parameters, slot.Type);
            slot.TypeName = type.Name;
            slot.ValueForm = type.ValueForm;
        }
        switch (slot.Kind) {
        case SlotKind::Input:
            slot.Dynamic = (layout.BehaviorFlags &
                (CKBEHAVIOR_VARIABLEINPUTS |
                 CKBEHAVIOR_INTERNALLYCREATEDINPUTS)) != 0;
            break;
        case SlotKind::Output:
            slot.Dynamic = (layout.BehaviorFlags &
                (CKBEHAVIOR_VARIABLEOUTPUTS |
                 CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS)) != 0;
            break;
        case SlotKind::InputParameter:
            slot.Dynamic = (layout.BehaviorFlags &
                (CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
                 CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS)) != 0;
            break;
        case SlotKind::OutputParameter:
            slot.Dynamic = (layout.BehaviorFlags &
                (CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS |
                 CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS)) != 0;
            break;
        case SlotKind::Setting:
        case SlotKind::Local:
            slot.Dynamic = (layout.BehaviorFlags &
                CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS) != 0;
            break;
        case SlotKind::Target:
            break;
        }
    }
    if (m_Catalog) {
        Layout declared;
        if (m_Catalog->DeclaredLayout({layout.Prototype, 0}, declared)) {
            layout.ProviderGeneration = declared.ProviderGeneration;
            layout.Provider = declared.Provider;
            layout.ProviderName = declared.ProviderName;
            layout.Author = declared.Author;
            layout.Description = declared.Description;
            layout.Version = declared.Version;
            if (!layout.TargetType.IsValid())
                layout.TargetType = declared.TargetType;
            layout.Managers = declared.Managers;
        }
    }
    return layout;
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

    const Layout layout = Describe(behavior);
    auto failSlot = [&](Error error, std::string message,
                        CKGUID actualType = CKGUID()) {
        Status status = SlotFailure(error, std::move(message), selector,
                                            actualType);
        status.Details.Prototype = layout.Prototype;
        return status;
    };
    std::vector<const SlotInfo *> matches;
    for (const SlotInfo &candidate : layout.Slots) {
        if (candidate.Kind != selector.Kind)
            continue;
        if (selector.UsesName()) {
            if (candidate.Name == selector.Name)
                matches.push_back(&candidate);
        } else if (selector.RequireOnly || candidate.Index == selector.Index) {
            matches.push_back(&candidate);
        }
    }
    if (matches.empty()) {
        std::ostringstream message;
        message << SlotKindName(selector.Kind) << " "
                << (selector.RequireOnly
                    ? "<only>"
                    : selector.UsesName()
                        ? "'" + selector.Name + "'"
                        : "#" + std::to_string(selector.Index))
                << " was not found on Building Block '" << SafeName(behavior) << "'."
                << CandidateList(layout, selector.Kind);
        return failSlot(Error::SlotNotFound, message.str());
    }
    if (selector.RequireOnly && matches.size() != 1) {
        return failSlot(
            Error::AmbiguousSlot,
            "Expected exactly one " + std::string(SlotKindName(selector.Kind)) +
                ", but the configured Layout contains " +
                std::to_string(matches.size()) + "." +
                CandidateList(layout, selector.Kind));
    }
    if (selector.UsesName() && selector.RequireUnique && matches.size() != 1) {
        return failSlot(
            Error::AmbiguousSlot,
            "Slot name '" + selector.Name + "' matched " +
                std::to_string(matches.size()) +
                " candidates; select an explicit occurrence." +
                CandidateList(layout, selector.Kind));
    }
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 || occurrence >= static_cast<int>(matches.size())) {
        return failSlot(
            Error::SlotNotFound,
            "Requested occurrence " + std::to_string(occurrence) + " of " +
                SlotKindName(selector.Kind) + " '" + selector.Name +
                "' does not exist; " + std::to_string(matches.size()) +
                " candidate(s) matched." + CandidateList(layout, selector.Kind));
    }
    slot = *matches[static_cast<std::size_t>(occurrence)];

    if (selector.ExpectedType.IsValid() && slot.Type.IsValid()) {
        CKParameterManager *manager = m_Context ? m_Context->GetParameterManager() : nullptr;
        if (!manager || !manager->IsTypeCompatible(slot.Type, selector.ExpectedType)) {
            return failSlot(
                Error::TypeMismatch,
                "Resolved " + std::string(SlotKindName(selector.Kind)) + " '" +
                    slot.Name + "' has type " + GuidText(slot.Type) +
                    ", incompatible with expected " + GuidText(selector.ExpectedType) + ".",
                slot.Type);
        }
    }
    return {};
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
    if (record->Poisoned)
        return Failure(Error::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
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
    if (!behavior)
        return nullptr;
    switch (slot.Kind) {
    case SlotKind::InputParameter: {
        CKParameterIn *input = behavior->GetInputParameter(slot.NativeIndex);
        return input ? input->GetRealSource() : nullptr;
    }
    case SlotKind::OutputParameter:
        return behavior->GetOutputParameter(slot.NativeIndex);
    case SlotKind::Setting:
    case SlotKind::Local:
        return behavior->GetLocalParameter(slot.NativeIndex);
    case SlotKind::Target: {
        CKParameterIn *target = behavior->GetTargetParameter();
        return target ? target->GetRealSource() : nullptr;
    }
    default:
        return nullptr;
    }
}

CKObject *Runtime::ResolveSlotObject(CKBehavior *behavior, const SlotInfo &slot) const {
    if (!behavior)
        return nullptr;
    switch (slot.Kind) {
    case SlotKind::Input:
        return behavior->GetInput(slot.NativeIndex);
    case SlotKind::Output:
        return behavior->GetOutput(slot.NativeIndex);
    case SlotKind::InputParameter:
        return behavior->GetInputParameter(slot.NativeIndex);
    case SlotKind::OutputParameter:
        return behavior->GetOutputParameter(slot.NativeIndex);
    case SlotKind::Setting:
    case SlotKind::Local:
        return behavior->GetLocalParameter(slot.NativeIndex);
    case SlotKind::Target:
        return behavior->GetTargetParameter();
    }
    return nullptr;
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
    } else if (record->Poisoned) {
        resolved = Failure(Error::InvalidState,
                           "Behavior instance requires a complete successful reconfiguration.");
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
    if (ready && record->Poisoned)
        ready = Failure(Error::InvalidState,
                        "Behavior instance requires a complete successful reconfiguration.");
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

    CKParameter *oldDirect = input->GetDirectSource();
    const ObjectStamp oldRef = CaptureObject(oldDirect);
    auto owned = std::find_if(record.OwnedSources.begin(), record.OwnedSources.end(),
                              [&](ObjectStamp candidate) { return candidate == oldRef; });
    const bool oldOwned = owned != record.OwnedSources.end();

    const bool storedValue = value.Kind() != Parameter::BindingKind::Direct &&
                             value.Kind() != Parameter::BindingKind::Shared;
    if (storedValue && oldOwned && oldDirect &&
        oldDirect->GetGUID() == input->GetGUID() &&
        SourceReferenceCount(oldDirect) == 1) {
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
        record.OwnedSources.push_back(CaptureObject(literal));
    }

    if (oldOwned && !IsSourceReferenced(oldDirect)) {
        QueueSourceDestroy(oldRef);
        record.OwnedSources.erase(owned);
    }
    return {};
}

Status Runtime::BindOperation(CKBehavior *behavior, Record &record,
                                              const SlotInfo &slot,
                                              const Operation &spec) {
    if (!behavior || slot.Kind != SlotKind::InputParameter) {
        return Failure(Error::OperationInvalid,
                       "Parameter operations can only target behavior input parameters.",
                       CKERR_INVALIDPARAMETER, CKBR_OK, Phase::ParameterBinding);
    }
    CKParameterIn *target = behavior->GetInputParameter(slot.NativeIndex);
    if (!target || !spec.m_Operation.IsValid()) {
        Status status = Failure(
            Error::OperationInvalid,
            !target ? "Operation target input parameter no longer exists."
                    : "Parameter operation GUID is invalid.",
            CKERR_INVALIDPARAMETER, CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid);
        status.Details.OperationGuid = spec.m_Operation;
        return status;
    }

    auto resolveInputType = [&](const Parameter::Binding &value, bool provided,
                                CKGUID &type, int inputIndex) -> Status {
        if (!provided) {
            type = CKPGUID_NONE;
            return {};
        }
        CKObject *sourceObject = nullptr;
        if (value.Kind() == Parameter::BindingKind::Direct) {
            sourceObject = value.SourceId()
                ? m_Context->GetObject(value.SourceId()) : nullptr;
            if (sourceObject != value.Source() || !sourceObject ||
                sourceObject->IsToBeDeleted() ||
                !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
                return Failure(Error::SourceInvalid,
                               "Parameter operation input " + std::to_string(inputIndex) +
                                   " direct source is invalid.",
                               CKERR_INVALIDOBJECT, CKBR_OK,
                               Phase::ParameterBinding);
            }
            type = value.Source()->GetGUID();
            return {};
        }
        if (value.Kind() == Parameter::BindingKind::Shared) {
            sourceObject = value.SharedSourceId()
                ? m_Context->GetObject(value.SharedSourceId()) : nullptr;
            if (sourceObject != value.SharedSource() || !sourceObject ||
                sourceObject->IsToBeDeleted() ||
                !CKIsChildClassOf(sourceObject, CKCID_PARAMETERIN)) {
                return Failure(Error::SourceInvalid,
                               "Parameter operation input " + std::to_string(inputIndex) +
                                   " shared source is invalid.",
                               CKERR_INVALIDOBJECT, CKBR_OK,
                               Phase::ParameterBinding);
            }
            type = value.SharedSource()->GetGUID();
            return {};
        }
        type = value.Type();
        if (!type.IsValid() || type == CKPGUID_NONE) {
            return Failure(Error::OperationInvalid,
                           "Parameter operation input " + std::to_string(inputIndex) +
                               " needs an explicit registered type.",
                           CKERR_INVALIDPARAMETER, CKBR_OK,
                           Phase::ParameterBinding);
        }
        return {};
    };

    CKGUID input1Type;
    CKGUID input2Type;
    Status status = resolveInputType(spec.m_Input1, spec.m_HasInput1,
                                             input1Type, 1);
    if (!status) {
        status.Details.OperationGuid = spec.m_Operation;
        return status;
    }
    status = resolveInputType(spec.m_Input2, spec.m_HasInput2, input2Type, 2);
    if (!status) {
        status.Details.OperationGuid = spec.m_Operation;
        return status;
    }

    const CKGUID resultType = spec.m_ResultType.IsValid() &&
                              spec.m_ResultType != CKPGUID_NONE
        ? spec.m_ResultType : target->GetGUID();
    CKBehavior *operationOwner = behavior;
    if (record.GraphResident) {
        CKObject *parentObject = ResolveObject(record.Parent);
        if (!parentObject || !CKIsChildClassOf(parentObject, CKCID_BEHAVIOR)) {
            Status failed = Failure(
                Error::OperationInvalid,
                "Graph-resident operation requires a live parent graph.",
                CKERR_INVALIDOBJECT, CKBR_OK, Phase::ParameterBinding,
                record.PrototypeGuid);
            failed.Details.OperationGuid = spec.m_Operation;
            return failed;
        }
        operationOwner = static_cast<CKBehavior *>(parentObject);
    }
    std::ostringstream name;
    name << "__BML_Operation_" << behavior->GetID() << "_"
         << record.OwnedOperations.size();
    CKParameterOperation *operation = m_Context->CreateCKParameterOperation(
        const_cast<CKSTRING>(name.str().c_str()), spec.m_Operation, resultType,
        input1Type, input2Type);
    if (!operation) {
        Status failed = Failure(
            Error::CreateFailed, "Failed to create CKParameterOperation.",
            CKERR_INVALIDPARAMETER, CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid);
        failed.Details.OperationGuid = spec.m_Operation;
        return failed;
    }

    OwnedOperation ownedOperation;
    ownedOperation.Owner = CaptureObject(operationOwner);
    ownedOperation.Operation = CaptureObject(operation);
    CKERROR addError = operationOwner->AddParameterOperation(operation);
    if (addError == CK_OK) {
        ownedOperation.AddedToOwner = true;
    } else if (operationOwner == behavior && behavior->IsUsingFunction()) {
        // Virtools permits owner-only operations for behaviors that are not
        // graph containers. The explicit owner still gives PreDelete a valid
        // relationship without pretending AddParameterOperation succeeded.
        operation->SetOwner(behavior);
    } else {
        m_Context->DestroyObject(operation);
        Status failed = Failure(
            Error::OperationInvalid,
            "Failed to add CKParameterOperation to its parent graph.", addError,
            CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid);
        failed.Details.OperationGuid = spec.m_Operation;
        return failed;
    }

    bool targetConnected = false;
    CKParameter *previousSource = target->GetDirectSource();
    auto rollback = [&](Status failed) -> Status {
        if (targetConnected && target->GetDirectSource() == operation->GetOutParameter())
            (void) target->SetDirectSource(previousSource);
        if (ownedOperation.AddedToOwner)
            (void) operationOwner->RemoveParameterOperation(operation);
        else
            operation->SetOwner(nullptr);
        if (!operation->IsToBeDeleted())
            m_Context->DestroyObject(operation);
        for (ObjectStamp source : ownedOperation.Sources) {
            if (CKObject *object = ResolveObject(source))
                m_Context->DestroyObject(object);
        }
        failed.Details.OperationGuid = spec.m_Operation;
        return failed;
    };

    if (!operation->GetOutParameter() || !operation->GetOperationFunction()) {
        return rollback(Failure(
            Error::OperationInvalid,
            !operation->GetOutParameter()
                ? "Parameter operation did not create an output parameter."
                : "No operation function is registered for the requested type tuple.",
            CKERR_INVALIDPARAMETER, CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid));
    }

    auto bindOperationInput = [&](CKParameterIn *input,
                                  const Parameter::Binding &value,
                                  bool provided, int inputIndex) -> Status {
        if (!input || input->GetGUID() == CKPGUID_NONE) {
            if (!provided)
                return {};
            return Failure(Error::OperationInvalid,
                           "Parameter operation input " + std::to_string(inputIndex) +
                               " is not accepted by the selected operation tuple.",
                           CKERR_INVALIDPARAMETER, CKBR_OK,
                           Phase::ParameterBinding);
        }
        if (!provided) {
            return Failure(Error::OperationInvalid,
                           "Parameter operation input " + std::to_string(inputIndex) +
                               " is required.",
                           CKERR_INVALIDPARAMETER, CKBR_OK,
                           Phase::ParameterBinding);
        }

        CKERROR error = CK_OK;
        if (value.Kind() == Parameter::BindingKind::Direct) {
            error = input->SetDirectSource(value.Source());
        } else if (value.Kind() == Parameter::BindingKind::Shared) {
            error = input->ShareSourceWith(value.SharedSource());
        } else {
            std::ostringstream sourceName;
            sourceName << name.str() << "_Input" << inputIndex;
            CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
                const_cast<CKSTRING>(sourceName.str().c_str()), input->GetGUID(), TRUE);
            if (!literal) {
                return Failure(Error::CreateFailed,
                               "Failed to create an independent operation input source.",
                               CKERR_INVALIDPARAMETER, CKBR_OK,
                               Phase::ParameterBinding);
            }
            Status applied = Parameter::Write(m_Context, literal, value);
            if (!applied) {
                m_Context->DestroyObject(literal);
                return applied;
            }
            ownedOperation.Sources.push_back(CaptureObject(literal));
            error = input->SetDirectSource(literal);
        }
        return error == CK_OK
            ? Status{}
            : Failure(Error::TypeMismatch,
                      "Parameter operation input " + std::to_string(inputIndex) +
                          " rejected its source.",
                      error, CKBR_OK, Phase::ParameterBinding);
    };

    status = bindOperationInput(operation->GetInParameter1(), spec.m_Input1,
                                spec.m_HasInput1, 1);
    if (!status)
        return rollback(std::move(status));
    status = bindOperationInput(operation->GetInParameter2(), spec.m_Input2,
                                spec.m_HasInput2, 2);
    if (!status)
        return rollback(std::move(status));

    const CKERROR operationError = operation->DoOperation();
    if (operationError != CK_OK) {
        return rollback(Failure(
            Error::OperationInvalid,
            "Parameter operation failed during initial evaluation.", operationError,
            CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid));
    }
    const CKERROR connectError = target->SetDirectSource(operation->GetOutParameter());
    if (connectError != CK_OK) {
        return rollback(Failure(
            Error::TypeMismatch,
            "Operation output is incompatible with its target input parameter.",
            connectError, CKBR_OK, Phase::ParameterBinding,
            record.PrototypeGuid));
    }
    targetConnected = true;

    const ObjectStamp previousRef = CaptureObject(previousSource);
    auto previousLiteral = std::find_if(
        record.OwnedSources.begin(), record.OwnedSources.end(),
        [&](ObjectStamp candidate) { return candidate == previousRef; });
    if (previousLiteral != record.OwnedSources.end() &&
        !IsSourceReferenced(previousSource)) {
        QueueSourceDestroy(*previousLiteral);
        record.OwnedSources.erase(previousLiteral);
    }
    auto previousOperation = std::find_if(
        record.OwnedOperations.begin(), record.OwnedOperations.end(),
        [&](const OwnedOperation &candidate) {
            CKObject *object = ResolveObject(candidate.Operation);
            auto *candidateOperation = object &&
                CKIsChildClassOf(object, CKCID_PARAMETEROPERATION)
                ? static_cast<CKParameterOperation *>(object) : nullptr;
            return candidateOperation && candidateOperation->GetOutParameter() == previousSource;
        });
    if (previousOperation != record.OwnedOperations.end() &&
        !IsSourceReferenced(previousSource)) {
        DetachOperation(*previousOperation);
        QueueOperationDestroy(std::move(*previousOperation));
        record.OwnedOperations.erase(previousOperation);
    }
    record.OwnedOperations.push_back(std::move(ownedOperation));
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
                                           const Spec &spec, Record &record) {
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

Status Runtime::ApplyBindings(CKBehavior *behavior, const Spec &spec,
                                              Record &record) {
    for (const Spec::Binding &binding : spec.m_Locals) {
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
    for (const Spec::Binding &binding : spec.m_Inputs) {
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
    for (const Spec::OperationBinding &binding : spec.m_Operations) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            record.PrototypeGuid, &binding.Target);
        status = BindOperation(behavior, record, slot, binding.Definition);
        if (!status) {
            status.Details.Selector = binding.Target;
            status.Details.Prototype = record.PrototypeGuid;
            return status;
        }
    }
    return {};
}

int Runtime::SourceReferenceCount(CKParameter *source,
                                  CKBehavior *ignoredBehavior,
                                  const std::vector<ObjectStamp> *ignoredInputs) const {
    if (!m_Context || !source || source->IsToBeDeleted())
        return 0;

    int references = 0;
    const XObjectPointerArray &inputs =
        m_Context->GetObjectListByType(CKCID_PARAMETERIN, TRUE);
    for (XObjectPointerArray::ConstIterator it = inputs.Begin();
         it != inputs.End(); ++it) {
        CKObject *object = *it;
        auto *input = object && !object->IsToBeDeleted()
            ? static_cast<CKParameterIn *>(object) : nullptr;
        const bool ignoredInput = input && ignoredInputs &&
            std::any_of(ignoredInputs->begin(), ignoredInputs->end(),
                        [&](ObjectStamp candidate) {
                            return candidate.Address == input &&
                                   candidate.Id == input->GetID();
                        });
        if (input && !ignoredInput &&
            (!ignoredBehavior || input->GetOwner() != ignoredBehavior) &&
            input->GetRealSource() == source) {
            ++references;
        }
    }
    return references;
}

bool Runtime::IsSourceReferenced(CKParameter *source) const {
    return SourceReferenceCount(source) != 0;
}

void Runtime::PruneOwnedSources(CKBehavior *behavior, Record &record) {
    if (!behavior)
        return;
    for (auto it = record.OwnedSources.begin(); it != record.OwnedSources.end();) {
        CKObject *object = ResolveObject(*it);
        auto *source = object && CKIsChildClassOf(object, CKCID_PARAMETER)
            ? static_cast<CKParameter *>(object) : nullptr;
        const bool used = source && IsSourceReferenced(source);
        if (used) {
            ++it;
        } else {
            QueueSourceDestroy(*it);
            it = record.OwnedSources.erase(it);
        }
    }
}

void Runtime::PruneOwnedOperations(CKBehavior *behavior, Record &record) {
    if (!behavior)
        return;
    for (auto it = record.OwnedOperations.begin();
         it != record.OwnedOperations.end();) {
        CKObject *object = ResolveObject(it->Operation);
        auto *operation = object && CKIsChildClassOf(object, CKCID_PARAMETEROPERATION)
            ? static_cast<CKParameterOperation *>(object) : nullptr;
        const bool used = operation &&
            IsSourceReferenced(operation->GetOutParameter());
        if (used) {
            ++it;
        } else {
            DetachOperation(*it);
            QueueOperationDestroy(std::move(*it));
            it = record.OwnedOperations.erase(it);
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
        PruneOwnedSources(behavior, record);
        PruneOwnedOperations(behavior, record);
    }
}

void Runtime::DetachOperation(OwnedOperation &owned) {
    if (owned.Operation.Id == 0)
        return;
    CKObject *operationObject = ResolveObject(owned.Operation);
    auto *operation = operationObject &&
        CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION)
        ? static_cast<CKParameterOperation *>(operationObject) : nullptr;
    if (!operation)
        return;
    CKObject *ownerObject = ResolveObject(owned.Owner);
    auto *owner = ownerObject && CKIsChildClassOf(ownerObject, CKCID_BEHAVIOR)
        ? static_cast<CKBehavior *>(ownerObject) : nullptr;
    if (owner && owned.AddedToOwner)
        (void) owner->RemoveParameterOperation(operation);
    operationObject = ResolveObject(owned.Operation);
    operation = operationObject &&
        CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION)
        ? static_cast<CKParameterOperation *>(operationObject) : nullptr;
    if (operation)
        operation->SetOwner(nullptr);
    owned.AddedToOwner = false;
}

Status Runtime::Configure(CKBehavior *behavior,
                                          CKBeObject *owner, CKBehavior *parent,
                                          const Spec &spec,
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
    plan.SettingStages.reserve(spec.m_SettingStages.size());
    for (const std::vector<Spec::Binding> &stage : spec.m_SettingStages)
        plan.SettingStages.push_back(!stage.empty());

    NativeLifecycleAdapter adapter(*this, record, owner, parent, &spec, frame);
    if (record.NativeLifecycle.Configure(plan, adapter))
        return {};
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
    CKERROR result = CK_OK;
    const CKDWORD mask = CallbackMaskForMessage(message);
    const CKBEHAVIORCALLBACKFCT callback = record.Callback;
    const CKDWORD callbackMask = record.CallbackMask;
    void *const callbackArg = record.CallbackArgument;
    if (callback && mask != 0 && (callbackMask & mask) != 0) {
        BehaviorContextScope scope(m_Context, behavior, frame);
        m_Context->m_BehaviorContext.CallbackMessage = message;
        m_Context->m_BehaviorContext.CallbackArg = callbackArg;
        result = callback(m_Context->m_BehaviorContext);
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
    if (record->Poisoned)
        return Failure(Error::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
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
    PruneOwnedSources(behavior, *record);
    PruneOwnedOperations(behavior, *record);
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
    if (record->Poisoned)
        return Failure(Error::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Locals can only be edited while the instance is idle.");
    Status status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != SlotKind::Local)
        return Failure(Error::InvalidState, "Resolved slot is not a local parameter.");
    return Parameter::Write(
        m_Context, ResolveParameter(behavior, slot.Slot), value);
}

Status Runtime::Reconfigure(Instance &instance, const Spec &spec,
                                            const CKBehaviorContext *frame) {
    Status ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(Error::InvalidState, "Behavior instance has expired.");
    if (record->Protocol.State() != ExecutionState::Idle)
        return Failure(Error::InvalidState,
                       "Reconfiguration requires an idle behavior instance.");
    if (spec.Prototype() != record->PrototypeGuid)
        return Failure(Error::InvalidState,
                       "Reconfiguration spec names a different Building Block Prototype.");
    if (!spec.m_AddedInputs.empty() || !spec.m_AddedOutputs.empty())
        return Failure(Error::InvalidState,
                       "Reconfiguration cannot append duplicate behavior IOs.");

    CKBehaviorPrototype *prototype = record->Prototype;
    if (!prototype)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype is unavailable.");
    CKBeObject *owner = behavior->GetOwner();
    const std::uint64_t instanceId = record->Id;
    record->Poisoned = true;
    ++record->LayoutGeneration;
    Status status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return status;
    status = BindTarget(behavior, owner, spec, *record);
    if (!status)
        return status;
    for (const auto &stage : spec.m_SettingStages) {
        for (const Spec::Binding &binding : stage) {
            SlotInfo setting;
            status = Resolve(behavior, binding.Target, setting);
            if (!status)
                return Annotate(std::move(status), Phase::Settings,
                                record->PrototypeGuid, &binding.Target);
            status = Parameter::Write(
                m_Context, ResolveParameter(behavior, setting), binding.Source);
            if (!status)
                return Annotate(std::move(status), Phase::Settings,
                                record->PrototypeGuid, &binding.Target);
        }
        if (stage.empty())
            continue;
        ++record->LayoutGeneration;
        status = CallCallback(*record, CKM_BEHAVIORSETTINGSEDITED, frame);
        if (!status)
            return status;
        status = Reacquire(instanceId, behavior, record);
        if (!status)
            return status;
        owner = behavior->GetOwner();
        prototype = record->Prototype;
        status = EnsurePrototypeLayout(behavior, prototype, true);
        if (!status)
            return status;
        status = BindTarget(behavior, owner, spec, *record);
        if (!status)
            return status;
        status = EnsurePrototypeDefaults(behavior, prototype, *record);
        if (!status)
            return status;
    }
    status = ApplyBindings(behavior, spec, *record);
    if (!status)
        return status;
    ++record->LayoutGeneration;
    status = CallCallback(*record, CKM_BEHAVIOREDITED, frame);
    if (!status)
        return status;
    status = Reacquire(instanceId, behavior, record);
    if (!status)
        return status;
    owner = behavior->GetOwner();
    prototype = record->Prototype;
    status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return status;
    status = BindTarget(behavior, owner, spec, *record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, *record);
    if (!status)
        return status;
    status = ApplyBindings(behavior, spec, *record);
    PruneOwnedSources(behavior, *record);
    PruneOwnedOperations(behavior, *record);
    if (status)
        record->Poisoned = false;
    return status;
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
    if (record->Poisoned)
        return {Failure(Error::InvalidState,
                        "Behavior instance requires a complete successful reconfiguration."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
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
    if (record->Poisoned)
        return {Failure(Error::InvalidState,
                        "Behavior instance requires a complete successful reconfiguration."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
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
    if (record->Protocol.State() != ExecutionState::Pending)
        return Failure(Error::InvalidState,
                       "Only a pending behavior instance can be continued.");
    record->Protocol.Continue();
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
    return record ? record->Protocol.State() : ExecutionState::Closed;
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
    const ExecutionFault &fault = record->Protocol.Failure();
    if (!fault)
        return {};
    Error error = Error::ExecutionFailed;
    switch (fault.Code) {
    case ExecutionError::UnsupportedBreak:
        error = Error::UnsupportedBreak;
        break;
    case ExecutionError::FrameQueueFull:
        error = Error::FrameQueueFull;
        break;
    case ExecutionError::Cancelled:
        error = Error::ExecutionCancelled;
        break;
    case ExecutionError::LayoutStale:
        error = Error::StaleLayout;
        break;
    case ExecutionError::SelectorNotFound:
        error = Error::SlotNotFound;
        break;
    case ExecutionError::SelectorAmbiguous:
        error = Error::AmbiguousSlot;
        break;
    case ExecutionError::InvalidState:
        error = Error::InvalidState;
        break;
    default:
        break;
    }
    return Failure(error, fault.Message, CK_OK, fault.NativeCode,
                   Phase::Execution);
}

void Runtime::ProcessTasks(const CKBehaviorContext *frame) {
    if (!ReadyStatus())
        return;
    if (m_ProcessingTasks)
        return;
    FlagScope processing(m_ProcessingTasks);
    std::vector<std::uint64_t> tasks;
    tasks.reserve(m_Records.size());
    for (const auto &[instanceId, record] : m_Records) {
        if (record.Protocol.NeedsFrame())
            tasks.push_back(instanceId);
    }
    for (std::uint64_t instanceId : tasks) {
        auto it = m_Records.find(instanceId);
        if (it == m_Records.end() || !it->second.Protocol.NeedsFrame())
            continue;
        (void) Execute(instanceId, nullptr, false, frame);
    }
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
    DrainCloseQueue();
    ProcessTasks(&m_Context->m_BehaviorContext);
    SweepRecords();
    DrainCloseQueue();
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
    if (record->GraphResident)
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
        for (const ExecutionOutput &output : executed.Frame->ActiveOutputs)
            result.ActiveOutputs.push_back(output.Index);
    }

    if (executed.Fault) {
        Error error = Error::ExecutionFailed;
        switch (executed.Fault.Code) {
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
        case ExecutionError::OutputUnavailable:
            error = Error::PoutUnavailable;
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
        result.Detail = Failure(error, executed.Fault.Message, CK_OK,
                                 executed.Fault.NativeCode,
                                 Phase::Execution, prototypeGuid);
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

    if (record->Expired || ResolveBehavior(*record) != behavior) {
        for (ObjectStamp source : record->OwnedSources)
            QueueSourceDestroy(source);
        for (OwnedOperation &operation : record->OwnedOperations)
            QueueOperationDestroy(std::move(operation));
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
    pending.Operations = std::move(record.OwnedOperations);
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

void Runtime::DrainCloseQueue(bool force) {
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        Record &record = it->second;
        if (!record.NativeLifecycle.CloseRequested() ||
            record.Protocol.State() == ExecutionState::Running) {
            ++it;
            continue;
        }
        NativeLifecycleAdapter adapter(*this, record, nullptr, nullptr,
                                       nullptr, nullptr);
        (void) record.NativeLifecycle.Drain(adapter);
        record.Protocol.MarkClosed();
        it = m_Records.erase(it);
    }
    if (force)
        DestroyReady(DestroyMode::Reset);
}

void Runtime::QueueSourceDestroy(ObjectStamp source, int frames) {
    if (source.Id == 0)
        return;
    PendingDestroy pending;
    pending.Sources.push_back(source);
    pending.Frames = frames;
    m_PendingDestroy.push_back(std::move(pending));
}

void Runtime::QueueOperationDestroy(OwnedOperation operation, int frames) {
    if (operation.Operation.Id == 0) {
        for (ObjectStamp source : operation.Sources)
            QueueSourceDestroy(source, frames);
        return;
    }
    PendingDestroy pending;
    pending.Operations.push_back(std::move(operation));
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
        if (behavior && it->DestroyBehavior) {
            auto rememberInput = [&](CKParameterIn *input) {
                const ObjectStamp stamp = CaptureObject(input);
                if (stamp.Id != 0 &&
                    std::find(it->IgnoredInputs.begin(), it->IgnoredInputs.end(),
                              stamp) == it->IgnoredInputs.end()) {
                    it->IgnoredInputs.push_back(stamp);
                }
            };
            rememberInput(behavior->GetTargetParameter());
            for (int i = 0; i < behavior->GetInputParameterCount(); ++i)
                rememberInput(behavior->GetInputParameter(i));
        }
        behavior = resolveBehavior();

        PendingDestroy retained;
        retained.Frames = 1;
        retained.IgnoredInputs = it->IgnoredInputs;
        if (!force) {
            CKBehavior *ignoredBehavior = it->DestroyBehavior ? behavior : nullptr;
            for (auto source = it->Sources.begin(); source != it->Sources.end();) {
                CKObject *object = ResolveObject(*source);
                auto *parameter = object && CKIsChildClassOf(object, CKCID_PARAMETER)
                    ? static_cast<CKParameter *>(object) : nullptr;
                if (parameter &&
                    SourceReferenceCount(parameter, ignoredBehavior,
                                         &it->IgnoredInputs) != 0) {
                    retained.Sources.push_back(*source);
                    source = it->Sources.erase(source);
                } else {
                    ++source;
                }
            }
            for (auto operation = it->Operations.begin();
                 operation != it->Operations.end();) {
                CKObject *object = ResolveObject(operation->Operation);
                auto *parameterOperation = object &&
                    CKIsChildClassOf(object, CKCID_PARAMETEROPERATION)
                    ? static_cast<CKParameterOperation *>(object) : nullptr;
                const bool referenced = parameterOperation &&
                    SourceReferenceCount(parameterOperation->GetOutParameter(),
                                         ignoredBehavior,
                                         &it->IgnoredInputs) != 0;
                if (referenced) {
                    if (it->DestroyBehavior)
                        DetachOperation(*operation);
                    retained.Operations.push_back(std::move(*operation));
                    operation = it->Operations.erase(operation);
                } else {
                    ++operation;
                }
            }
        }

        std::vector<ObjectStamp> sources = std::move(it->Sources);
        it->Sources.clear();
        for (OwnedOperation &pendingOperation : it->Operations) {
            sources.insert(sources.end(), pendingOperation.Sources.begin(),
                           pendingOperation.Sources.end());
            const ObjectStamp operationRef = pendingOperation.Operation;
            CKObject *object = ResolveObject(operationRef);
            auto *operation = object &&
                CKIsChildClassOf(object, CKCID_PARAMETEROPERATION)
                ? static_cast<CKParameterOperation *>(object) : nullptr;
            if (!operation)
                continue;
            CKObject *ownerObject = ResolveObject(pendingOperation.Owner);
            auto *operationOwner = ownerObject &&
                CKIsChildClassOf(ownerObject, CKCID_BEHAVIOR)
                ? static_cast<CKBehavior *>(ownerObject) : nullptr;
            if (operationOwner)
                (void) operationOwner->RemoveParameterOperation(operation);
            object = ResolveObject(operationRef);
            operation = object && CKIsChildClassOf(object, CKCID_PARAMETEROPERATION)
                ? static_cast<CKParameterOperation *>(object) : nullptr;
            if (operation) {
                operation->SetOwner(nullptr);
                m_Context->DestroyObject(operation);
            }
        }
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
            if (CKObject *object = ResolveObject(source))
                m_Context->DestroyObject(object);
        }
        for (std::shared_ptr<CallbackResource> &resource : it->KeepAlive) {
            if (resource && !resource->RetireAtSafePoint())
                retained.KeepAlive.push_back(std::move(resource));
        }
        it = m_PendingDestroy.erase(it);
        if (!retained.Sources.empty() || !retained.Operations.empty() ||
            !retained.KeepAlive.empty()) {
            if (closing && m_SharedBindings)
                m_SharedBindings->Pending.push_back(std::move(retained));
            else
                m_PendingDestroy.push_back(std::move(retained));
        }
    }
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
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (ContainsId(ids, count, pending.Behavior.Id)) {
            pending.Behavior = {};
            pending.GraphResident = false;
        }
        pending.Sources.erase(
            std::remove_if(pending.Sources.begin(), pending.Sources.end(),
                           [&](ObjectStamp source) { return ContainsId(ids, count, source.Id); }),
            pending.Sources.end());
        for (OwnedOperation &operation : pending.Operations) {
            if (ContainsId(ids, count, operation.Owner.Id))
                operation.Owner = {};
            if (ContainsId(ids, count, operation.Operation.Id))
                operation.Operation = {};
            operation.Sources.erase(
                std::remove_if(operation.Sources.begin(), operation.Sources.end(),
                               [&](ObjectStamp source) {
                                   return ContainsId(ids, count, source.Id);
                               }),
                operation.Sources.end());
        }
    }
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        const bool deleting = ContainsId(ids, count, it->second.Behavior.Id) ||
                              ContainsId(ids, count, it->second.Parent.Id);
        if (!deleting) {
            it->second.OwnedSources.erase(
                std::remove_if(it->second.OwnedSources.begin(), it->second.OwnedSources.end(),
                               [&](ObjectStamp source) { return ContainsId(ids, count, source.Id); }),
                it->second.OwnedSources.end());
            for (auto operation = it->second.OwnedOperations.begin();
                 operation != it->second.OwnedOperations.end();) {
                if (ContainsId(ids, count, operation->Operation.Id)) {
                    for (ObjectStamp source : operation->Sources) {
                        if (!ContainsId(ids, count, source.Id))
                            QueueSourceDestroy(source);
                    }
                    operation = it->second.OwnedOperations.erase(operation);
                    continue;
                }
                operation->Sources.erase(
                    std::remove_if(operation->Sources.begin(), operation->Sources.end(),
                                   [&](ObjectStamp source) {
                                       return ContainsId(ids, count, source.Id);
                                   }),
                    operation->Sources.end());
                ++operation;
            }
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

} // namespace BML::Behavior
