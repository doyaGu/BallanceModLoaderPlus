#include "UI/Automation/UiTestFramework.h"

#include <initializer_list>

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterModMenuScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "mod_menu_all_pages");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        const auto hasAll = [ctx](std::initializer_list<const char *> paths) {
            for (const char *path : paths) {
                if (!WaitForItem(ctx, path))
                    return false;
            }
            return true;
        };
        IM_CHECK(ObserveModList(ctx));
        ctx->ItemClick("**/Ballance Mod Loader");

        IM_CHECK(OpenConfigCategory(ctx, "GUI", "**/FontFilename"));
        IM_CHECK(hasAll({"**/FontSize", "**/FontRanges", "**/EnableSecondaryFont", "**/NextPage"}));
        ctx->ItemClick("**/NextPage");
        IM_CHECK(hasAll({"**/SecondaryFontFilename", "**/SecondaryFontSize",
                         "**/SecondaryFontRanges", "**/EnableIniSettings"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "Graphics", "**/UnlockFrameRate"));
        IM_CHECK(hasAll({"**/SetMaxFrameRate", "**/WidescreenFix"}));
        ctx->ItemInputValue("**/SetMaxFrameRate/##InputInt", 17);
        IM_CHECK(WaitForItem(ctx, "**/Revert"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::ModMenu));
        ctx->ItemClick("**/Revert");
        IM_CHECK(WaitForItemToDisappear(ctx, "**/Revert"));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "HUD", "**/ShowTitle"));
        IM_CHECK(hasAll({"**/ShowFPS", "**/ShowSRTimer", "**/FPSUpdateFrequency"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "CommandBar", "**/MessageDuration"));
        IM_CHECK(hasAll(
            {"**/TabColumns", "**/LineSpacing", "**/MessageBackgroundAlpha", "**/NextPage"}));
        ctx->ItemClick("**/NextPage");
        IM_CHECK(hasAll({"**/WindowBackgroundAlpha", "**/FadeMaxAlpha"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "CustomMap", "**/LevelNumber"));
        IM_CHECK(hasAll({"**/ShowTooltip", "**/MaxDepth"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(OpenConfigCategory(ctx, "Tweak", "**/LanternAlphaTest"));
        IM_CHECK(hasAll({"**/FixLifeBallFreeze", "**/Overclock"}));
        ctx->ItemClick("**/Back");

        ctx->ItemClick("**/Back");
        IM_CHECK(WaitForItem(ctx, "**/Ballance Mod Loader"));
        IM_CHECK(WaitForItem(ctx, "**/New Ball Type"));
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
