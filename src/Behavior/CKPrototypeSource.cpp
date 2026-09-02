#include "Behavior/PrototypeCatalog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

namespace BML::Behavior {
namespace {

using RemoveDeclaration = CKERROR(__cdecl *)(CKObjectDeclaration *);

constexpr std::size_t kRetirementCapacity = 64;
std::array<std::atomic<std::uint64_t>, kRetirementCapacity> g_Retirements{};
std::atomic<bool> g_RetirementOverflow{false};
RemoveDeclaration g_RemoveDeclaration = nullptr;
void *g_RemoveTarget = nullptr;

std::uint64_t EncodeGuid(CKGUID guid) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(guid.d1)) << 32u) |
           static_cast<std::uint32_t>(guid.d2);
}

CKGUID DecodeGuid(std::uint64_t value) noexcept {
    return CKGUID(static_cast<int>(value >> 32u),
                  static_cast<int>(value & 0xffffffffu));
}

void RecordRetirement(CKGUID guid) noexcept {
    const std::uint64_t encoded = EncodeGuid(guid);
    if (!encoded) {
        g_RetirementOverflow.store(true, std::memory_order_release);
        return;
    }
    for (auto &slot : g_Retirements) {
        std::uint64_t empty = 0;
        if (slot.compare_exchange_strong(empty, encoded,
                                         std::memory_order_release,
                                         std::memory_order_relaxed))
            return;
    }
    g_RetirementOverflow.store(true, std::memory_order_release);
}

CKERROR __cdecl RemoveDeclarationHook(CKObjectDeclaration *declaration) {
    const CKGUID guid = declaration ? declaration->GetGuid() : CKGUID();
    const CKERROR result = g_RemoveDeclaration
        ? g_RemoveDeclaration(declaration) : CKERR_INVALIDOBJECT;
    if (result == CK_OK)
        RecordRetirement(guid);
    return result;
}

std::string Safe(CKSTRING text) {
    return text ? text : "";
}

