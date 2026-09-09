#include "UI/UiAutomation.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "BML/Bui.h"
#include "BML/ILogger.h"
#include "Loader/ModContext.h"
#include "Mods/BMLMod.h"
#include "UI/Automation/UiAutomationSession.h"
#include "UI/Automation/UiTestFramework.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_internal.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

namespace {

using UiAutomation::Test::GameAction;

ImGuiTestEngine *g_Engine = nullptr;
BMLMod *g_Mod = nullptr;
ILogger *g_Logger = nullptr;
ModContext *g_Runtime = nullptr;
std::string g_ResultPath;
std::string g_ScenarioName;
std::unique_ptr<UiAutomationSession::PlayerEndpoint> g_Session;
std::string g_ActiveCheckpointName;
UiAutomationSession::CheckpointKind g_ActiveCheckpointKind =
    UiAutomationSession::CheckpointKind::Input;
std::string g_SessionFailure;
bool g_QueueFinished = false;
bool g_ExitPending = false;
bool g_Armed = false;
bool g_QueueStarted = false;
unsigned int g_FrameCount = 0;
unsigned int g_NativeFrameCount = 0;
unsigned int g_ExitFrameCount = 0;
std::chrono::steady_clock::time_point g_NativeEntryReadyAt;
std::atomic_bool g_ImGuiEntryObserved = false;
std::atomic_bool g_ImGuiCloseRequested = false;
std::atomic_bool g_NativeReturnObserved = false;
std::atomic_bool g_NativeReturnSucceeded = false;
std::atomic_bool g_LevelOneStarted = false;
std::atomic_bool g_LevelOneObserved = false;
std::atomic_bool g_LevelOneSucceeded = false;
int g_InitialHudMode = 0;

std::atomic_int g_GameAction = static_cast<int>(GameAction::None);
std::atomic_uint g_GameActionCompleted = 0;
std::atomic_bool g_GameActionSucceeded = false;
GameAction g_ActiveGameAction = GameAction::None;

enum class NativeEntryPhase {
    SettleMainMenu,
    CaptureGameMenu,
    RequestMainToOptions,
    WaitForOptionsMenu,
    RequestOptionsToImGui,
    WaitForImGuiMenu,
};

NativeEntryPhase g_NativeEntryPhase = NativeEntryPhase::SettleMainMenu;

bool HasText(const char *value) { return value && value[0] != '\0'; }

enum class CheckpointProgress {
    Waiting,
    Succeeded,
    Failed,
};

CheckpointProgress AdvanceCheckpoint(UiAutomationSession::CheckpointKind kind, const char *name) {
    if (!g_Session) {
        g_SessionFailure = "session-not-configured";
        return CheckpointProgress::Failed;
    }
    if (!g_Session->HasPendingCheckpoint()) {
        if (!g_ActiveCheckpointName.empty()) {
            g_SessionFailure = "checkpoint-state-mismatch";
            return CheckpointProgress::Failed;
        }
        if (!g_Session->Publish(kind, name)) {
            g_SessionFailure = g_Session->LastError();
            return CheckpointProgress::Failed;
        }
        g_ActiveCheckpointKind = kind;
        g_ActiveCheckpointName = name;
        if (g_Logger) {
            g_Logger->Info("UI automation: checkpoint=%u kind=%s name=%s state=published",
                           g_Session->PendingSequence(), UiAutomationSession::ToString(kind), name);
        }
        return CheckpointProgress::Waiting;
    }
    if (g_ActiveCheckpointKind != kind || g_ActiveCheckpointName != name) {
        g_SessionFailure = "concurrent-checkpoint-request";
        return CheckpointProgress::Failed;
    }

    const UiAutomationSession::Acknowledgement acknowledgement = g_Session->PollAcknowledgement();
    if (acknowledgement.State == UiAutomationSession::AcknowledgementState::Waiting)
        return CheckpointProgress::Waiting;

    const bool succeeded =
        acknowledgement.State == UiAutomationSession::AcknowledgementState::Succeeded;
    if (g_Logger) {
        g_Logger->Info("UI automation: checkpoint=%u kind=%s name=%s state=%s reason=%s",
                       acknowledgement.Sequence, UiAutomationSession::ToString(kind), name,
                       succeeded ? "acknowledged" : "failed",
                       acknowledgement.Reason.empty() ? "none" : acknowledgement.Reason.c_str());
    }
    if (!succeeded) {
        g_SessionFailure =
            acknowledgement.Reason.empty() ? "checkpoint-failed" : acknowledgement.Reason;
    }
    g_ActiveCheckpointName.clear();
    return succeeded ? CheckpointProgress::Succeeded : CheckpointProgress::Failed;
}

bool IsEntityVisible(const char *name) {
    CK2dEntity *entity = g_Runtime ? g_Runtime->Get2dEntityByName(name) : nullptr;
    return entity && entity->IsVisible();
}

bool HasExpectedNativeVisibility(GameAction action) {
    const bool mainVisible = IsEntityVisible("M_Main_But_1");
    const bool optionsVisible = IsEntityVisible("M_Options_But_4");
    const bool startVisible = IsEntityVisible("M_Start_But_01");

    switch (action) {
    case GameAction::MarkModMenuSurface:
    case GameAction::MarkCustomMapsSurface:
        return !mainVisible && !optionsVisible && !startVisible;
    case GameAction::MarkHudSurface:
    case GameAction::MarkConsoleSurface:
    case GameAction::MarkScriptToolsSurface:
        return g_LevelOneObserved.load(std::memory_order_acquire) && !mainVisible &&
               !optionsVisible && !startVisible;
    default:
        return true;
    }
}

void CompleteGameAction(GameAction action, bool succeeded) {
    g_ActiveGameAction = GameAction::None;
    g_GameActionSucceeded.store(succeeded, std::memory_order_release);
    g_GameActionCompleted.fetch_add(1, std::memory_order_acq_rel);
    if (g_Logger) {
        g_Logger->Info("UI automation: game_action=%d succeeded=%s", static_cast<int>(action),
                       succeeded ? "true" : "false");
    }
}

const char *GameActionCheckpoint(GameAction action) {
    switch (action) {
    case GameAction::OptionsMenuToMain:
        return "input-options-to-main";
    case GameAction::MainMenuToStart:
        return "input-main-to-start";
    case GameAction::StartMenuToLevelOne:
        return "input-start-to-level-1";
    case GameAction::MarkModMenuSurface:
        return "capture-mod-menu";
    case GameAction::MarkHudSurface:
        return "capture-hud";
    case GameAction::MarkCustomMapsSurface:
        return "capture-custom-maps";
    case GameAction::MarkConsoleSurface:
        return "capture-console";
    case GameAction::MarkScriptToolsSurface:
        return "capture-script-tools";
    default:
        return nullptr;
    }
}

bool IsCaptureAction(GameAction action) {
    return action == GameAction::MarkModMenuSurface || action == GameAction::MarkHudSurface ||
           action == GameAction::MarkCustomMapsSurface ||
           action == GameAction::MarkConsoleSurface || action == GameAction::MarkScriptToolsSurface;
}

const char *SurfaceName(GameAction action) {
    switch (action) {
    case GameAction::MarkModMenuSurface:
        return "mod-menu";
    case GameAction::MarkHudSurface:
        return "hud";
    case GameAction::MarkCustomMapsSurface:
        return "custom-maps";
    case GameAction::MarkConsoleSurface:
        return "console";
    case GameAction::MarkScriptToolsSurface:
        return "script-tools";
    default:
        return nullptr;
    }
}

void ProcessGameAction() {
    if (g_ActiveGameAction == GameAction::None) {
        const GameAction action = static_cast<GameAction>(
            g_GameAction.exchange(static_cast<int>(GameAction::None), std::memory_order_acq_rel));
        if (action == GameAction::None)
            return;

        bool succeeded = false;
        switch (action) {
        case GameAction::OptionsMenuToMain:
            if (IsEntityVisible("M_Options_But_Back")) {
                if (g_Logger) {
                    g_Logger->Info("UI automation: native_transition=options-to-main "
                                   "requested=true input=keyboard");
                }
                succeeded = true;
            }
            break;
        case GameAction::MainMenuToStart:
            if (IsEntityVisible("M_Main_But_1") && !IsEntityVisible("M_Options_But_4") &&
                !IsEntityVisible("M_Start_But_01")) {
                if (g_Logger) {
                    g_Logger->Info("UI automation: native_transition=main-to-start "
                                   "requested=true input=keyboard");
                }
                succeeded = true;
            }
            break;
        case GameAction::StartMenuToLevelOne:
            if (IsEntityVisible("M_Start_But_01")) {
                if (g_Logger) {
                    g_Logger->Info("UI automation: native_transition=start-to-level-1 "
                                   "requested=true input=keyboard");
                }
                succeeded = true;
            }
            break;
        case GameAction::ShowAllHud:
            if (g_Mod) {
                g_Mod->ShowTitle(true);
                g_Mod->ShowFPS(true);
                g_Mod->ShowSRTimer(true);
                succeeded = true;
            }
            break;
        case GameAction::RestoreHud:
            if (g_Mod) {
                g_Mod->ShowTitle((g_InitialHudMode & HUD_TITLE) != 0);
                g_Mod->ShowFPS((g_InitialHudMode & HUD_FPS) != 0);
                g_Mod->ShowSRTimer(false);
                succeeded = true;
            }
            break;
        case GameAction::ShowScriptDeveloperTools:
#if BML_ENABLE_ANGELSCRIPT
            if (g_Runtime && g_Runtime->GetScriptDevTools()) {
                g_Runtime->GetScriptDevTools()->Show();
                succeeded = true;
            }
#endif
            break;
        case GameAction::MarkModMenuSurface:
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case GameAction::MarkHudSurface:
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case GameAction::MarkCustomMapsSurface:
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case GameAction::MarkConsoleSurface:
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case GameAction::MarkScriptToolsSurface:
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case GameAction::None:
            break;
        }

        const char *surface = SurfaceName(action);
        if (g_Logger && surface) {
            g_Logger->Info("UI automation: surface=%s native_options_visible=%s "
                           "native_main_visible=%s native_start_visible=%s "
                           "layout_valid=%s",
                           surface, IsEntityVisible("M_Options_But_4") ? "true" : "false",
                           IsEntityVisible("M_Main_But_1") ? "true" : "false",
                           IsEntityVisible("M_Start_But_01") ? "true" : "false",
                           succeeded ? "true" : "false");
        }
        const char *checkpoint = GameActionCheckpoint(action);
        if (!checkpoint || !succeeded) {
            CompleteGameAction(action, succeeded);
            return;
        }
        g_ActiveGameAction = action;
    }

    const char *checkpoint = GameActionCheckpoint(g_ActiveGameAction);
    const UiAutomationSession::CheckpointKind kind =
        IsCaptureAction(g_ActiveGameAction) ? UiAutomationSession::CheckpointKind::Capture
                                            : UiAutomationSession::CheckpointKind::Input;
    const CheckpointProgress progress = AdvanceCheckpoint(kind, checkpoint);
    if (progress != CheckpointProgress::Waiting)
        CompleteGameAction(g_ActiveGameAction, progress == CheckpointProgress::Succeeded);
}

bool AdvanceNativeEntry() {
    ++g_NativeFrameCount;
    switch (g_NativeEntryPhase) {
    case NativeEntryPhase::SettleMainMenu:
        if (std::chrono::steady_clock::now() < g_NativeEntryReadyAt)
            return false;
        g_NativeEntryPhase = NativeEntryPhase::CaptureGameMenu;
        return false;
    case NativeEntryPhase::CaptureGameMenu: {
        const CheckpointProgress progress =
            AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Capture, "capture-game-menu");
        if (progress == CheckpointProgress::Failed)
            return false;
        if (progress == CheckpointProgress::Succeeded)
            g_NativeEntryPhase = NativeEntryPhase::RequestMainToOptions;
        return false;
    }
    case NativeEntryPhase::RequestMainToOptions: {
        if (g_Logger && !g_Session->HasPendingCheckpoint()) {
            g_Logger->Info("UI automation: native_transition=main-to-options "
                           "requested=true input=keyboard");
        }
        const CheckpointProgress progress =
            AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Input, "input-main-to-options");
        if (progress == CheckpointProgress::Succeeded) {
            g_NativeEntryPhase = NativeEntryPhase::WaitForOptionsMenu;
            g_NativeFrameCount = 0;
        }
        return false;
    }
    case NativeEntryPhase::WaitForOptionsMenu:
        if (IsEntityVisible("M_Options_But_4")) {
            if (g_Logger) {
                g_Logger->Info("UI automation: native_transition=main-to-options "
                               "observed=true");
            }
            g_NativeEntryPhase = NativeEntryPhase::RequestOptionsToImGui;
            g_NativeFrameCount = 0;
        }
        return false;
    case NativeEntryPhase::RequestOptionsToImGui: {
        if (g_Logger && !g_Session->HasPendingCheckpoint()) {
            g_Logger->Info("UI automation: native_transition=options-to-imgui "
                           "requested=true input=keyboard");
        }
        const CheckpointProgress progress =
            AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Input, "input-options-to-imgui");
        if (progress == CheckpointProgress::Succeeded) {
            g_NativeEntryPhase = NativeEntryPhase::WaitForImGuiMenu;
            g_NativeFrameCount = 0;
        }
        return false;
    }
    case NativeEntryPhase::WaitForImGuiMenu:
        return g_NativeFrameCount >= 2 && !IsEntityVisible("M_Options_But_4");
    }
    return false;
}

