#include "UI/Automation/UiTestFramework.h"

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterCustomMapsScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "custom_maps_roundtrip");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(EnterStartMenuFromModList(ctx));

        // Use the production ImGui arrow attached to the native Start menu.
        ctx->ItemClick("**/Enter_Custom_Maps");
        IM_CHECK(WaitForItem(ctx, "**/##SearchBar"));
        IM_CHECK(WaitForItem(ctx, "**/BMLUiAutomation"));
        IM_CHECK(RunGameAction(ctx, GameAction::MarkCustomMapsSurface));
        ctx->SleepStandard();

        ctx->ItemInputValue("**/##SearchBar", "BMLUi");
        IM_CHECK(WaitForItem(ctx, "**/BMLUiAutomation"));
        ctx->ItemInputValue("**/##SearchBar", "no-such-map");
        IM_CHECK(WaitForItemToDisappear(ctx, "**/BMLUiAutomation"));
        ctx->ItemInputValue("**/##SearchBar", "");
        IM_CHECK(WaitForItem(ctx, "**/BMLUiAutomation"));
        ctx->ItemClick("**/Back");

        IM_CHECK(WaitForItem(ctx, "**/Enter_Custom_Maps"));
        IM_CHECK(EnterLevelOneFromStartMenu(ctx));
    };
}

BML_REGISTER_UI_SCENARIO(CustomMaps, "custom-maps", "custom_maps_roundtrip",
                         &RegisterCustomMapsScenario)

} // namespace
