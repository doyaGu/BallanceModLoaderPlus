#include "Virtools/BehaviorRuntime.h"

#include <algorithm>
#include <sstream>

#include "Virtools/CKIdentityRegistry.h"

namespace BML::Virtools {
namespace {

BehaviorStatus Failure(BehaviorError error, std::string message, CKERROR ckError = CK_OK,
                       int behaviorResult = CKBR_OK) {
    return {error, ckError, behaviorResult, std::move(message)};
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

bool SameRef(BML_ObjectRef left, BML_ObjectRef right) {
    return left.Domain == right.Domain && left.Slot == right.Slot &&
           left.Generation == right.Generation;
}

bool ContainsId(const CK_ID *ids, int count, BML_ObjectRef reference) {
    if (!ids || reference.Slot == 0)
        return false;
    for (int i = 0; i < count; ++i) {
        if (ids[i] == static_cast<CK_ID>(reference.Slot))
            return true;
    }
    return false;
}

} // namespace

SlotSelector SlotSelector::At(BehaviorSlotKind kind, int index, CKGUID expectedType) {
    SlotSelector selector;
    selector.Kind = kind;
    selector.Index = index;
    selector.ExpectedType = expectedType;
    return selector;
}

SlotSelector SlotSelector::Named(BehaviorSlotKind kind, std::string name, CKGUID expectedType) {
    SlotSelector selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.ExpectedType = expectedType;
    selector.RequireUnique = true;
    return selector;
}

SlotSelector SlotSelector::OccurrenceOf(BehaviorSlotKind kind, std::string name, int occurrence,
                                        CKGUID expectedType) {
    SlotSelector selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.Occurrence = occurrence;
    selector.ExpectedType = expectedType;
    return selector;
}

ParameterValue ParameterValue::Raw(CKGUID type, const void *data, std::size_t size) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::Raw;
    value.m_Type = type;
    if (data && size != 0) {
        value.m_Bytes.resize(size);
        std::memcpy(value.m_Bytes.data(), data, size);
    }
    return value;
}

ParameterValue ParameterValue::UntypedRaw(const void *data, std::size_t size) {
    return Raw(CKGUID(), data, size);
}

ParameterValue ParameterValue::Text(CKGUID type, std::string text) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::Text;
    value.m_Type = type;
    value.m_Text = std::move(text);
    return value;
}

ParameterValue ParameterValue::String(std::string text) {
    return Text(CKPGUID_STRING, std::move(text));
}

ParameterValue ParameterValue::Object(CKGUID type, CKObject *object) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::Object;
    value.m_Type = type;
    value.m_Object = object;
    value.m_ObjectId = object ? object->GetID() : 0;
    return value;
}

ParameterValue ParameterValue::Snapshot(CKParameter *source) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::Snapshot;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

ParameterValue ParameterValue::DirectSource(CKParameter *source) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::DirectSource;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

ParameterValue ParameterValue::SharedSource(CKParameterIn *source) {
    ParameterValue value;
    value.m_Kind = ParameterValueKind::SharedSource;
    value.m_SharedSource = source;
    value.m_SharedSourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

BehaviorSpec &BehaviorSpec::TargetOwner() {
    m_TargetMode = TargetMode::Owner;
    m_TargetType = CKGUID();
    m_TargetValue = ParameterValue();
    return *this;
}

BehaviorSpec &BehaviorSpec::Target(CKGUID type, CKObject *object) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = ParameterValue::Object(type, object);
    return *this;
}

BehaviorSpec &BehaviorSpec::NullTarget(CKGUID type) {
    m_TargetMode = TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = ParameterValue::Object(type, nullptr);
    return *this;
}

BehaviorSpec &BehaviorSpec::TargetSource(CKGUID type, CKParameter *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = ParameterValue::DirectSource(source);
    return *this;
}

BehaviorSpec &BehaviorSpec::TargetShared(CKGUID type, CKParameterIn *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = ParameterValue::SharedSource(source);
    return *this;
}

BehaviorSpec &BehaviorSpec::Setting(SlotSelector slot, ParameterValue value) {
    slot.Kind = BehaviorSlotKind::Setting;
    m_SettingStages.back().push_back({std::move(slot), std::move(value)});
    return *this;
}

BehaviorSpec &BehaviorSpec::RefreshLayout() {
    if (!m_SettingStages.back().empty())
        m_SettingStages.emplace_back();
    return *this;
}

BehaviorSpec &BehaviorSpec::Input(SlotSelector slot, ParameterValue value) {
    slot.Kind = BehaviorSlotKind::InputParameter;
    for (Binding &binding : m_Inputs) {
        const bool same = binding.Slot.Kind == slot.Kind &&
            (slot.UsesName()
                ? binding.Slot.Name == slot.Name && binding.Slot.Occurrence == slot.Occurrence
                : !binding.Slot.UsesName() && binding.Slot.Index == slot.Index);
        if (same) {
            binding = {std::move(slot), std::move(value)};
            return *this;
        }
    }
    m_Inputs.push_back({std::move(slot), std::move(value)});
    return *this;
}

BehaviorSpec &BehaviorSpec::Local(SlotSelector slot, ParameterValue value) {
    slot.Kind = BehaviorSlotKind::Local;
    m_Locals.push_back({std::move(slot), std::move(value)});
    return *this;
}

BehaviorSpec &BehaviorSpec::AddInput(std::string name) {
    m_AddedInputs.push_back(std::move(name));
    return *this;
}

BehaviorSpec &BehaviorSpec::AddOutput(std::string name) {
    m_AddedOutputs.push_back(std::move(name));
    return *this;
}

BehaviorInstance::~BehaviorInstance() {
    Reset();
}

BehaviorInstance::BehaviorInstance(BehaviorInstance &&other) noexcept
    : m_Runtime(std::exchange(other.m_Runtime, nullptr)),
      m_Token(std::exchange(other.m_Token, 0)) {}

BehaviorInstance &BehaviorInstance::operator=(BehaviorInstance &&other) noexcept {
    if (this == &other)
        return *this;
    Reset();
    m_Runtime = std::exchange(other.m_Runtime, nullptr);
    m_Token = std::exchange(other.m_Token, 0);
    return *this;
}

CKBehavior *BehaviorInstance::Get() const {
    if (!m_Runtime || !m_Runtime->ReadyStatus())
        return nullptr;
    const BehaviorRuntime::Record *record = m_Runtime->FindRecord(*this);
    return record ? m_Runtime->ResolveBehavior(*record) : nullptr;
}

std::uint64_t BehaviorInstance::LayoutGeneration() const {
    if (!m_Runtime || !m_Runtime->ReadyStatus())
        return 0;
    const BehaviorRuntime::Record *record = m_Runtime->FindRecord(*this);
    return record ? record->LayoutGeneration : 0;
}

void BehaviorInstance::Reset() {
    if (m_Runtime && m_Token)
        m_Runtime->RequestRelease(m_Token);
    m_Runtime = nullptr;
    m_Token = 0;
}

BehaviorRuntime::BehaviorRuntime(CKContext *context, CKIdentityRegistry &identities)
    : m_Context(context), m_Identities(&identities), m_Thread(std::this_thread::get_id()) {}

BehaviorRuntime::~BehaviorRuntime() {
    ResetWorld();
}

