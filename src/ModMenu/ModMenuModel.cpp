#include "ModMenu/ModMenuModel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Config/Config.h"
#include "Loader/ModContext.h"

#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptMod.h"
#endif

namespace {
    const char *SafeText(const char *text) {
        return text ? text : "";
    }

    std::string DisplayLabel(const char *id) {
        const char *label = SafeText(id);
        return label[0] == '@' ? label + 1 : label;
    }

    std::uint32_t ScriptContentGeneration(IMod *mod) {
#if BML_ENABLE_ANGELSCRIPT
        const auto *scriptMod = dynamic_cast<const BML::ScriptMod *>(mod);
        return scriptMod ? scriptMod->GetModGeneration() : 0;
#else
        (void) mod;
        return 0;
#endif
    }

    std::string ScriptStatus(IMod *mod) {
#if BML_ENABLE_ANGELSCRIPT
        const auto *scriptMod = dynamic_cast<const BML::ScriptMod *>(mod);
        if (!scriptMod)
            return {};
        if (scriptMod->IsFailed())
            return "failed";
        return scriptMod->IsLoaded() ? "loaded" : "registered";
#else
        (void) mod;
        return {};
#endif
    }

    std::string ScriptDiagnostic(IMod *mod) {
#if BML_ENABLE_ANGELSCRIPT
        const auto *scriptMod = dynamic_cast<const BML::ScriptMod *>(mod);
        return scriptMod ? scriptMod->GetLastDiagnostic() : std::string();
#else
        (void) mod;
        return {};
#endif
    }

    const char *ApplyErrorText(Config::ApplyError error) {
        switch (error) {
        case Config::ApplyError::OwnerChanged:
            return "The Mod was replaced before its settings could be applied.";
        case Config::ApplyError::SchemaChanged:
            return "The settings structure changed. Review the current values and try again.";
        case Config::ApplyError::PropertyMissing:
        case Config::ApplyError::TypeChanged:
            return "A setting changed or disappeared. Review the current values and try again.";
        case Config::ApplyError::BaseChanged:
            return "A setting was changed elsewhere. Resolve the highlighted conflict first.";
        case Config::ApplyError::DuplicateTarget:
        case Config::ApplyError::InvalidValue:
            return "The pending settings batch is invalid and was not applied.";
        case Config::ApplyError::None:
        default:
            return "Settings could not be applied.";
        }
    }

}

ModMenuModel::ModMenuModel(ModContext &context) : m_Context(context) {}

void ModMenuModel::OnOpen() {
    m_Session.Revert();
    m_Session.ClearSelection();
    m_SelectedStamp.reset();
    m_PendingCommand = Command::None;
    m_Notice.clear();
}

void ModMenuModel::OnClose() {
    OnOpen();
}

const std::vector<ModMenuModSummary> &ModMenuModel::GetMods() {
    RefreshMods();
    return m_Mods;
}

bool ModMenuModel::SelectMod(const ModMenuOwner &owner) {
    if (!m_Session.SelectMod(owner))
        return false;

    m_SelectedStamp.reset();
    m_Notice.clear();
    SynchronizeSelected(true);
    return true;
}

bool ModMenuModel::ClearSelection() {
    if (!m_Session.ClearSelection())
        return false;
    m_SelectedStamp.reset();
    return true;
}

bool ModMenuModel::SelectDetailsAction(const ModMenuDetailsActionKey &action) {
    return m_Session.SelectDetailsAction(action);
}

ModMenuEditResult ModMenuModel::EditSetting(const ModMenuSettingKey &key,
                                            ModMenuSettingValue value) {
    const ModMenuEditResult result = m_Session.Edit(key, std::move(value));
    if (result == ModMenuEditResult::Changed)
        m_Notice.clear();
    return result;
}

const ModMenuDetailsActionDocument *ModMenuModel::GetSelectedDetailsAction() const {
    return m_Session.GetSelectedDetailsAction();
}

