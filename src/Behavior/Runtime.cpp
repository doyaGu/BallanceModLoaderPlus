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

Slot Slot::At(SlotKind kind, int index, CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Index = index;
    selector.ExpectedType = expectedType;
    return selector;
}

Slot Slot::Named(SlotKind kind, std::string name, CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.ExpectedType = expectedType;
    selector.RequireUnique = true;
    return selector;
}

Slot Slot::OccurrenceOf(SlotKind kind, std::string name, int occurrence,
                                        CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.Occurrence = occurrence;
    selector.ExpectedType = expectedType;
    return selector;
}

Value Value::Raw(CKGUID type, const void *data, std::size_t size) {
    Value value;
    value.m_Kind = ValueKind::Raw;
    value.m_Type = type;
    if (data && size != 0) {
        value.m_Bytes.resize(size);
        std::memcpy(value.m_Bytes.data(), data, size);
    }
    return value;
}

Value Value::UntypedRaw(const void *data, std::size_t size) {
    return Raw(CKGUID(), data, size);
}

Value Value::Text(CKGUID type, std::string text) {
    Value value;
    value.m_Kind = ValueKind::Text;
    value.m_Type = type;
    value.m_Text = std::move(text);
    return value;
}

Value Value::String(std::string text) {
    return Text(CKPGUID_STRING, std::move(text));
}

Value Value::Object(CKGUID type, CKObject *object) {
    Value value;
    value.m_Kind = ValueKind::Object;
    value.m_Type = type;
    value.m_Object = object;
    value.m_ObjectId = object ? object->GetID() : 0;
    return value;
}

