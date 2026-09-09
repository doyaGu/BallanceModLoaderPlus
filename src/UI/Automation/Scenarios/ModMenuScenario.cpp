#include "UI/Automation/UiTestFramework.h"

#include <cstring>
#include <initializer_list>

#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_internal.h"

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
        const auto findVisibleMenuItem = [ctx](const char *label) {
            if (!WaitForItem(ctx, "**/Back"))
                return ImGuiID{0};
            const ImGuiTestItemInfo back = ctx->ItemInfo("**/Back");
            if (!back.Window)
                return ImGuiID{0};

            ImGuiTestItemList items;
            ctx->GatherItems(&items, back.Window->ID, -1);
            for (const ImGuiTestItemInfo &item : items) {
                if (std::strcmp(item.DebugLabel, label) == 0)
                    return item.ID;
            }
            return ImGuiID{0};
        };
        const auto openCategory = [ctx, &findVisibleMenuItem](const char *category,
                                                              const char *firstProperty) {
            for (int page = 0; page < 16 && ctx->ItemExists("**/PrevPage"); ++page) {
                ctx->ItemClick("**/PrevPage");
            }
            for (int page = 0; page < 16; ++page) {
                const ImGuiID categoryId = findVisibleMenuItem(category);
                if (categoryId != 0) {
                    ctx->ItemClick(categoryId);
                    return WaitForItem(ctx, firstProperty);
                }
                if (!ctx->ItemExists("**/NextPage"))
                    return false;
                ctx->ItemClick("**/NextPage");
            }
            return false;
        };

        IM_CHECK(ObserveModList(ctx));
        ctx->ItemClick("**/Ballance Mod Loader");
        IM_CHECK(RunGameAction(ctx, GameAction::MarkModMenuSurface));
        ctx->SleepStandard();

        IM_CHECK(openCategory("GUI", "**/FontFilename"));
        IM_CHECK(hasAll({"**/FontSize", "**/FontRanges", "**/EnableSecondaryFont", "**/NextPage"}));
        ctx->ItemClick("**/NextPage");
        IM_CHECK(hasAll({"**/SecondaryFontFilename", "**/SecondaryFontSize",
                         "**/SecondaryFontRanges", "**/EnableIniSettings"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(openCategory("Graphics", "**/UnlockFrameRate"));
        IM_CHECK(hasAll({"**/SetMaxFrameRate", "**/WidescreenFix"}));
        ctx->ItemInputValue("**/SetMaxFrameRate/##InputInt", 17);
        IM_CHECK(WaitForItem(ctx, "**/Revert"));
        ctx->ItemClick("**/Revert");
        IM_CHECK(WaitForItemToDisappear(ctx, "**/Revert"));
        ctx->ItemClick("**/Back");

        IM_CHECK(openCategory("HUD", "**/ShowTitle"));
        IM_CHECK(hasAll({"**/ShowFPS", "**/ShowSRTimer", "**/FPSUpdateFrequency"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(openCategory("CommandBar", "**/MessageDuration"));
        IM_CHECK(hasAll(
            {"**/TabColumns", "**/LineSpacing", "**/MessageBackgroundAlpha", "**/NextPage"}));
        ctx->ItemClick("**/NextPage");
        IM_CHECK(hasAll({"**/WindowBackgroundAlpha", "**/FadeMaxAlpha"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(openCategory("CustomMap", "**/LevelNumber"));
        IM_CHECK(hasAll({"**/ShowTooltip", "**/MaxDepth"}));
        ctx->ItemClick("**/Back");

        IM_CHECK(openCategory("Tweak", "**/LanternAlphaTest"));
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
