#include "ModMenu/ModMenu.h"

#include <memory>

#include "BML/Bui.h"

#include "UI/BuiInternal.h"
#include "ModMenu/ModMenuModel.h"
#include "ModMenu/ModMenuPresentation.h"

namespace {
    constexpr char ModListRoute[] = "mods";
    constexpr char ModDetailsRoute[] = "details";
    constexpr char ModSettingsRoute[] = "settings";
    constexpr char ModPageRoute[] = "page";

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
            if (action == ModMenuRouteAction::OpenPage)
                return Bui::PageAction::Push(ModPageRoute);
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
        ModPage(ModMenuModel &model, ModMenuPresentation &presentation)
            : m_Model(model), m_Presentation(presentation) {}

    protected:
        void OnEnter(Bui::PageEnterReason) override {
            m_Entered = m_Model.EnterPage() == BML_OK;
            m_Status = m_Entered ? ModMenuPageStatus::Ready
                                 : ModMenuPageStatus::EnterFailed;
        }

        Bui::PageAction OnFrame() override {
            const ModMenuPagePresentationResult result =
                m_Presentation.DrawPage(m_Model, m_Status);
            m_Status = result.status;
            if (result.action == ModMenuRouteAction::Back)
                return Bui::PageAction::Back();
            if (result.action == ModMenuRouteAction::Close)
                return Bui::PageAction::Close();
            return Bui::PageAction::None();
        }

        void OnLeave(Bui::PageLeaveReason reason) override {
            if (m_Entered) {
                (void) m_Model.LeavePage(
                    reason == Bui::PageLeaveReason::Close
                        ? BML_MOD_MENU_PAGE_LEAVE_CLOSE
                        : BML_MOD_MENU_PAGE_LEAVE_BACK);
            }
            m_Entered = false;
            m_Status = ModMenuPageStatus::Ready;
        }

    private:
        ModMenuModel &m_Model;
        ModMenuPresentation &m_Presentation;
        bool m_Entered = false;
        ModMenuPageStatus m_Status = ModMenuPageStatus::Ready;
    };
}

struct ModMenu::State {
    enum class CloseDestination {
        Options,
        Shutdown,
    };

    explicit State(ModContext &context)
        : model(context),
          routes(
              [this]() {
                  model.OnOpen();
                  Bui::BlockKeyboardInput(this);
              },
              [this]() {
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
        routes.CreatePage<ModPage>(ModPageRoute, model, presentation);
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
    CloseDestination closeDestination = CloseDestination::Options;
    Bui::Menu routes;
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

void ModMenu::OnProcess() {
    if (!m_State)
        return;

    if (!m_State->routes.Render())
        m_State->routes.Close();
    m_State->model.ProcessPendingCommand();
}