BehaviorStatus BehaviorRuntime::ReadyStatus() const {
    if (!m_Context || !m_Identities)
        return Failure(BehaviorError::ContextExpired, "Virtools context is no longer available.");
    if (m_Thread != std::this_thread::get_id())
        return Failure(BehaviorError::WrongThread, "BehaviorRuntime may only be used on the game thread.");
    return {};
}

InstanceResult BehaviorRuntime::Instantiate(CKBeObject *owner, const BehaviorSpec &spec,
                                            const CKBehaviorContext *frame) {
    InstanceResult result;
    result.Status = ReadyStatus();
    if (!result.Status)
        return result;
    if (owner && owner->GetCKContext() != m_Context) {
        result.Status = Failure(BehaviorError::OwnerInvalid, "Behavior owner belongs to another CKContext.");
        return result;
    }

    CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(spec.Prototype());
    if (!prototype) {
        result.Status = Failure(BehaviorError::PrototypeNotFound, "Building Block Prototype GUID is not registered.");
        return result;
    }
    if (CKObjectDeclaration *declaration = CKGetObjectDeclarationFromGuid(spec.Prototype())) {
        for (int i = 0; i < declaration->GetManagerNeededCount(); ++i) {
            if (!m_Context->GetManagerByGuid(declaration->GetManagerNeeded(i))) {
                result.Status = Failure(BehaviorError::RequiredManagerMissing,
                                        std::string("Required manager is missing for Building Block '") +
                                            (declaration->GetName() ? declaration->GetName() : "<unnamed>") + "'.");
                return result;
            }
        }
    }

    auto *behavior = static_cast<CKBehavior *>(
        m_Context->CreateObject(CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
    if (!behavior) {
        result.Status = Failure(BehaviorError::CreateFailed, "Failed to create CKBehavior.");
        return result;
    }
    behavior->UseFunction();
    const CKERROR initError = behavior->InitFromGuid(spec.Prototype());
    if (initError != CK_OK) {
        m_Context->DestroyObject(behavior);
        result.Status = Failure(BehaviorError::InitFailed, "Failed to initialize Building Block from Prototype.",
                                initError);
        return result;
    }

    Record record;
    record.Token = m_NextToken++;
    record.Behavior = m_Identities->Make(behavior);
    result.Status = Configure(behavior, owner, nullptr, spec, frame, record);
    if (!result.Status) {
        QueueDestroy(record);
        return result;
    }

    const std::uint64_t token = record.Token;
    result.Layout = Describe(behavior, record.LayoutGeneration);
    m_Records.emplace(token, std::move(record));
    result.Instance = BehaviorInstance(this, token);
    return result;
}

CallResult BehaviorRuntime::Call(CKBeObject *owner, const BehaviorSpec &spec,
                                 const SlotSelector &input,
                                 const CKBehaviorContext *frame) {
    CallResult result;
    InstanceResult created = Instantiate(owner, spec, frame);
    result.Status = created.Status;
    if (!created)
        return result;
    result.Layout = std::move(created.Layout);
    result.Instance = std::move(created.Instance);
    result.Execution = Pulse(result.Instance, input, frame);
    if (!result.Execution)
        result.Status = result.Execution.Status;
    return result;
}

GraphBlockResult BehaviorRuntime::AddToGraph(CKBehavior *parent, const BehaviorSpec &spec,
                                             const CKBehaviorContext *frame) {
    GraphBlockResult result;
    result.Status = ReadyStatus();
    if (!result.Status)
        return result;
    if (!parent || parent->GetCKContext() != m_Context) {
        result.Status = Failure(BehaviorError::OwnerInvalid, "Parent graph is invalid or belongs to another CKContext.");
        return result;
    }

    CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(spec.Prototype());
    if (!prototype) {
        result.Status = Failure(BehaviorError::PrototypeNotFound, "Building Block Prototype GUID is not registered.");
        return result;
    }
    if (CKObjectDeclaration *declaration = CKGetObjectDeclarationFromGuid(spec.Prototype())) {
        for (int i = 0; i < declaration->GetManagerNeededCount(); ++i) {
            if (!m_Context->GetManagerByGuid(declaration->GetManagerNeeded(i))) {
                result.Status = Failure(BehaviorError::RequiredManagerMissing,
                                        "A manager required by the Building Block is not registered.");
                return result;
            }
        }
    }

    auto *behavior = static_cast<CKBehavior *>(
        m_Context->CreateObject(CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
    if (!behavior) {
        result.Status = Failure(BehaviorError::CreateFailed, "Failed to create CKBehavior.");
        return result;
    }
    behavior->UseFunction();
    const CKERROR initError = behavior->InitFromGuid(spec.Prototype());
    if (initError != CK_OK) {
        m_Context->DestroyObject(behavior);
        result.Status = Failure(BehaviorError::InitFailed, "Failed to initialize Building Block from Prototype.",
                                initError);
        return result;
    }

    Record record;
    record.Token = m_NextToken++;
    record.Behavior = m_Identities->Make(behavior);
    record.Parent = m_Identities->Make(parent);
    record.GraphResident = true;
    result.Status = Configure(behavior, parent->GetOwner(), parent, spec, frame, record);
    if (!result.Status) {
        QueueDestroy(record);
        return result;
    }

    result.Behavior = behavior;
    result.Layout = Describe(behavior, record.LayoutGeneration);
    m_Records.emplace(record.Token, std::move(record));
    return result;
}

BehaviorLayout BehaviorRuntime::Describe(CKBehavior *behavior, std::uint64_t generation) const {
    BehaviorLayout layout;
    if (!ReadyStatus() || !behavior)
        return layout;
    layout.Prototype = behavior->GetPrototypeGuid();
    layout.CompatibleClass = behavior->GetCompatibleClassID();
    layout.BehaviorFlags = behavior->GetFlags();
    layout.Generation = generation;

    for (int i = 0; i < behavior->GetInputCount(); ++i) {
        CKBehaviorIO *io = behavior->GetInput(i);
        layout.Slots.push_back({BehaviorSlotKind::Input, i, i, io && io->GetName() ? io->GetName() : "", CKGUID(), 0});
    }
    for (int i = 0; i < behavior->GetOutputCount(); ++i) {
        CKBehaviorIO *io = behavior->GetOutput(i);
        layout.Slots.push_back({BehaviorSlotKind::Output, i, i, io && io->GetName() ? io->GetName() : "", CKGUID(), 0});
    }
    for (int i = 0; i < behavior->GetInputParameterCount(); ++i) {
        CKParameterIn *parameter = behavior->GetInputParameter(i);
        CKParameterTypeDesc *description = parameter && m_Context
            ? m_Context->GetParameterManager()->GetParameterTypeDescription(parameter->GetGUID())
            : nullptr;
        layout.Slots.push_back({BehaviorSlotKind::InputParameter, i, i,
                                parameter && parameter->GetName() ? parameter->GetName() : "",
                                parameter ? parameter->GetGUID() : CKGUID(),
                                parameter && parameter->GetRealSource()
                                    ? parameter->GetRealSource()->GetDataSize()
                                    : (description ? description->DefaultSize : 0)});
    }
    for (int i = 0; i < behavior->GetOutputParameterCount(); ++i) {
        CKParameterOut *parameter = behavior->GetOutputParameter(i);
        layout.Slots.push_back({BehaviorSlotKind::OutputParameter, i, i,
                                parameter && parameter->GetName() ? parameter->GetName() : "",
                                parameter ? parameter->GetGUID() : CKGUID(),
                                parameter ? parameter->GetDataSize() : 0});
    }

    int settingIndex = 0;
    int localIndex = 0;
    for (int nativeIndex = 0; nativeIndex < behavior->GetLocalParameterCount(); ++nativeIndex) {
        CKParameterLocal *parameter = behavior->GetLocalParameter(nativeIndex);
        const bool setting = behavior->IsLocalParameterSetting(nativeIndex) != FALSE;
        const BehaviorSlotKind kind = setting ? BehaviorSlotKind::Setting : BehaviorSlotKind::Local;
        const int index = setting ? settingIndex++ : localIndex++;
        layout.Slots.push_back({kind, index, nativeIndex,
                                parameter && parameter->GetName() ? parameter->GetName() : "",
                                parameter ? parameter->GetGUID() : CKGUID(),
                                parameter ? parameter->GetDataSize() : 0});
    }
    if (CKParameterIn *target = behavior->GetTargetParameter()) {
        CKParameterTypeDesc *description = m_Context
            ? m_Context->GetParameterManager()->GetParameterTypeDescription(target->GetGUID())
            : nullptr;
        layout.Slots.push_back({BehaviorSlotKind::Target, 0, 0,
                                target->GetName() ? target->GetName() : "Target",
                                target->GetGUID(), target->GetRealSource()
                                    ? target->GetRealSource()->GetDataSize()
                                    : (description ? description->DefaultSize : 0)});
    }
    return layout;
}

BehaviorStatus BehaviorRuntime::Resolve(CKBehavior *behavior, const SlotSelector &selector,
                                        BehaviorSlot &slot) const {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return ready;
    if (!behavior)
        return Failure(BehaviorError::InvalidState, "Behavior no longer exists.");

    const BehaviorLayout layout = Describe(behavior);
    std::vector<const BehaviorSlot *> matches;
    for (const BehaviorSlot &candidate : layout.Slots) {
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
        message << "Slot not found on Building Block '" << SafeName(behavior) << "'.";
        return Failure(BehaviorError::SlotNotFound, message.str());
    }
    if (selector.UsesName() && selector.RequireUnique && matches.size() != 1)
        return Failure(BehaviorError::AmbiguousSlot, "Slot name is not unique; select an explicit occurrence.");
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 || occurrence >= static_cast<int>(matches.size()))
        return Failure(BehaviorError::SlotNotFound, "Requested slot occurrence does not exist.");
    slot = *matches[static_cast<std::size_t>(occurrence)];

    if (selector.ExpectedType.IsValid() && slot.Type.IsValid()) {
        CKParameterManager *manager = m_Context ? m_Context->GetParameterManager() : nullptr;
        if (!manager || !manager->IsTypeCompatible(slot.Type, selector.ExpectedType))
            return Failure(BehaviorError::TypeMismatch, "Resolved slot has an incompatible parameter type.");
    }
    return {};
}

BehaviorStatus BehaviorRuntime::Resolve(const BehaviorInstance &instance,
                                        const SlotSelector &selector,
                                        BehaviorSlotHandle &slot) const {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return ready;
    const Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(BehaviorError::InvalidState, "Behavior instance has expired.");
    if (record->Poisoned)
        return Failure(BehaviorError::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
    BehaviorStatus status = Resolve(behavior, selector, slot.Slot);
    if (!status)
        return status;
    CKObject *object = ResolveSlotObject(behavior, slot.Slot);
    slot.Behavior = record->Behavior;
    slot.Object = object ? m_Identities->Make(object) : BML_ObjectRef{};
    if (!object || slot.Object.Slot == 0)
        return Failure(BehaviorError::StaleLayout, "Resolved slot object has expired.");
    slot.LayoutGeneration = record->LayoutGeneration;
    return {};
}

CKParameter *BehaviorRuntime::ResolveParameter(CKBehavior *behavior, const BehaviorSlot &slot) const {
    if (!behavior)
        return nullptr;
    switch (slot.Kind) {
    case BehaviorSlotKind::InputParameter: {
        CKParameterIn *input = behavior->GetInputParameter(slot.NativeIndex);
        return input ? input->GetRealSource() : nullptr;
    }
    case BehaviorSlotKind::OutputParameter:
        return behavior->GetOutputParameter(slot.NativeIndex);
    case BehaviorSlotKind::Setting:
    case BehaviorSlotKind::Local:
        return behavior->GetLocalParameter(slot.NativeIndex);
    case BehaviorSlotKind::Target: {
        CKParameterIn *target = behavior->GetTargetParameter();
        return target ? target->GetRealSource() : nullptr;
    }
    default:
        return nullptr;
    }
}

CKObject *BehaviorRuntime::ResolveSlotObject(CKBehavior *behavior, const BehaviorSlot &slot) const {
    if (!behavior)
        return nullptr;
    switch (slot.Kind) {
    case BehaviorSlotKind::Input:
        return behavior->GetInput(slot.NativeIndex);
    case BehaviorSlotKind::Output:
        return behavior->GetOutput(slot.NativeIndex);
    case BehaviorSlotKind::InputParameter:
        return behavior->GetInputParameter(slot.NativeIndex);
    case BehaviorSlotKind::OutputParameter:
        return behavior->GetOutputParameter(slot.NativeIndex);
    case BehaviorSlotKind::Setting:
    case BehaviorSlotKind::Local:
        return behavior->GetLocalParameter(slot.NativeIndex);
    case BehaviorSlotKind::Target:
        return behavior->GetTargetParameter();
    }
    return nullptr;
}

BehaviorStatus BehaviorRuntime::ValidateSlot(const Record &record,
                                             const BehaviorSlotHandle &slot) const {
    if (!SameRef(record.Behavior, slot.Behavior) ||
        record.LayoutGeneration != slot.LayoutGeneration) {
        return Failure(BehaviorError::StaleLayout,
                       "Resolved slot belongs to an older Building Block layout.");
    }
    CKBehavior *behavior = ResolveBehavior(record);
    CKObject *object = ResolveSlotObject(behavior, slot.Slot);
    if (!object || !SameRef(m_Identities->Make(object), slot.Object)) {
        return Failure(BehaviorError::StaleLayout,
                       "Resolved slot object has been replaced or deleted.");
    }
    return {};
}

CKParameter *BehaviorRuntime::Parameter(const BehaviorInstance &instance, const SlotSelector &selector,
                                         BehaviorStatus *status) const {
    BehaviorStatus ready = ReadyStatus();
    if (!ready) {
        if (status)
            *status = ready;
        return nullptr;
    }
    const Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    BehaviorSlot slot;
    BehaviorStatus resolved = Resolve(behavior, selector, slot);
    if (status)
        *status = resolved;
    return resolved ? ResolveParameter(behavior, slot) : nullptr;
}

CKParameter *BehaviorRuntime::Parameter(const BehaviorInstance &instance,
                                        const BehaviorSlotHandle &slot,
                                        BehaviorStatus *status) const {
    BehaviorStatus ready = ReadyStatus();
    const Record *record = ready ? FindRecord(instance) : nullptr;
    if (ready && !record)
        ready = Failure(BehaviorError::InvalidState, "Behavior instance has expired.");
    if (ready)
        ready = ValidateSlot(*record, slot);
    if (status)
        *status = ready;
    return ready ? ResolveParameter(ResolveBehavior(*record), slot.Slot) : nullptr;
}

BehaviorStatus BehaviorRuntime::ApplyValue(CKParameter *parameter, const ParameterValue &value) const {
    if (!parameter)
        return Failure(BehaviorError::SourceInvalid, "Parameter is not writable.");

    if (value.Type().IsValid()) {
        CKParameterManager *manager = m_Context ? m_Context->GetParameterManager() : nullptr;
        if (!manager || !manager->IsTypeCompatible(parameter->GetGUID(), value.Type()))
            return Failure(BehaviorError::TypeMismatch, "Parameter value type is incompatible with the slot type.");
    }

    CKERROR error = CK_OK;
    switch (value.Kind()) {
    case ParameterValueKind::Raw:
        if (value.Bytes().empty())
            return Failure(BehaviorError::ValueWriteFailed, "Raw parameter value is empty.",
                           CKERR_INVALIDPARAMETER);
        if (parameter->GetDataSize() != static_cast<int>(value.Bytes().size())) {
            return Failure(BehaviorError::TypeMismatch,
                           "Raw value size does not match the registered parameter type.");
        }
        error = parameter->SetValue(value.Bytes().data(), static_cast<int>(value.Bytes().size()));
        break;
    case ParameterValueKind::Text:
        error = parameter->SetStringValue(const_cast<CKSTRING>(value.StringValue().c_str()));
        break;
    case ParameterValueKind::Object: {
        CKObject *object = value.ObjectId() ? m_Context->GetObject(value.ObjectId()) : nullptr;
        if (object != value.ObjectValue() || (object && object->IsToBeDeleted()))
            return Failure(BehaviorError::SourceInvalid, "Object value has expired.");
        CKParameterManager *manager = m_Context->GetParameterManager();
        CKParameterTypeDesc *description = manager
            ? manager->GetParameterTypeDescription(parameter->GetGUID()) : nullptr;
        if (object && description && description->Cid != 0 &&
            !CKIsChildClassOf(object, static_cast<CK_CLASSID>(description->Cid))) {
            return Failure(BehaviorError::TypeMismatch,
                           "Object value is incompatible with the parameter class.");
        }
        const CK_ID id = object ? object->GetID() : 0;
        error = parameter->SetValue(&id, sizeof(id));
        break;
    }
    case ParameterValueKind::Snapshot: {
        CKObject *sourceObject = value.ParameterSourceId()
            ? m_Context->GetObject(value.ParameterSourceId()) : nullptr;
        if (sourceObject != value.ParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
            return Failure(BehaviorError::SourceInvalid, "Snapshot source is invalid.");
        }
        error = parameter->CopyValue(value.ParameterSource(), TRUE);
        break;
    }
    case ParameterValueKind::DirectSource:
    case ParameterValueKind::SharedSource:
        return Failure(BehaviorError::InvalidState, "Source bindings may only be applied to input parameters.");
    }
    return error == CK_OK ? BehaviorStatus{}
                          : Failure(BehaviorError::ValueWriteFailed, "CKParameter rejected the value.", error);
}

BehaviorStatus BehaviorRuntime::BindInput(CKBehavior *behavior, Record &record,
                                          const BehaviorSlot &slot, const ParameterValue &value) {
    if (!behavior)
        return Failure(BehaviorError::InvalidState, "Behavior is unavailable.");
    CKParameterIn *input = slot.Kind == BehaviorSlotKind::Target
        ? behavior->GetTargetParameter()
        : behavior->GetInputParameter(slot.NativeIndex);
    if (!input)
        return Failure(BehaviorError::SlotNotFound, "Input parameter no longer exists.");

    CKParameter *oldDirect = input->GetDirectSource();
    BML_ObjectRef oldRef = oldDirect ? m_Identities->Make(oldDirect) : BML_ObjectRef{};
    auto owned = std::find_if(record.OwnedSources.begin(), record.OwnedSources.end(),
                              [&](BML_ObjectRef candidate) { return SameRef(candidate, oldRef); });
    const bool oldOwned = owned != record.OwnedSources.end();

    const bool literalValue = value.Kind() != ParameterValueKind::DirectSource &&
                              value.Kind() != ParameterValueKind::SharedSource;
    if (literalValue && oldOwned && oldDirect && oldDirect->GetGUID() == input->GetGUID())
        return ApplyValue(oldDirect, value);

    if (value.Kind() == ParameterValueKind::DirectSource) {
        CKObject *sourceObject = value.ParameterSourceId()
            ? m_Context->GetObject(value.ParameterSourceId()) : nullptr;
        if (sourceObject != value.ParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETER)) {
            return Failure(BehaviorError::SourceInvalid, "Direct source is invalid.");
        }
        const CKERROR error = input->SetDirectSource(value.ParameterSource());
        if (error != CK_OK)
            return Failure(BehaviorError::TypeMismatch, "Input rejected its direct source.", error);
    } else if (value.Kind() == ParameterValueKind::SharedSource) {
        CKObject *sourceObject = value.SharedParameterSourceId()
            ? m_Context->GetObject(value.SharedParameterSourceId()) : nullptr;
        if (sourceObject != value.SharedParameterSource() || !sourceObject || sourceObject->IsToBeDeleted() ||
            !CKIsChildClassOf(sourceObject, CKCID_PARAMETERIN)) {
            return Failure(BehaviorError::SourceInvalid, "Shared input source is invalid.");
        }
        const CKERROR error = input->ShareSourceWith(value.SharedParameterSource());
        if (error != CK_OK)
            return Failure(BehaviorError::TypeMismatch, "Input rejected its shared source.", error);
    } else {
        std::ostringstream name;
        name << "__BML_BehaviorSource_" << behavior->GetID() << "_"
             << static_cast<int>(slot.Kind) << "_" << slot.NativeIndex;
        CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
            const_cast<CKSTRING>(name.str().c_str()), input->GetGUID(), TRUE);
        if (!literal)
            return Failure(BehaviorError::CreateFailed, "Failed to create an input source parameter.");
        BehaviorStatus status = ApplyValue(literal, value);
        if (!status) {
            m_Context->DestroyObject(literal);
            return status;
        }
        const CKERROR error = input->SetDirectSource(literal);
        if (error != CK_OK) {
            m_Context->DestroyObject(literal);
            return Failure(BehaviorError::TypeMismatch, "Input rejected its literal source.", error);
        }
        record.OwnedSources.push_back(m_Identities->Make(literal));
    }

    if (oldOwned) {
        QueueSourceDestroy(oldRef);
        record.OwnedSources.erase(owned);
    }
    return {};
}

BehaviorStatus BehaviorRuntime::EnsurePrototypeDefaults(CKBehavior *behavior,
                                                        CKBehaviorPrototype *prototype,
                                                        Record &record) {
    if (!behavior || !prototype)
        return Failure(BehaviorError::InvalidState, "Behavior Prototype is unavailable.");
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
        const ParameterValue value = hasRaw
            ? ParameterValue::Raw(description->Guid, description->DefaultValue,
                                  static_cast<std::size_t>(description->DefaultValueSize))
            : ParameterValue::Text(description->Guid, description->DefaultValueString);
        BehaviorSlot slot{BehaviorSlotKind::InputParameter, index, index,
                          input->GetName() ? input->GetName() : "", input->GetGUID(),
                          input->GetRealSource() ? input->GetRealSource()->GetDataSize() : 0};
        BehaviorStatus status = BindInput(behavior, record, slot, value);
        if (!status)
            return status;
        if (CKParameter *source = input->GetDirectSource())
            behavior->SetInputParameterDefaultValue(input, source);
    }
    return {};
}

BehaviorStatus BehaviorRuntime::BindTarget(CKBehavior *behavior, CKBeObject *owner,
                                           const BehaviorSpec &spec, Record &record) {
    if (!behavior)
        return Failure(BehaviorError::InvalidState, "Behavior is unavailable.");
    if (spec.m_TargetMode == TargetMode::Owner) {
        const CKERROR error = behavior->UseTarget(FALSE);
        if (error != CK_OK)
            return Failure(BehaviorError::TargetInvalid, "Failed to disable the explicit target.", error);
        if (owner && !CKIsChildClassOf(owner, behavior->GetCompatibleClassID())) {
            return Failure(BehaviorError::OwnerInvalid,
                           "Owner is incompatible with the Building Block Prototype.");
        }
        return {};
    }

    const CKERROR useError = behavior->UseTarget(TRUE);
    if (useError != CK_OK)
        return Failure(BehaviorError::TargetInvalid, "Failed to enable the explicit target.", useError);
    if (spec.m_TargetValue.Kind() == ParameterValueKind::Object &&
        spec.m_TargetValue.ObjectValue()) {
        CKObject *target = spec.m_TargetValue.ObjectId()
            ? m_Context->GetObject(spec.m_TargetValue.ObjectId()) : nullptr;
        if (target != spec.m_TargetValue.ObjectValue() || target->IsToBeDeleted())
            return Failure(BehaviorError::TargetInvalid, "Explicit target has expired.");
        if (!CKIsChildClassOf(target, behavior->GetCompatibleClassID())) {
            return Failure(BehaviorError::TargetInvalid,
                           "Explicit target is incompatible with the Building Block Prototype.");
        }
    }
    BehaviorSlot targetSlot;
    BehaviorStatus status = Resolve(
        behavior, SlotSelector::At(BehaviorSlotKind::Target, 0, spec.m_TargetType), targetSlot);
    return status ? BindInput(behavior, record, targetSlot, spec.m_TargetValue) : status;
}

BehaviorStatus BehaviorRuntime::ApplyBindings(CKBehavior *behavior, const BehaviorSpec &spec,
                                              Record &record) {
    for (const BehaviorSpec::Binding &binding : spec.m_Locals) {
        BehaviorSlot slot;
        BehaviorStatus status = Resolve(behavior, binding.Slot, slot);
        if (!status)
            return status;
        status = ApplyValue(ResolveParameter(behavior, slot), binding.Value);
        if (!status)
            return status;
    }
    for (const BehaviorSpec::Binding &binding : spec.m_Inputs) {
        BehaviorSlot slot;
        BehaviorStatus status = Resolve(behavior, binding.Slot, slot);
        if (!status)
            return status;
        status = BindInput(behavior, record, slot, binding.Value);
        if (!status)
            return status;
    }
    return {};
}

void BehaviorRuntime::PruneOwnedSources(CKBehavior *behavior, Record &record) {
    if (!behavior)
        return;
    std::vector<BML_ObjectRef> referenced;
    if (CKParameterIn *target = behavior->GetTargetParameter()) {
        if (CKParameter *source = target->GetDirectSource())
            referenced.push_back(m_Identities->Make(source));
    }
    for (int i = 0; i < behavior->GetInputParameterCount(); ++i) {
        CKParameterIn *input = behavior->GetInputParameter(i);
        if (input) {
            if (CKParameter *source = input->GetDirectSource())
                referenced.push_back(m_Identities->Make(source));
        }
    }
    for (auto it = record.OwnedSources.begin(); it != record.OwnedSources.end();) {
        const bool used = std::any_of(referenced.begin(), referenced.end(),
                                      [&](BML_ObjectRef ref) { return SameRef(ref, *it); });
        if (used) {
            ++it;
        } else {
            QueueSourceDestroy(*it);
            it = record.OwnedSources.erase(it);
        }
    }
}

BehaviorStatus BehaviorRuntime::Configure(CKBehavior *behavior,
                                          CKBeObject *owner, CKBehavior *parent,
                                          const BehaviorSpec &spec,
                                          const CKBehaviorContext *frame,
                                          Record &record) {
    if (!behavior)
        return Failure(BehaviorError::InvalidState, "Behavior configuration target is invalid.");
    CKBehaviorPrototype *prototype = behavior->GetPrototype();
    if (!prototype)
        return Failure(BehaviorError::PrototypeNotFound, "Building Block Prototype is unavailable.");

    if (owner) {
        const CKERROR ownerError = behavior->SetOwner(owner, FALSE);
        if (ownerError != CK_OK)
            return Failure(BehaviorError::OwnerInvalid, "Failed to assign Building Block owner.", ownerError);
    }
    BehaviorStatus status = BindTarget(behavior, owner, spec, record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, record);
    if (!status)
        return status;

    const std::vector<BehaviorSpec::Binding> &initialSettings = spec.m_SettingStages.front();
    for (const BehaviorSpec::Binding &binding : initialSettings) {
        BehaviorSlot slot;
        status = Resolve(behavior, binding.Slot, slot);
        if (!status)
            return status;
        status = ApplyValue(ResolveParameter(behavior, slot), binding.Value);
        if (!status)
            return status;
    }

    if (parent) {
        const CKERROR addError = parent->AddSubBehavior(behavior);
        if (addError != CK_OK)
            return Failure(BehaviorError::OwnerInvalid,
                           "Failed to add Building Block to parent graph.", addError);
    }

    record.Created = true;
    ++record.LayoutGeneration;
    status = CallCallback(behavior, CKM_BEHAVIORCREATE, frame);
    if (!status)
        return status;
    if (owner) {
        record.Attached = true;
        ++record.LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORATTACH, frame);
        if (!status)
            return status;
    }

    for (std::size_t stageIndex = 0; stageIndex < spec.m_SettingStages.size(); ++stageIndex) {
        const std::vector<BehaviorSpec::Binding> &stage = spec.m_SettingStages[stageIndex];
        if (stageIndex != 0) {
            for (const BehaviorSpec::Binding &binding : stage) {
                BehaviorSlot slot;
                status = Resolve(behavior, binding.Slot, slot);
                if (!status)
                    return status;
                status = ApplyValue(ResolveParameter(behavior, slot), binding.Value);
                if (!status)
                    return status;
            }
        }
        if (stage.empty())
            continue;
        ++record.LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORSETTINGSEDITED, frame);
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
            return Failure(BehaviorError::CreateFailed, "Failed to add a Building Block input.");
    }
    for (const std::string &name : spec.m_AddedOutputs) {
        if (!behavior->CreateOutput(const_cast<CKSTRING>(name.c_str())))
            return Failure(BehaviorError::CreateFailed, "Failed to add a Building Block output.");
    }
    status = ApplyBindings(behavior, spec, record);
    if (!status)
        return status;

    ++record.LayoutGeneration;
    status = CallCallback(behavior, CKM_BEHAVIOREDITED, frame);
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
    return {};
}