Value Value::Snapshot(CKParameter *source) {
    Value value;
    value.m_Kind = ValueKind::Snapshot;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

Value Value::DirectSource(CKParameter *source) {
    Value value;
    value.m_Kind = ValueKind::DirectSource;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

Value Value::SharedSource(CKParameterIn *source) {
    Value value;
    value.m_Kind = ValueKind::SharedSource;
    value.m_SharedSource = source;
    value.m_SharedSourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

Operation &Operation::Result(CKGUID type) {
    m_ResultType = type;
    return *this;
}

Operation &Operation::Input1(Value value) {
    m_Input1 = std::move(value);
    m_HasInput1 = true;
    return *this;
}

Operation &Operation::Input2(Value value) {
    m_Input2 = std::move(value);
    m_HasInput2 = true;
    return *this;
}

Spec &Spec::TargetOwner() {
    m_TargetMode = TargetMode::Owner;
    m_TargetType = CKGUID();
    m_TargetValue = Value();
    return *this;
}

Spec &Spec::Target(CKGUID type, CKObject *object) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Value::Object(type, object);
    return *this;
}

Spec &Spec::NullTarget(CKGUID type) {
    m_TargetMode = TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = Value::Object(type, nullptr);
    return *this;
}

Spec &Spec::TargetSource(CKGUID type, CKParameter *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Value::DirectSource(source);
    return *this;
}

Spec &Spec::TargetShared(CKGUID type, CKParameterIn *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Value::SharedSource(source);
    return *this;
}

Spec &Spec::Setting(Slot slot, Value value) {
    slot.Kind = SlotKind::Setting;
    m_SettingStages.back().push_back({std::move(slot), std::move(value)});
    return *this;
}

Spec &Spec::RefreshLayout() {
    if (!m_SettingStages.back().empty())
        m_SettingStages.emplace_back();
    return *this;
}

Spec &Spec::Input(Slot slot, Value value) {
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

Spec &Spec::Local(Slot slot, Value value) {
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

Runtime::Runtime(CKContext *context)
    : m_Context(context), m_Thread(std::this_thread::get_id()),
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

Status Runtime::ResolvePrototype(CKGUID guid) const {
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

Status Runtime::CreateBehavior(const Spec &spec,
                                               CKBehavior *&behavior) const {
    behavior = nullptr;
    Status status = ResolvePrototype(spec.Prototype());
    if (!status)
        return status;

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
    return {};
}

CreateResult Runtime::Instantiate(CKBeObject *owner, const Spec &spec,
                                            const CKBehaviorContext *frame) {
    CreateResult result;
    result.Outcome = ReadyStatus();
    if (!result.Outcome)
        return result;
    if (owner && owner->GetCKContext() != m_Context) {
        result.Outcome = Failure(Error::OwnerInvalid, "Behavior owner belongs to another CKContext.");
        return result;
    }

    CKBehavior *behavior = nullptr;
    result.Outcome = CreateBehavior(spec, behavior);
    if (!result.Outcome)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.Behavior = CaptureObject(behavior);
    result.Outcome = Configure(behavior, owner, nullptr, spec, frame, record);
    if (!result.Outcome) {
        QueueDestroy(record);
        return result;
    }

    const std::uint64_t instanceId = record.Id;
    result.Descriptor = Describe(behavior, record.LayoutGeneration);
    m_Records.emplace(instanceId, std::move(record));
    result.Handle = Instance(m_Access, instanceId);
    return result;
}

CallResult Runtime::Call(CKBeObject *owner, const Spec &spec,
                                 const Slot &input,
                                 const CKBehaviorContext *frame) {
    CallResult result;
    CreateResult created = Instantiate(owner, spec, frame);
    result.Outcome = created.Outcome;
    if (!created)
        return result;
    result.Descriptor = std::move(created.Descriptor);
    result.Handle = std::move(created.Handle);
    result.Run = Pulse(result.Handle, input, frame);
    if (!result.Run)
        result.Outcome = result.Run.Outcome;
    return result;
}

AttachResult Runtime::AddToGraph(CKBehavior *parent, const Spec &spec,
                                             const CKBehaviorContext *frame) {
    AttachResult result;
    result.Outcome = ReadyStatus();
    if (!result.Outcome)
        return result;
    if (!parent || parent->GetCKContext() != m_Context) {
        result.Outcome = Failure(Error::OwnerInvalid, "Parent graph is invalid or belongs to another CKContext.");
        return result;
    }

    CKBehavior *behavior = nullptr;
    result.Outcome = CreateBehavior(spec, behavior);
    if (!result.Outcome)
        return result;

    Record record;
    record.Id = m_NextInstanceId++;
    record.Behavior = CaptureObject(behavior);
    record.Parent = CaptureObject(parent);
    record.GraphResident = true;
    result.Outcome = Configure(behavior, parent->GetOwner(), parent, spec, frame, record);
    if (!result.Outcome) {
        QueueDestroy(record);
        return result;
    }

    result.Block = behavior;
    result.Descriptor = Describe(behavior, record.LayoutGeneration);
    m_Records.emplace(record.Id, std::move(record));
    return result;
}

Layout Runtime::Describe(CKBehavior *behavior, std::uint64_t generation) const {
    Layout layout;
    if (!ReadyStatus() || !behavior)
        return layout;
    layout.Prototype = behavior->GetPrototypeGuid();
    if (CKBehaviorPrototype *prototype = behavior->GetPrototype()) {
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
        const bool setting = behavior->IsLocalParameterSetting(nativeIndex) != FALSE;
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
        CKParameterTypeDesc *description = m_Context
            ? m_Context->GetParameterManager()->GetParameterTypeDescription(target->GetGUID())
            : nullptr;
        layout.Slots.push_back({SlotKind::Target, 0, 0,
                                target->GetName() ? target->GetName() : "Target",
                                target->GetGUID(), target->GetRealSource()
                                    ? target->GetRealSource()->GetDataSize()
                                    : (description ? description->DefaultSize : 0)});
    }
    return layout;
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
        } else if (candidate.Index == selector.Index) {
            matches.push_back(&candidate);
        }
    }
    if (matches.empty()) {
        std::ostringstream message;
        message << SlotKindName(selector.Kind) << " "
                << (selector.UsesName()
                    ? "'" + selector.Name + "'"
                    : "#" + std::to_string(selector.Index))
                << " was not found on Building Block '" << SafeName(behavior) << "'."
                << CandidateList(layout, selector.Kind);
        return failSlot(Error::SlotNotFound, message.str());
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

Status Runtime::ApplyValue(CKParameter *parameter, const Value &value) const {
    if (!parameter)
        return Failure(Error::SourceInvalid, "Parameter is not writable.");

    if (value.Type().IsValid()) {
        CKParameterManager *manager = m_Context ? m_Context->GetParameterManager() : nullptr;
        if (!manager || !manager->IsTypeCompatible(parameter->GetGUID(), value.Type()))
            return Failure(Error::TypeMismatch, "Parameter value type is incompatible with the slot type.");
    }

    CKERROR error = CK_OK;
    switch (value.Kind()) {
    case ValueKind::Raw:
        if (value.Bytes().empty())
            return Failure(Error::ValueWriteFailed, "Raw parameter value is empty.",
                           CKERR_INVALIDPARAMETER);
        if (parameter->GetDataSize() != static_cast<int>(value.Bytes().size())) {
            return Failure(Error::TypeMismatch,
                           "Raw value size does not match the registered parameter type.");
        }
        error = parameter->SetValue(value.Bytes().data(), static_cast<int>(value.Bytes().size()));
        break;
    case ValueKind::Text:
        error = parameter->SetStringValue(const_cast<CKSTRING>(value.StringValue().c_str()));
        break;
    case ValueKind::Object: {
        CKObject *object = value.ObjectId() ? m_Context->GetObject(value.ObjectId()) : nullptr;
        if (object != value.ObjectValue() || (object && object->IsToBeDeleted()))
            return Failure(Error::SourceInvalid, "Object value has expired.");
        CKParameterManager *manager = m_Context->GetParameterManager();
        CKParameterTypeDesc *description = manager
            ? manager->GetParameterTypeDescription(parameter->GetGUID()) : nullptr;
        if (object && description && description->Cid != 0 &&
            !CKIsChildClassOf(object, static_cast<CK_CLASSID>(description->Cid))) {
            return Failure(Error::TypeMismatch,
                           "Object value is incompatible with the parameter class.");
        }
        const CK_ID id = object ? object->GetID() : 0;
        error = parameter->SetValue(&id, sizeof(id));
        break;
    }
    case ValueKind::Snapshot: {
        CKObject *sourceObject = value.ParameterSourceId()
            ? m_Context->GetObject(value.ParameterSourceId()) : nullptr;
        if (sourceObject != value.ParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
            return Failure(Error::SourceInvalid, "Snapshot source is invalid.");
        }
        error = parameter->CopyValue(value.ParameterSource(), TRUE);
        break;
    }
    case ValueKind::DirectSource:
    case ValueKind::SharedSource:
        return Failure(Error::InvalidState, "Source bindings may only be applied to input parameters.");
    }
    return error == CK_OK ? Status{}
                          : Failure(Error::ValueWriteFailed, "CKParameter rejected the value.", error);
}

Status Runtime::BindInput(CKBehavior *behavior, Record &record,
                                          const SlotInfo &slot, const Value &value) {
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

    const bool literalValue = value.Kind() != ValueKind::DirectSource &&
                              value.Kind() != ValueKind::SharedSource;
    if (literalValue && oldOwned && oldDirect &&
        oldDirect->GetGUID() == input->GetGUID() &&
        SourceReferenceCount(oldDirect) == 1) {
        return ApplyValue(oldDirect, value);
    }

    if (value.Kind() == ValueKind::DirectSource) {
        CKObject *sourceObject = value.ParameterSourceId()
            ? m_Context->GetObject(value.ParameterSourceId()) : nullptr;
        if (sourceObject != value.ParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
            return Failure(Error::SourceInvalid, "Direct source is invalid.");
        }
        const CKERROR error = input->SetDirectSource(value.ParameterSource());
        if (error != CK_OK)
            return Failure(Error::TypeMismatch, "Input rejected its direct source.", error);
    } else if (value.Kind() == ValueKind::SharedSource) {
        CKObject *sourceObject = value.SharedParameterSourceId()
            ? m_Context->GetObject(value.SharedParameterSourceId()) : nullptr;
        if (sourceObject != value.SharedParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETERIN)) {
            return Failure(Error::SourceInvalid, "Shared input source is invalid.");
        }
        const CKERROR error = input->ShareSourceWith(value.SharedParameterSource());
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
        Status status = ApplyValue(literal, value);
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
            behavior->GetPrototypeGuid());
        status.Details.OperationGuid = spec.m_Operation;
        return status;
    }

    auto resolveInputType = [&](const Value &value, bool provided,
                                CKGUID &type, int inputIndex) -> Status {
        if (!provided) {
            type = CKPGUID_NONE;
            return {};
        }
        CKObject *sourceObject = nullptr;
        if (value.Kind() == ValueKind::DirectSource) {
            sourceObject = value.ParameterSourceId()
                ? m_Context->GetObject(value.ParameterSourceId()) : nullptr;
            if (sourceObject != value.ParameterSource() || !sourceObject ||
                sourceObject->IsToBeDeleted() ||
                !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
                return Failure(Error::SourceInvalid,
                               "Parameter operation input " + std::to_string(inputIndex) +
                                   " direct source is invalid.",
                               CKERR_INVALIDOBJECT, CKBR_OK,
                               Phase::ParameterBinding);
            }
            type = value.ParameterSource()->GetGUID();
            return {};
        }
        if (value.Kind() == ValueKind::SharedSource) {
            sourceObject = value.SharedParameterSourceId()
                ? m_Context->GetObject(value.SharedParameterSourceId()) : nullptr;
            if (sourceObject != value.SharedParameterSource() || !sourceObject ||
                sourceObject->IsToBeDeleted() ||
                !CKIsChildClassOf(sourceObject, CKCID_PARAMETERIN)) {
                return Failure(Error::SourceInvalid,
                               "Parameter operation input " + std::to_string(inputIndex) +
                                   " shared source is invalid.",
                               CKERR_INVALIDOBJECT, CKBR_OK,
                               Phase::ParameterBinding);
            }
            type = value.SharedParameterSource()->GetGUID();
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
                behavior->GetPrototypeGuid());
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
            behavior->GetPrototypeGuid());
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
            behavior->GetPrototypeGuid());
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
            behavior->GetPrototypeGuid()));
    }

    auto bindOperationInput = [&](CKParameterIn *input, const Value &value,
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
        if (value.Kind() == ValueKind::DirectSource) {
            error = input->SetDirectSource(value.ParameterSource());
        } else if (value.Kind() == ValueKind::SharedSource) {
            error = input->ShareSourceWith(value.SharedParameterSource());
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
            Status applied = ApplyValue(literal, value);
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
            behavior->GetPrototypeGuid()));
    }
    const CKERROR connectError = target->SetDirectSource(operation->GetOutParameter());
    if (connectError != CK_OK) {
        return rollback(Failure(
            Error::TypeMismatch,
            "Operation output is incompatible with its target input parameter.",
            connectError, CKBR_OK, Phase::ParameterBinding,
            behavior->GetPrototypeGuid()));
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
                           behavior->GetPrototypeGuid());
        if (owner && !CKIsChildClassOf(owner, behavior->GetCompatibleClassID())) {
            return Failure(Error::OwnerInvalid,
                           "Owner is incompatible with the Building Block Prototype.",
                           CK_OK, CKBR_OK, Phase::OwnerBinding,
                           behavior->GetPrototypeGuid());
        }
        return {};
    }

    const CKERROR useError = behavior->UseTarget(TRUE);
    if (useError != CK_OK)
        return Failure(Error::TargetInvalid,
                       "Failed to enable the explicit target.", useError,
                       CKBR_OK, Phase::TargetBinding,
                       behavior->GetPrototypeGuid());
    if (spec.m_TargetValue.Kind() == ValueKind::Object &&
        spec.m_TargetValue.ObjectValue()) {
        CKObject *target = spec.m_TargetValue.ObjectId()
            ? m_Context->GetObject(spec.m_TargetValue.ObjectId()) : nullptr;
        if (target != spec.m_TargetValue.ObjectValue() || target->IsToBeDeleted())
            return Failure(Error::TargetInvalid, "Explicit target has expired.",
                           CK_OK, CKBR_OK, Phase::TargetBinding,
                           behavior->GetPrototypeGuid());
        if (!CKIsChildClassOf(target, behavior->GetCompatibleClassID())) {
            return Failure(Error::TargetInvalid,
                           "Explicit target is incompatible with the Building Block Prototype.",
                           CK_OK, CKBR_OK, Phase::TargetBinding,
                           behavior->GetPrototypeGuid());
        }
    }
    SlotInfo targetSlot;
    Status status = Resolve(
        behavior, Slot::At(SlotKind::Target, 0, spec.m_TargetType), targetSlot);
    if (status)
        status = BindInput(behavior, record, targetSlot, spec.m_TargetValue);
    return Annotate(std::move(status), Phase::TargetBinding,
                    behavior->GetPrototypeGuid());
}

