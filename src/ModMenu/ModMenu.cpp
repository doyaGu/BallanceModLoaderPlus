#include "ModMenu/ModMenu.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "BML/Bui.h"

#include "UI/BuiInternal.h"
#include "ModMenu/ModMenuModel.h"
#include "ModMenu/ModMenuPresentation.h"

namespace {
    constexpr char ModListRoute[] = "mods";
    constexpr char ModDetailsRoute[] = "details";
    constexpr char ModSettingsRoute[] = "settings";

    std::string PageRoute(const ModMenuOwner &owner, const ModMenuPageKey &key) {
        return "mod-page/" + std::to_string(owner.registration) + "/" +
               std::to_string(owner.contentGeneration) + "/" +
               std::to_string(key.generation);
    }

    struct PageNavigationState {
        struct Pending {
            std::string sourceRoute;
            ModMenuPageNavigation navigation;
        };

        Pending pending;
        std::string notice;
    };

    class ModListPage final : public Bui::Page {
    public:
        ModListPage(ModMenuModel &model, ModMenuPresentation &presentation)
            : m_Model(model), m_Presentation(presentation) {}

    protected:
        void OnEnter(Bui::PageEnterReason reason) override {
            if (reason == Bui::PageEnterReason::Back)
                m_Model.ClearSelection();
        }

        Bui::PageAction OnFrame() override {
            const ModMenuRouteAction action = m_Presentation.DrawModListPage(m_Model);
            if (action == ModMenuRouteAction::OpenDetails)
                return Bui::PageAction::Push(ModDetailsRoute);
            return action == ModMenuRouteAction::Back ? Bui::PageAction::Back() : Bui::PageAction::None();
        }

    private:
        ModMenuModel &m_Model;
        ModMenuPresentation &m_Presentation;
    };

    class ModDetailsPage final : public Bui::Page {
    public:
        ModDetailsPage(ModMenuModel &model, ModMenuPresentation &presentation)
            : m_Model(model), m_Presentation(presentation) {}

    protected:
        Bui::PageAction OnFrame() override {
            const ModMenuRouteAction action = m_Presentation.DrawDetailsPage(m_Model);
            if (action == ModMenuRouteAction::OpenSettings)
                return Bui::PageAction::Push(ModSettingsRoute);
            if (action == ModMenuRouteAction::OpenPage) {
                const ModMenuOwner *owner = m_Model.GetSession().GetSelectedMod();
                const ModMenuDetailsActionDocument *selected = m_Model.GetSelectedDetailsAction();
                const auto *page = selected ? std::get_if<ModMenuPageInfo>(selected) : nullptr;
                if (owner && page)
                    return Bui::PageAction::Push(PageRoute(*owner, page->key));
            }
            return action == ModMenuRouteAction::Back ? Bui::PageAction::Back() : Bui::PageAction::None();
        }

    private:
        ModMenuModel &m_Model;
        ModMenuPresentation &m_Presentation;
    };

    class ModSettingsPage final : public Bui::Page {
    public:
        ModSettingsPage(ModMenuModel &model, ModMenuPresentation &presentation)
            : m_Model(model), m_Presentation(presentation) {}

    protected:
        Bui::PageAction OnFrame() override {
            const ModMenuRouteAction action = m_Presentation.DrawSettingsPage(m_Model);
            return action == ModMenuRouteAction::Back ? Bui::PageAction::Back() : Bui::PageAction::None();
        }

    private:
        ModMenuModel &m_Model;
        ModMenuPresentation &m_Presentation;
    };

    class ModPage final : public Bui::Page {
    public:
        ModPage(ModMenuModel &model, ModMenuPresentation &presentation,
                PageNavigationState &navigation, ModMenuOwner owner,
                ModMenuPageInfo page)
            : m_Model(model), m_Presentation(presentation),
              m_Navigation(navigation), m_Owner(std::move(owner)),
              m_Page(std::move(page)), m_Route(PageRoute(m_Owner, m_Page.key)) {}

    protected:
        void OnEnter(Bui::PageEnterReason reason) override {
            m_Navigation.notice.clear();
            BML_ModMenuPageEnterReason pageReason = BML_MOD_MENU_PAGE_ENTER_PUSH;
            if (reason == Bui::PageEnterReason::Back)
                pageReason = BML_MOD_MENU_PAGE_ENTER_BACK;
            else if (reason == Bui::PageEnterReason::Replace)
                pageReason = BML_MOD_MENU_PAGE_ENTER_REPLACE;
            m_Entered = m_Model.EnterPage(m_Owner, m_Page.key, pageReason) == BML_OK;
            m_Status = m_Entered ? ModMenuPageStatus::Ready
                                 : ModMenuPageStatus::EnterFailed;
        }