void ObserveNativeReturn() {
    if (!g_ImGuiEntryObserved.load(std::memory_order_acquire) ||
        !g_ImGuiCloseRequested.load(std::memory_order_acquire) ||
        g_NativeReturnObserved.load(std::memory_order_relaxed) || !g_Runtime) {
        return;
    }

    CKBehavior *menuOptions = g_Runtime->GetScriptByName("Menu_Options");
    CK2dEntity *button = g_Runtime->Get2dEntityByName("M_Options_But_4");
    if (!menuOptions || !button || !menuOptions->IsActive() || !button->IsVisible()) {
        return;
    }

    const CheckpointProgress progress =
        AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Capture, "capture-native-options");
    if (progress == CheckpointProgress::Waiting)
        return;
    const bool succeeded = progress == CheckpointProgress::Succeeded;
    g_NativeReturnSucceeded.store(succeeded, std::memory_order_release);
    g_NativeReturnObserved.store(true, std::memory_order_release);
    if (g_Logger)
        g_Logger->Info("UI automation: native_transition=imgui-to-options observed=true");
}

void AdvanceLevelEntry() {
    if (!g_LevelOneStarted.load(std::memory_order_acquire) ||
        g_LevelOneObserved.load(std::memory_order_relaxed) || !g_Session ||
        g_ActiveGameAction != GameAction::None ||
        (g_Session->HasPendingCheckpoint() && g_ActiveCheckpointName != "input-dismiss-tutorial")) {
        return;
    }
    if (g_Logger && !g_Session->HasPendingCheckpoint()) {
        g_Logger->Info("UI automation: gameplay_tutorial=dismiss requested=true "
                       "input=keyboard");
    }
    const CheckpointProgress progress =
        AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Input, "input-dismiss-tutorial");
    if (progress == CheckpointProgress::Waiting)
        return;
    g_LevelOneSucceeded.store(progress == CheckpointProgress::Succeeded, std::memory_order_release);
    g_LevelOneObserved.store(true, std::memory_order_release);
}

} // namespace

