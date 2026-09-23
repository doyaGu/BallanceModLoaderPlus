#include "ModMenu/ModMenuPages.h"

#include "BML/ModMenu.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace {
    struct CallbackState {
        BML_ModMenuPageAction action = BML_MOD_MENU_PAGE_NONE;
        int draws = 0;
        int enters = 0;
        int leaves = 0;
        int drawStatus = BML_OK;
        int enterStatus = BML_OK;
        int leaveStatus = BML_OK;
        std::string targetPageId;
        BML_ModMenuPageEnterReason enterReason = BML_MOD_MENU_PAGE_ENTER_PUSH;
        BML_ModMenuPageLeaveReason leaveReason = BML_MOD_MENU_PAGE_LEAVE_BACK;
    };

    int BML_CDECL DrawPage(void *userData, BML_ModMenuPageFrame *frame) {
        if (!userData || !frame)
            return BML_ERROR_INVALID_PARAMETER;
        auto &state = *static_cast<CallbackState *>(userData);
        ++state.draws;
        frame->Action = state.action;
        if (state.targetPageId.size() < sizeof(frame->TargetPageId))
            std::memcpy(frame->TargetPageId, state.targetPageId.c_str(), state.targetPageId.size() + 1);
        return state.drawStatus;
    }

    int BML_CDECL DrawUnterminatedTarget(void *, BML_ModMenuPageFrame *frame) {
        std::memset(frame->TargetPageId, 'x', sizeof(frame->TargetPageId));
        frame->Action = BML_MOD_MENU_PAGE_PUSH;
        return BML_OK;
    }

    int BML_CDECL EnterPage(void *userData, BML_ModMenuPageEnterReason reason) {
        auto &state = *static_cast<CallbackState *>(userData);
        ++state.enters;
        state.enterReason = reason;
        return state.enterStatus;
    }

    int BML_CDECL LeavePage(void *userData, BML_ModMenuPageLeaveReason reason) {
        auto &state = *static_cast<CallbackState *>(userData);
        ++state.leaves;
        state.leaveReason = reason;
        return state.leaveStatus;
    }

    BML_ModMenuPage MakePage(const char *id, const char *label,
                             const char *description, CallbackState &state) {
        return {
            sizeof(BML_ModMenuPage),
            id,
            label,
            description,
            &state,
            &DrawPage,
            &EnterPage,
            &LeavePage,
            nullptr,
            BML_MOD_MENU_PAGE_VISIBLE,
        };
    }

    ModMenuPageKey KeyOf(const ModMenuPages &pages, std::string_view owner,
                         std::string_view pageId) {
        for (const ModMenuPageInfo &page : pages.Snapshot(owner).pages) {
            if (page.key.id == pageId)
                return page.key;
        }
        return {};
    }

    struct SelfRemovingState {
        ModMenuPages *pages = nullptr;
        const char *owner = nullptr;
        const char *page = nullptr;
        int releases = 0;
    };

    int BML_CDECL DrawAndRemovePage(
        void *userData, BML_ModMenuPageFrame *frame) {
        auto &state = *static_cast<SelfRemovingState *>(userData);
        EXPECT_EQ(state.pages->Unregister(state.owner, state.page), BML_OK);
        frame->Action = BML_MOD_MENU_PAGE_BACK;
        return BML_OK;
    }

    void BML_CDECL ReleaseSelfRemovingPage(void *userData) {
        ++static_cast<SelfRemovingState *>(userData)->releases;
    }

    ModMenuPages *facadePages = nullptr;

    class FacadeRegistryScope final {
    public:
        explicit FacadeRegistryScope(ModMenuPages &pages) {
            facadePages = &pages;
        }

        ~FacadeRegistryScope() { facadePages = nullptr; }

        FacadeRegistryScope(const FacadeRegistryScope &) = delete;
        FacadeRegistryScope &operator=(const FacadeRegistryScope &) = delete;
    };

    int BML_CDECL RegisterFacadePage(const char *ownerId, const BML_ModMenuPage *page) {
        return facadePages && ownerId && page
                   ? facadePages->Register(ownerId, *page)
                   : BML_ERROR_INVALID_PARAMETER;
    }

    int BML_CDECL UnregisterFacadePage(const char *ownerId, const char *pageId) {
        return facadePages && ownerId && pageId
                   ? facadePages->Unregister(ownerId, pageId)
                   : BML_ERROR_INVALID_PARAMETER;
    }

    const BML_ModMenuInterface FacadeInterface = {
        BML_IFACE_HEADER(BML_ModMenuInterface, BML_MOD_MENU_INTERFACE_ID,
                         BML_MOD_MENU_INTERFACE_MAJOR, BML_MOD_MENU_INTERFACE_MINOR),
        &RegisterFacadePage,
        &UnregisterFacadePage,
    };

    class FacadePage final : public BML::ModMenu::Page {
    public:
        explicit FacadePage(BML::ModMenu::PageVisibility visibility =
                                BML::ModMenu::PageVisibility::Visible)
            : Page("facade", "Facade", "C++ wrapper", visibility) {}

        BML::ModMenu::PageAction action = BML::ModMenu::PageAction::None();
        int draws = 0;
        int enters = 0;
        int leaves = 0;
        bool throwOnDraw = false;
        bool throwOnEnter = false;
        bool throwOnLeave = false;
        bool unregisterOnDraw = false;
        int unregisterStatus = BML_ERROR_FAIL;

    protected:
        BML::ModMenu::PageAction OnFrame() override {
            ++draws;
            if (throwOnDraw)
                throw std::runtime_error("draw failed");
            if (unregisterOnDraw)
                unregisterStatus = Unregister();
            return action;
        }

        void OnEnter(BML::ModMenu::PageEnterReason) override {
            ++enters;
            if (throwOnEnter)
                throw std::runtime_error("enter failed");
        }

        void OnLeave(BML::ModMenu::PageLeaveReason) override {
            ++leaves;
            if (throwOnLeave)
                throw std::runtime_error("leave failed");
        }
    };
}