        Bui::PageAction OnFrame() override {
            const ModMenuPagePresentationResult result =
                m_Presentation.DrawPage(m_Model, m_Owner, m_Page,
                                        m_Status, m_Navigation.notice);
            m_Status = result.status;
            if (result.navigation.action != BML_MOD_MENU_PAGE_NONE) {
                m_Navigation.pending.sourceRoute = m_Route;
                m_Navigation.pending.navigation = result.navigation;
            }
            return Bui::PageAction::None();
        }

        void OnLeave(Bui::PageLeaveReason reason) override {
            if (m_Entered) {
                BML_ModMenuPageLeaveReason pageReason = BML_MOD_MENU_PAGE_LEAVE_CLOSE;
                if (reason == Bui::PageLeaveReason::Back)
                    pageReason = BML_MOD_MENU_PAGE_LEAVE_BACK;
                else if (reason == Bui::PageLeaveReason::Push)
                    pageReason = BML_MOD_MENU_PAGE_LEAVE_PUSH;
                else if (reason == Bui::PageLeaveReason::Replace)
                    pageReason = BML_MOD_MENU_PAGE_LEAVE_REPLACE;
                (void) m_Model.LeavePage(m_Owner, m_Page.key, pageReason);
            }
            m_Entered = false;
            m_Status = ModMenuPageStatus::Ready;
        }

    private:
        ModMenuModel &m_Model;
        ModMenuPresentation &m_Presentation;
        PageNavigationState &m_Navigation;
        ModMenuOwner m_Owner;
        ModMenuPageInfo m_Page;
        std::string m_Route;
        bool m_Entered = false;
        ModMenuPageStatus m_Status = ModMenuPageStatus::Ready;
    };
}

struct ModMenu::State {
    struct PageRouteEntry {
        ModMenuOwner owner;
        ModMenuPageInfo page;
        std::string route;
    };

    enum class CloseDestination {
        Options,
        Shutdown,
    };

    explicit State(ModContext &context)
        : model(context),
          routes(
              [this]() {
                  navigation = {};
                  model.OnOpen();
                  Bui::BlockKeyboardInput(this);
              },
              [this]() {
                  navigation = {};
                  presentation.Reset();
                  model.OnClose();
                  if (closeDestination == CloseDestination::Options)
                      Bui::TransitionToScriptAndUnblock("Menu_Options", this);
                  else
                      Bui::UnblockKeyboardAfterRelease(this);
                  closeDestination = CloseDestination::Options;
              }) {
        routes.CreatePage<ModListPage>(ModListRoute, model, presentation);
        routes.CreatePage<ModDetailsPage>(ModDetailsRoute, model, presentation);
        routes.CreatePage<ModSettingsPage>(ModSettingsRoute, model, presentation);
    }

    bool IsCurrent(const PageRouteEntry &entry,
                   const ModMenuOwner *selected) const {
        return selected && entry.owner == *selected &&
               model.HasPage(entry.owner, entry.page.key);
    }

    PageRouteEntry *FindCurrentPageRoute() {
        for (PageRouteEntry &entry : pageRoutes) {
            if (routes.IsCurrentPage(entry.route))
                return &entry;
        }
        return nullptr;
    }

    const PageRouteEntry *FindPageRoute(const std::string &route) const {
        for (const PageRouteEntry &entry : pageRoutes) {
            if (entry.route == route)
                return &entry;
        }
        return nullptr;
    }

