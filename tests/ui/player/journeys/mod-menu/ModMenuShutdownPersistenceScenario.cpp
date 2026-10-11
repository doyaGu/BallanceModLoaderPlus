#include "player/UiTestFramework.h"

#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;

void RegisterModMenuShutdownPersistenceScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(
        engine, ScenarioCategory, "mod_menu_shutdown_persistence");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(OpenModConfigCategory(ctx, "Public Authoring Test",
                                       "Persistence", "**/Enabled"));
        IM_CHECK(!ctx->ItemIsChecked("**/Enabled"));
        IM_CHECK(ToggleConfigBoolean(ctx, "**/Enabled"));
        IM_CHECK(!ctx->ItemExists("**/Back"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::ModMenu));
    };
}

BML_REGISTER_UI_SCENARIO(ModMenuShutdownPersistence,
                         "mod-menu-shutdown-persistence",
                         "mod_menu_shutdown_persistence",
                         &RegisterModMenuShutdownPersistenceScenario)

} // namespace