extern "C" int BML_CDECL BML_GetInterface(
    const char *interfaceId, std::uint16_t majorVersion, const void **out) {
    if (!interfaceId || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (std::strcmp(interfaceId, BML_MOD_MENU_INTERFACE_ID) != 0)
        return BML_ERROR_NOT_FOUND;
    if (majorVersion != BML_MOD_MENU_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &FacadeInterface;
    return BML_OK;
}

TEST(ModMenuPagesTest, CopiesMetadataAndKeepsRegistrationOrderPerOwner) {
    ModMenuPages pages;
    CallbackState state;
    std::string id = "advanced";
    std::string label = "Advanced";
    std::string description = "Fine-grained controls";
    BML_ModMenuPage first = MakePage(id.c_str(), label.c_str(), description.c_str(), state);
    BML_ModMenuPage second = MakePage("about", "About", "Build information", state);

    EXPECT_EQ(pages.Register("sample.mod", first), BML_OK);
    const std::uint64_t firstRevision = pages.Revision("sample.mod");
    EXPECT_NE(firstRevision, 0U);
    EXPECT_EQ(pages.Register("other.mod", second), BML_OK);
    EXPECT_EQ(pages.Register("sample.mod", second), BML_OK);
    EXPECT_GT(pages.Revision("sample.mod"), firstRevision);

    id.assign("changed");
    label.assign("Changed");
    description.assign("Changed");

    const ModMenuPageCatalog catalog = pages.Snapshot("sample.mod");
    ASSERT_EQ(catalog.pages.size(), 2U);
    EXPECT_EQ(catalog.pages[0].key.owner, "sample.mod");
    EXPECT_EQ(catalog.pages[0].key.id, "advanced");
    EXPECT_EQ(catalog.pages[0].label, "Advanced");
    EXPECT_EQ(catalog.pages[0].description, "Fine-grained controls");
    EXPECT_EQ(catalog.pages[1].key.id, "about");
    EXPECT_NE(catalog.pages[0].key.generation, 0U);
    EXPECT_NE(catalog.pages[0].key.generation, catalog.pages[1].key.generation);
    EXPECT_EQ(catalog.revision, pages.Revision("sample.mod"));
}

TEST(ModMenuPagesTest, HiddenPagesRemainAddressableButAreNotDetailEntries) {
    ModMenuPages pages;
    CallbackState state;
    BML_ModMenuPage child = MakePage("child", "Child", "", state);
    child.Flags = BML_MOD_MENU_PAGE_HIDDEN;
    ASSERT_EQ(pages.Register("sample.mod", child), BML_OK);

    const ModMenuPageCatalog catalog = pages.Snapshot("sample.mod");
    ASSERT_EQ(catalog.pages.size(), 1U);
    EXPECT_FALSE(catalog.pages.front().showInDetails);
    EXPECT_TRUE(pages.Lookup("sample.mod", "child").has_value());
    EXPECT_TRUE(pages.Contains("sample.mod", "child"));
    EXPECT_TRUE(pages.Contains(catalog.pages.front().key));
    EXPECT_FALSE(pages.Contains("other.mod", "child"));
}

TEST(ModMenuPagesTest, CopiesPushAndReplaceTargetsFromTheDrawFrame) {
    ModMenuPages pages;
    CallbackState state;
    BML_ModMenuPage page = MakePage("entry", "Entry", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);
    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "entry");

    state.action = BML_MOD_MENU_PAGE_PUSH;
    state.targetPageId = "child";
    ModMenuPageNavigation navigation;
    ASSERT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_PUSH);
    EXPECT_EQ(navigation.targetPageId, "child");

    state.action = BML_MOD_MENU_PAGE_REPLACE;
    state.targetPageId = "replacement";
    ASSERT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_REPLACE);
    EXPECT_EQ(navigation.targetPageId, "replacement");

    state.action = BML_MOD_MENU_PAGE_BACK;
    ASSERT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_TRUE(navigation.targetPageId.empty());
}