Status Runtime::ApplyBindings(CKBehavior *behavior, const Spec &spec,
                                              Record &record) {
    for (const Spec::Binding &binding : spec.m_Locals) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            behavior->GetPrototypeGuid(), &binding.Target);
        status = ApplyValue(ResolveParameter(behavior, slot), binding.Source);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            behavior->GetPrototypeGuid(), &binding.Target);
    }
    for (const Spec::Binding &binding : spec.m_Inputs) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            behavior->GetPrototypeGuid(), &binding.Target);
        status = BindInput(behavior, record, slot, binding.Source);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            behavior->GetPrototypeGuid(), &binding.Target);
    }
    for (const Spec::OperationBinding &binding : spec.m_Operations) {
        SlotInfo slot;
        Status status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::ParameterBinding,
                            behavior->GetPrototypeGuid(), &binding.Target);
        status = BindOperation(behavior, record, slot, binding.Definition);
        if (!status) {
            status.Details.Selector = binding.Target;
            status.Details.Prototype = behavior->GetPrototypeGuid();
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
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        Record &record = it->second;
        CKBehavior *behavior = ResolveBehavior(record);
        if (!behavior) {
            if (record.Running) {
                record.Expired = true;
                record.Task = false;
                ++it;
            } else {
                QueueDestroy(record);
                it = m_Records.erase(it);
            }
            continue;
        }
        PruneOwnedSources(behavior, record);
        PruneOwnedOperations(behavior, record);
        ++it;
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
    CKBehaviorPrototype *prototype = behavior->GetPrototype();
    if (!prototype)
        return Failure(Error::PrototypeNotFound, "Building Block Prototype is unavailable.");

    Status status = EnsurePrototypeLayout(behavior, prototype, false);
    if (!status)
        return status;

    if (owner) {
        const CKERROR ownerError = behavior->SetOwner(owner, FALSE);
        if (ownerError != CK_OK)
            return Failure(Error::OwnerInvalid,
                           "Failed to assign Building Block owner.", ownerError,
                           CKBR_OK, Phase::OwnerBinding,
                           behavior->GetPrototypeGuid());
    }
    status = BindTarget(behavior, owner, spec, record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, record);
    if (!status)
        return status;

    const std::vector<Spec::Binding> &initialSettings = spec.m_SettingStages.front();
    for (const Spec::Binding &binding : initialSettings) {
        SlotInfo slot;
        status = Resolve(behavior, binding.Target, slot);
        if (!status)
            return Annotate(std::move(status), Phase::Settings,
                            behavior->GetPrototypeGuid(), &binding.Target);
        status = ApplyValue(ResolveParameter(behavior, slot), binding.Source);
        if (!status)
            return Annotate(std::move(status), Phase::Settings,
                            behavior->GetPrototypeGuid(), &binding.Target);
    }

    if (parent) {
        const CKERROR addError = parent->AddSubBehavior(behavior);
        if (addError != CK_OK)
            return Failure(Error::OwnerInvalid,
                           "Failed to add Building Block to parent graph.", addError);
        record.Placed = true;
    }

    record.Created = true;
    ++record.LayoutGeneration;
    status = CallCallback(behavior, CKM_BEHAVIORCREATE, frame);
    if (!status)
        return status;
    status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return status;
    status = BindTarget(behavior, owner, spec, record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, record);
    if (!status)
        return status;
    if (owner) {
        record.Attached = true;
        ++record.LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORATTACH, frame);
        if (!status)
            return status;
        status = EnsurePrototypeLayout(behavior, prototype, true);
        if (!status)
            return status;
        status = BindTarget(behavior, owner, spec, record);
        if (!status)
            return status;
        status = EnsurePrototypeDefaults(behavior, prototype, record);
        if (!status)
            return status;
    }

    for (std::size_t stageIndex = 0; stageIndex < spec.m_SettingStages.size(); ++stageIndex) {
        const std::vector<Spec::Binding> &stage = spec.m_SettingStages[stageIndex];
        if (stageIndex != 0) {
            for (const Spec::Binding &binding : stage) {
                SlotInfo slot;
                status = Resolve(behavior, binding.Target, slot);
                if (!status)
                    return Annotate(std::move(status), Phase::Settings,
                                    behavior->GetPrototypeGuid(), &binding.Target);
                status = ApplyValue(ResolveParameter(behavior, slot), binding.Source);
                if (!status)
                    return Annotate(std::move(status), Phase::Settings,
                                    behavior->GetPrototypeGuid(), &binding.Target);
            }
        }
        if (stage.empty())
            continue;
        ++record.LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORSETTINGSEDITED, frame);
        if (!status)
            return status;
        status = EnsurePrototypeLayout(behavior, prototype, true);
        if (!status)
            return status;
        status = BindTarget(behavior, owner, spec, record);
        if (!status)
            return status;
        status = EnsurePrototypeDefaults(behavior, prototype, record);
        if (!status)
            return status;
    }

    for (const std::string &name : spec.m_AddedInputs) {
        if (!behavior->CreateInput(const_cast<CKSTRING>(name.c_str())))
            return Failure(Error::CreateFailed, "Failed to add a Building Block input.");
    }
    for (const std::string &name : spec.m_AddedOutputs) {
        if (!behavior->CreateOutput(const_cast<CKSTRING>(name.c_str())))
            return Failure(Error::CreateFailed, "Failed to add a Building Block output.");
    }
    status = ApplyBindings(behavior, spec, record);
    if (!status)
        return status;

    ++record.LayoutGeneration;
    status = CallCallback(behavior, CKM_BEHAVIOREDITED, frame);
    if (!status)
        return status;
    status = EnsurePrototypeLayout(behavior, prototype, true);
    if (!status)
        return status;
    status = BindTarget(behavior, owner, spec, record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, record);
    if (!status)
        return status;
    status = ApplyBindings(behavior, spec, record);
    if (!status)
        return status;
    PruneOwnedSources(behavior, record);
    PruneOwnedOperations(behavior, record);
    return {};
}