std::string GuidText(CKGUID guid) {
    std::ostringstream out;
    out << std::hex << guid.d1 << ':' << guid.d2;
    return out.str();
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

Status Failure(Error error, std::string message, CKGUID prototype = CKGUID(),
               CKERROR ckError = CKERR_INVALIDPARAMETER) {
    Status status{error, ckError, CKBR_PARAMETERERROR, std::move(message)};
    status.Details.Stage = Phase::PrototypeResolution;
    status.Details.Prototype = prototype;
    return status;
}

void AddSlot(Layout &layout, SlotInfo slot) {
    slot.Occurrence = 0;
    for (const SlotInfo &candidate : layout.Slots) {
        if (candidate.Kind == slot.Kind && candidate.Name == slot.Name)
            ++slot.Occurrence;
    }
    layout.Slots.push_back(std::move(slot));
}

bool HasFlag(CKDWORD value, CKDWORD flag) noexcept {
    return (value & flag) != 0;
}

class CKPrototypeSource final : public PrototypeSource {
public:
    explicit CKPrototypeSource(CKContext *context) : m_Context(context) {
        HMODULE ck2 = ::GetModuleHandleA("CK2.dll");
        g_RemoveTarget = ck2 ? reinterpret_cast<void *>(::GetProcAddress(
            ck2, "?CKRemovePrototypeDeclaration@@YAJPAVCKObjectDeclaration@@@Z"))
                             : nullptr;
        if (!g_RemoveTarget)
            return;
        const MH_STATUS created = MH_CreateHook(
            g_RemoveTarget, reinterpret_cast<void *>(&RemoveDeclarationHook),
            reinterpret_cast<void **>(&g_RemoveDeclaration));
        if (created != MH_OK)
            return;
        if (MH_EnableHook(g_RemoveTarget) != MH_OK) {
            MH_RemoveHook(g_RemoveTarget);
            g_RemoveDeclaration = nullptr;
            g_RemoveTarget = nullptr;
            return;
        }
        m_TracksRetirement = true;
    }

    ~CKPrototypeSource() override {
        if (m_TracksRetirement && g_RemoveTarget) {
            MH_DisableHook(g_RemoveTarget);
            MH_RemoveHook(g_RemoveTarget);
        }
        g_RemoveDeclaration = nullptr;
        g_RemoveTarget = nullptr;
    }

    Status ReadDeclarations(std::vector<PrototypeInfo> &out) override {
        out.clear();
        if (!m_Context)
            return Failure(Error::ContextExpired,
                           "Virtools context is unavailable.");
        CKPluginManager *plugins = CKGetPluginManager();
        const int behaviorCategory = plugins
            ? plugins->GetCategoryIndex(CKPLUGIN_BEHAVIOR) : -1;

        const int count = CKGetPrototypeDeclarationCount();
        if (count < 0)
            return Failure(Error::InvalidState,
                           "Virtools returned an invalid Prototype declaration count.");
        out.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            CKObjectDeclaration *declaration = CKGetPrototypeDeclaration(index);
            if (!declaration)
                continue;

            PrototypeInfo prototype;
            prototype.Ref.Guid = declaration->GetGuid();
            prototype.Name = Safe(declaration->GetName());
            prototype.Category = Safe(declaration->GetCategory());
            prototype.Author = Safe(declaration->GetAuthorName());
            prototype.Description = Safe(declaration->GetDescription());
            prototype.Version = declaration->GetVersion();
            prototype.CompatibleClass = declaration->GetCompatibleClassId();
            for (int manager = 0;
                 manager < declaration->GetManagerNeededCount(); ++manager) {
                const CKGUID guid = declaration->GetManagerNeeded(manager);
                prototype.Managers.push_back(
                    {guid, m_Context->GetManagerByGuid(guid) != nullptr});
            }

            CKPluginEntry *entry = behaviorCategory >= 0 && plugins
                ? plugins->GetPluginInfo(behaviorCategory,
                                         declaration->GetPluginIndex())
                : nullptr;
            CKPluginDll *dll = entry && plugins
                ? plugins->GetPluginDllInfo(entry->m_PluginDllIndex) : nullptr;
            if (entry) {
                prototype.Provider.Guid = entry->m_PluginInfo.m_GUID;
                prototype.Provider.Author =
                    entry->m_PluginInfo.m_Author.CStr();
                prototype.Provider.Description =
                    entry->m_PluginInfo.m_Description.CStr();
            }
            if (dll) {
                prototype.Provider.Path = dll->m_DllFileName.CStr();
                prototype.Provider.Name = std::filesystem::path(
                    prototype.Provider.Path).stem().string();
            }
            if (prototype.Provider.Name.empty())
                prototype.Provider.Name = prototype.Author;
            prototype.Provider.Key =
                Lower(prototype.Provider.Path) + '|' +
                GuidText(prototype.Provider.Guid);
            if (prototype.Provider.Key == "|0:0")
                prototype.Provider.Key = "prototype|" +
                    GuidText(prototype.Ref.Guid);
            out.push_back(std::move(prototype));
        }
        return {};
    }

    Status PrototypeCount(std::size_t &out) const override {
        out = 0;
        if (!m_Context)
            return Failure(Error::ContextExpired,
                           "Virtools context is unavailable.");
        const int count = CKGetPrototypeDeclarationCount();
        if (count < 0)
            return Failure(
                Error::InvalidState,
                "Virtools returned an invalid Prototype declaration count.");
        out = static_cast<std::size_t>(count);
        return {};
    }

    Status ReadDeclaredLayout(CKGUID guid, Layout &layout) override {
        layout = {};
        if (!m_Context)
            return Failure(Error::ContextExpired,
                           "Virtools context is unavailable.", guid);
        CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(guid);
        if (!prototype)
            return Failure(Error::PrototypeLoadFailed,
                           "The Building Block Prototype factory failed.", guid);

        CKParameterManager *parameters = m_Context->GetParameterManager();
        layout.Origin = LayoutOrigin::Declared;
        layout.Prototype = guid;
        layout.PrototypeName = Safe(prototype->GetName());
        layout.PrototypeFlags = prototype->GetFlags();
        layout.BehaviorFlags = prototype->GetBehaviorFlags();
        layout.Kind = prototype->GetFunction()
            ? BehaviorKind::Function : BehaviorKind::Callback;
        layout.CompatibleClass = prototype->GetApplyToClassID();

        const CKDWORD flags = layout.BehaviorFlags;
        CKBEHAVIORIO_DESC **inputs = prototype->GetInIOList();
        for (int index = 0; index < prototype->GetInputCount(); ++index) {
            CKBEHAVIORIO_DESC *description = inputs ? inputs[index] : nullptr;
            SlotInfo slot{SlotKind::Input, index, index,
                          Safe(description ? description->Name : nullptr),
                          CKGUID(), 0};
            slot.Dynamic = HasFlag(flags, CKBEHAVIOR_VARIABLEINPUTS) ||
                           HasFlag(flags, CKBEHAVIOR_INTERNALLYCREATEDINPUTS);
            AddSlot(layout, std::move(slot));
        }
        CKBEHAVIORIO_DESC **outputs = prototype->GetOutIOList();
        for (int index = 0; index < prototype->GetOutputCount(); ++index) {
            CKBEHAVIORIO_DESC *description = outputs ? outputs[index] : nullptr;
            SlotInfo slot{SlotKind::Output, index, index,
                          Safe(description ? description->Name : nullptr),
                          CKGUID(), 0};
            slot.Dynamic = HasFlag(flags, CKBEHAVIOR_VARIABLEOUTPUTS) ||
                           HasFlag(flags, CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS);
            AddSlot(layout, std::move(slot));
        }

        CKPARAMETER_DESC **pins = prototype->GetInParameterList();
        for (int index = 0; index < prototype->GetInParameterCount(); ++index) {
            CKPARAMETER_DESC *description = pins ? pins[index] : nullptr;
            const CKGUID type = description ? description->Guid : CKGUID();
            const Parameter::Type parameter = Parameter::Describe(parameters, type);
            SlotInfo slot{SlotKind::InputParameter, index, index,
                          Safe(description ? description->Name : nullptr),
                          type, parameter.Size};
            slot.TypeName = parameter.Name;
            slot.ValueForm = parameter.ValueForm;
            slot.Dynamic = HasFlag(flags, CKBEHAVIOR_VARIABLEPARAMETERINPUTS) ||
                           HasFlag(flags, CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS);
            AddSlot(layout, std::move(slot));
        }
        CKPARAMETER_DESC **pouts = prototype->GetOutParameterList();
        for (int index = 0; index < prototype->GetOutParameterCount(); ++index) {
            CKPARAMETER_DESC *description = pouts ? pouts[index] : nullptr;
            const CKGUID type = description ? description->Guid : CKGUID();
            const Parameter::Type parameter = Parameter::Describe(parameters, type);
            SlotInfo slot{SlotKind::OutputParameter, index, index,
                          Safe(description ? description->Name : nullptr),
                          type, parameter.Size};
            slot.TypeName = parameter.Name;
            slot.ValueForm = parameter.ValueForm;
            slot.Dynamic = HasFlag(flags, CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS) ||
                           HasFlag(flags, CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS);
            AddSlot(layout, std::move(slot));
        }

        int settingIndex = 0;
        CKPARAMETER_DESC **locals = prototype->GetLocalParameterList();
        for (int nativeIndex = 0;
             nativeIndex < prototype->GetLocalParameterCount(); ++nativeIndex) {
            CKPARAMETER_DESC *description = locals ? locals[nativeIndex] : nullptr;
            const bool setting = description &&
                                 description->Type == CKPARAMETER_SETTING;
            const CKGUID type = description ? description->Guid : CKGUID();
            const Parameter::Type parameter = Parameter::Describe(parameters, type);
            SlotInfo slot{setting ? SlotKind::Setting : SlotKind::Local,
                          setting ? settingIndex++ : nativeIndex, nativeIndex,
                          Safe(description ? description->Name : nullptr),
                          type, parameter.Size};
            slot.TypeName = parameter.Name;
            slot.ValueForm = parameter.ValueForm;
            slot.Dynamic = HasFlag(flags, CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS);
            AddSlot(layout, std::move(slot));
        }

        if (HasFlag(flags, CKBEHAVIOR_TARGETABLE)) {
            layout.TargetType = parameters
                ? parameters->ClassIDToGuid(layout.CompatibleClass) : CKGUID();
            const Parameter::Type parameter =
                Parameter::Describe(parameters, layout.TargetType);
            SlotInfo target{SlotKind::Target, 0, 0, "Target",
                            layout.TargetType, parameter.Size};
            target.TypeName = parameter.Name;
            target.ValueForm = parameter.ValueForm;
            AddSlot(layout, std::move(target));
        }
        return {};
    }

    void TakeRetirements(std::vector<CKGUID> &out,
                         bool &retireAll) override {
        out.clear();
        retireAll = g_RetirementOverflow.exchange(false,
                                                   std::memory_order_acq_rel);
        for (auto &slot : g_Retirements) {
            const std::uint64_t encoded = slot.exchange(
                0, std::memory_order_acq_rel);
            if (encoded)
                out.push_back(DecodeGuid(encoded));
        }
    }

    [[nodiscard]] bool TracksRetirement() const noexcept override {
        return m_TracksRetirement;
    }

private:
    CKContext *m_Context = nullptr;
    bool m_TracksRetirement = false;
};

} // namespace

std::unique_ptr<PrototypeSource> MakeCKPrototypeSource(CKContext *context) {
    return std::make_unique<CKPrototypeSource>(context);
}

} // namespace BML::Behavior