TEST(ModMenuPagesTest, RejectsMalformedAndDuplicatePages) {
    ModMenuPages pages;
    CallbackState state;
    BML_ModMenuPage page = MakePage("advanced", "Advanced", "", state);

    EXPECT_EQ(pages.Register("sample.mod", page), BML_OK);
    EXPECT_EQ(pages.Register("sample.mod", page), BML_ERROR_ALREADY_EXISTS);
    EXPECT_EQ(pages.Register("other.mod", page), BML_OK);

    page.StructSize = offsetof(BML_ModMenuPage, Draw);
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_VERSION_MISMATCH);
    page.StructSize = sizeof(BML_ModMenuPage);
    page.Id = "";
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_INVALID_PARAMETER);
    page.Id = "page";
    page.Label = nullptr;
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_INVALID_PARAMETER);
    page.Label = "Page";
    page.Draw = nullptr;
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_INVALID_PARAMETER);
    page.Draw = &DrawPage;
    page.Flags = 2;
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_INVALID_PARAMETER);
    page.Flags = BML_MOD_MENU_PAGE_VISIBLE;
    const std::string oversizedId(BML_MOD_MENU_PAGE_ID_CAPACITY, 'x');
    page.Id = oversizedId.c_str();
    EXPECT_EQ(pages.Register("third.mod", page), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(pages.Register("", MakePage("page", "Page", "", state)),
              BML_ERROR_INVALID_PARAMETER);
}

TEST(ModMenuPagesTest, InvokesDrawAndLifecycleCallbacks) {
    ModMenuPages pages;
    CallbackState state;
    state.action = BML_MOD_MENU_PAGE_CLOSE;
    const BML_ModMenuPage page = MakePage("advanced", "Advanced", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);
    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "advanced");
    ASSERT_NE(key.generation, 0U);

    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Enter(key, BML_MOD_MENU_PAGE_ENTER_PUSH), BML_OK);
    EXPECT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(pages.Leave(key, BML_MOD_MENU_PAGE_LEAVE_CLOSE), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_CLOSE);
    EXPECT_EQ(state.draws, 1);
    EXPECT_EQ(state.enters, 1);
    EXPECT_EQ(state.leaves, 1);
    EXPECT_EQ(state.enterReason, BML_MOD_MENU_PAGE_ENTER_PUSH);
    EXPECT_EQ(state.leaveReason, BML_MOD_MENU_PAGE_LEAVE_CLOSE);
}

