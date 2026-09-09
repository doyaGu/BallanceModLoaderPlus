#ifndef BML_UI_TEST_FRAMEWORK_H
#define BML_UI_TEST_FRAMEWORK_H

#include <chrono>

struct ImGuiTestContext;
struct ImGuiTestEngine;

namespace UiAutomation::Test {

inline constexpr char ScenarioCategory[] = "ui_scenarios";

enum class NativeMenuTransition {
    OptionsToMain,
    MainToStart,
    StartToLevelOne,
};

enum class SurfaceCapture {
    ModMenu,
    Hud,
    CustomMaps,
    Console,
    ScriptTools,
};

bool WaitForItem(ImGuiTestContext *ctx, const char *path, int maximumFrames = 600);
bool WaitForItemToDisappear(ImGuiTestContext *ctx, const char *path, int maximumFrames = 600);
void WaitForDuration(ImGuiTestContext *ctx, std::chrono::milliseconds duration);
bool RunNativeMenuTransition(ImGuiTestContext *ctx, NativeMenuTransition transition,
                             std::chrono::milliseconds timeout = std::chrono::seconds(15));
bool CaptureSurface(ImGuiTestContext *ctx, SurfaceCapture surface,
                    std::chrono::milliseconds timeout = std::chrono::seconds(15));

bool ObserveModList(ImGuiTestContext *ctx);
bool OpenConfigCategory(ImGuiTestContext *ctx, const char *category, const char *firstProperty);
bool OpenModConfigCategory(ImGuiTestContext *ctx, const char *modName, const char *category,
                           const char *firstProperty);
bool ToggleConfigBoolean(ImGuiTestContext *ctx, const char *path);
bool SubmitConsoleCommand(ImGuiTestContext *ctx, const char *command);
bool LeaveModListForOptions(ImGuiTestContext *ctx);
bool EnterStartMenuFromModList(ImGuiTestContext *ctx);
bool WaitForLevelStart(ImGuiTestContext *ctx);
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
