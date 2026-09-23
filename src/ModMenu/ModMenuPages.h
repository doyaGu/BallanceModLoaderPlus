#ifndef BML_MOD_MENU_PAGES_H
#define BML_MOD_MENU_PAGES_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "BML/ModMenu.h"

struct ModMenuPageKey {
    std::string owner;
    std::string id;
    std::uint64_t generation = 0;

    bool operator==(const ModMenuPageKey &) const = default;
};

struct ModMenuPageInfo {
    ModMenuPageKey key;
    std::string label;
    std::string description;
    bool showInDetails = true;
};

struct ModMenuPageNavigation {
    BML_ModMenuPageAction action = BML_MOD_MENU_PAGE_NONE;
    std::string targetPageId;
};

struct ModMenuPageCatalog {
    std::uint64_t revision = 0;
    std::vector<ModMenuPageInfo> pages;
};

class ModMenuPages final {
public:
    int Register(std::string owner, const BML_ModMenuPage &page);
    int Unregister(std::string_view owner, std::string_view pageId);
    std::size_t RemoveOwner(std::string_view owner);

    std::uint64_t Revision(std::string_view owner) const noexcept;
    ModMenuPageCatalog Snapshot(std::string_view owner) const;
    std::optional<ModMenuPageInfo> Lookup(std::string_view owner,
                                          std::string_view pageId) const;
    bool Contains(std::string_view owner, std::string_view pageId) const noexcept;
    bool Contains(const ModMenuPageKey &key) const noexcept;

    int Enter(const ModMenuPageKey &key, BML_ModMenuPageEnterReason reason) const noexcept;
    int Draw(const ModMenuPageKey &key, ModMenuPageNavigation &navigation) const noexcept;
    int Leave(const ModMenuPageKey &key, BML_ModMenuPageLeaveReason reason) const noexcept;

private:
    struct Callbacks {
        ~Callbacks() noexcept;

        void *userData = nullptr;
        BML_ModMenuPageDraw draw = nullptr;
        BML_ModMenuPageEnter enter = nullptr;
        BML_ModMenuPageLeave leave = nullptr;
        BML_ModMenuPageRelease release = nullptr;
    };

    struct Page {
        ModMenuPageInfo info;
        std::shared_ptr<Callbacks> callbacks;
    };

    struct OwnerState {
        std::string id;
        std::uint64_t revision = 0;
        std::vector<Page> pages;
    };

    const Page *Find(const ModMenuPageKey &key) const noexcept;
    OwnerState *FindOwner(std::string_view owner) noexcept;
    const OwnerState *FindOwner(std::string_view owner) const noexcept;
    std::uint64_t NextSequence() noexcept;

    std::vector<OwnerState> m_Owners;
    std::uint64_t m_NextSequence = 1;
};

#endif // BML_MOD_MENU_PAGES_H
