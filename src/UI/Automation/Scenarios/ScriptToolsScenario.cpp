#include "UI/Automation/UiTestFramework.h"

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

#if BML_ENABLE_ANGELSCRIPT
namespace {

using namespace UiAutomation::Test;

void RegisterScriptToolsScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "script_developer_tools");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(EnterLevelOneFromModList(ctx));
        IM_CHECK(SubmitConsoleCommand(ctx, "script panel"));
        IM_CHECK(WaitForItem(ctx, "**/Script Developer Tools"));
        IM_CHECK(WaitForItem(ctx, "**/Diag"));
        IM_CHECK(WaitForItem(ctx, "**/Reload"));
        IM_CHECK(WaitForItem(ctx, "**/Res"));
        IM_CHECK(WaitForItem(ctx, "**/Deps"));
        IM_CHECK(WaitForItem(ctx, "**/Logs"));
        ctx->ItemClick("**/script-dev-tabs/Logs");
        IM_CHECK(WaitForItem(ctx, "**/Advanced"));
        IM_CHECK(WaitForItem(ctx, "**/This Mod"));
        IM_CHECK(WaitForItem(ctx, "**/Reload Only"));
        IM_CHECK(WaitForItem(ctx, "**/Pause"));
        ctx->ItemClick("**/Advanced");
        IM_CHECK(WaitForItem(ctx, "**/##script-dev-code"));
        IM_CHECK(WaitForItem(ctx, "**/##script-dev-attempt"));
        ctx->ItemInputValue("**/##script-dev-code", "ui");
        ctx->ItemInputValue("**/##script-dev-attempt", "1");
        ctx->ItemCheck("**/This Mod");
        ctx->ItemCheck("**/Reload Only");
        ctx->ItemCheck("**/Pause");
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::ScriptTools));
        ctx->ItemUncheck("**/Pause");
        ctx->ItemUncheck("**/Reload Only");
        ctx->ItemUncheck("**/This Mod");
        ctx->ItemInputValue("**/##script-dev-code", "");
        ctx->ItemInputValue("**/##script-dev-attempt", "");
        ctx->ItemClick("**/Hide Advanced");
        IM_CHECK(WaitForItemToDisappear(ctx, "**/##script-dev-code"));

        ctx->ItemClick("**/script-dev-tabs/Deps");
        IM_CHECK(WaitForItem(ctx, "**/No script mod selected."));
        ctx->ItemClick("**/script-dev-tabs/Res");
        IM_CHECK(WaitForItem(ctx, "**/No script mod selected."));
        ctx->ItemClick("**/script-dev-tabs/Reload");
        IM_CHECK(WaitForItem(ctx, "**/No script mod selected."));
        ctx->ItemClick("**/script-dev-tabs/Diag");
        IM_CHECK(WaitForItem(ctx, "**/No script mod selected."));
        ctx->ItemClick("**/script-dev-tabs/Logs");

        IM_CHECK(WaitForItem(ctx, "**/Close"));
        ctx->ItemClick("**/Close");
        IM_CHECK(WaitForItemToDisappear(ctx, "**/script-dev-tabs/Diag"));
    };
}

BML_REGISTER_UI_SCENARIO(ScriptTools, "script-tools", "script_developer_tools",
                         &RegisterScriptToolsScenario)

} // namespace
#endif
