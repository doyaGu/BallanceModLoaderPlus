#ifndef BML_MOD_MENU_MODEL_H
#define BML_MOD_MENU_MODEL_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "BML/ModMenu.h"

#include "ModMenu/ModMenuSession.h"

class IMod;
class ModContext;

struct ModMenuModSummary {
    ModMenuOwner owner;
    std::string name;
};

class ModMenuModel final {
public:
    explicit ModMenuModel(ModContext &context);

    void OnOpen();
    void OnClose();

    const std::vector<ModMenuModSummary> &GetMods();
    bool SelectMod(const ModMenuOwner &owner);
    bool ClearSelection();
    bool SelectDetailsAction(const ModMenuDetailsActionKey &action);
    ModMenuEditResult EditSetting(const ModMenuSettingKey &key, ModMenuSettingValue value);
    const ModMenuDetailsActionDocument *GetSelectedDetailsAction() const;
    void SynchronizeSelected(bool force = false);
    bool IsCurrentOwner(const ModMenuOwner &owner) const;
    std::uint64_t GetPageRevision(const ModMenuOwner &owner) const;
    ModMenuPageCatalog GetPages(const ModMenuOwner &owner) const;
    std::optional<ModMenuPageInfo> FindPage(const ModMenuOwner &owner,
                                            std::string_view id) const;
    bool HasPage(const ModMenuOwner &owner, const ModMenuPageKey &key) const;
    int EnterPage(const ModMenuOwner &owner, const ModMenuPageKey &key,
                  BML_ModMenuPageEnterReason reason) const noexcept;
    int DrawPage(const ModMenuOwner &owner, const ModMenuPageKey &key,
                 ModMenuPageNavigation &navigation) const noexcept;
    int LeavePage(const ModMenuOwner &owner, const ModMenuPageKey &key,
                  BML_ModMenuPageLeaveReason reason) const noexcept;

    void RequestApply();
    void RequestRevert();
    void ProcessPendingCommand();

    const ModMenuSession &GetSession() const;
    const std::string &GetError() const;

private:
    struct SourceStamp {
        ModMenuOwner owner;
        std::uint64_t schemaRevision = 0;
        std::uint64_t valueRevision = 0;
        std::uint64_t pageRevision = 0;

        bool operator==(const SourceStamp &) const = default;
    };

    struct ModListSource {
        IMod *mod = nullptr;
        std::uint64_t registration = 0;
        std::uint32_t contentGeneration = 0;

        bool operator==(const ModListSource &) const = default;
    };

    enum class Command {
        None,
        Apply,
        Revert,
    };

    ModMenuOwner MakeOwner(IMod *mod) const;
    ModMenuDocument BuildDocument(IMod *mod, const ModMenuOwner &owner) const;
    void RefreshMods();
    void Apply();
    void Revert();

    ModContext &m_Context;
    ModMenuSession m_Session;
    std::vector<ModMenuModSummary> m_Mods;
    std::vector<ModListSource> m_ModSources;
    std::optional<SourceStamp> m_SelectedStamp;
    std::optional<std::uint64_t> m_ObservedModRegistryRevision;
    Command m_PendingCommand = Command::None;
    std::string m_Error;
};

#endif // BML_MOD_MENU_MODEL_H
