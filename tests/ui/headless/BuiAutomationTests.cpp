#include "BML/Bui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

struct WidgetState {
    int LaunchCount = 0;
    bool Selected = false;
    bool Fullscreen = false;
    int Lives = 3;
};

class LifecycleWindow final : public Bui::Window {
public:
    LifecycleWindow() : Bui::Window("Bui Lifecycle") {}

    int DrawCount = 0;
    int HideCount = 0;

    void OnPreBegin() override {
        ImGui::SetNextWindowPos(ImVec2(60.0f, 60.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Always);
    }

    void OnDraw() override {
        ++DrawCount;
        ImGui::TextUnformatted("Close this window through its title-bar button.");
    }

    void OnHide() override {
        ++HideCount;
    }
};

struct WindowState {
    LifecycleWindow Window;
};

class LandingPage final : public Bui::Page {
public:
    LandingPage(ImGuiWindow **window, ImGuiID *item)
        : m_Window(window), m_Item(item) {}

    Bui::PageAction OnFrame() override {
        *m_Window = ImGui::GetCurrentWindow();
        *m_Item = ImGui::GetID("Open options");
        if (Bui::MainButton("Open options"))
            return Bui::PageAction::Push("options");
        return Bui::PageAction::None();
    }

private:
    ImGuiWindow **m_Window;
    ImGuiID *m_Item;
};

class OptionsPage final : public Bui::Page {
public:
    OptionsPage(ImGuiWindow **window, ImGuiID *item)
        : m_Window(window), m_Item(item) {}

    Bui::PageAction OnFrame() override {
        *m_Window = ImGui::GetCurrentWindow();
        *m_Item = ImGui::GetID("Back");
        if (Bui::BackButton("Back"))
            return Bui::PageAction::Back();
        return Bui::PageAction::None();
    }

private:
    ImGuiWindow **m_Window;
    ImGuiID *m_Item;
};

struct MenuState {
    MenuState() : Menu([] {}, [] {}) {
        Menu.CreatePage<LandingPage>("landing", &CurrentWindow, &CurrentItem);
        Menu.CreatePage<OptionsPage>("options", &CurrentWindow, &CurrentItem);
        Menu.Open("landing");
    }

    ImGuiWindow *CurrentWindow = nullptr;
    ImGuiID CurrentItem = 0;
    Bui::Menu Menu;
};

} // namespace

void RegisterBuiAutomationTests(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, "bui", "widgets_accept_simulated_input");
    test->SetVarsDataType<WidgetState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        WidgetState &state = ctx->GetVars<WidgetState>();
        ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(700.0f, 620.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Widgets", nullptr, ImGuiWindowFlags_NoSavedSettings);

        if (Bui::MainButton("Launch"))
            ++state.LaunchCount;
        Bui::LevelButton("Level 02", &state.Selected);
        Bui::YesNoButton("Fullscreen", &state.Fullscreen);
        Bui::InputIntButton("Lives", &state.Lives);

        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        WidgetState &state = ctx->GetVars<WidgetState>();
        ctx->SetRef("Bui Widgets");

        IM_CHECK_EQ(state.LaunchCount, 0);
        ctx->ItemClick("Launch");
        IM_CHECK_EQ(state.LaunchCount, 1);

        IM_CHECK_EQ(state.Selected, false);
        ctx->ItemClick("Level 02");
        IM_CHECK_EQ(state.Selected, true);

        IM_CHECK_EQ(state.Fullscreen, false);
        const ImGuiTestItemInfo fullscreen = ctx->ItemInfo("Fullscreen");
        ctx->MouseMoveToPos(ImVec2(fullscreen.RectFull.Max.x - 4.0f,
                                   fullscreen.RectFull.GetCenter().y));
        ctx->MouseClick();
        IM_CHECK_EQ(state.Fullscreen, true);
        ctx->MouseClick();
        IM_CHECK_EQ(state.Fullscreen, false);

        ctx->ItemInputValue("**/##InputInt", 7);
        IM_CHECK_EQ(state.Lives, 7);
    };

    test = IM_REGISTER_TEST(engine, "bui", "window_close_runs_lifecycle");
    test->SetVarsDataType<WindowState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ctx->GetVars<WindowState>().Window.Render();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        LifecycleWindow &window = ctx->GetVars<WindowState>().Window;
        ctx->Yield();
        IM_CHECK(window.IsVisible());
        IM_CHECK_GT(window.DrawCount, 0);

        ctx->WindowClose("//Bui Lifecycle");
        ctx->Yield();

        IM_CHECK_EQ(window.IsVisible(), false);
        IM_CHECK_EQ(window.HideCount, 1);
    };

    test = IM_REGISTER_TEST(engine, "bui", "menu_buttons_drive_navigation");
    test->SetVarsDataType<MenuState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ctx->GetVars<MenuState>().Menu.Render();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        MenuState &state = ctx->GetVars<MenuState>();
        Bui::Menu &menu = state.Menu;
        ctx->Yield();
        IM_CHECK_NE(state.CurrentWindow, nullptr);
        IM_CHECK_NE(state.CurrentItem, 0u);
        IM_CHECK(menu.IsCurrentPage("landing"));

        ctx->ItemClick(state.CurrentItem);
        IM_CHECK(menu.IsCurrentPage("options"));

        ctx->Yield();
        ctx->ItemClick(state.CurrentItem);
        IM_CHECK(menu.IsCurrentPage("landing"));
    };
}
