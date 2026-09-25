#include "player/UiTestFramework.h"

#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

class ClipboardSnapshot {
public:
    ClipboardSnapshot() {
        const char *text = ImGui::GetClipboardText();
        if (text)
            m_Text = text;
    }

    ~ClipboardSnapshot() {
        ImGui::SetClipboardText(m_Text.c_str());
    }

private:
    std::string m_Text;
};

void RegisterConsoleScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test =
        IM_REGISTER_TEST(engine, ScenarioCategory, "console_command_and_message_board");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        ClipboardSnapshot clipboard;
        IM_CHECK(EnterLevelOneFromModList(ctx));
        IM_CHECK(SubmitConsoleCommand(ctx, "echo -n ui-automation-console"));
        IM_CHECK(WaitForItem(ctx, "**/ui-automation-console"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::Console));

        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/##MessageText"));
        const ImGuiTestItemInfo messages = ctx->ItemInfo("**/##MessageText");
        ctx->MouseMoveToPos(ImVec2(messages.RectFull.Max.x - 2.0f,
                                   messages.RectFull.GetCenter().y));
        ctx->MouseDown();
        ctx->MouseMoveToPos(ImVec2(messages.RectFull.Min.x + 2.0f,
                                   messages.RectFull.GetCenter().y));
        ctx->MouseUp();
        ctx->Yield();
        IM_CHECK(ctx->ItemExists("**/##CmdBar"));
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
        IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "ui-automation-console");

        ctx->ItemClick("**/##CmdBar");
        constexpr char ClipboardCommand[] =
            "echo -n clipboard-\xE4\xB8\xAD\xE6\x96\x87";
        ImGui::SetClipboardText(ClipboardCommand);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/clipboard-\xE4\xB8\xAD\xE6\x96\x87"));

        IM_CHECK(SubmitConsoleCommand(ctx, "help"));
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/##MessageText"));
        const ImGuiTestItemInfo scrollback = ctx->ItemInfo("**/##MessageText");
        ctx->MouseMoveToPos(ImVec2(scrollback.RectFull.Min.x + 1.0f,
                                   scrollback.RectFull.Max.y - 4.0f));
        ctx->MouseDown();
        ctx->MouseMoveToPos(ImVec2(scrollback.RectFull.Min.x + 1.0f,
                                   scrollback.RectFull.Min.y - 24.0f));
        IM_CHECK(WaitForItem(ctx, "**/ui-automation-console"));
        ctx->MouseUp();
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
        const char *selectedScrollback = ImGui::GetClipboardText();
        IM_CHECK(selectedScrollback != nullptr);
        IM_CHECK(std::string(selectedScrollback).find("ui-automation-console") != std::string::npos);
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));

        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("echo -n mouse-selection");
        const ImGuiTestItemInfo input = ctx->ItemInfo("**/##CmdBar");
        ctx->MouseMoveToPos(ImVec2(input.RectFull.Max.x - 2.0f,
                                   input.RectFull.GetCenter().y));
        ctx->MouseDown();
        ctx->MouseMoveToPos(ImVec2(input.RectFull.Min.x + 2.0f,
                                   input.RectFull.GetCenter().y));
        ctx->MouseUp();
        ctx->Yield();
        ImGuiInputTextState *inputState = ImGui::GetInputTextState(input.ID);
        IM_CHECK(inputState != nullptr && inputState->HasSelection());
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
        IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "echo -n mouse-selection");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_X);
        ctx->Yield();
        inputState = ImGui::GetInputTextState(input.ID);
        IM_CHECK(inputState != nullptr);
        IM_CHECK_STR_EQ(inputState->GetText(), "");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
        ctx->Yield();
        inputState = ImGui::GetInputTextState(input.ID);
        IM_CHECK(inputState != nullptr);
        IM_CHECK_STR_EQ(inputState->GetText(), "echo -n mouse-selection");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/mouse-selection"));

        // A command list runs both halves in order.
        IM_CHECK(SubmitConsoleCommand(ctx, "echo -n alpha && echo -n beta"));
        IM_CHECK(WaitForItem(ctx, "**/alpha"));
        IM_CHECK(WaitForItem(ctx, "**/beta"));

        // A pipeline hands the first stage's output to the second; only the
        // filtered line reaches the board.
        IM_CHECK(SubmitConsoleCommand(ctx, "echo $'p1\\np2' | grep p2"));
        IM_CHECK(WaitForItem(ctx, "**/p2"));

        // Tab completes the command word after a separator.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("echo -n tab-a; ech");
        ctx->KeyPress(ImGuiKey_Tab);
        ctx->KeyChars("-n tab-b");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/tab-b"));

        // A trailing operator keeps the bar open and grows it upward. Opening
        // and dismissing an ambiguous completion rail must not clear the
        // pending row; the message board still receives both commands.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("echo -n multi-a &&");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->KeyChars("e");
        ctx->KeyPress(ImGuiKey_Tab);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
        ctx->KeyChars("echo -n multi-b");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/multi-a"));
        IM_CHECK(WaitForItem(ctx, "**/multi-b"));

        // Recalling a multi-line history entry restores its previous rows
        // instead of embedding hidden newlines in the one-line text field.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_L);
        ctx->KeyPress(ImGuiKey_UpArrow);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
        ctx->KeyChars("echo -n multi-history");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/multi-a"));
        IM_CHECK(WaitForItem(ctx, "**/multi-history"));

        // An empty continuation row can accept the remaining physical rows of
        // a history suggestion without duplicating the pending prefix.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_L);
        ctx->KeyChars("echo -n multi-a &&");
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_End);
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/multi-a"));
        IM_CHECK(WaitForItem(ctx, "**/multi-history"));

        // Reverse search previews embedded newlines in its one-row rail, then
        // restores the selected entry as real continuation rows.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_L);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_R);
        ctx->Yield(2);
        ctx->KeyChars("multi-history");
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/multi-a"));
        IM_CHECK(WaitForItem(ctx, "**/multi-history"));

        // Argument completion keeps the command context across a backslash
        // continuation. Completing -n inserts the separating space.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("echo \\");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->KeyChars("-n");
        ctx->KeyPress(ImGuiKey_Tab);
        ctx->KeyChars("multi-completion");
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/multi-completion"));

        // A public Native Mod supplies three argument candidates. Walk the
        // visible rail in both directions before accepting alpha; this covers
        // Tab, Shift+Tab, Up, and Down through the real command registry.
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("public-authoring ");
        ctx->KeyPress(ImGuiKey_Tab);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiMod_Shift | ImGuiKey_Tab);
        ctx->KeyPress(ImGuiKey_UpArrow);
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->KeyPress(ImGuiKey_Tab);
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Enter);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/public-authoring:alpha"));

        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
    };
}

BML_REGISTER_UI_SCENARIO(Console, "console", "console_command_and_message_board",
                         &RegisterConsoleScenario)

} // namespace