Status Runtime::CallCallback(CKBehavior *behavior, CKDWORD message,
                                             const CKBehaviorContext *frame) const {
    if (!behavior || !m_Context)
        return Failure(Error::InvalidState, "Cannot invoke callback on an expired behavior.");

    const CK_ID behaviorId = behavior->GetID();
    const CKGUID prototypeGuid = behavior->GetPrototypeGuid();
    CKERROR result = CK_OK;
    BehaviorBlockData *block = BehaviorInternals::BlockData(behavior);
    const CKDWORD mask = CallbackMaskForMessage(message);
    const CKBEHAVIORCALLBACKFCT callback = block ? block->m_Callback : nullptr;
    const CKDWORD callbackMask = block ? block->m_CallbackMask : 0;
    void *const callbackArg = block ? block->m_CallbackArg : nullptr;
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
                                         const Value &value) {
    SlotRef slot;
    Status status = Resolve(instance, selector, slot);
    return status ? SetInput(instance, slot, value) : status;
}

Status Runtime::SetInput(Instance &instance,
                                         const SlotRef &slot,
                                         const Value &value) {
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
    if (record->Running || record->Task)
        return Failure(Error::InvalidState,
                       "Cannot rebind an input while the instance is executing or managed.");
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
                                         const Value &value) {
    SlotRef slot;
    Status status = Resolve(instance, selector, slot);
    return status ? SetLocal(instance, slot, value) : status;
}