    void SynchronizeRoutes() {
        model.SynchronizeSelected();

        // A replaced Mod invalidates the selected settings action. Once its
        // pending edits are resolved, return to the nearest usable page.
        if (routes.IsCurrentPage(ModSettingsRoute) &&
            !model.GetSession().IsDirty() &&
            !model.GetSelectedDetailsAction()) {
            routes.Back();
        }

        const ModMenuOwner *sessionOwner = model.GetSession().GetSelectedMod();
        if (!sessionOwner || !model.IsCurrentOwner(*sessionOwner)) {
            if (routes.IsOpen()) {
                if (model.GetSession().IsDirty()) {
                    // Keep the Revert action available until the user decides
                    // what to do with edits that can no longer be applied.
                    if (!routes.IsCurrentPage(ModSettingsRoute)) {
                        routes.Open(ModListRoute);
                        routes.Push(ModSettingsRoute);
                    }
                } else if (!routes.IsCurrentPage(ModListRoute)) {
                    routes.Open(ModListRoute);
                }
            }
            sessionOwner = nullptr;
        }

        std::uint64_t revision = sessionOwner ? model.GetPageRevision(*sessionOwner) : 0;
        if (!sessionOwner && !observedRouteOwner)
            return;
        if (sessionOwner && observedRouteOwner && *sessionOwner == *observedRouteOwner &&
            revision == observedPageRevision)
            return;

        // Bui navigation can clear the session selection, so retain a value
        // only on frames that need route reconciliation.
        std::optional<ModMenuOwner> selected;
        if (sessionOwner)
            selected = *sessionOwner;

        // A removed page may still be the active route, or appear in history.
        // Back past invalid routes before removing them from Bui's registry.
        while (true) {
            PageRouteEntry *active = FindCurrentPageRoute();
            if (!active || IsCurrent(*active, selected ? &*selected : nullptr))
                break;
            if (!routes.Back())
                break;
        }

        sessionOwner = model.GetSession().GetSelectedMod();
        if (!sessionOwner || !selected || *sessionOwner != *selected)
            selected.reset();

        for (auto route = pageRoutes.begin(); route != pageRoutes.end();) {
            if (IsCurrent(*route, selected ? &*selected : nullptr)) {
                ++route;
                continue;
            }
            routes.RemovePage(route->route);
            route = pageRoutes.erase(route);
        }

        if (!selected) {
            observedRouteOwner.reset();
            observedPageRevision = 0;
            return;
        }
        revision = model.GetPageRevision(*selected);
        const ModMenuPageCatalog catalog = model.GetPages(*selected);
        for (const ModMenuPageInfo &page : catalog.pages) {
            const std::string route = PageRoute(*selected, page.key);
            if (routes.HasPage(route))
                continue;
            if (!routes.CreatePage<ModPage>(route, model, presentation,
                                            navigation, *selected, page))
                return;
            pageRoutes.push_back({*selected, page, route});
        }
        observedRouteOwner = *selected;
        observedPageRevision = revision;
    }

    void ApplyPageNavigation() {
        PageNavigationState::Pending pending = std::move(navigation.pending);
        navigation.pending = {};
        if (pending.sourceRoute.empty() ||
            !routes.IsCurrentPage(pending.sourceRoute))
            return;

        bool moved = false;
        switch (pending.navigation.action) {
        case BML_MOD_MENU_PAGE_BACK:
            moved = routes.Back();
            break;
        case BML_MOD_MENU_PAGE_CLOSE:
            moved = Close(CloseDestination::Options);
            break;
        case BML_MOD_MENU_PAGE_PUSH:
        case BML_MOD_MENU_PAGE_REPLACE: {
            const PageRouteEntry *source = FindPageRoute(pending.sourceRoute);
            if (!source || !IsCurrent(*source,
                    model.GetSession().GetSelectedMod()))
                break;
            const std::optional<ModMenuPageInfo> target = model.FindPage(
                source->owner, pending.navigation.targetPageId);
            if (!target)
                break;
            const std::string route = PageRoute(source->owner, target->key);
            moved = pending.navigation.action == BML_MOD_MENU_PAGE_PUSH
                ? routes.Push(route) : routes.Replace(route);
            break;
        }
        case BML_MOD_MENU_PAGE_NONE:
        default:
            return;
        }
        navigation.notice = moved ? "" : "Page navigation failed: target unavailable or history full.";
    }

    bool Close(CloseDestination destination) {
        closeDestination = destination;
        if (routes.Close())
            return true;
        closeDestination = CloseDestination::Options;
        return false;
    }

    ModMenuModel model;
    ModMenuPresentation presentation;
    PageNavigationState navigation;
    CloseDestination closeDestination = CloseDestination::Options;
    Bui::Menu routes;
    std::vector<PageRouteEntry> pageRoutes;
    std::optional<ModMenuOwner> observedRouteOwner;
    std::uint64_t observedPageRevision = 0;
};

ModMenu::ModMenu(ModContext &context) : m_State(std::make_unique<State>(context)) {}

ModMenu::~ModMenu() = default;

bool ModMenu::Open() {
    if (!m_State)
        return false;
    return m_State->routes.IsOpen() || m_State->routes.Open(ModListRoute);
}

bool ModMenu::Close() {
    return m_State && m_State->Close(State::CloseDestination::Options);
}

bool ModMenu::CloseForShutdown() {
    return m_State && m_State->Close(State::CloseDestination::Shutdown);
}

bool ModMenu::IsOpen() const {
    return m_State && m_State->routes.IsOpen();
}

void ModMenu::OnProcess() {
    if (!m_State)
        return;

    m_State->SynchronizeRoutes();
    if (!m_State->routes.Render())
        m_State->routes.Close();
    m_State->SynchronizeRoutes();
    m_State->ApplyPageNavigation();
    m_State->model.ProcessPendingCommand();
}
