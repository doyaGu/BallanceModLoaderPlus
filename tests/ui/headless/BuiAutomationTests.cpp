#include "BML/Bui.h"
#include "UI/BuiInternal.h"
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
    ImVec4 Accent = ImVec4(0.38f, 0.69f, 0.94f, 1.0f);
};

struct ColorStringState {
    std::string Value = "invalid";
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
        Bui::ColorButton("Accent", &state.Accent);

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

        ctx->ItemInputValue("**/##Text", "#11223344");
        IM_CHECK_FLOAT_NEAR_EQ(state.Accent.x, 0x11 / 255.0f, 0.0001f);
        IM_CHECK_FLOAT_NEAR_EQ(state.Accent.y, 0x22 / 255.0f, 0.0001f);
        IM_CHECK_FLOAT_NEAR_EQ(state.Accent.z, 0x33 / 255.0f, 0.0001f);
        IM_CHECK_FLOAT_NEAR_EQ(state.Accent.w, 0x44 / 255.0f, 0.0001f);
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

    test = IM_REGISTER_TEST(engine, "bui", "invalid_color_string_remains_repairable");
    test->SetVarsDataType<ColorStringState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ColorStringState &state = ctx->GetVars<ColorStringState>();
        ImGui::SetNextWindowSize(ImVec2(700.0f, 240.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Color String", nullptr, ImGuiWindowFlags_NoSavedSettings);
        Bui::ColorStringButton("Theme color", &state.Value);
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        ColorStringState &state = ctx->GetVars<ColorStringState>();
        ctx->SetRef("Bui Color String");

        IM_CHECK_EQ(state.Value, "invalid");
        ctx->ItemInputValue("**/##InputText", "#AABBCC80");
        IM_CHECK_EQ(state.Value, "#AABBCC80");
        ctx->Yield();
        IM_CHECK(ctx->ItemExists("**/ColorSwatch"));
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