TEST(ModMenuPagesTest, PropagatesLifecycleFailures) {
    ModMenuPages pages;
    CallbackState state;
    state.enterStatus = BML_ERROR_FAIL;
    state.leaveStatus = BML_ERROR_OUT_OF_MEMORY;
    const BML_ModMenuPage page = MakePage("advanced", "Advanced", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);
    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "advanced");

    EXPECT_EQ(pages.Enter(key, BML_MOD_MENU_PAGE_ENTER_REPLACE), BML_ERROR_FAIL);
    EXPECT_EQ(pages.Leave(key, BML_MOD_MENU_PAGE_LEAVE_CLOSE),
              BML_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(state.enters, 1);
    EXPECT_EQ(state.leaves, 1);
    EXPECT_EQ(state.enterReason, BML_MOD_MENU_PAGE_ENTER_REPLACE);
    EXPECT_EQ(state.leaveReason, BML_MOD_MENU_PAGE_LEAVE_CLOSE);
}

TEST(ModMenuPagesTest, KeepsDrawStatusSeparateFromNavigation) {
    ModMenuPages pages;
    CallbackState state;
    state.action = BML_MOD_MENU_PAGE_CLOSE;
    state.drawStatus = BML_ERROR_FAIL;
    const BML_ModMenuPage page = MakePage("advanced", "Advanced", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);
    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "advanced");

    ModMenuPageNavigation navigation{BML_MOD_MENU_PAGE_BACK, {}};
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_FAIL);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_BACK);
    EXPECT_EQ(state.draws, 1);
}

TEST(ModMenuPagesTest, RejectsOldAlphaPageLayoutBeforeCallingCallbacks) {
    ModMenuPages pages;
    CallbackState state;
    BML_ModMenuPage page = MakePage("advanced", "Advanced", "", state);
    page.StructSize = offsetof(BML_ModMenuPage, Flags);
    EXPECT_EQ(pages.Register("sample.mod", page), BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(state.enters, 0);
    EXPECT_EQ(state.leaves, 0);
}

TEST(ModMenuPagesTest, RejectsInvalidActionsAndSurvivesSelfRemoval) {
    ModMenuPages pages;
    CallbackState invalidState;
    invalidState.action = static_cast<BML_ModMenuPageAction>(99);
    BML_ModMenuPage invalid = MakePage("invalid", "Invalid", "", invalidState);
    ASSERT_EQ(pages.Register("sample.mod", invalid), BML_OK);
    const ModMenuPageKey invalidKey = KeyOf(pages, "sample.mod", "invalid");

    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Draw(invalidKey, navigation), BML_ERROR_MALFORMED_MESSAGE);

    SelfRemovingState removingState{&pages, "sample.mod", "removing"};
    const BML_ModMenuPage removing = {
        sizeof(BML_ModMenuPage),
        "removing",
        "Removing",
        "",
        &removingState,
        &DrawAndRemovePage,
        nullptr,
        nullptr,
        &ReleaseSelfRemovingPage,
        BML_MOD_MENU_PAGE_VISIBLE,
    };
    ASSERT_EQ(pages.Register("sample.mod", removing), BML_OK);
    const ModMenuPageKey removingKey = KeyOf(pages, "sample.mod", "removing");
    EXPECT_EQ(pages.Draw(removingKey, navigation), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_BACK);
    EXPECT_EQ(removingState.releases, 1);
    EXPECT_EQ(pages.Draw(removingKey, navigation), BML_ERROR_NOT_FOUND);
}

TEST(ModMenuPagesTest, RejectsNavigationWithoutATerminatedTarget) {
    ModMenuPages pages;
    CallbackState state;
    state.action = BML_MOD_MENU_PAGE_PUSH;
    BML_ModMenuPage page = MakePage("entry", "Entry", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);
    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Draw(KeyOf(pages, "sample.mod", "entry"), navigation),
              BML_ERROR_MALFORMED_MESSAGE);

    BML_ModMenuPage unterminated = MakePage("unterminated", "Unterminated", "", state);
    unterminated.Draw = &DrawUnterminatedTarget;
    ASSERT_EQ(pages.Register("sample.mod", unterminated), BML_OK);
    EXPECT_EQ(pages.Draw(KeyOf(pages, "sample.mod", "unterminated"), navigation),
              BML_ERROR_MALFORMED_MESSAGE);
}