Status Runtime::SetLocal(Instance &instance,
                                         const SlotRef &slot,
                                         const Value &value) {
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
    if (record->Running || record->Task)
        return Failure(Error::InvalidState,
                       "Cannot edit a local while the instance is executing or managed.");
    Status status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != SlotKind::Local)
        return Failure(Error::InvalidState, "Resolved slot is not a local parameter.");
    return ApplyValue(ResolveParameter(behavior, slot.Slot), value);
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
    if (record->Running || record->Task)
        return Failure(Error::InvalidState,
                       "Cannot reconfigure while the instance is executing or managed.");
    if (spec.Prototype() != behavior->GetPrototypeGuid())
        return Failure(Error::InvalidState,
                       "Reconfiguration spec names a different Building Block Prototype.");
    if (!spec.m_AddedInputs.empty() || !spec.m_AddedOutputs.empty())
        return Failure(Error::InvalidState,
                       "Reconfiguration cannot append duplicate behavior IOs.");

    CKBehaviorPrototype *prototype = behavior->GetPrototype();
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
                                behavior->GetPrototypeGuid(), &binding.Target);
            status = ApplyValue(ResolveParameter(behavior, setting), binding.Source);
            if (!status)
                return Annotate(std::move(status), Phase::Settings,
                                behavior->GetPrototypeGuid(), &binding.Target);
        }
        if (stage.empty())
            continue;
        ++record->LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORSETTINGSEDITED, frame);
        if (!status)
            return status;
        status = Reacquire(instanceId, behavior, record);
        if (!status)
            return status;
        owner = behavior->GetOwner();
        prototype = behavior->GetPrototype();
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
    status = CallCallback(behavior, CKM_BEHAVIOREDITED, frame);
    if (!status)
        return status;
    status = Reacquire(instanceId, behavior, record);
    if (!status)
        return status;
    owner = behavior->GetOwner();
    prototype = behavior->GetPrototype();
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
    return Pulse(instance, slot, frame);
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
    return Execute(record->Id, input.Slot.NativeIndex, true, frame);
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
    return Execute(record->Id, -1, false, frame);
}

