#include "Behavior/Layout.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "Behavior/Status.h"

namespace BML::Behavior::Internal {
namespace {

constexpr std::uint64_t kIdentityOffset = 1469598103934665603ull;
constexpr std::uint64_t kIdentityPrime = 1099511628211ull;

constexpr CKDWORD kLayoutFlags =
    static_cast<CKDWORD>(CKBEHAVIOR_TARGETABLE) |
    static_cast<CKDWORD>(CKBEHAVIOR_VARIABLEINPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_VARIABLEOUTPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_VARIABLEPARAMETERINPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDINPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS);

constexpr CKDWORD kExecuteLayoutFlags =
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDINPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS) |
    static_cast<CKDWORD>(CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS);

void IdentityBytes(std::uint64_t &identity, const void *data,
                   std::size_t size) noexcept {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index) {
        identity ^= bytes[index];
        identity *= kIdentityPrime;
    }
}

template <class T>
void IdentityPart(std::uint64_t &identity, const T &value) noexcept {
    IdentityBytes(identity, &value, sizeof(value));
}

void IdentityText(std::uint64_t &identity, const char *text) noexcept {
    const std::size_t size = text ? std::strlen(text) : 0;
    IdentityPart(identity, size);
    if (size)
        IdentityBytes(identity, text, size);
}

void IdentityObject(std::uint64_t &identity, CKObject *object) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    const CK_ID id = object ? object->GetID() : 0;
    IdentityPart(identity, address);
    IdentityPart(identity, id);
    IdentityText(identity, object ? object->GetName() : nullptr);
}

