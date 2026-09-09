#include "UI/Automation/UiTestFramework.h"

#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterConsoleScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test =
        IM_REGISTER_TEST(engine, ScenarioCategory, "console_command_and_message_board");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(EnterLevelOneFromModList(ctx));
        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        IM_CHECK(RunGameAction(ctx, GameAction::MarkConsoleSurface));
        ctx->SleepStandard();
        ctx->ItemClick("**/##CmdBar");
        ctx->KeyChars("echo -n ui-automation-console");
        ctx->KeyPress(ImGuiKey_Enter);

        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
        IM_CHECK(WaitForItem(ctx, "**/ui-automation-console"));

        ctx->KeyPress(ImGuiKey_Slash);
        IM_CHECK(WaitForItem(ctx, "**/##CmdBar"));
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##CmdBar"));
    };
}

BML_REGISTER_UI_SCENARIO(Console, "console", "console_command_and_message_board",
                         &RegisterConsoleScenario)

} // namespace