TEST(ModMenuPagesTest, NeverDispatchesAnOldRouteToAReplacementPage) {
    ModMenuPages pages;
    CallbackState firstState;
    CallbackState replacementState;
    const BML_ModMenuPage first = MakePage("advanced", "Advanced", "", firstState);
    const BML_ModMenuPage replacement = MakePage("advanced", "Replacement", "", replacementState);
    ASSERT_EQ(pages.Register("sample.mod", first), BML_OK);
    const ModMenuPageKey firstKey = KeyOf(pages, "sample.mod", "advanced");

    ASSERT_EQ(pages.Unregister("sample.mod", "advanced"), BML_OK);
    ASSERT_EQ(pages.Register("sample.mod", replacement), BML_OK);
    const ModMenuPageKey replacementKey = KeyOf(pages, "sample.mod", "advanced");
    ASSERT_NE(firstKey, replacementKey);

    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Enter(firstKey, BML_MOD_MENU_PAGE_ENTER_PUSH), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(pages.Draw(firstKey, navigation), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(pages.Leave(firstKey, BML_MOD_MENU_PAGE_LEAVE_BACK), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(replacementState.enters, 0);
    EXPECT_EQ(replacementState.draws, 0);
    EXPECT_EQ(replacementState.leaves, 0);

    EXPECT_EQ(pages.Enter(replacementKey, BML_MOD_MENU_PAGE_ENTER_BACK), BML_OK);
    EXPECT_EQ(replacementState.enters, 1);
}

TEST(ModMenuPagesTest, UnregistersOnePageAndCleansOnlyTheRequestedOwner) {
    ModMenuPages pages;
    CallbackState state;
    const BML_ModMenuPage first = MakePage("first", "First", "", state);
    const BML_ModMenuPage second = MakePage("second", "Second", "", state);
    ASSERT_EQ(pages.Register("sample.mod", first), BML_OK);
    ASSERT_EQ(pages.Register("sample.mod", second), BML_OK);
    ASSERT_EQ(pages.Register("other.mod", first), BML_OK);

    const std::uint64_t revision = pages.Revision("sample.mod");
    const ModMenuPageKey firstKey = pages.Snapshot("sample.mod").pages[0].key;
    EXPECT_EQ(pages.Unregister("sample.mod", "first"), BML_OK);
    EXPECT_GT(pages.Revision("sample.mod"), revision);
    EXPECT_EQ(pages.Unregister("sample.mod", "missing"), BML_ERROR_NOT_FOUND);
    ASSERT_EQ(pages.Register("sample.mod", first), BML_OK);
    const ModMenuPageCatalog replacement = pages.Snapshot("sample.mod");
    ASSERT_EQ(replacement.pages.size(), 2U);
    EXPECT_NE(replacement.pages[1].key, firstKey);

    EXPECT_EQ(pages.RemoveOwner("sample.mod"), 2U);
    EXPECT_TRUE(pages.Snapshot("sample.mod").pages.empty());
    EXPECT_EQ(pages.Snapshot("other.mod").pages.size(), 1U);
    EXPECT_EQ(pages.RemoveOwner("sample.mod"), 0U);
}

TEST(ModMenuPagesTest, KeepsAnEmptyCatalogRevisionUntilOwnerCleanup) {
    ModMenuPages pages;
    CallbackState state;
    const BML_ModMenuPage page = MakePage("only", "Only", "", state);
    ASSERT_EQ(pages.Register("sample.mod", page), BML_OK);

    const std::uint64_t registeredRevision = pages.Revision("sample.mod");
    ASSERT_EQ(pages.Unregister("sample.mod", "only"), BML_OK);

    const ModMenuPageCatalog empty = pages.Snapshot("sample.mod");
    EXPECT_TRUE(empty.pages.empty());
    EXPECT_GT(empty.revision, registeredRevision);
    EXPECT_EQ(empty.revision, pages.Revision("sample.mod"));

    EXPECT_EQ(pages.RemoveOwner("sample.mod"), 0U);
    EXPECT_EQ(pages.Revision("sample.mod"), 0U);
}

TEST(ModMenuPagesTest, CppFacadeOwnsRegistrationAndContainsExceptions) {
    ModMenuPages pages;
    FacadeRegistryScope registryScope(pages);

    FacadePage page;
    EXPECT_EQ(page.Register("sample.mod"), BML_OK);
    EXPECT_TRUE(page.IsRegistered());
    ASSERT_EQ(pages.Snapshot("sample.mod").pages.size(), 1U);
    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "facade");

    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Enter(key, BML_MOD_MENU_PAGE_ENTER_PUSH), BML_OK);
    EXPECT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(pages.Leave(key, BML_MOD_MENU_PAGE_LEAVE_BACK), BML_OK);
    EXPECT_EQ(page.draws, 1);
    EXPECT_EQ(page.enters, 1);
    EXPECT_EQ(page.leaves, 1);

    page.throwOnDraw = true;
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_FAIL);

    page.throwOnEnter = true;
    EXPECT_EQ(pages.Enter(key, BML_MOD_MENU_PAGE_ENTER_BACK), BML_ERROR_FAIL);
    page.throwOnLeave = true;
    EXPECT_EQ(pages.Leave(key, BML_MOD_MENU_PAGE_LEAVE_BACK), BML_ERROR_FAIL);

    EXPECT_EQ(page.Unregister(), BML_OK);
    EXPECT_FALSE(page.IsRegistered());
    EXPECT_TRUE(pages.Snapshot("sample.mod").pages.empty());
}