template <class Parameter>
void IdentityParameter(std::uint64_t &identity,
                       Parameter *parameter) noexcept {
    IdentityObject(identity, parameter);
    const CKGUID type = parameter ? parameter->GetGUID() : CKGUID();
    IdentityPart(identity, type.d1);
    IdentityPart(identity, type.d2);
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

const char *SafeName(CKObject *object, const char *fallback = "") {
    return object && object->GetName() ? object->GetName() : fallback;
}

bool IsSetting(CKBehavior *behavior, CKBehaviorPrototype *prototype,
               int nativeIndex) {
    bool setting = behavior &&
        behavior->IsLocalParameterSetting(nativeIndex) != FALSE;
    if (prototype && nativeIndex < prototype->GetLocalParameterCount()) {
        CKPARAMETER_DESC **locals = prototype->GetLocalParameterList();
        setting = locals && locals[nativeIndex] &&
            locals[nativeIndex]->Type == 3;
    }
    return setting;
}

bool DynamicSlot(CKDWORD flags, SlotKind kind) {
    switch (kind) {
    case SlotKind::Input:
        return (flags & (CKBEHAVIOR_VARIABLEINPUTS |
                         CKBEHAVIOR_INTERNALLYCREATEDINPUTS)) != 0;
    case SlotKind::Output:
        return (flags & (CKBEHAVIOR_VARIABLEOUTPUTS |
                         CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS)) != 0;
    case SlotKind::InputParameter:
        return (flags & (CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
                         CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS)) != 0;
    case SlotKind::OutputParameter:
        return (flags & (CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS |
                         CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS)) != 0;
    case SlotKind::Setting:
    case SlotKind::Local:
        return (flags & CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS) != 0;
    case SlotKind::Target:
        return false;
    }
    return false;
}

int DefaultSize(CKContext *context, CKGUID type) {
    if (!context || !type.IsValid())
        return 0;
    CKParameterTypeDesc *description = context->GetParameterManager()
        ? context->GetParameterManager()->GetParameterTypeDescription(
              type)
        : nullptr;
    return description ? description->DefaultSize : 0;
}

SlotInfo MakeSlot(CKContext *context, CKBehavior *behavior, SlotKind kind,
                  int index, int nativeIndex) {
    SlotInfo slot;
    slot.Kind = kind;
    slot.Index = index;
    slot.NativeIndex = nativeIndex;
    slot.Dynamic = behavior && DynamicSlot(behavior->GetFlags(), kind);
    if (!behavior)
        return slot;

    switch (kind) {
    case SlotKind::Input: {
        CKBehaviorIO *io = behavior->GetInput(nativeIndex);
        slot.Name = SafeName(io);
        break;
    }
    case SlotKind::Output: {
        CKBehaviorIO *io = behavior->GetOutput(nativeIndex);
        slot.Name = SafeName(io);
        break;
    }
    case SlotKind::InputParameter: {
        CKParameterIn *parameter = behavior->GetInputParameter(nativeIndex);
        slot.Name = SafeName(parameter);
        if (parameter) {
            slot.Type = parameter->GetGUID();
            CKParameter *source = parameter->GetRealSource();
            slot.DataSize = source ? source->GetDataSize()
                                   : DefaultSize(context, parameter->GetGUID());
        }
        break;
    }
    case SlotKind::OutputParameter: {
        CKParameterOut *parameter = behavior->GetOutputParameter(nativeIndex);
        slot.Name = SafeName(parameter);
        if (parameter) {
            slot.Type = parameter->GetGUID();
            slot.DataSize = parameter->GetDataSize();
        }
        break;
    }
    case SlotKind::Setting:
    case SlotKind::Local: {
        CKParameterLocal *parameter = behavior->GetLocalParameter(nativeIndex);
        slot.Name = SafeName(parameter);
        if (parameter) {
            slot.Type = parameter->GetGUID();
            slot.DataSize = parameter->GetDataSize();
        }
        break;
    }
    case SlotKind::Target: {
        CKParameterIn *parameter = behavior->GetTargetParameter();
        slot.Name = SafeName(parameter, "Target");
        if (parameter) {
            slot.Type = parameter->GetGUID();
            CKParameter *source = parameter->GetRealSource();
            slot.DataSize = source ? source->GetDataSize()
                                   : DefaultSize(context, parameter->GetGUID());
        }
        break;
    }
    }
    return slot;
}

bool SlotAt(CKContext *context, CKBehavior *behavior,
            CKBehaviorPrototype *prototype, SlotKind kind, int index,
            SlotInfo &slot) {
    if (!behavior || index < 0)
        return false;
    int native = index;
    switch (kind) {
    case SlotKind::Input:
        if (index >= behavior->GetInputCount()) return false;
        break;
    case SlotKind::Output:
        if (index >= behavior->GetOutputCount()) return false;
        break;
    case SlotKind::InputParameter:
        if (index >= behavior->GetInputParameterCount()) return false;
        break;
    case SlotKind::OutputParameter:
        if (index >= behavior->GetOutputParameterCount()) return false;
        break;
    case SlotKind::Setting: {
        int current = 0;
        native = -1;
        for (int candidate = 0;
             candidate < behavior->GetLocalParameterCount(); ++candidate) {
            if (IsSetting(behavior, prototype, candidate) &&
                current++ == index) {
                native = candidate;
                break;
            }
        }
        if (native < 0) return false;
        break;
    }
    case SlotKind::Local:
        if (index >= behavior->GetLocalParameterCount() ||
            IsSetting(behavior, prototype, index))
            return false;
        break;
    case SlotKind::Target:
        if (index != 0 || !behavior->GetTargetParameter()) return false;
        break;
    }
    slot = MakeSlot(context, behavior, kind, index, native);
    return true;
}

template <typename Visitor>
void VisitSlots(CKContext *context, CKBehavior *behavior,
                CKBehaviorPrototype *prototype, SlotKind kind,
                Visitor &&visitor) {
    if (!behavior)
        return;
    switch (kind) {
    case SlotKind::Input:
        for (int index = 0; index < behavior->GetInputCount(); ++index)
            visitor(MakeSlot(context, behavior, kind, index, index));
        break;
    case SlotKind::Output:
        for (int index = 0; index < behavior->GetOutputCount(); ++index)
            visitor(MakeSlot(context, behavior, kind, index, index));
        break;
    case SlotKind::InputParameter:
        for (int index = 0; index < behavior->GetInputParameterCount(); ++index)
            visitor(MakeSlot(context, behavior, kind, index, index));
        break;
    case SlotKind::OutputParameter:
        for (int index = 0; index < behavior->GetOutputParameterCount(); ++index)
            visitor(MakeSlot(context, behavior, kind, index, index));
        break;
    case SlotKind::Setting: {
        int setting = 0;
        for (int native = 0; native < behavior->GetLocalParameterCount(); ++native) {
            if (IsSetting(behavior, prototype, native))
                visitor(MakeSlot(context, behavior, kind, setting++, native));
        }
        break;
    }
    case SlotKind::Local:
        for (int native = 0; native < behavior->GetLocalParameterCount(); ++native) {
            if (!IsSetting(behavior, prototype, native))
                visitor(MakeSlot(context, behavior, kind, native, native));
        }
        break;
    case SlotKind::Target:
        if (behavior->GetTargetParameter())
            visitor(MakeSlot(context, behavior, kind, 0, 0));
        break;
    }
}

void DescribeType(CKParameterManager *manager, SlotInfo &slot) {
    if (!slot.Type.IsValid())
        return;
    const Parameter::Type type = Parameter::Describe(manager, slot.Type);
    slot.TypeName = type.Name;
    slot.ValueForm = type.ValueForm;
}

std::string CandidateList(const std::vector<SlotInfo> &slots) {
    std::ostringstream stream;
    bool first = true;
    for (const SlotInfo &slot : slots) {
        stream << (first ? " Available: " : ", ") << "[" << slot.Index
               << "] '" << slot.Name << "'";
        if (slot.Type.IsValid())
            stream << " " << GuidText(slot.Type);
        first = false;
    }
    if (first)
        stream << " No slots of this kind are present.";
    return stream.str();
}

Status SlotFailure(Error error, std::string message, CKGUID prototype,
                   const Slot &selector, CKGUID actualType = CKGUID()) {
    Status status{error, CK_OK, CKBR_OK, std::move(message)};
    status.Details.Stage = Phase::ParameterBinding;
    status.Details.Prototype = prototype;
    status.Details.Selector = selector;
    status.Details.ActualType = actualType;
    return status;
}

} // namespace

