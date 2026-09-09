#ifndef BML_UI_TEST_FRAMEWORK_H
#define BML_UI_TEST_FRAMEWORK_H

#include <chrono>

struct ImGuiTestContext;
struct ImGuiTestEngine;

namespace UiAutomation::Test {

inline constexpr char ScenarioCategory[] = "ui_scenarios";

enum class GameAction : int {
    None,
    OptionsMenuToMain,
    MainMenuToStart,
    StartMenuToLevelOne,
    ShowAllHud,
    RestoreHud,
    ShowScriptDeveloperTools,
    MarkModMenuSurface,
    MarkHudSurface,
    MarkCustomMapsSurface,
    MarkConsoleSurface,
    MarkScriptToolsSurface,
};

bool WaitForItem(ImGuiTestContext *ctx, const char *path, int maximumFrames = 600);
bool WaitForItemToDisappear(ImGuiTestContext *ctx, const char *path, int maximumFrames = 600);
bool RunGameAction(ImGuiTestContext *ctx, GameAction action,
                   std::chrono::milliseconds timeout = std::chrono::seconds(15));

bool ObserveModList(ImGuiTestContext *ctx);
bool LeaveModListForOptions(ImGuiTestContext *ctx);
bool EnterStartMenuFromModList(ImGuiTestContext *ctx);
bool EnterLevelOneFromStartMenu(ImGuiTestContext *ctx);
bool EnterLevelOneFromModList(ImGuiTestContext *ctx);

using RegisterScenarioFunction = void (*)(ImGuiTestEngine *engine);

struct ScenarioDefinition {
    const char *Name;
    const char *TestName;
    RegisterScenarioFunction Register;
};

class ScenarioRegistration {
  public:
    ScenarioRegistration(const char *name, const char *testName,
                         RegisterScenarioFunction registerScenario);
};

const ScenarioDefinition *FindScenario(const char *name);

} // namespace UiAutomation::Test

#define BML_REGISTER_UI_SCENARIO(Token, Name, TestName, Register)                                  \
    namespace {                                                                                    \
    const UiAutomation::Test::ScenarioRegistration                                                 \
        g_UiAutomationRegistration_##Token(Name, TestName, Register);                              \
    }

#endif // BML_UI_TEST_FRAMEWORK_H