namespace UiAutomation::Test {

namespace {

std::vector<ScenarioDefinition> &ScenarioRegistry() {
    static std::vector<ScenarioDefinition> scenarios;
    return scenarios;
}

} // namespace

ScenarioRegistration::ScenarioRegistration(const char *name, const char *testName,
                                           RegisterScenarioFunction registerScenario) {
    ScenarioRegistry().push_back({name, testName, registerScenario});
}

const ScenarioDefinition *FindScenario(const char *name) {
    for (const ScenarioDefinition &scenario : ScenarioRegistry()) {
        if (name && std::strcmp(name, scenario.Name) == 0)
            return &scenario;
    }
    return nullptr;
}

bool WaitForItem(ImGuiTestContext *ctx, const char *path, int maximumFrames) {
    for (int frame = 0; frame < maximumFrames; ++frame) {
        if (ctx->ItemExists(path))
            return true;
        ctx->Yield();
    }
    return false;
}

bool WaitForItemToDisappear(ImGuiTestContext *ctx, const char *path, int maximumFrames) {
    for (int frame = 0; frame < maximumFrames; ++frame) {
        if (!ctx->ItemExists(path))
            return true;
        ctx->Yield();
    }
    return false;
}

bool RunGameAction(ImGuiTestContext *ctx, GameAction action, std::chrono::milliseconds timeout) {
    if (g_GameAction.load(std::memory_order_acquire) != static_cast<int>(GameAction::None)) {
        return false;
    }

    const unsigned int completed = g_GameActionCompleted.load(std::memory_order_acquire);
    g_GameAction.store(static_cast<int>(action), std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (g_GameActionCompleted.load(std::memory_order_acquire) != completed)
            return g_GameActionSucceeded.load(std::memory_order_acquire);
        ctx->Yield();
    }
    return false;
}

bool ObserveModList(ImGuiTestContext *ctx) {
    if (!WaitForItem(ctx, "**/Ballance Mod Loader"))
        return false;
    g_ImGuiEntryObserved.store(true, std::memory_order_release);
    return true;
}

bool LeaveModListForOptions(ImGuiTestContext *ctx) {
    if (!ObserveModList(ctx) || !WaitForItem(ctx, "**/Back"))
        return false;
    ctx->ItemClick("**/Back");
    g_ImGuiCloseRequested.store(true, std::memory_order_release);
    ctx->Yield(2);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline &&
           !g_NativeReturnObserved.load(std::memory_order_acquire)) {
        ctx->Yield();
    }
    return g_NativeReturnObserved.load(std::memory_order_acquire) &&
           g_NativeReturnSucceeded.load(std::memory_order_acquire);
}

