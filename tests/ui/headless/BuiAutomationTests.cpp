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

struct ColorPopupState {
    std::string Value = "#61B0F0FF";
    Bui::ColorPickerArea Area;
};

struct NavigationShortcutState {
    char Text[32] = "draft";
    int PreviousCount = 0;
    int NextCount = 0;
    int BackCount = 0;
};

struct KeyboardButtonState {
    bool Enabled = false;
    int EnabledChanges = 0;
    int Mode = 0;
    int ModeChanges = 0;
    bool Listening = false;
    ImGuiKeyChord Chord = 0;
    int BindingChanges = 0;
    int PreviousCount = 0;
    int BackCount = 0;
};

struct InputRowFocusState {
    char Text[32] = "draft";
    ImGuiID Input = 0;
    bool RequestFocus = true;
};

struct ShortcutButtonState {
    char Text[32] = "draft";
    int ActivationCount = 0;
};

struct ButtonFamilyState {
    int MainCount = 0;
    int OkCount = 0;
    int BackCount = 0;
    int OptionCount = 0;
    int LevelCount = 0;
    int SmallCount = 0;
    int LeftCount = 0;
    int RightCount = 0;
    int PlusCount = 0;
    int MinusCount = 0;
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
    LandingPage(ImGuiWindow **window, ImGuiID *firstItem, ImGuiID *item)
        : m_Window(window), m_FirstItem(firstItem), m_Item(item) {}

