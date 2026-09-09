#include "UI/Automation/UiTestFramework.h"

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterHudScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "hud_overlay");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(EnterLevelOneFromModList(ctx));
        IM_CHECK(RunGameAction(ctx, GameAction::ShowAllHud));
        IM_CHECK(WaitForItem(ctx, "**/title"));
        IM_CHECK(WaitForItem(ctx, "**/fps"));
        IM_CHECK(WaitForItem(ctx, "**/sr"));
        IM_CHECK(RunGameAction(ctx, GameAction::MarkHudSurface));
        ctx->SleepStandard();

        IM_CHECK(RunGameAction(ctx, GameAction::RestoreHud));
        IM_CHECK(WaitForItemToDisappear(ctx, "**/sr"));
    };
}

BML_REGISTER_UI_SCENARIO(Hud, "hud", "hud_overlay", &RegisterHudScenario)

} // namespace
