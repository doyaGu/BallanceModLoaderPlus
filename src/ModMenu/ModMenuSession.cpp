#include "ModMenu/ModMenuSession.h"

#include <functional>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace {
    void CombineHash(std::size_t &target, std::size_t value) noexcept {
        target ^= value + 0x9e3779b9U + (target << 6U) + (target >> 2U);
    }

    bool IsValidDocument(const ModMenuDocument &document) {
        if (document.owner.id.empty() || document.owner.registration == 0)
            return false;

        std::unordered_set<std::string> categories;
        std::unordered_set<std::string> pages;
        categories.reserve(document.detailsActions.size());
        pages.reserve(document.detailsActions.size());
        for (const ModMenuDetailsActionDocument &action : document.detailsActions) {
            if (const auto *page = std::get_if<ModMenuPageInfo>(&action)) {
                if (page->key.owner != document.owner.id || page->key.id.empty() ||
                    page->key.generation == 0 || page->label.empty() ||
                    !pages.emplace(page->key.id).second) {
                    return false;
                }
                continue;
            }

            const auto *category = std::get_if<ModMenuCategoryDocument>(&action);
            if (!category || category->key.id.empty() || category->label.empty() ||
                !categories.emplace(category->key.id).second) {
                return false;
            }
            std::unordered_set<std::string> settings;
            settings.reserve(category->settings.size());
            for (const ModMenuSettingDocument &setting : category->settings) {
                if (setting.key.owner != document.owner ||
                    setting.key.category != category->key.id || setting.key.property.empty() ||
                    !settings.emplace(setting.key.property).second ||
                    !ConfigValueMatchesType(setting.type, setting.value)) {
                    return false;
                }
            }
        }

        return true;
    }

    const ModMenuDetailsActionDocument *FindDetailsAction(
        const ModMenuDocument &document,
        const ModMenuDetailsActionKey &key) noexcept {
        for (const ModMenuDetailsActionDocument &action : document.detailsActions) {
            if (const auto *categoryKey = std::get_if<ModMenuCategoryKey>(&key)) {
                const auto *category = std::get_if<ModMenuCategoryDocument>(&action);
                if (category && category->key == *categoryKey)
                    return &action;
            } else if (const auto *pageKey = std::get_if<ModMenuPageKey>(&key)) {
                const auto *page = std::get_if<ModMenuPageInfo>(&action);
                if (page && page->key == *pageKey)
                    return &action;
            }
        }
        return nullptr;
    }

    const ModMenuCategoryDocument *FindCategory(const ModMenuDocument &document,
                                                std::string_view id) noexcept {
        for (const ModMenuDetailsActionDocument &action : document.detailsActions) {
            const auto *category = std::get_if<ModMenuCategoryDocument>(&action);
            if (category && category->key.id == id)
                return category;
        }
        return nullptr;
    }

    const ModMenuSettingDocument *FindSetting(const ModMenuDocument &document,
                                              const ModMenuSettingKey &key) noexcept {
        if (document.owner != key.owner)
            return nullptr;

        const ModMenuCategoryDocument *category = FindCategory(document, key.category);
        if (!category)
            return nullptr;

        for (const ModMenuSettingDocument &setting : category->settings) {
            if (setting.key == key)
                return &setting;
        }
        return nullptr;
    }

    ModMenuSettingDocument *FindSetting(ModMenuDocument &document,
                                        const ModMenuSettingKey &key) noexcept {
        if (document.owner != key.owner)
            return nullptr;

        for (ModMenuDetailsActionDocument &action : document.detailsActions) {
            auto *category = std::get_if<ModMenuCategoryDocument>(&action);
            if (!category || category->key.id != key.category)
                continue;
            for (ModMenuSettingDocument &setting : category->settings) {
                if (setting.key == key)
                    return &setting;
            }
            return nullptr;
        }
        return nullptr;
    }

}

ModMenuSession::PreparedEdits::PreparedEdits(ModMenuEditBatch batch,
                                             ModMenuDocument acceptedDocument)
    : m_Batch(std::move(batch)), m_AcceptedDocument(std::move(acceptedDocument)) {}