BehaviorStatus BehaviorRuntime::CallCallback(CKBehavior *behavior, CKDWORD message,
                                             const CKBehaviorContext *frame) const {
    if (!behavior || !m_Context)
        return Failure(BehaviorError::InvalidState, "Cannot invoke callback on an expired behavior.");

    const CK_ID behaviorId = behavior->GetID();
    CKBehaviorContext saved = m_Context->m_BehaviorContext;
    if (frame)
        m_Context->m_BehaviorContext = *frame;
    m_Context->m_BehaviorContext.Context = m_Context;
    m_Context->m_BehaviorContext.Behavior = behavior;
    const CKERROR result = behavior->CallCallbackFunction(message);
    m_Context->m_BehaviorContext = saved;
    CKObject *current = m_Context->GetObject(behaviorId);
    if (current != behavior || (current && current->IsToBeDeleted()))
        return Failure(BehaviorError::InvalidState,
                       "Building Block was destroyed by its lifecycle callback.");
    return result == CK_OK ? BehaviorStatus{}
                           : Failure(BehaviorError::CallbackFailed, "Building Block lifecycle callback failed.", result);
}

BehaviorStatus BehaviorRuntime::SetInput(BehaviorInstance &instance, const SlotSelector &selector,
                                         const ParameterValue &value) {
    BehaviorSlotHandle slot;
    BehaviorStatus status = Resolve(instance, selector, slot);
    return status ? SetInput(instance, slot, value) : status;
}