int ModMenuModel::EnterPage() const noexcept {
    const ModMenuPageKey *key = GetSelectedPageKey();
    if (!key)
        return BML_ERROR_NOT_FOUND;

    try {
        auto invocation = m_Context.LockModInvocation();
        return m_Context.GetModMenuPages().Enter(*key);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModMenuModel::DrawPage(BML_ModMenuPageAction &action) const noexcept {
    const ModMenuPageKey *key = GetSelectedPageKey();
    if (!key)
        return BML_ERROR_NOT_FOUND;

    try {
        auto invocation = m_Context.LockModInvocation();
        return m_Context.GetModMenuPages().Draw(*key, action);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModMenuModel::LeavePage(BML_ModMenuPageLeaveReason reason) const noexcept {
    const ModMenuPageKey *key = GetSelectedPageKey();
    if (!key)
        return BML_ERROR_NOT_FOUND;

    try {
        auto invocation = m_Context.LockModInvocation();
        return m_Context.GetModMenuPages().Leave(*key, reason);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

void ModMenuModel::SynchronizeSelected(bool force) {
    const ModMenuOwner *selected = m_Session.GetSelectedMod();
    if (!selected)
        return;

    IMod *mod = m_Context.FindMod(selected->id.c_str());
    if (!mod) {
        m_SelectedStamp.reset();
        m_Session.Reconcile(std::nullopt);
        return;
    }

    const ModMenuOwner currentOwner = MakeOwner(mod);
    if (currentOwner.registration == 0) {
        m_SelectedStamp.reset();
        m_Session.Reconcile(std::nullopt);
        return;
    }

    if (currentOwner != *selected) {
        if (m_Session.IsDirty()) {
            m_SelectedStamp.reset();
            m_Session.Reconcile(BuildDocument(mod, currentOwner));
            return;
        }

        m_Session.ClearSelection();
        m_Session.SelectMod(currentOwner);
    }

    Config *config = m_Context.GetConfig(mod);
    SourceStamp stamp;
    stamp.owner = currentOwner;
    if (config) {
        stamp.schemaRevision = config->GetSchemaRevision();
        stamp.valueRevision = config->GetValueRevision();
    }
    stamp.pageRevision = m_Context.GetModMenuPages().Revision(currentOwner.id);

    if (!force && m_SelectedStamp && *m_SelectedStamp == stamp)
        return;

    ModMenuDocument document = BuildDocument(mod, currentOwner);
    stamp.schemaRevision = document.schemaRevision;
    stamp.valueRevision = document.valueRevision;
    stamp.pageRevision = document.pageRevision;
    if (m_Session.Reconcile(std::move(document)))
        m_SelectedStamp = std::move(stamp);
}

void ModMenuModel::RequestApply() {
    m_PendingCommand = Command::Apply;
}

void ModMenuModel::RequestRevert() {
    m_PendingCommand = Command::Revert;
}

void ModMenuModel::ProcessPendingCommand() {
    const Command command = std::exchange(m_PendingCommand, Command::None);
    if (command == Command::Apply)
        Apply();
    else if (command == Command::Revert)
        Revert();
}

const ModMenuSession &ModMenuModel::GetSession() const {
    return m_Session;
}

const std::string &ModMenuModel::GetNotice() const {
    return m_Notice;
}

ModMenuOwner ModMenuModel::MakeOwner(IMod *mod) const {
    if (!mod)
        return {};

    const char *id = mod->GetID();
    const std::uint64_t registration = m_Context.GetModGeneration(mod);
    if (!id || !*id || registration == 0)
        return {};

    return {id, registration, ScriptContentGeneration(mod)};
}

const ModMenuPageKey *ModMenuModel::GetSelectedPageKey() const noexcept {
    const ModMenuDetailsActionDocument *action = GetSelectedDetailsAction();
    const auto *page = action ? std::get_if<ModMenuPageInfo>(action) : nullptr;
    return page ? &page->key : nullptr;
}

ModMenuDocument ModMenuModel::BuildDocument(IMod *mod, const ModMenuOwner &owner) const {
    ModMenuDocument document;
    document.owner = owner;
    document.name = SafeText(mod->GetName());
    if (document.name.empty())
        document.name = owner.id;
    document.author = SafeText(mod->GetAuthor());
    document.version = SafeText(mod->GetVersion());
    document.description = SafeText(mod->GetDescription());
    document.status = ScriptStatus(mod);
    document.diagnostic = ScriptDiagnostic(mod);
    ModMenuPageCatalog pages = m_Context.GetModMenuPages().Snapshot(owner.id);
    document.pageRevision = pages.revision;

    Config *config = m_Context.GetConfig(mod);
    const std::size_t categoryCount = config ? config->GetCategoryCount() : 0;
    document.detailsActions.reserve(categoryCount + pages.pages.size());
    if (config) {
        document.schemaRevision = config->GetSchemaRevision();
        document.valueRevision = config->GetValueRevision();
        for (std::size_t categoryIndex = 0;
             categoryIndex < categoryCount; ++categoryIndex) {
            Category *sourceCategory = config->GetCategory(categoryIndex);
            if (!sourceCategory)
                continue;

            ModMenuCategoryDocument category;
            category.key.id = SafeText(sourceCategory->GetName());
            category.label = DisplayLabel(sourceCategory->GetName());
            category.description = SafeText(sourceCategory->GetComment());
            const std::size_t settingCount = sourceCategory->GetPropertyCount();
            category.settings.reserve(settingCount);
            for (std::size_t settingIndex = 0;
                 settingIndex < settingCount; ++settingIndex) {
                Property *sourceSetting = sourceCategory->GetProperty(settingIndex);
                if (!sourceSetting || sourceSetting->GetType() == IProperty::NONE)
                    continue;

                ModMenuSettingDocument setting;
                setting.key = {owner, category.key.id, SafeText(sourceSetting->GetName())};
                setting.label = DisplayLabel(sourceSetting->GetName());
                setting.description = SafeText(sourceSetting->GetComment());
                setting.type = sourceSetting->GetType();
                setting.value = sourceSetting->GetValue();
                setting.editor = BML_GetConfigPropertyEditor(sourceSetting);
                category.settings.push_back(std::move(setting));
            }

            document.detailsActions.emplace_back(std::move(category));
        }
    }

    for (ModMenuPageInfo &page : pages.pages)
        document.detailsActions.emplace_back(std::move(page));
    return document;
}

void ModMenuModel::RefreshMods() {
    const std::uint64_t registryRevision = m_Context.GetModRegistryRevision();
    bool changed = !m_ObservedModRegistryRevision ||
                   *m_ObservedModRegistryRevision != registryRevision;
#if BML_ENABLE_ANGELSCRIPT
    if (!changed) {
        for (const ModListSource &source : m_ModSources) {
            if (ScriptContentGeneration(source.mod) != source.contentGeneration) {
                changed = true;
                break;
            }
        }
    }
#endif
    if (!changed)
        return;

    const int count = m_Context.GetModCount();
    std::vector<ModMenuModSummary> mods;
    std::vector<ModListSource> sources;
    mods.reserve(static_cast<std::size_t>(std::max(count, 0)));
    sources.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; index < count; ++index) {
        IMod *mod = m_Context.GetMod(index);
        const ModMenuOwner owner = MakeOwner(mod);
        if (!mod || owner.id.empty() || owner.registration == 0)
            continue;

        ModMenuModSummary summary;
        summary.owner = owner;
        summary.name = SafeText(mod->GetName());
        if (summary.name.empty())
            summary.name = owner.id;
        mods.push_back(std::move(summary));
        sources.push_back({mod, owner.registration, owner.contentGeneration});
    }

    m_Mods = std::move(mods);
    m_ModSources = std::move(sources);
    m_ObservedModRegistryRevision = registryRevision;
}

void ModMenuModel::Apply() {
    std::optional<ModMenuSession::PreparedEdits> prepared = m_Session.PrepareEdits();
    if (!prepared) {
        m_Notice = "There are no conflict-free changes to apply.";
        return;
    }
    const ModMenuEditBatch &batch = prepared->GetBatch();

    IMod *mod = m_Context.FindMod(batch.owner.id.c_str());
    if (!mod || MakeOwner(mod) != batch.owner) {
        m_Notice = "The Mod changed before its settings could be applied.";
        SynchronizeSelected(true);
        return;
    }

    Config *config = m_Context.GetConfig(mod);
    if (!config) {
        m_Notice = "The Mod no longer owns a configuration.";
        SynchronizeSelected(true);
        return;
    }

    std::vector<Config::Edit> edits;
    edits.reserve(batch.edits.size());
    for (const ModMenuEdit &source : batch.edits) {
        Config::Edit edit;
        edit.Category = source.key.category;
        edit.Key = source.key.property;
        edit.ExpectedType = source.type;
        edit.BaseValue = source.baseline;
        edit.NewValue = source.value;
        edits.push_back(std::move(edit));
    }
    std::string successNotice = edits.size() == 1
        ? "1 setting applied."
        : std::to_string(edits.size()) + " settings applied.";

    const Config::ApplyResult result = config->ApplyEdits(mod, batch.expectedSchemaRevision, edits);
    if (!result) {
        m_Notice = ApplyErrorText(result.Error);
        SynchronizeSelected(true);
        return;
    }

    const std::uint64_t valueRevision = config->GetValueRevision();
    m_Session.AcceptEdits(std::move(*prepared), valueRevision);

    if (m_SelectedStamp)
        m_SelectedStamp->valueRevision = valueRevision;
    m_Notice = std::move(successNotice);
}

void ModMenuModel::Revert() {
    m_Session.Revert();
    m_Notice = "Pending changes reverted.";
    SynchronizeSelected(true);
}