std::size_t ModMenuSession::SettingKeyHash::operator()(
    const ModMenuSettingKey &key) const noexcept {
    std::size_t hash = std::hash<std::string>{}(key.owner.id);
    CombineHash(hash, std::hash<std::uint64_t>{}(key.owner.registration));
    CombineHash(hash, std::hash<std::uint32_t>{}(key.owner.contentGeneration));
    CombineHash(hash, std::hash<std::string>{}(key.category));
    CombineHash(hash, std::hash<std::string>{}(key.property));
    return hash;
}

void ModMenuSession::UpdateStatus() noexcept {
    if (!m_SelectedMod) {
        m_Status = ModMenuSessionStatus::NoSelection;
        return;
    }
    if (!m_Document) {
        m_Status = m_OwnerGone ? ModMenuSessionStatus::OwnerGone
                               : ModMenuSessionStatus::AwaitingDocument;
        return;
    }

    bool conflict = false;
    for (const auto &[key, draft] : m_Drafts) {
        if (m_Document->schemaRevision != draft.schemaRevision) {
            m_Status = ModMenuSessionStatus::Stale;
            return;
        }

        const ModMenuSettingDocument *setting = FindSetting(*m_Document, key);
        if (!setting || setting->type != draft.type) {
            m_Status = ModMenuSessionStatus::Stale;
            return;
        }
        if (!ConfigValuesEqual(setting->type, setting->value, draft.baseline))
            conflict = true;
    }

    m_Status = conflict ? ModMenuSessionStatus::Conflict : ModMenuSessionStatus::Ready;
}

bool ModMenuSession::SelectMod(ModMenuOwner owner) {
    if (owner.id.empty() || owner.registration == 0)
        return false;
    if (m_SelectedMod && *m_SelectedMod == owner)
        return true;
    if (!m_Drafts.empty())
        return false;

    m_SelectedMod = std::move(owner);
    m_SelectedDetailsAction.reset();
    m_Document.reset();
    m_OwnerGone = false;
    UpdateStatus();
    return true;
}

bool ModMenuSession::ClearSelection() {
    if (!m_Drafts.empty())
        return false;

    m_SelectedMod.reset();
    m_SelectedDetailsAction.reset();
    m_Document.reset();
    m_OwnerGone = false;
    UpdateStatus();
    return true;
}

bool ModMenuSession::SelectDetailsAction(ModMenuDetailsActionKey action) {
    if (!m_Document)
        return false;

    const ModMenuDetailsActionDocument *selected =
        FindDetailsAction(*m_Document, action);
    if (!selected)
        return false;

    m_SelectedDetailsAction = std::move(action);
    return true;
}

bool ModMenuSession::Reconcile(std::optional<ModMenuDocument> document) {
    if (document && !IsValidDocument(*document))
        return false;
    if (!m_SelectedMod)
        return !document;

    if (!document || document->owner != *m_SelectedMod) {
        m_Document.reset();
        m_SelectedDetailsAction.reset();
        m_OwnerGone = true;
        UpdateStatus();
        return true;
    }

    m_Document = std::move(document);
    m_OwnerGone = false;
    if (m_SelectedDetailsAction &&
        !FindDetailsAction(*m_Document, *m_SelectedDetailsAction)) {
        m_SelectedDetailsAction.reset();
    }
    UpdateStatus();
    return true;
}

ModMenuEditResult ModMenuSession::Edit(const ModMenuSettingKey &key,
                                       ModMenuSettingValue value) {
    if (!m_Document || !m_SelectedMod || key.owner != *m_SelectedMod ||
        m_Status == ModMenuSessionStatus::Stale ||
        m_Status == ModMenuSessionStatus::OwnerGone) {
        return ModMenuEditResult::NotReady;
    }

    const ModMenuSettingDocument *setting = FindSetting(*m_Document, key);
    if (!setting)
        return ModMenuEditResult::UnknownSetting;
    if (!ConfigValueMatchesType(setting->type, value))
        return ModMenuEditResult::TypeMismatch;

    auto draftIt = m_Drafts.find(key);
    if (ConfigValuesEqual(setting->type, value, setting->value)) {
        if (draftIt == m_Drafts.end())
            return ModMenuEditResult::Unchanged;

        m_Drafts.erase(draftIt);
        UpdateStatus();
        return ModMenuEditResult::Changed;
    }

    if (draftIt == m_Drafts.end()) {
        Draft draft;
        draft.type = setting->type;
        draft.baseline = setting->value;
        draft.value = std::move(value);
        draft.schemaRevision = m_Document->schemaRevision;
        m_Drafts.emplace(key, std::move(draft));
    } else {
        if (ConfigValuesEqual(setting->type, draftIt->second.value, value))
            return ModMenuEditResult::Unchanged;
        draftIt->second.value = std::move(value);
    }

    UpdateStatus();
    return ModMenuEditResult::Changed;
}