std::uint64_t LayoutIdentity(CKBehavior *behavior) noexcept {
    std::uint64_t identity = kIdentityOffset;
    if (!behavior)
        return identity;

    const CKGUID prototype = behavior->GetPrototypeGuid();
    IdentityPart(identity, prototype.d1);
    IdentityPart(identity, prototype.d2);
    IdentityPart(identity, behavior->GetCompatibleClassID());
    const CKDWORD layoutFlags =
        static_cast<CKDWORD>(behavior->GetFlags()) & kLayoutFlags;
    IdentityPart(identity, layoutFlags);
    const CKBOOL function = behavior->IsUsingFunction();
    IdentityPart(identity, function);

    IdentityParameter(identity, behavior->GetTargetParameter());

    IdentityPart(identity, behavior->GetInputCount());
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        IdentityObject(identity, behavior->GetInput(index));

    IdentityPart(identity, behavior->GetOutputCount());
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        IdentityObject(identity, behavior->GetOutput(index));

    IdentityPart(identity, behavior->GetInputParameterCount());
    for (int index = 0; index < behavior->GetInputParameterCount(); ++index)
        IdentityParameter(identity, behavior->GetInputParameter(index));

    IdentityPart(identity, behavior->GetOutputParameterCount());
    for (int index = 0; index < behavior->GetOutputParameterCount(); ++index)
        IdentityParameter(identity, behavior->GetOutputParameter(index));

    IdentityPart(identity, behavior->GetLocalParameterCount());
    for (int index = 0; index < behavior->GetLocalParameterCount(); ++index) {
        IdentityParameter(identity, behavior->GetLocalParameter(index));
        const CKBOOL setting = behavior->IsLocalParameterSetting(index);
        IdentityPart(identity, setting);
    }
    return identity;
}

bool IsLayoutDynamic(CKBehavior *behavior) noexcept {
    return behavior &&
        (static_cast<CKDWORD>(behavior->GetFlags()) & kExecuteLayoutFlags) != 0;
}

