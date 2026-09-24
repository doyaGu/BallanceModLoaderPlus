#include "player/UiTestFramework.h"

#include <chrono>
#include <filesystem>
#include <system_error>

#include "AngelScript/ScriptMod.h"
#include "Loader/ModContext.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

using namespace UiAutomation::Test;
namespace fs = std::filesystem;

constexpr char ReloadModId[] = "bml.ui.mod-menu-reload";

BML::ScriptMod *FindReloadMod() {
    ModContext *context = BML_GetModContext();
    return context ? dynamic_cast<BML::ScriptMod *>(context->FindMod(ReloadModId)) : nullptr;
}

bool WaitForReload(ImGuiTestContext *ctx, unsigned int previousGeneration) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        ctx->Yield();
        BML::ScriptMod *mod = FindReloadMod();
        if (mod && mod->GetModGeneration() > previousGeneration)
            return true;
    }
    return false;
}

void RegisterModMenuReloadScenario(ImGuiTestEngine *engine) {
    ImGuiTest *test = IM_REGISTER_TEST(engine, ScenarioCategory, "mod_menu_replaced_draft");
    test->TestFunc = [](ImGuiTestContext *ctx) {
        IM_CHECK(OpenModConfigCategory(ctx, "Mod Menu Reload Fixture", "General", "**/Enabled"));
        IM_CHECK(ToggleConfigBoolean(ctx, "**/Enabled"));
        IM_CHECK(WaitForItem(ctx, "**/Revert"));

        BML::ScriptMod *mod = FindReloadMod();
        IM_CHECK(mod != nullptr);
        IM_CHECK(mod->GetEntry().SourceKind == BML::ScriptModEntrySourceKind::Directory);
        const unsigned int previousGeneration = mod->GetModGeneration();
        const fs::path entry = mod->GetEntry().EntryPath;
        const fs::path replacement = entry.parent_path() / "runtime.v2.txt";
        std::error_code error;
        const bool copied = fs::copy_file(replacement, entry,
                                          fs::copy_options::overwrite_existing, error);
        IM_CHECK(copied && !error);
        IM_CHECK(WaitForReload(ctx, previousGeneration));

        IM_CHECK(WaitForItem(ctx, "**/Revert"));
        IM_CHECK(!ctx->ItemExists("**/Enabled"));
        IM_CHECK(CaptureSurface(ctx, SurfaceCapture::ModMenu));
        ctx->ItemClick("**/Revert");
        IM_CHECK(WaitForItem(ctx, "**/General"));
        IM_CHECK(!ctx->ItemExists("**/Revert"));

        ctx->ItemClick("**/Back");
        IM_CHECK(WaitForItem(ctx, "**/Mod Menu Reload Fixture"));
        IM_CHECK(OpenModConfigCategory(ctx, "Mod Menu Reload Fixture", "General", "**/Enabled"));
        ctx->ItemClick("**/Back");
        ctx->ItemClick("**/Back");
        IM_CHECK(LeaveModListForOptions(ctx));
    };
}

BML_REGISTER_UI_SCENARIO(ModMenuReload, "mod-menu-reload", "mod_menu_replaced_draft",
                         &RegisterModMenuReloadScenario)

} // namespace