    Bui::PageAction OnFrame() override {
        *m_Window = ImGui::GetCurrentWindow();
        *m_FirstItem = ImGui::GetID("Previous item");
        Bui::MainButton("Previous item");
        *m_Item = ImGui::GetID("Open options");
        if (Bui::MainButton("Open options"))
            return Bui::PageAction::Push("options");
        return Bui::PageAction::None();
    }

private:
    ImGuiWindow **m_Window;
    ImGuiID *m_FirstItem;
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
        Menu.CreatePage<LandingPage>("landing", &CurrentWindow, &FirstItem, &CurrentItem);
        Menu.CreatePage<OptionsPage>("options", &CurrentWindow, &CurrentItem);
        Menu.Open("landing");
    }

    ImGuiWindow *CurrentWindow = nullptr;
    ImGuiID FirstItem = 0;
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

    test = IM_REGISTER_TEST(engine, "bui", "color_picker_stays_inside_page");
    test->SetVarsDataType<ColorPopupState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ColorPopupState &state = ctx->GetVars<ColorPopupState>();
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
        ImGui::Begin("Bui Color Popup", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorScreenPos(ImVec2(
            viewport->WorkPos.x + viewport->WorkSize.x * 0.35f,
            viewport->WorkPos.y + viewport->WorkSize.y * 0.66f));
        state.Area = {
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.40f,
                   viewport->WorkPos.y + viewport->WorkSize.y * 0.15f),
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.60f,
                   viewport->WorkPos.y + viewport->WorkSize.y * 0.82f),
        };
        Bui::ColorStringButton("Accent", &state.Value, state.Area);
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        ctx->SetRef("Bui Color Popup");
        ctx->ItemClick("**/ColorSwatch");
        ctx->Yield();

        ImGuiContext &imgui = *GImGui;
        IM_CHECK_GT(imgui.OpenPopupStack.Size, 0);
        ImGuiWindow *popup = imgui.OpenPopupStack.back().Window;
        IM_CHECK(popup != nullptr);
        const Bui::ColorPickerArea &page = ctx->GetVars<ColorPopupState>().Area;
        IM_CHECK_GE(popup->Pos.x, page.minimum.x);
        IM_CHECK_GE(popup->Pos.y, page.minimum.y);
        IM_CHECK_LE(popup->Pos.x + popup->Size.x, page.maximum.x);
        IM_CHECK_LE(popup->Pos.y + popup->Size.y, page.maximum.y);

        const std::string initial = ctx->GetVars<ColorPopupState>().Value;
        ctx->MouseMoveToPos(ImVec2(popup->Pos.x + 20.0f, popup->Pos.y + 20.0f));
        ctx->MouseClick();
        ctx->Yield();
        IM_CHECK_NE(ctx->GetVars<ColorPopupState>().Value, initial);
    };

    test = IM_REGISTER_TEST(engine, "bui", "navigation_shortcuts_do_not_interrupt_text_input");
    test->SetVarsDataType<NavigationShortcutState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        NavigationShortcutState &state = ctx->GetVars<NavigationShortcutState>();
        ImGui::SetNextWindowSize(ImVec2(700.0f, 240.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Navigation Shortcuts", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::InputText("Text", state.Text, IM_ARRAYSIZE(state.Text));
        if (Bui::NavLeft(0.3f, 0.2f))
            ++state.PreviousCount;
        if (Bui::NavRight(0.5f, 0.2f))
            ++state.NextCount;
        if (Bui::NavBack(0.1f, 0.2f))
            ++state.BackCount;
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        NavigationShortcutState &state = ctx->GetVars<NavigationShortcutState>();
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->SetRef("Bui Navigation Shortcuts");
        ctx->ItemClick("Text");
        IM_CHECK(ImGui::IsAnyItemActive());

        ctx->KeyPress(ImGuiKey_PageUp);
        IM_CHECK_EQ(state.PreviousCount, 0);
        ctx->ItemClick("Text");
        ctx->KeyPress(ImGuiKey_PageDown);
        IM_CHECK_EQ(state.NextCount, 0);
        ctx->ItemClick("Text");

        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK_EQ(state.BackCount, 0);
        ctx->KeyPress(ImGuiKey_PageUp);
        ctx->KeyPress(ImGuiKey_PageDown);
        IM_CHECK_EQ(state.PreviousCount, 1);
        IM_CHECK_EQ(state.NextCount, 1);
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK_EQ(state.BackCount, 1);
    };

    test = IM_REGISTER_TEST(engine, "bui", "option_rows_accept_keyboard_input");
    test->SetVarsDataType<KeyboardButtonState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        KeyboardButtonState &state = ctx->GetVars<KeyboardButtonState>();
        ImGui::SetNextWindowSize(ImVec2(700.0f, 360.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Keyboard Buttons", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (Bui::YesNoButton("Enabled", &state.Enabled))
            ++state.EnabledChanges;
        const char *modes[] = {"First", "Second", "Third"};
        if (Bui::RadioButton("Mode", &state.Mode, modes, IM_ARRAYSIZE(modes)))
            ++state.ModeChanges;
        if (Bui::KeyButton("Binding", &state.Listening, &state.Chord))
            ++state.BindingChanges;
        if (Bui::NavLeft(0.3f, 0.8f))
            ++state.PreviousCount;
        if (Bui::NavBack(0.5f, 0.8f))
            ++state.BackCount;
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        KeyboardButtonState &state = ctx->GetVars<KeyboardButtonState>();
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->SetRef("Bui Keyboard Buttons");

        ctx->NavMoveTo("Enabled");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK_EQ(state.Enabled, true);
        IM_CHECK_EQ(state.EnabledChanges, 1);

        ctx->NavMoveTo("Mode");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK_EQ(state.Mode, 1);
        IM_CHECK_EQ(state.ModeChanges, 1);

        ctx->KeyPress(ImGuiKey_RightArrow);
        IM_CHECK_EQ(state.Mode, 2);
        IM_CHECK_EQ(state.ModeChanges, 2);
        ctx->KeyPress(ImGuiKey_LeftArrow);
        IM_CHECK_EQ(state.Mode, 1);
        IM_CHECK_EQ(state.ModeChanges, 3);

        ctx->NavMoveTo("Binding");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK_EQ(state.Listening, true);
        ctx->KeyPress(ImGuiKey_PageUp);
        IM_CHECK_EQ(state.Listening, false);
        IM_CHECK_EQ(state.Chord, ImGuiKey_PageUp);
        IM_CHECK_EQ(state.BindingChanges, 1);
        IM_CHECK_EQ(state.PreviousCount, 0);

        ctx->NavMoveTo("Binding");
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK_EQ(state.Listening, false);
        IM_CHECK_EQ(state.Chord, ImGuiKey_Escape);
        IM_CHECK_EQ(state.BindingChanges, 2);
        IM_CHECK_EQ(state.BackCount, 0);
    };

    test = IM_REGISTER_TEST(engine, "bui", "input_row_focuses_its_editor");
    test->SetVarsDataType<InputRowFocusState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        InputRowFocusState &state = ctx->GetVars<InputRowFocusState>();
        ImGui::SetNextWindowSize(ImVec2(700.0f, 240.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Input Row", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushID("Text");
        state.Input = ImGui::GetID("##InputText");
        ImGui::PopID();
        if (state.RequestFocus) {
            ImGui::SetKeyboardFocusHere();
            state.RequestFocus = false;
        }
        Bui::InputTextButton("Text", state.Text, IM_ARRAYSIZE(state.Text));
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        InputRowFocusState &state = ctx->GetVars<InputRowFocusState>();
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->Yield();
        IM_CHECK_EQ(GImGui->NavId, state.Input);
        IM_CHECK_EQ(GImGui->ActiveId, state.Input);
    };

    test = IM_REGISTER_TEST(engine, "bui", "item_shortcut_activates_non_navigation_button");
    test->SetVarsDataType<ShortcutButtonState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ShortcutButtonState &state = ctx->GetVars<ShortcutButtonState>();
        ImGui::SetNextWindowSize(ImVec2(360.0f, 160.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Shortcut Input", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::InputText("Text", state.Text, IM_ARRAYSIZE(state.Text));
        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(240.0f, 160.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Shortcut Button", nullptr,
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        ImGui::SetNextItemShortcut(ImGuiKey_RightArrow, ImGuiInputFlags_RouteGlobal);
        if (Bui::RightButton("Enter"))
            ++state.ActivationCount;
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        ShortcutButtonState &state = ctx->GetVars<ShortcutButtonState>();
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->SetRef("Bui Shortcut Input");
        ctx->ItemClick("Text");
        ctx->KeyPress(ImGuiKey_RightArrow);
        IM_CHECK_EQ(state.ActivationCount, 0);
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield();
        ctx->KeyPress(ImGuiKey_RightArrow);
        IM_CHECK_EQ(state.ActivationCount, 1);
    };

    test = IM_REGISTER_TEST(engine, "bui", "button_family_accepts_nav_activation");
    test->SetVarsDataType<ButtonFamilyState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ButtonFamilyState &state = ctx->GetVars<ButtonFamilyState>();
        ImGui::SetNextWindowSize(ImVec2(700.0f, 700.0f), ImGuiCond_Always);
        ImGui::Begin("Bui Button Family", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (Bui::MainButton("Main"))
            ++state.MainCount;
        if (Bui::OkButton("Ok"))
            ++state.OkCount;
        if (Bui::BackButton("Back"))
            ++state.BackCount;
        if (Bui::OptionButton("Option"))
            ++state.OptionCount;
        if (Bui::LevelButton("Level"))
            ++state.LevelCount;
        if (Bui::SmallButton("Small"))
            ++state.SmallCount;
        if (Bui::LeftButton("Left"))
            ++state.LeftCount;
        if (Bui::RightButton("Right"))
            ++state.RightCount;
        if (Bui::PlusButton("Plus"))
            ++state.PlusCount;
        if (Bui::MinusButton("Minus"))
            ++state.MinusCount;
        ImGui::End();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        ButtonFamilyState &state = ctx->GetVars<ButtonFamilyState>();
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->SetRef("Bui Button Family");
        ctx->ItemNavActivate("Main");
        ctx->ItemNavActivate("Ok");
        ctx->ItemNavActivate("Back");
        ctx->ItemNavActivate("Option");
        ctx->ItemNavActivate("Level");
        ctx->ItemNavActivate("Small");
        ctx->ItemNavActivate("Left");
        ctx->ItemNavActivate("Right");
        ctx->ItemNavActivate("Plus");
        ctx->ItemNavActivate("Minus");
        IM_CHECK_EQ(state.MainCount, 1);
        IM_CHECK_EQ(state.OkCount, 1);
        IM_CHECK_EQ(state.BackCount, 1);
        IM_CHECK_EQ(state.OptionCount, 1);
        IM_CHECK_EQ(state.LevelCount, 1);
        IM_CHECK_EQ(state.SmallCount, 1);
        IM_CHECK_EQ(state.LeftCount, 1);
        IM_CHECK_EQ(state.RightCount, 1);
        IM_CHECK_EQ(state.PlusCount, 1);
        IM_CHECK_EQ(state.MinusCount, 1);
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

    test = IM_REGISTER_TEST(engine, "bui", "menu_keyboard_drives_navigation");
    test->SetVarsDataType<MenuState>();
    test->GuiFunc = [](ImGuiTestContext *ctx) {
        ctx->GetVars<MenuState>().Menu.Render();
    };
    test->TestFunc = [](ImGuiTestContext *ctx) {
        MenuState &state = ctx->GetVars<MenuState>();
        Bui::Menu &menu = state.Menu;
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->Yield();

        IM_CHECK(menu.IsCurrentPage("landing"));
        IM_CHECK_EQ(GImGui->NavWindow, state.CurrentWindow);
        IM_CHECK_EQ(GImGui->NavId, state.FirstItem);

        ctx->KeyPress(ImGuiKey_DownArrow);
        IM_CHECK_EQ(GImGui->NavId, state.CurrentItem);

        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(menu.IsCurrentPage("options"));
        ctx->Yield();
        IM_CHECK_EQ(GImGui->NavWindow, state.CurrentWindow);
        IM_CHECK_EQ(GImGui->NavId, state.CurrentItem);

        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(menu.IsCurrentPage("landing"));
        ctx->Yield();
        IM_CHECK_EQ(GImGui->NavWindow, state.CurrentWindow);
        IM_CHECK_EQ(GImGui->NavId, state.FirstItem);
    };
}