Layout LiveLayout::Describe(std::uint64_t generation,
                            const Layout *declared) const {
    Layout layout;
    if (!m_Context || !m_Behavior)
        return layout;

    layout.Prototype = m_Prototype;
    layout.Origin = LayoutOrigin::Live;
    if (!m_Behavior->IsUsingFunction())
        layout.Kind = BehaviorKind::Graph;
    else if (m_Declaration && !m_Declaration->GetFunction())
        layout.Kind = BehaviorKind::Callback;
    else
        layout.Kind = BehaviorKind::Function;
    if (m_Declaration) {
        layout.PrototypeName = m_Declaration->GetName()
            ? m_Declaration->GetName() : "";
        layout.PrototypeFlags = m_Declaration->GetFlags();
    }
    if (CKObjectDeclaration *object =
            CKGetObjectDeclarationFromGuid(m_Prototype)) {
        layout.Category = object->GetCategory() ? object->GetCategory() : "";
        for (int index = 0; index < object->GetManagerNeededCount(); ++index)
            layout.RequiredManagers.push_back(object->GetManagerNeeded(index));
    }
    layout.CompatibleClass = m_Behavior->GetCompatibleClassID();
    layout.BehaviorFlags = m_Behavior->GetFlags();
    layout.Generation = generation;

    constexpr SlotKind kinds[] = {
        SlotKind::Input, SlotKind::Output, SlotKind::InputParameter,
        SlotKind::OutputParameter, SlotKind::Setting, SlotKind::Local,
        SlotKind::Target,
    };
    CKParameterManager *parameters = m_Context->GetParameterManager();
    for (SlotKind kind : kinds) {
        std::unordered_map<std::string, int> occurrences;
        VisitSlots(m_Context, m_Behavior, m_Declaration, kind,
                   [&](SlotInfo slot) {
                       slot.Occurrence = occurrences[slot.Name]++;
                       DescribeType(parameters, slot);
                       if (kind == SlotKind::Target)
                           layout.TargetType = slot.Type;
                       layout.Slots.push_back(std::move(slot));
                   });
    }

    if (declared) {
        layout.ProviderGeneration = declared->ProviderGeneration;
        layout.Provider = declared->Provider;
        layout.ProviderName = declared->ProviderName;
        layout.Author = declared->Author;
        layout.Description = declared->Description;
        layout.Version = declared->Version;
        if (!layout.TargetType.IsValid())
            layout.TargetType = declared->TargetType;
        layout.Managers = declared->Managers;
    }
    return layout;
}

