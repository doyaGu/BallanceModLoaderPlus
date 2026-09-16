#ifndef BML_MOD_MENU_SESSION_H
#define BML_MOD_MENU_SESSION_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "BML/IConfig.h"

#include "Config/ConfigValue.h"
#include "ModMenu/ModMenuPages.h"

struct ModMenuOwner {
    std::string id;
    std::uint64_t registration = 0;
    std::uint32_t contentGeneration = 0;

    bool operator==(const ModMenuOwner &) const = default;
};

struct ModMenuSettingKey {
    ModMenuOwner owner;
    std::string category;
    std::string property;

    bool operator==(const ModMenuSettingKey &) const = default;
};

using ModMenuSettingValue = ConfigValue;

struct ModMenuSettingDocument {
    ModMenuSettingKey key;
    std::string label;
    std::string description;
    IProperty::PropertyType type = IProperty::NONE;
    ModMenuSettingValue value = 0;
};

struct ModMenuCategoryKey {
    std::string id;

    bool operator==(const ModMenuCategoryKey &) const = default;
};

struct ModMenuCategoryDocument {
    ModMenuCategoryKey key;
    std::string label;
    std::string description;
    std::vector<ModMenuSettingDocument> settings;
};

using ModMenuDetailsActionKey = std::variant<ModMenuCategoryKey, ModMenuPageKey>;
using ModMenuDetailsActionDocument = std::variant<ModMenuCategoryDocument, ModMenuPageInfo>;

// Details actions are kept in display order. Category setting vectors retain
// declaration order, which is also used for deterministic edit batches.
struct ModMenuDocument {
    ModMenuOwner owner;
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string status;
    std::string diagnostic;
    std::uint64_t schemaRevision = 0;
    std::uint64_t valueRevision = 0;
    std::uint64_t pageRevision = 0;
    std::vector<ModMenuDetailsActionDocument> detailsActions;
};

struct ModMenuEdit {
    ModMenuSettingKey key;
    IProperty::PropertyType type = IProperty::NONE;
    ModMenuSettingValue baseline = 0;
    ModMenuSettingValue value = 0;
};

struct ModMenuEditBatch {
    ModMenuOwner owner;
    std::uint64_t expectedSchemaRevision = 0;
    std::vector<ModMenuEdit> edits;
};

enum class ModMenuSessionStatus {
    NoSelection,
    AwaitingDocument,
    Ready,
    Conflict,
    Stale,
    OwnerGone,
};

enum class ModMenuEditResult {
    Changed,
    Unchanged,
    NotReady,
    UnknownSetting,
    TypeMismatch,
};

enum class ModMenuSettingState {
    Unchanged,
    Edited,
    Conflict,
    Stale,
    Missing,
};

class ModMenuSession {
public:
    // Prepared before Config is mutated so accepting a successful transaction
    // only moves already-owned state and cannot fail. Callers can inspect the
    // Config batch but cannot separate it from the matching accepted snapshot.
    class PreparedEdits {
    public:
        PreparedEdits(const PreparedEdits &) = delete;
        PreparedEdits &operator=(const PreparedEdits &) = delete;
        PreparedEdits(PreparedEdits &&) noexcept = default;
        PreparedEdits &operator=(PreparedEdits &&) noexcept = default;

        const ModMenuEditBatch &GetBatch() const noexcept { return m_Batch; }

    private:
        PreparedEdits(ModMenuEditBatch batch, ModMenuDocument acceptedDocument);

        ModMenuEditBatch m_Batch;
        ModMenuDocument m_AcceptedDocument;

        friend class ModMenuSession;
    };

    ModMenuSession() = default;

    ModMenuSession(const ModMenuSession &) = delete;
    ModMenuSession &operator=(const ModMenuSession &) = delete;
    ModMenuSession(ModMenuSession &&) noexcept = default;
    ModMenuSession &operator=(ModMenuSession &&) noexcept = default;

    // A dirty selection must be explicitly applied or reverted before changing
    // Mods. Re-selecting the same owner is always harmless.
    bool SelectMod(ModMenuOwner owner);
    bool ClearSelection();
    bool SelectDetailsAction(ModMenuDetailsActionKey action);

    // A missing document means that the selected owner no longer exists. A
    // document for a different generation has the same meaning for this session.
    // Invalid documents are rejected without changing the current state.
    bool Reconcile(std::optional<ModMenuDocument> document);

    ModMenuEditResult Edit(const ModMenuSettingKey &key, ModMenuSettingValue value);
    void Revert() noexcept;

    // Preparation performs every allocation needed by both the Config batch and
    // the accepted local snapshot without changing this session. Call AcceptEdits
    // only after Config committed the prepared batch successfully.
    std::optional<PreparedEdits> PrepareEdits() const;
    void AcceptEdits(PreparedEdits &&prepared, std::uint64_t valueRevision) noexcept;

    ModMenuSessionStatus GetStatus() const noexcept;
    bool IsDirty() const noexcept;
    bool CanApply() const noexcept;

    const ModMenuOwner *GetSelectedMod() const noexcept;
    const ModMenuDetailsActionKey *GetSelectedDetailsActionKey() const noexcept;
    const ModMenuDetailsActionDocument *GetSelectedDetailsAction() const noexcept;
    const ModMenuDocument *GetDocument() const noexcept;
    const ModMenuSettingValue *GetValue(const ModMenuSettingKey &key) const noexcept;
    ModMenuSettingState GetSettingState(const ModMenuSettingKey &key) const noexcept;

private:
    struct Draft {
        IProperty::PropertyType type = IProperty::NONE;
        ModMenuSettingValue baseline = 0;
        ModMenuSettingValue value = 0;
        std::uint64_t schemaRevision = 0;
    };

    struct SettingKeyHash {
        std::size_t operator()(const ModMenuSettingKey &key) const noexcept;
    };

    void UpdateStatus() noexcept;

    std::optional<ModMenuOwner> m_SelectedMod;
    std::optional<ModMenuDetailsActionKey> m_SelectedDetailsAction;
    std::optional<ModMenuDocument> m_Document;
    std::unordered_map<ModMenuSettingKey, Draft, SettingKeyHash> m_Drafts;
    ModMenuSessionStatus m_Status = ModMenuSessionStatus::NoSelection;
    bool m_OwnerGone = false;
};

#endif // BML_MOD_MENU_SESSION_H
