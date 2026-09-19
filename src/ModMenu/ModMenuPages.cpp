#include "ModMenu/ModMenuPages.h"

#include <cstddef>
#include <utility>

namespace {
    constexpr std::size_t RequiredPageSize =
        offsetof(BML_ModMenuPage, Draw) + sizeof(BML_ModMenuPageDraw);

    bool HasMember(const BML_ModMenuPage &page, std::size_t offset, std::size_t size) {
        return page.StructSize >= offset + size;
    }
}

int ModMenuPages::Register(std::string owner, const BML_ModMenuPage &page) {
    if (owner.empty() || page.StructSize < RequiredPageSize ||
        !page.Id || page.Id[0] == '\0' || !page.Label || page.Label[0] == '\0' ||
        !page.Draw) {
        return BML_ERROR_INVALID_PARAMETER;
    }
    OwnerState *ownerState = FindOwner(owner);
    if (ownerState) {
        for (const Page &registeredPage : ownerState->pages) {
            if (registeredPage.info.key.id == page.Id)
                return BML_ERROR_ALREADY_EXISTS;
        }
    }

    Page registeredPage;
    registeredPage.info.key.owner = owner;
    registeredPage.info.key.id = page.Id;
    registeredPage.info.label = page.Label;
    registeredPage.info.description = page.Description ? page.Description : "";
    registeredPage.userData = page.UserData;
    registeredPage.draw = page.Draw;
    if (HasMember(page, offsetof(BML_ModMenuPage, Enter), sizeof(page.Enter)))
        registeredPage.enter = page.Enter;
    if (HasMember(page, offsetof(BML_ModMenuPage, Leave), sizeof(page.Leave)))
        registeredPage.leave = page.Leave;

    if (!ownerState) {
        OwnerState newOwner;
        newOwner.id = std::move(owner);
        newOwner.pages.push_back(std::move(registeredPage));
        m_Owners.push_back(std::move(newOwner));
        ownerState = &m_Owners.back();
    } else {
        ownerState->pages.push_back(std::move(registeredPage));
    }

    ownerState->revision = NextSequence();
    ownerState->pages.back().info.key.generation = ownerState->revision;
    return BML_OK;
}

int ModMenuPages::Unregister(std::string_view owner, std::string_view pageId) {
    OwnerState *ownerState = FindOwner(owner);
    if (!ownerState)
        return BML_ERROR_NOT_FOUND;

    auto page = ownerState->pages.begin();
    while (page != ownerState->pages.end() && page->info.key.id != pageId)
        ++page;
    if (page == ownerState->pages.end())
        return BML_ERROR_NOT_FOUND;

    ownerState->pages.erase(page);
    ownerState->revision = NextSequence();
    return BML_OK;
}

std::size_t ModMenuPages::RemoveOwner(std::string_view owner) {
    auto ownerState = m_Owners.begin();
    while (ownerState != m_Owners.end() && ownerState->id != owner)
        ++ownerState;
    if (ownerState == m_Owners.end())
        return 0;

    const std::size_t removed = ownerState->pages.size();
    m_Owners.erase(ownerState);
    return removed;
}

std::uint64_t ModMenuPages::Revision(std::string_view owner) const noexcept {
    const OwnerState *state = FindOwner(owner);
    return state ? state->revision : 0;
}

ModMenuPageCatalog ModMenuPages::Snapshot(std::string_view owner) const {
    ModMenuPageCatalog catalog;
    const OwnerState *ownerState = FindOwner(owner);
    if (!ownerState)
        return catalog;

    catalog.revision = ownerState->revision;
    catalog.pages.reserve(ownerState->pages.size());
    for (const Page &page : ownerState->pages)
        catalog.pages.push_back(page.info);
    return catalog;
}

int ModMenuPages::Enter(const ModMenuPageKey &key) const noexcept {
    const Page *page = Find(key);
    if (!page)
        return BML_ERROR_NOT_FOUND;

    const BML_ModMenuPageEnter enter = page->enter;
    if (!enter)
        return BML_OK;
    void *const userData = page->userData;
    try {
        return enter(userData);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModMenuPages::Draw(const ModMenuPageKey &key, BML_ModMenuPageAction &action) const noexcept {
    const Page *page = Find(key);
    if (!page)
        return BML_ERROR_NOT_FOUND;

    const BML_ModMenuPageDraw draw = page->draw;
    void *const userData = page->userData;
    BML_ModMenuPageFrame frame = {
        sizeof(BML_ModMenuPageFrame),
        BML_MOD_MENU_PAGE_NONE,
    };
    try {
        const int status = draw(userData, &frame);
        if (status != BML_OK)
            return status;
        if (frame.Action < BML_MOD_MENU_PAGE_NONE || frame.Action > BML_MOD_MENU_PAGE_CLOSE)
            return BML_ERROR_MALFORMED_MESSAGE;
        action = frame.Action;
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModMenuPages::Leave(const ModMenuPageKey &key,
                        BML_ModMenuPageLeaveReason reason) const noexcept {
    const Page *page = Find(key);
    if (!page)
        return BML_ERROR_NOT_FOUND;

    const BML_ModMenuPageLeave leave = page->leave;
    if (!leave)
        return BML_OK;
    void *const userData = page->userData;
    try {
        return leave(userData, reason);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

const ModMenuPages::Page *ModMenuPages::Find(const ModMenuPageKey &key) const noexcept {
    const OwnerState *ownerState = FindOwner(key.owner);
    if (!ownerState)
        return nullptr;

    for (const Page &page : ownerState->pages) {
        if (page.info.key.id == key.id && page.info.key.generation == key.generation)
            return &page;
    }
    return nullptr;
}

ModMenuPages::OwnerState *ModMenuPages::FindOwner(std::string_view owner) noexcept {
    for (OwnerState &state : m_Owners) {
        if (state.id == owner)
            return &state;
    }
    return nullptr;
}

const ModMenuPages::OwnerState *ModMenuPages::FindOwner(std::string_view owner) const noexcept {
    for (const OwnerState &state : m_Owners) {
        if (state.id == owner)
            return &state;
    }
    return nullptr;
}

std::uint64_t ModMenuPages::NextSequence() noexcept {
    const std::uint64_t sequence = m_NextSequence++;
    if (m_NextSequence == 0)
        m_NextSequence = 1;
    return sequence;
}