Status LiveLayout::Resolve(const Slot &selector, SlotInfo &slot) const {
    slot = SlotInfo();
    if (!m_Context || !m_Behavior) {
        return SlotFailure(Error::InvalidState,
                           "Behavior no longer exists.", m_Prototype,
                           selector);
    }

    auto fail = [&](Error error, std::string message,
                    CKGUID actualType = CKGUID()) {
        return SlotFailure(error, std::move(message), m_Prototype,
                           selector, actualType);
    };
    auto finish = [&](SlotInfo candidate) {
        DescribeType(m_Context->GetParameterManager(), candidate);
        if (selector.ExpectedType.IsValid() && candidate.Type.IsValid() &&
            !Parameter::Compatible(m_Context->GetParameterManager(),
                                   candidate.Type,
                                   selector.ExpectedType)) {
            return fail(
                Error::TypeMismatch,
                "Resolved " + std::string(SlotKindName(selector.Kind)) +
                    " '" + candidate.Name + "' has type " +
                    GuidText(candidate.Type) +
                    ", incompatible with expected " +
                    GuidText(selector.ExpectedType) + ".",
                candidate.Type);
        }
        slot = std::move(candidate);
        return Status{};
    };

    // Index selectors are the common hot path. They do not need names,
    // occurrence lists, or a complete Layout to reach the native slot.
    if (!selector.UsesName() && !selector.RequireOnly) {
        SlotInfo indexed;
        if (SlotAt(m_Context, m_Behavior, m_Declaration, selector.Kind,
                   selector.Index, indexed))
            return finish(std::move(indexed));
    }

    std::vector<SlotInfo> candidates;
    std::vector<std::size_t> matches;
    std::unordered_map<std::string, int> occurrences;
    VisitSlots(m_Context, m_Behavior, m_Declaration, selector.Kind,
               [&](SlotInfo candidate) {
                   candidate.Occurrence = occurrences[candidate.Name]++;
                   const bool match = selector.UsesName()
                       ? candidate.Name == selector.Name
                       : selector.RequireOnly || candidate.Index == selector.Index;
                   candidates.push_back(std::move(candidate));
                   if (match)
                       matches.push_back(candidates.size() - 1);
               });
    if (matches.empty()) {
        std::ostringstream message;
        message << SlotKindName(selector.Kind) << " "
                << (selector.RequireOnly
                    ? "<only>"
                    : selector.UsesName()
                        ? "'" + selector.Name + "'"
                        : "#" + std::to_string(selector.Index))
                << " was not found on Building Block '"
                << SafeName(m_Behavior, "<unnamed>") << "'."
                << CandidateList(candidates);
        return fail(Error::SlotNotFound, message.str());
    }
    if (selector.RequireOnly && matches.size() != 1) {
        return fail(
            Error::AmbiguousSlot,
            "Expected exactly one " + std::string(SlotKindName(selector.Kind)) +
                ", but the configured Layout contains " +
                std::to_string(matches.size()) + "." +
                CandidateList(candidates));
    }
    if (selector.UsesName() && selector.RequireUnique && matches.size() != 1) {
        return fail(
            Error::AmbiguousSlot,
            "Slot name '" + selector.Name + "' matched " +
                std::to_string(matches.size()) +
                " candidates; select an explicit occurrence." +
                CandidateList(candidates));
    }
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 || occurrence >= static_cast<int>(matches.size())) {
        return fail(
            Error::SlotNotFound,
            "Requested occurrence " + std::to_string(occurrence) + " of " +
                SlotKindName(selector.Kind) + " '" + selector.Name +
                "' does not exist; " + std::to_string(matches.size()) +
                " candidate(s) matched." + CandidateList(candidates));
    }

    return finish(std::move(
        candidates[matches[static_cast<std::size_t>(occurrence)]]));
}

CKObject *LiveLayout::Object(const SlotInfo &slot) const {
    if (!m_Behavior)
        return nullptr;
    switch (slot.Kind) {
    case SlotKind::Input:
        return m_Behavior->GetInput(slot.NativeIndex);
    case SlotKind::Output:
        return m_Behavior->GetOutput(slot.NativeIndex);
    case SlotKind::InputParameter:
        return m_Behavior->GetInputParameter(slot.NativeIndex);
    case SlotKind::OutputParameter:
        return m_Behavior->GetOutputParameter(slot.NativeIndex);
    case SlotKind::Setting:
    case SlotKind::Local:
        return m_Behavior->GetLocalParameter(slot.NativeIndex);
    case SlotKind::Target:
        return m_Behavior->GetTargetParameter();
    }
    return nullptr;
}

CKParameter *LiveLayout::Parameter(const SlotInfo &slot) const {
    if (!m_Behavior)
        return nullptr;
    switch (slot.Kind) {
    case SlotKind::InputParameter: {
        CKParameterIn *input = m_Behavior->GetInputParameter(slot.NativeIndex);
        return input ? input->GetRealSource() : nullptr;
    }
    case SlotKind::OutputParameter:
        return m_Behavior->GetOutputParameter(slot.NativeIndex);
    case SlotKind::Setting:
    case SlotKind::Local:
        return m_Behavior->GetLocalParameter(slot.NativeIndex);
    case SlotKind::Target: {
        CKParameterIn *target = m_Behavior->GetTargetParameter();
        return target ? target->GetRealSource() : nullptr;
    }
    case SlotKind::Input:
    case SlotKind::Output:
        return nullptr;
    }
    return nullptr;
}

} // namespace BML::Behavior::Internal