void ModMenuSession::Revert() noexcept {
    m_Drafts.clear();
    UpdateStatus();
}

std::optional<ModMenuSession::PreparedEdits> ModMenuSession::PrepareEdits() const {
    if (!CanApply())
        return std::nullopt;

    ModMenuEditBatch batch;
    batch.owner = m_Document->owner;
    batch.expectedSchemaRevision = m_Document->schemaRevision;
    batch.edits.reserve(m_Drafts.size());
    ModMenuDocument acceptedDocument = *m_Document;

    for (ModMenuDetailsActionDocument &action : acceptedDocument.detailsActions) {
        auto *category = std::get_if<ModMenuCategoryDocument>(&action);
        if (!category)
            continue;
        for (ModMenuSettingDocument &setting : category->settings) {
            const auto draftIt = m_Drafts.find(setting.key);
            if (draftIt == m_Drafts.end())
                continue;

            const Draft &draft = draftIt->second;
            batch.edits.push_back({setting.key, draft.type, draft.baseline, draft.value});
            setting.value = draft.value;
        }
    }

    if (batch.edits.size() != m_Drafts.size())
        return std::nullopt;
    return PreparedEdits(std::move(batch), std::move(acceptedDocument));
}

void ModMenuSession::AcceptEdits(PreparedEdits &&prepared,
                                 std::uint64_t valueRevision) noexcept {
    static_assert(std::is_nothrow_assignable_v<std::optional<ModMenuDocument> &,
                                               ModMenuDocument &&>);

    prepared.m_AcceptedDocument.valueRevision = valueRevision;
    m_Document = std::move(prepared.m_AcceptedDocument);
    m_Drafts.clear();
    UpdateStatus();
}

ModMenuSessionStatus ModMenuSession::GetStatus() const noexcept {
    return m_Status;
}

bool ModMenuSession::IsDirty() const noexcept {
    return !m_Drafts.empty();
}

bool ModMenuSession::CanApply() const noexcept {
    return m_Status == ModMenuSessionStatus::Ready && !m_Drafts.empty();
}

const ModMenuOwner *ModMenuSession::GetSelectedMod() const noexcept {
    return m_SelectedMod ? &*m_SelectedMod : nullptr;
}

const ModMenuDetailsActionKey *ModMenuSession::GetSelectedDetailsActionKey() const noexcept {
    return m_SelectedDetailsAction ? &*m_SelectedDetailsAction : nullptr;
}

const ModMenuDetailsActionDocument *ModMenuSession::GetSelectedDetailsAction() const noexcept {
    if (!m_Document || !m_SelectedDetailsAction)
        return nullptr;
    return FindDetailsAction(*m_Document, *m_SelectedDetailsAction);
}

const ModMenuDocument *ModMenuSession::GetDocument() const noexcept {
    return m_Document ? &*m_Document : nullptr;
}

const ModMenuSettingValue *ModMenuSession::GetValue(const ModMenuSettingKey &key) const noexcept {
    const auto draftIt = m_Drafts.find(key);
    if (draftIt != m_Drafts.end())
        return &draftIt->second.value;
    if (!m_Document)
        return nullptr;

    const ModMenuSettingDocument *setting = FindSetting(*m_Document, key);
    return setting ? &setting->value : nullptr;
}

ModMenuSettingState ModMenuSession::GetSettingState(const ModMenuSettingKey &key) const noexcept {
    const auto draftIt = m_Drafts.find(key);
    if (draftIt == m_Drafts.end())
        return m_Document && FindSetting(*m_Document, key)
                   ? ModMenuSettingState::Unchanged
                   : ModMenuSettingState::Missing;
    if (!m_Document || m_Document->schemaRevision != draftIt->second.schemaRevision)
        return ModMenuSettingState::Stale;

    const ModMenuSettingDocument *setting = FindSetting(*m_Document, key);
    if (!setting || setting->type != draftIt->second.type)
        return ModMenuSettingState::Stale;
    if (!ConfigValuesEqual(setting->type, setting->value, draftIt->second.baseline))
        return ModMenuSettingState::Conflict;
    return ModMenuSettingState::Edited;
}