RunResult Runtime::StartTask(Instance &instance, const Slot &input,
                                           const CKBehaviorContext *frame) {
    RunResult result = Pulse(instance, input, frame);
    if (Record *record = FindRecord(instance))
        record->Task = result.State == RunState::Continuing || result.State == RunState::Suspended;
    return result;
}

bool Runtime::IsTaskActive(const Instance &instance) const {
    if (!ReadyStatus())
        return false;
    const Record *record = FindRecord(instance);
    return record && record->Task;
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
        if (record.Task)
            tasks.push_back(instanceId);
    }
    for (std::uint64_t instanceId : tasks) {
        auto it = m_Records.find(instanceId);
        if (it == m_Records.end() || !it->second.Task)
            continue;
        RunResult result = Execute(instanceId, -1, false, frame);
        it = m_Records.find(instanceId);
        if (it != m_Records.end() &&
            result.State != RunState::Continuing &&
            result.State != RunState::Suspended) {
            it->second.Task = false;
        }
    }
}

void Runtime::ProcessFrame() {
    if (!ReadyStatus())
        return;
    if (m_ProcessingFrame)
        return;
    FlagScope processing(m_ProcessingFrame);
    DrainDeferredReleases();
    ProcessTasks(&m_Context->m_BehaviorContext);
    SweepRecords();
    AdoptSharedBindings();
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (pending.Frames > 0)
            --pending.Frames;
    }
    DestroyReady(DestroyMode::Ready);
}

