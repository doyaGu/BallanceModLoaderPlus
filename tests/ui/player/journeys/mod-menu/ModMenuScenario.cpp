#include "player/UiTestFramework.h"

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterModMenuScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "mod_menu_all_pages");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(ObserveModList(ctx));
        IM_CHECK(MenuPagesContain(ctx, {"Ballance Mod Loader", "New Ball Type"}));
        ctx->ItemClick("**/Ballance Mod Loader");
        IM_CHECK(WaitForItem(ctx, "**/Back"));
        IM_CHECK(MenuPagesContain(ctx, {"GUI", "Graphics", "HUD", "CommandBar",
                                       "CustomMap", "Tweak"}));

        IM_CHECK(OpenConfigCategory(ctx, "GUI", "**/FontFilename"));
        IM_CHECK(MenuPagesContain(ctx, {"FontFilename", "FontSize", "FontFallbacks",
                                       "UseSystemFontFallbacks", "EnableIniSettings"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "Graphics", "**/UnlockFrameRate"));
        IM_CHECK(MenuPagesContain(ctx,
                                  {"UnlockFrameRate", "SetMaxFrameRate", "WidescreenFix"}));
        IM_CHECK(ToggleConfigBoolean(ctx, "**/UnlockFrameRate"));
        IM_CHECK(ToggleConfigBoolean(ctx, "**/UnlockFrameRate"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::ModMenu));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "HUD", "**/ShowTitle"));
        IM_CHECK(MenuPagesContain(ctx,
                                  {"ShowTitle", "ShowFPS", "ShowSRTimer", "FPSUpdateFrequency"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "CommandBar", "**/MessageDuration"));
        IM_CHECK(MenuPagesContain(ctx,
                                  {"MessageDuration", "TabColumns", "LineSpacing",
                                   "MessageBackgroundAlpha", "FadeMaxAlpha"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "CustomMap", "**/LevelNumber"));
        IM_CHECK(MenuPagesContain(ctx, {"LevelNumber", "ShowTooltip", "MaxDepth"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "Tweak", "**/LanternAlphaTest"));
        IM_CHECK(MenuPagesContain(ctx,
                                  {"LanternAlphaTest", "FixLifeBallFreeze", "Overclock"}));
        ctx->ItemClick("**/Back");

        ctx->ItemClick("**/Back");
        IM_CHECK(WaitForItem(ctx, "**/Ballance Mod Loader"));
        IM_CHECK(MenuPagesContain(ctx, {"Ballance Mod Loader", "New Ball Type"}));
        ctx->ItemClick("**/New Ball Type");
        IM_CHECK(WaitForItem(ctx, "**/Back"));
        ctx->ItemClick("**/Back");
        IM_CHECK(WaitForItem(ctx, "**/Ballance Mod Loader"));

        IM_CHECK(LeaveModListForOptions(ctx));
        IM_CHECK(!ctx->ItemExists("**/FontFilename"));
    };
}

BML_REGISTER_UI_SCENARIO(ModMenu, "mod-menu", "mod_menu_all_pages", &RegisterModMenuScenario)

} // namespace