BehaviorStatus BehaviorRuntime::SetInput(BehaviorInstance &instance,
                                         const BehaviorSlotHandle &slot,
                                         const ParameterValue &value) {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(BehaviorError::InvalidState, "Behavior instance has expired.");
    if (record->Poisoned)
        return Failure(BehaviorError::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
    if (record->Running || record->Task)
        return Failure(BehaviorError::InvalidState,
                       "Cannot rebind an input while the instance is executing or managed.");
    BehaviorStatus status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != BehaviorSlotKind::InputParameter &&
        slot.Slot.Kind != BehaviorSlotKind::Target) {
        return Failure(BehaviorError::InvalidState, "Resolved slot is not an input parameter.");
    }
    status = BindInput(behavior, *record, slot.Slot, value);
    PruneOwnedSources(behavior, *record);
    return status;
}

BehaviorStatus BehaviorRuntime::SetLocal(BehaviorInstance &instance, const SlotSelector &selector,
                                         const ParameterValue &value) {
    BehaviorSlotHandle slot;
    BehaviorStatus status = Resolve(instance, selector, slot);
    return status ? SetLocal(instance, slot, value) : status;
}

BehaviorStatus BehaviorRuntime::SetLocal(BehaviorInstance &instance,
                                         const BehaviorSlotHandle &slot,
                                         const ParameterValue &value) {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(BehaviorError::InvalidState, "Behavior instance has expired.");
    if (record->Poisoned)
        return Failure(BehaviorError::InvalidState,
                       "Behavior instance requires a complete successful reconfiguration.");
    if (record->Running || record->Task)
        return Failure(BehaviorError::InvalidState,
                       "Cannot edit a local while the instance is executing or managed.");
    BehaviorStatus status = ValidateSlot(*record, slot);
    if (!status)
        return status;
    if (slot.Slot.Kind != BehaviorSlotKind::Local)
        return Failure(BehaviorError::InvalidState, "Resolved slot is not a local parameter.");
    return ApplyValue(ResolveParameter(behavior, slot.Slot), value);
}