RunResult Runtime::Execute(std::uint64_t instanceId, int input, bool activateInput,
                                         const CKBehaviorContext *frame) {
    Record *record = FindRecord(instanceId);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return {Failure(Error::InvalidState, "Behavior instance has expired."),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    const CKGUID prototypeGuid = behavior->GetPrototypeGuid();
    if (record->GraphResident)
        return {Failure(Error::InvalidState,
                        "Graph-resident Building Blocks must be executed by their parent graph.",
                        CK_OK, CKBR_OK, Phase::Execution,
                        prototypeGuid),
                RunState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Running)
        return {Failure(Error::InvalidState,
                        "Behavior instance execution is reentrant.", CK_OK, CKBR_OK,
                        Phase::Execution, prototypeGuid),
                RunState::Failed, CKBR_LOCKED, {}};

    if (activateInput) {
        behavior->Activate(TRUE, TRUE);
        for (int i = 0; i < behavior->GetInputCount(); ++i)
            behavior->ActivateInput(i, i == input ? TRUE : FALSE);
    }

    record->Running = true;
    ++record->LayoutGeneration;
    const int returnCode = ExecuteNative(behavior, frame);
    record = FindRecord(instanceId);
    if (!record) {
        return {Failure(Error::InvalidState,
                        "Building Block record disappeared during execution.", CK_OK,
                        returnCode, Phase::Execution,
                        prototypeGuid),
                RunState::Failed, returnCode, {}};
    }
    record->Running = false;

    if (record->Expired || ResolveBehavior(*record) != behavior) {
        record->Task = false;
        for (ObjectStamp source : record->OwnedSources)
            QueueSourceDestroy(source);
        for (OwnedOperation &operation : record->OwnedOperations)
            QueueOperationDestroy(std::move(operation));
        m_Records.erase(instanceId);
        return {Failure(Error::InvalidState,
                        "Building Block destroyed itself during execution.", CK_OK,
                        returnCode, Phase::Execution),
                RunState::Failed, returnCode, {}};
    }

    RunResult result;
    result.ReturnCode = returnCode;
    result.Outcome.BehaviorResult = returnCode;
    for (int i = 0; i < behavior->GetOutputCount(); ++i) {
        if (behavior->IsOutputActive(i))
            result.ActiveOutputs.push_back(i);
        behavior->ActivateOutput(i, FALSE);
    }
    for (int i = 0; i < behavior->GetInputCount(); ++i)
        behavior->ActivateInput(i, FALSE);

    if (IsExecutionError(returnCode)) {
        result.Outcome = Failure(Error::ExecutionFailed,
                                "Building Block execution returned an error.", CK_OK,
                                returnCode, Phase::Execution,
                                prototypeGuid);
        result.State = HasContinuation(returnCode)
            ? RunState::Continuing : RunState::Failed;
    } else if (returnCode == CKBR_BREAK) {
        result.State = RunState::Suspended;
    } else if (HasContinuation(returnCode)) {
        result.State = RunState::Continuing;
    } else {
        result.State = RunState::Completed;
    }

    const bool releaseRequested = record->ReleaseRequested;
    const bool forceDestroy = record->ForceDestroy;
    if (releaseRequested) {
        result.State = RunState::Completed;
        record->Task = false;
        QueueDestroy(*record, forceDestroy);
        m_Records.erase(instanceId);
        if (forceDestroy)
            DestroyReady(DestroyMode::Reset);
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
                       behavior ? behavior->GetPrototypeGuid() : CKGUID());
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
    if (it->second.Running) {
        it->second.ReleaseRequested = true;
        it->second.Task = false;
        return;
    }
    QueueDestroy(it->second);
    m_Records.erase(it);
}

void Runtime::QueueDestroy(Record &record, bool reset) {
    record.Task = false;
    CKBehavior *behavior = ResolveBehavior(record);
    if (behavior)
        behavior->Activate(FALSE, FALSE);
    PendingDestroy pending;
    pending.Behavior = record.Behavior;
    pending.Parent = record.Parent;
    pending.Sources = std::move(record.OwnedSources);
    pending.Operations = std::move(record.OwnedOperations);
    pending.DestroyBehavior = true;
    pending.GraphResident = record.Placed;
    pending.Created = record.Created;
    pending.Attached = record.Attached;
    pending.Reset = reset;
    m_PendingDestroy.push_back(std::move(pending));
    record.Placed = false;
    record.Created = false;
    record.Attached = false;
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

void Runtime::AdoptSharedBindings() {
    if (!m_SharedBindings || m_SharedBindings->Pending.empty())
        return;
    m_PendingDestroy.splice(m_PendingDestroy.end(), m_SharedBindings->Pending);
}

void Runtime::Close() {
    if (!m_Context || m_Thread != std::this_thread::get_id())
        return;
    DrainDeferredReleases();
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        QueueDestroy(it->second);
        it = m_Records.erase(it);
    }
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
            behavior->Activate(FALSE, FALSE);
            if (it->Reset)
                (void) CallCallback(behavior, CKM_BEHAVIORRESET, nullptr);
            behavior = resolveBehavior();
            if (behavior && it->Attached)
                (void) CallCallback(behavior, CKM_BEHAVIORDETACH, nullptr);
            behavior = resolveBehavior();
            if (behavior && it->Created)
                (void) CallCallback(behavior, CKM_BEHAVIORDELETE, nullptr);
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
                if (parentObject && CKIsChildClassOf(parentObject, CKCID_BEHAVIOR))
                    static_cast<CKBehavior *>(parentObject)->RemoveSubBehavior(behavior);
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
        it = m_PendingDestroy.erase(it);
        if (!retained.Sources.empty() || !retained.Operations.empty()) {
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
            pending.Created = false;
            pending.Attached = false;
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
        if (it->second.Running) {
            it->second.Expired = true;
            it->second.Task = false;
            it->second.Created = false;
            it->second.Attached = false;
            ++it;
            continue;
        }
        for (ObjectStamp source : it->second.OwnedSources) {
            if (!ContainsId(ids, count, source.Id))
                QueueSourceDestroy(source);
        }
        for (OwnedOperation &operation : it->second.OwnedOperations)
            QueueOperationDestroy(std::move(operation));
        it = m_Records.erase(it);
    }
}

void Runtime::ResetWorld() {
    if (!m_Context || m_Thread != std::this_thread::get_id())
        return;
    DrainDeferredReleases();
    AdoptSharedBindings();
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (pending.Behavior.Id != 0)
            pending.Reset = true;
    }
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        Record &record = it->second;
        if (record.Running) {
            record.ReleaseRequested = true;
            record.ForceDestroy = true;
            record.Task = false;
            ++it;
            continue;
        }
        QueueDestroy(record, true);
        it = m_Records.erase(it);
    }
    DestroyReady(DestroyMode::Reset);
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
    case Phase::Teardown: return "teardown";
    }
    return "unknown";
}

} // namespace BML::Behavior