TEST(ModMenuPagesTest, CppFacadePublishesHiddenPagesAndOwnedNavigationTargets) {
    ModMenuPages pages;
    FacadeRegistryScope registryScope(pages);
    FacadePage page(BML::ModMenu::PageVisibility::Hidden);
    ASSERT_EQ(page.Register("sample.mod"), BML_OK);
    const ModMenuPageCatalog catalog = pages.Snapshot("sample.mod");
    ASSERT_EQ(catalog.pages.size(), 1U);
    EXPECT_FALSE(catalog.pages.front().showInDetails);

    const ModMenuPageKey key = catalog.pages.front().key;
    ModMenuPageNavigation navigation;
    page.action = BML::ModMenu::PageAction::Push("child");
    ASSERT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_PUSH);
    EXPECT_EQ(navigation.targetPageId, "child");

    page.action = BML::ModMenu::PageAction::Replace("replacement");
    ASSERT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_REPLACE);
    EXPECT_EQ(navigation.targetPageId, "replacement");

    constexpr char invalidTarget[] = "child\0other";
    page.action = BML::ModMenu::PageAction::Push(
        std::string(invalidTarget, sizeof(invalidTarget) - 1));
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_INVALID_PARAMETER);

    page.action = BML::ModMenu::PageAction::Replace(
        std::string(BML_MOD_MENU_PAGE_ID_CAPACITY, 'x'));
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_INVALID_PARAMETER);

    ASSERT_EQ(page.Unregister(), BML_OK);
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_NOT_FOUND);
}

TEST(ModMenuPagesTest, CppFacadeCanUnregisterDuringItsOwnDraw) {
    ModMenuPages pages;
    FacadeRegistryScope registryScope(pages);
    FacadePage page;
    ASSERT_EQ(page.Register("sample.mod"), BML_OK);

    const ModMenuPageKey key = KeyOf(pages, "sample.mod", "facade");
    page.unregisterOnDraw = true;
    page.action = BML::ModMenu::PageAction::Back();

    ModMenuPageNavigation navigation;
    EXPECT_EQ(pages.Draw(key, navigation), BML_OK);
    EXPECT_EQ(page.unregisterStatus, BML_OK);
    EXPECT_FALSE(page.IsRegistered());
    EXPECT_EQ(navigation.action, BML_MOD_MENU_PAGE_BACK);
    EXPECT_EQ(pages.Draw(key, navigation), BML_ERROR_NOT_FOUND);
}