bool EnterStartMenuFromModList(ImGuiTestContext *ctx) {
    if (!LeaveModListForOptions(ctx) || !RunGameAction(ctx, GameAction::OptionsMenuToMain)) {
        return false;
    }
    ctx->Yield(360);
    if (!RunGameAction(ctx, GameAction::MainMenuToStart))
        return false;
    return WaitForItem(ctx, "**/Enter_Custom_Maps");
}

bool EnterLevelOneFromStartMenu(ImGuiTestContext *ctx) {
    if (!RunGameAction(ctx, GameAction::StartMenuToLevelOne))
        return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline &&
           !g_LevelOneObserved.load(std::memory_order_acquire)) {
        ctx->Yield();
    }
    if (!g_LevelOneObserved.load(std::memory_order_acquire) ||
        !g_LevelOneSucceeded.load(std::memory_order_acquire))
        return false;
    ctx->Yield(720);
    ctx->SleepStandard();
    return true;
}

bool EnterLevelOneFromModList(ImGuiTestContext *ctx) {
    return EnterStartMenuFromModList(ctx) && EnterLevelOneFromStartMenu(ctx);
}

} // namespace UiAutomation::Test

namespace {

bool StartEngine() {
    ImGuiContext *imguiContext = Bui::GetImGuiContext();
    if (!imguiContext)
        return false;

    const UiAutomation::Test::ScenarioDefinition *scenario =
        UiAutomation::Test::FindScenario(g_ScenarioName.c_str());
    if (!scenario)
        return false;

    g_QueueFinished = false;
    g_FrameCount = 0;
    g_Engine = ImGuiTestEngine_CreateContext();

    ImGuiTestEngineIO &io = ImGuiTestEngine_GetIO(g_Engine);
    io.ConfigRunSpeed = ImGuiTestRunSpeed_Cinematic;
    io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Warning;
    io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    io.ConfigLogToTTY = true;
    io.ConfigSavedSettings = false;
    io.ConfigCaptureEnabled = false;
    io.ConfigStopOnError = false;
    scenario->Register(g_Engine);
    ImGuiTestEngine_Start(g_Engine, imguiContext);

    if (g_Logger) {
        g_Logger->Info("UI automation: scenario=%s selected=true test=%s", scenario->Name,
                       scenario->TestName);
        g_Logger->Info("UI automation: gate=game-menu engine=bound");
    }
    return true;
}

bool WriteResult(bool passed, int failures) {
    const UiAutomation::Test::ScenarioDefinition *scenario =
        UiAutomation::Test::FindScenario(g_ScenarioName.c_str());
    if (!scenario)
        return false;

    std::ofstream output(g_ResultPath, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output << "format=bml-ui-result-v1\n"
           << "scenario=" << scenario->Name << '\n'
           << "test=" << scenario->TestName << '\n'
           << "status=" << (passed ? "passed" : "failed") << '\n'
           << "failures=" << failures << '\n';
    return output.good();
}

bool WriteScenarioResult() {
    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(g_Engine, &summary);
    const bool passed =
        summary.CountTested == 1 && summary.CountSuccess == 1 && summary.CountInQueue == 0;
    int failures = summary.CountTested - summary.CountSuccess + summary.CountInQueue;
    if (!passed && failures < 1)
        failures = 1;
    return WriteResult(passed, failures);
}

void FinishStartupFailure() {
    const bool resultWritten = WriteResult(false, 1);
    g_Armed = false;
    g_QueueFinished = true;
    g_ExitFrameCount = 0;
    g_ExitPending = true;
    if (g_Logger) {
        g_Logger->Error("UI automation: session failed before engine start reason=%s "
                        "result_written=%s",
                        g_SessionFailure.c_str(), resultWritten ? "true" : "false");
    }
}

void QueueTest() {
    ImGuiTestEngine_QueueTests(g_Engine, ImGuiTestGroup_Tests, UiAutomation::Test::ScenarioCategory,
                               ImGuiTestRunFlags_RunFromCommandLine);
    g_QueueStarted = true;
    if (g_Logger)
        g_Logger->Info("UI automation: gate=game-menu engine=started");
}

} // namespace

namespace UiAutomation {

void Start(BMLMod &mod) {
    if (g_Engine || g_Armed)
        return;

    const char *resultPath = std::getenv("BML_UI_AUTOMATION_RESULT");
    const char *scenarioName = std::getenv("BML_UI_AUTOMATION_SCENARIO");
    const char *sessionPath = std::getenv("BML_UI_AUTOMATION_SESSION");
    ILogger *logger = nullptr;
    if (ModContext *runtime = mod.GetRuntimeContext())
        logger = runtime->GetLogger();
    if (logger) {
        logger->Info("UI automation: gate=game-menu requested=%s",
                     HasText(resultPath) ? "true" : "false");
    }
    if (!HasText(resultPath))
        return;

    const Test::ScenarioDefinition *scenario = Test::FindScenario(scenarioName);
    if (!scenario) {
        if (logger) {
            logger->Error("UI automation: unknown or missing scenario '%s'",
                          scenarioName ? scenarioName : "");
        }
        return;
    }

    g_Mod = &mod;
    g_Logger = logger;
    g_Runtime = mod.GetRuntimeContext();
    g_InitialHudMode = mod.GetHUD();
    g_ResultPath = resultPath;
    g_ScenarioName = scenario->Name;
    if (!HasText(sessionPath)) {
        g_SessionFailure = "session-path-missing";
        FinishStartupFailure();
        return;
    }
    g_Session = std::make_unique<UiAutomationSession::PlayerEndpoint>(sessionPath);
    g_ActiveCheckpointName.clear();
    g_SessionFailure.clear();
    g_QueueFinished = false;
    g_ExitPending = false;
    g_Armed = true;
    g_QueueStarted = false;
    g_FrameCount = 0;
    g_NativeFrameCount = 0;
    g_ExitFrameCount = 0;
    g_NativeEntryReadyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    g_NativeEntryPhase = NativeEntryPhase::SettleMainMenu;
    g_ImGuiEntryObserved.store(false, std::memory_order_relaxed);
    g_ImGuiCloseRequested.store(false, std::memory_order_relaxed);
    g_NativeReturnObserved.store(false, std::memory_order_relaxed);
    g_NativeReturnSucceeded.store(false, std::memory_order_relaxed);
    g_LevelOneStarted.store(false, std::memory_order_relaxed);
    g_LevelOneObserved.store(false, std::memory_order_relaxed);
    g_LevelOneSucceeded.store(false, std::memory_order_relaxed);
    g_GameAction.store(static_cast<int>(GameAction::None), std::memory_order_relaxed);
    g_GameActionCompleted.store(0, std::memory_order_relaxed);
    g_GameActionSucceeded.store(false, std::memory_order_relaxed);
    g_ActiveGameAction = GameAction::None;
    if (g_Logger)
        g_Logger->Info("UI automation: gate=game-menu state=armed");
}

void OnStartLevel() {
    if (!g_Engine && !g_Armed)
        return;
    g_LevelOneStarted.store(true, std::memory_order_release);
    if (g_Logger)
        g_Logger->Info("UI automation: native_transition=start-to-level-1 observed=true");
}

void AdvanceFrame() {
    if (g_ExitPending) {
        if (++g_ExitFrameCount >= 5) {
            g_ExitPending = false;
            if (g_Runtime)
                g_Runtime->ExitGame();
        }
        return;
    }

    if (g_Armed && !g_Engine) {
        const bool ready = AdvanceNativeEntry();
        if (!g_SessionFailure.empty()) {
            FinishStartupFailure();
            return;
        }
        if (!ready)
            return;
        if (StartEngine())
            g_Armed = false;
        return;
    }
    if (!g_Engine || g_QueueFinished)
        return;

    ProcessGameAction();
    ObserveNativeReturn();
    AdvanceLevelEntry();

    if (!g_QueueStarted)
        QueueTest();

    ++g_FrameCount;
    if (g_Logger && (g_FrameCount == 1 || g_FrameCount % 300 == 0)) {
        const ImGuiTestEngineIO &io = ImGuiTestEngine_GetIO(g_Engine);
        g_Logger->Info("UI automation: frame=%u running=%s queue_empty=%s", g_FrameCount,
                       io.IsRunningTests ? "true" : "false",
                       ImGuiTestEngine_IsTestQueueEmpty(g_Engine) ? "true" : "false");
    }

    ImGuiTestEngine_PreSwap(g_Engine);
    ImGuiTestEngine_PostSwap(g_Engine);

    ImGuiTestEngineIO &io = ImGuiTestEngine_GetIO(g_Engine);
    if (!io.IsRunningTests && ImGuiTestEngine_IsTestQueueEmpty(g_Engine)) {
        ImGuiContext *imguiContext = Bui::GetImGuiContext();
        ImGuiTestEngine_Stop(g_Engine);
        const bool resultWritten = WriteScenarioResult();
        ImGuiTestEngine_UnbindImGuiContext(g_Engine, imguiContext);
        g_QueueFinished = true;
        if (g_Logger) {
            g_Logger->Info("UI automation: status=complete frames=%u result=%s "
                           "result_written=%s",
                           g_FrameCount, g_ResultPath.c_str(), resultWritten ? "true" : "false");
        }
        g_ExitFrameCount = 0;
        g_ExitPending = true;
    }
}

void Shutdown() {
    if (g_Engine) {
        if (!g_QueueFinished)
            ImGuiTestEngine_Stop(g_Engine);
        ImGuiTestEngine_DestroyContext(g_Engine);
    }
    g_Engine = nullptr;
    g_Mod = nullptr;
    g_Logger = nullptr;
    g_Runtime = nullptr;
    g_ResultPath.clear();
    g_ScenarioName.clear();
    g_Session.reset();
    g_ActiveCheckpointName.clear();
    g_SessionFailure.clear();
    g_QueueFinished = false;
    g_ExitPending = false;
    g_Armed = false;
    g_QueueStarted = false;
    g_FrameCount = 0;
    g_NativeFrameCount = 0;
    g_ExitFrameCount = 0;
    g_InitialHudMode = 0;
    g_ImGuiEntryObserved.store(false, std::memory_order_relaxed);
    g_ImGuiCloseRequested.store(false, std::memory_order_relaxed);
    g_NativeReturnObserved.store(false, std::memory_order_relaxed);
    g_NativeReturnSucceeded.store(false, std::memory_order_relaxed);
    g_LevelOneStarted.store(false, std::memory_order_relaxed);
    g_LevelOneObserved.store(false, std::memory_order_relaxed);
    g_LevelOneSucceeded.store(false, std::memory_order_relaxed);
    g_GameAction.store(static_cast<int>(GameAction::None), std::memory_order_relaxed);
    g_GameActionCompleted.store(0, std::memory_order_relaxed);
    g_GameActionSucceeded.store(false, std::memory_order_relaxed);
    g_ActiveGameAction = GameAction::None;
}

} // namespace UiAutomation
