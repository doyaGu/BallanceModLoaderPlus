#include "UI/Automation/UiTestFramework.h"

#include <array>

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterHudScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "hud_overlay");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(OpenModConfigCategory(ctx, "Ballance Mod Loader", "HUD", "**/ShowTitle"));
        for (const char *setting : std::array{"**/ShowTitle", "**/ShowFPS", "**/ShowSRTimer"}) {
            const bool initial = ctx->ItemIsChecked(setting);
            IM_CHECK(ToggleConfigBoolean(ctx, setting));
            IM_CHECK(ctx->ItemIsChecked(setting) != initial);
            IM_CHECK(ToggleConfigBoolean(ctx, setting));
            IM_CHECK(ctx->ItemIsChecked(setting) == initial);
            if (!initial)
                IM_CHECK(ToggleConfigBoolean(ctx, setting));
            IM_CHECK(ctx->ItemIsChecked(setting));
        }
        ctx->ItemClick("**/Back");
        IM_CHECK(WaitForItem(ctx, "**/HUD"));
        ctx->ItemClick("**/Back");
        IM_CHECK(ObserveModList(ctx));

        IM_CHECK(EnterLevelOneFromModList(ctx));
        IM_CHECK(WaitForItem(ctx, "**/title"));
        IM_CHECK(WaitForItem(ctx, "**/fps"));
        IM_CHECK(WaitForItem(ctx, "**/sr"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::Hud));
    };
}

BML_REGISTER_UI_SCENARIO(Hud, "hud", "hud_overlay", &RegisterHudScenario)

} // namespace