BehaviorStatus BehaviorRuntime::Reconfigure(BehaviorInstance &instance, const BehaviorSpec &spec,
                                            const CKBehaviorContext *frame) {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return ready;
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return Failure(BehaviorError::InvalidState, "Behavior instance has expired.");
    if (record->Running || record->Task)
        return Failure(BehaviorError::InvalidState,
                       "Cannot reconfigure while the instance is executing or managed.");
    if (spec.Prototype() != behavior->GetPrototypeGuid())
        return Failure(BehaviorError::InvalidState,
                       "Reconfiguration spec names a different Building Block Prototype.");
    if (!spec.m_AddedInputs.empty() || !spec.m_AddedOutputs.empty())
        return Failure(BehaviorError::InvalidState,
                       "Reconfiguration cannot append duplicate behavior IOs.");

    CKBehaviorPrototype *prototype = behavior->GetPrototype();
    if (!prototype)
        return Failure(BehaviorError::PrototypeNotFound,
                       "Building Block Prototype is unavailable.");
    CKBeObject *owner = behavior->GetOwner();
    record->Poisoned = true;
    ++record->LayoutGeneration;
    BehaviorStatus status = BindTarget(behavior, owner, spec, *record);
    if (!status)
        return status;
    for (const auto &stage : spec.m_SettingStages) {
        for (const BehaviorSpec::Binding &binding : stage) {
            BehaviorSlot setting;
            status = Resolve(behavior, binding.Slot, setting);
            if (!status)
                return status;
            status = ApplyValue(ResolveParameter(behavior, setting), binding.Value);
            if (!status)
                return status;
        }
        if (stage.empty())
            continue;
        ++record->LayoutGeneration;
        status = CallCallback(behavior, CKM_BEHAVIORSETTINGSEDITED, frame);
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
    status = BindTarget(behavior, owner, spec, *record);
    if (!status)
        return status;
    status = EnsurePrototypeDefaults(behavior, prototype, *record);
    if (!status)
        return status;
    status = ApplyBindings(behavior, spec, *record);
    PruneOwnedSources(behavior, *record);
    if (status)
        record->Poisoned = false;
    return status;
}

ExecutionResult BehaviorRuntime::Pulse(BehaviorInstance &instance, const SlotSelector &input,
                                       const CKBehaviorContext *frame) {
    SlotSelector entry = input;
    entry.Kind = BehaviorSlotKind::Input;
    BehaviorSlotHandle slot;
    BehaviorStatus status = Resolve(instance, entry, slot);
    if (!status)
        return {std::move(status), ExecutionState::Failed, CKBR_PARAMETERERROR, {}};
    return Pulse(instance, slot, frame);
}

ExecutionResult BehaviorRuntime::Pulse(BehaviorInstance &instance,
                                       const BehaviorSlotHandle &input,
                                       const CKBehaviorContext *frame) {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return {std::move(ready), ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    Record *record = FindRecord(instance);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return {Failure(BehaviorError::InvalidState, "Behavior instance has expired."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Poisoned)
        return {Failure(BehaviorError::InvalidState,
                        "Behavior instance requires a complete successful reconfiguration."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    BehaviorStatus status = ValidateSlot(*record, input);
    if (!status)
        return {std::move(status), ExecutionState::Failed, CKBR_PARAMETERERROR, {}};
    if (input.Slot.Kind != BehaviorSlotKind::Input)
        return {Failure(BehaviorError::InvalidState, "Resolved slot is not a behavior input."),
                ExecutionState::Failed, CKBR_PARAMETERERROR, {}};
    return Execute(record->Token, input.Slot.NativeIndex, true, frame);
}

ExecutionResult BehaviorRuntime::Step(BehaviorInstance &instance, const CKBehaviorContext *frame) {
    BehaviorStatus ready = ReadyStatus();
    if (!ready)
        return {std::move(ready), ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    Record *record = FindRecord(instance);
    if (!record)
        return {Failure(BehaviorError::InvalidState, "Behavior instance has expired."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Poisoned)
        return {Failure(BehaviorError::InvalidState,
                        "Behavior instance requires a complete successful reconfiguration."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    return Execute(record->Token, -1, false, frame);
}

ExecutionResult BehaviorRuntime::StartTask(BehaviorInstance &instance, const SlotSelector &input,
                                           const CKBehaviorContext *frame) {
    ExecutionResult result = Pulse(instance, input, frame);
    if (Record *record = FindRecord(instance))
        record->Task = result.State == ExecutionState::Continuing || result.State == ExecutionState::Suspended;
    return result;
}

bool BehaviorRuntime::IsTaskActive(const BehaviorInstance &instance) const {
    if (!ReadyStatus())
        return false;
    const Record *record = FindRecord(instance);
    return record && record->Task;
}

void BehaviorRuntime::ProcessTasks(const CKBehaviorContext *frame) {
    if (!ReadyStatus())
        return;
    std::vector<std::uint64_t> tasks;
    tasks.reserve(m_Records.size());
    for (const auto &[token, record] : m_Records) {
        if (record.Task)
            tasks.push_back(token);
    }
    for (std::uint64_t token : tasks) {
        auto it = m_Records.find(token);
        if (it == m_Records.end() || !it->second.Task)
            continue;
        ExecutionResult result = Execute(token, -1, false, frame);
        it = m_Records.find(token);
        if (it != m_Records.end() &&
            result.State != ExecutionState::Continuing &&
            result.State != ExecutionState::Suspended) {
            it->second.Task = false;
        }
    }
}

void BehaviorRuntime::ProcessFrame() {
    if (!ReadyStatus())
        return;
    DrainDeferredReleases();
    ProcessTasks(&m_Context->m_BehaviorContext);
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (pending.Frames > 0)
            --pending.Frames;
    }
    DestroyReady(false);
}

ExecutionResult BehaviorRuntime::Execute(std::uint64_t token, int input, bool activateInput,
                                         const CKBehaviorContext *frame) {
    Record *record = FindRecord(token);
    CKBehavior *behavior = record ? ResolveBehavior(*record) : nullptr;
    if (!record || !behavior)
        return {Failure(BehaviorError::InvalidState, "Behavior instance has expired."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->GraphResident)
        return {Failure(BehaviorError::InvalidState,
                        "Graph-resident Building Blocks must be executed by their parent graph."),
                ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    if (record->Running)
        return {Failure(BehaviorError::InvalidState, "Behavior instance execution is reentrant."),
                ExecutionState::Failed, CKBR_LOCKED, {}};

    if (activateInput) {
        behavior->Activate(TRUE, TRUE);
        for (int i = 0; i < behavior->GetInputCount(); ++i)
            behavior->ActivateInput(i, i == input ? TRUE : FALSE);
    }

    record->Running = true;
    ++record->LayoutGeneration;
    const int returnCode = ExecuteNative(behavior, frame);
    record = FindRecord(token);
    if (!record) {
        return {Failure(BehaviorError::InvalidState,
                        "Building Block record disappeared during execution.", CK_OK, returnCode),
                ExecutionState::Failed, returnCode, {}};
    }
    record->Running = false;

    if (record->Expired || ResolveBehavior(*record) != behavior) {
        record->Task = false;
        for (BML_ObjectRef source : record->OwnedSources)
            QueueSourceDestroy(source);
        m_Records.erase(token);
        return {Failure(BehaviorError::InvalidState,
                        "Building Block destroyed itself during execution.", CK_OK, returnCode),
                ExecutionState::Failed, returnCode, {}};
    }

    ExecutionResult result;
    result.ReturnCode = returnCode;
    result.Status.BehaviorResult = returnCode;
    for (int i = 0; i < behavior->GetOutputCount(); ++i) {
        if (behavior->IsOutputActive(i))
            result.ActiveOutputs.push_back(i);
        behavior->ActivateOutput(i, FALSE);
    }
    for (int i = 0; i < behavior->GetInputCount(); ++i)
        behavior->ActivateInput(i, FALSE);

    if (IsExecutionError(returnCode)) {
        result.Status = Failure(BehaviorError::ExecutionFailed,
                                "Building Block execution returned an error.", CK_OK, returnCode);
        result.State = HasContinuation(returnCode)
            ? ExecutionState::Continuing : ExecutionState::Failed;
    } else if (returnCode == CKBR_BREAK) {
        result.State = ExecutionState::Suspended;
    } else if (HasContinuation(returnCode)) {
        result.State = ExecutionState::Continuing;
    } else {
        result.State = ExecutionState::Completed;
    }

    const bool releaseRequested = record->ReleaseRequested;
    const bool forceDestroy = record->ForceDestroy;
    if (releaseRequested) {
        QueueDestroy(*record, forceDestroy);
        m_Records.erase(token);
        if (forceDestroy)
            DestroyReady(true);
    }
    return result;
}

int BehaviorRuntime::ExecuteNative(CKBehavior *behavior, const CKBehaviorContext *frame) const {
    if (!behavior || !m_Context)
        return CKBR_BEHAVIORERROR;
    CKBehaviorContext savedContext = m_Context->m_BehaviorContext;
    CKBehaviorManager *manager = m_Context->GetBehaviorManager();
    CKBehavior *savedCurrent = manager ? manager->m_CurrentBehavior : nullptr;
    if (frame)
        m_Context->m_BehaviorContext = *frame;
    m_Context->m_BehaviorContext.Context = m_Context;
    m_Context->m_BehaviorContext.Behavior = behavior;
    const int result = behavior->Execute(m_Context->m_BehaviorContext.DeltaTime);
    m_Context->m_BehaviorContext = savedContext;
    if (manager)
        manager->m_CurrentBehavior = savedCurrent;
    return result;
}

BehaviorRuntime::Record *BehaviorRuntime::FindRecord(const BehaviorInstance &instance) {
    if (instance.m_Runtime != this || !instance.m_Token)
        return nullptr;
    return FindRecord(instance.m_Token);
}

const BehaviorRuntime::Record *BehaviorRuntime::FindRecord(const BehaviorInstance &instance) const {
    if (instance.m_Runtime != this || !instance.m_Token)
        return nullptr;
    return FindRecord(instance.m_Token);
}

BehaviorRuntime::Record *BehaviorRuntime::FindRecord(std::uint64_t token) {
    auto it = m_Records.find(token);
    return it == m_Records.end() ? nullptr : &it->second;
}

const BehaviorRuntime::Record *BehaviorRuntime::FindRecord(std::uint64_t token) const {
    auto it = m_Records.find(token);
    return it == m_Records.end() ? nullptr : &it->second;
}

CKBehavior *BehaviorRuntime::ResolveBehavior(const Record &record) const {
    CKObject *object = m_Identities ? m_Identities->Resolve(record.Behavior) : nullptr;
    return object && CKIsChildClassOf(object, CKCID_BEHAVIOR)
        ? static_cast<CKBehavior *>(object) : nullptr;
}

void BehaviorRuntime::RequestRelease(std::uint64_t token) {
    if (!token)
        return;
    if (m_Thread == std::this_thread::get_id()) {
        Release(token);
        return;
    }
    std::lock_guard<std::mutex> lock(m_DeferredMutex);
    m_DeferredReleases.push_back(token);
}

void BehaviorRuntime::DrainDeferredReleases() {
    std::vector<std::uint64_t> releases;
    {
        std::lock_guard<std::mutex> lock(m_DeferredMutex);
        releases.swap(m_DeferredReleases);
    }
    for (std::uint64_t token : releases)
        Release(token);
}

void BehaviorRuntime::Release(std::uint64_t token) {
    auto it = m_Records.find(token);
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

void BehaviorRuntime::QueueDestroy(Record &record, bool reset) {
    record.Task = false;
    CKBehavior *behavior = ResolveBehavior(record);
    if (behavior)
        behavior->Activate(FALSE, FALSE);
    if (behavior && record.GraphResident) {
        CKObject *parentObject = m_Identities->Resolve(record.Parent);
        if (parentObject && CKIsChildClassOf(parentObject, CKCID_BEHAVIOR))
            static_cast<CKBehavior *>(parentObject)->RemoveSubBehavior(behavior);
    }
    PendingDestroy pending;
    pending.Behavior = record.Behavior;
    pending.Parent = record.Parent;
    pending.Sources = std::move(record.OwnedSources);
    pending.GraphResident = false;
    pending.Created = record.Created;
    pending.Attached = record.Attached;
    pending.Reset = reset;
    m_PendingDestroy.push_back(std::move(pending));
    record.Created = false;
    record.Attached = false;
}

void BehaviorRuntime::QueueSourceDestroy(BML_ObjectRef source, int frames) {
    if (source.Slot == 0)
        return;
    PendingDestroy pending;
    pending.Sources.push_back(source);
    pending.Frames = frames;
    m_PendingDestroy.push_back(std::move(pending));
}

void BehaviorRuntime::DestroyReady(bool force) {
    if (m_Destroying) {
        m_ForceDestroyPending = m_ForceDestroyPending || force;
        return;
    }
    m_Destroying = true;
    force = force || std::exchange(m_ForceDestroyPending, false);
    auto it = m_PendingDestroy.begin();
    while (it != m_PendingDestroy.end()) {
        if (!force && it->Frames > 0) {
            ++it;
            continue;
        }
        CKBehavior *behavior = nullptr;
        std::vector<BML_ObjectRef> sources = std::move(it->Sources);
        it->Sources.clear();
        if (CKObject *object = m_Identities ? m_Identities->Resolve(it->Behavior) : nullptr) {
            if (CKIsChildClassOf(object, CKCID_BEHAVIOR))
                behavior = static_cast<CKBehavior *>(object);
        }
        if (behavior) {
            behavior->Activate(FALSE, FALSE);
            if (it->Reset)
                (void) CallCallback(behavior, CKM_BEHAVIORRESET, nullptr);
            behavior = static_cast<CKBehavior *>(m_Identities->Resolve(it->Behavior));
            if (behavior && it->Attached)
                (void) CallCallback(behavior, CKM_BEHAVIORDETACH, nullptr);
            behavior = static_cast<CKBehavior *>(m_Identities->Resolve(it->Behavior));
            if (behavior && it->Created)
                (void) CallCallback(behavior, CKM_BEHAVIORDELETE, nullptr);
            behavior = static_cast<CKBehavior *>(m_Identities->Resolve(it->Behavior));
            if (behavior && it->GraphResident) {
                CKObject *parentObject = m_Identities->Resolve(it->Parent);
                if (parentObject && CKIsChildClassOf(parentObject, CKCID_BEHAVIOR))
                    static_cast<CKBehavior *>(parentObject)->RemoveSubBehavior(behavior);
            }
            behavior = static_cast<CKBehavior *>(m_Identities->Resolve(it->Behavior));
            if (behavior) {
                behavior->SetOwner(nullptr, FALSE);
                m_Context->DestroyObject(behavior);
            }
        }
        for (BML_ObjectRef source : sources) {
            if (CKObject *object = m_Identities ? m_Identities->Resolve(source) : nullptr)
                m_Context->DestroyObject(object);
        }
        it = m_PendingDestroy.erase(it);
    }
    m_Destroying = false;
    if (m_ForceDestroyPending)
        DestroyReady(true);
}

void BehaviorRuntime::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0)
        return;
    if (!ReadyStatus())
        return;
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (ContainsId(ids, count, pending.Behavior)) {
            pending.Behavior = {};
            pending.Created = false;
            pending.Attached = false;
            pending.GraphResident = false;
        }
        pending.Sources.erase(
            std::remove_if(pending.Sources.begin(), pending.Sources.end(),
                           [&](BML_ObjectRef source) { return ContainsId(ids, count, source); }),
            pending.Sources.end());
    }
    for (auto it = m_Records.begin(); it != m_Records.end();) {
        const bool deleting = ContainsId(ids, count, it->second.Behavior) ||
                              ContainsId(ids, count, it->second.Parent);
        if (!deleting) {
            it->second.OwnedSources.erase(
                std::remove_if(it->second.OwnedSources.begin(), it->second.OwnedSources.end(),
                               [&](BML_ObjectRef source) { return ContainsId(ids, count, source); }),
                it->second.OwnedSources.end());
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
        for (BML_ObjectRef source : it->second.OwnedSources) {
            if (!ContainsId(ids, count, source))
                QueueSourceDestroy(source);
        }
        it = m_Records.erase(it);
    }
}

void BehaviorRuntime::ResetWorld() {
    if (!m_Context || m_Thread != std::this_thread::get_id())
        return;
    DrainDeferredReleases();
    for (PendingDestroy &pending : m_PendingDestroy) {
        if (pending.Behavior.Slot != 0)
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
    DestroyReady(true);
}

const char *DescribeBehaviorError(BehaviorError error) {
    switch (error) {
    case BehaviorError::None: return "none";
    case BehaviorError::WrongThread: return "wrong thread";
    case BehaviorError::ContextExpired: return "context expired";
    case BehaviorError::PrototypeNotFound: return "prototype not found";
    case BehaviorError::RequiredManagerMissing: return "required manager missing";
    case BehaviorError::CreateFailed: return "creation failed";
    case BehaviorError::InitFailed: return "initialization failed";
    case BehaviorError::OwnerInvalid: return "invalid owner";
    case BehaviorError::TargetInvalid: return "invalid target";
    case BehaviorError::CallbackFailed: return "callback failed";
    case BehaviorError::SlotNotFound: return "slot not found";
    case BehaviorError::AmbiguousSlot: return "ambiguous slot";
    case BehaviorError::StaleLayout: return "stale layout";
    case BehaviorError::TypeMismatch: return "type mismatch";
    case BehaviorError::ValueWriteFailed: return "value write failed";
    case BehaviorError::SourceInvalid: return "invalid source";
    case BehaviorError::InvalidState: return "invalid state";
    case BehaviorError::ExecutionFailed: return "execution failed";
    }
    return "unknown";
}

} // namespace BML::Virtools
