#include "UI/UiAutomation.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

namespace {

using UiAutomation::Test::NativeMenuTransition;
using UiAutomation::Test::SurfaceCapture;

enum class PlayerAction : int {
    None,
    OptionsToMain,
    MainToStart,
    StartToLevelOne,
    CaptureModMenu,
    CaptureHud,
    CaptureCustomMaps,
    CaptureConsole,
    CaptureScriptTools,
};

struct NativeMenuRoute {
    const char *Checkpoint;
    const char *Script;
    const char *Menu;
    const char *EntityPrefix;
    const char *TargetEntity;
};

struct NativeMenuItem {
    CK2dEntity *Entity = nullptr;
    float Position = 0.0f;
    float Size = 0.0f;
};

struct NativeMenuLayout {
    std::vector<NativeMenuItem> Items;
    int TargetRow = -1;
};

constexpr NativeMenuRoute OptionsToMainRoute = {
    "input-options-to-main", "Menu_Options", "Options Menu",
    "M_Options_But_", "M_Options_But_Back"};
constexpr NativeMenuRoute MainToStartRoute = {
    "input-main-to-start", "Menu_Main", "Main Menu",
    "M_Main_But_", "M_Main_But_1"};
constexpr NativeMenuRoute MainToOptionsRoute = {
    "input-main-to-options", "Menu_Main", "Main Menu",
    "M_Main_But_", "M_Main_But_3"};
constexpr NativeMenuRoute OptionsToImGuiRoute = {
    "input-options-to-imgui", "Menu_Options", "Options Menu",
    "M_Options_But_", "M_Options_But_4"};

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
bool g_NativeOptionsLayoutValid = false;
bool g_ImGuiNativeSurfacesHidden = false;
unsigned int g_FrameCount = 0;
unsigned int g_NativeFrameCount = 0;
unsigned int g_ExitFrameCount = 0;
std::chrono::steady_clock::time_point g_NativeEntryReadyAt;
std::atomic_bool g_ImGuiEntryObserved = false;
std::atomic_bool g_ImGuiCloseRequested = false;
std::atomic_bool g_NativeReturnObserved = false;
std::atomic_bool g_NativeReturnSucceeded = false;
std::atomic_bool g_LevelStarted = false;
std::atomic_bool g_LevelObserved = false;
std::atomic_bool g_LevelSucceeded = false;
int g_InitialHudMode = 0;

std::atomic_int g_PlayerAction = static_cast<int>(PlayerAction::None);
std::atomic_uint g_PlayerActionCompleted = 0;
std::atomic_bool g_PlayerActionSucceeded = false;
PlayerAction g_ActivePlayerAction = PlayerAction::None;
std::string g_ActivePlayerCheckpoint;

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

CKBehavior *DirectBehavior(CKBehavior *graph, const char *name) {
    if (!graph || !name)
        return nullptr;
    CKBehavior *match = nullptr;
    for (int i = 0; i < graph->GetSubBehaviorCount(); ++i) {
        CKBehavior *candidate = graph->GetSubBehavior(i);
        if (!candidate || !candidate->GetName() ||
            std::strcmp(candidate->GetName(), name) != 0) {
            continue;
        }
        if (match)
            return nullptr;
        match = candidate;
    }
    return match;
}

std::optional<int> MenuRow(const char *scriptName, const char *menuName) {
    CKBehavior *script = g_Runtime ? g_Runtime->GetScriptByName(scriptName) : nullptr;
    CKBehavior *menu = DirectBehavior(script, menuName);
    CKBehavior *keyboard = DirectBehavior(menu, "Keyboard");
    if (!keyboard)
        return std::nullopt;
    for (int i = 0; i < keyboard->GetLocalParameterCount(); ++i) {
        CKParameterLocal *parameter = keyboard->GetLocalParameter(i);
        if (!parameter || !parameter->GetName() ||
            std::strcmp(parameter->GetName(), "Active Row") != 0) {
            continue;
        }
        int row = -1;
        if (keyboard->GetLocalParameterValue(i, &row) != CK_OK)
            return std::nullopt;
        return row;
    }
    return std::nullopt;
}

bool IsMenuItemBefore(const NativeMenuItem &left, const NativeMenuItem &right) {
    return left.Position < right.Position;
}

NativeMenuLayout ReadNativeMenuLayout(const NativeMenuRoute &route) {
    NativeMenuLayout layout;
    CKContext *context = g_Runtime ? g_Runtime->GetCKContext() : nullptr;
    if (!context)
        return layout;

    const std::string_view prefix(route.EntityPrefix);
    const XObjectPointerArray &objects = context->GetObjectListByType(CKCID_2DENTITY, TRUE);
    for (XObjectPointerArray::ConstIterator iterator = objects.Begin(); iterator != objects.End(); ++iterator) {
        CK2dEntity *entity = CK2dEntity::Cast(*iterator);
        const char *name = entity ? entity->GetName() : nullptr;
        if (!entity || !entity->IsVisible() || !name ||
            std::string_view(name).substr(0, prefix.size()) != prefix) {
            continue;
        }

        Vx2DVector position;
        Vx2DVector size;
        entity->GetPosition(position, TRUE);
        entity->GetSize(size, TRUE);
        layout.Items.push_back({entity, position.y, size.y});
    }

    std::sort(layout.Items.begin(), layout.Items.end(), IsMenuItemBefore);
    for (std::size_t i = 0; i < layout.Items.size(); ++i) {
        const char *name = layout.Items[i].Entity->GetName();
        if (!name || std::strcmp(name, route.TargetEntity) != 0)
            continue;
        if (layout.TargetRow >= 0) {
            layout.TargetRow = -1;
            return layout;
        }
        layout.TargetRow = static_cast<int>(i);
    }
    return layout;
}

CheckpointProgress AdvanceMenuInput(const NativeMenuRoute &route) {
    std::string checkpoint;
    if (g_Session && g_Session->HasPendingCheckpoint()) {
        checkpoint = g_ActiveCheckpointName;
    } else {
        const NativeMenuLayout layout = ReadNativeMenuLayout(route);
        const int rowCount = static_cast<int>(layout.Items.size());
        const int targetRow = layout.TargetRow;
        const std::optional<int> row = MenuRow(route.Script, route.Menu);
        if (!row || *row < 0 || *row >= rowCount) {
            g_SessionFailure = std::string(route.Checkpoint) + "-row-unavailable";
            return CheckpointProgress::Failed;
        }
        if (targetRow < 0 || targetRow >= rowCount) {
            g_SessionFailure = std::string(route.Checkpoint) + "-target-unavailable";
            return CheckpointProgress::Failed;
        }
        if (g_Logger) {
            g_Logger->Info("UI automation: native_transition=%s requested=true input=keyboard "
                           "row=%d target=%d rows=%d",
                           route.Checkpoint, *row, targetRow, rowCount);
        }
        if (*row == targetRow) {
            checkpoint = std::string(route.Checkpoint) + "-activate-" +
                std::to_string(targetRow);
        } else {
            const int down = (targetRow - *row + rowCount) % rowCount;
            const int up = (*row - targetRow + rowCount) % rowCount;
            checkpoint = std::string(route.Checkpoint) +
                (down <= up ? "-down-from-" : "-up-from-") +
                std::to_string(*row) + "-to-" + std::to_string(targetRow);
        }
    }

    const bool activates = checkpoint.find("-activate-") != std::string::npos;
    const CheckpointProgress progress =
        AdvanceCheckpoint(UiAutomationSession::CheckpointKind::Input, checkpoint.c_str());
    if (progress == CheckpointProgress::Succeeded && !activates)
        return CheckpointProgress::Waiting;
    return progress;
}

bool OptionsMenuRowsAreSeparated(float &rowStep, float &minimumGap) {
    const NativeMenuLayout layout = ReadNativeMenuLayout(OptionsToImGuiRoute);
    if (layout.Items.size() < 2 || layout.TargetRow < 0)
        return false;

    rowStep = layout.Items[1].Position - layout.Items[0].Position;
    minimumGap = rowStep;
    if (rowStep <= 0.0f)
        return false;
    for (std::size_t i = 1; i < layout.Items.size(); ++i) {
        const float step = layout.Items[i].Position - layout.Items[i - 1].Position;
        const float gap = step - layout.Items[i - 1].Size;
        minimumGap = std::min(minimumGap, gap);
        if (std::fabs(step - rowStep) > 0.002f || gap < 0.02f)
            return false;
    }
    return true;
}

bool HasExpectedNativeVisibility(PlayerAction action) {
    const bool mainVisible = IsEntityVisible("M_Main_But_1");
    const bool optionsVisible = IsEntityVisible("M_Options_But_4");
    const bool startVisible = IsEntityVisible("M_Start_But_01");

    switch (action) {
    case PlayerAction::CaptureModMenu:
    case PlayerAction::CaptureCustomMaps:
        return !mainVisible && !optionsVisible && !startVisible;
    case PlayerAction::CaptureHud:
    case PlayerAction::CaptureConsole:
    case PlayerAction::CaptureScriptTools:
        return g_LevelObserved.load(std::memory_order_acquire) && !mainVisible && !optionsVisible &&
               !startVisible;
    default:
        return true;
    }
}

void CompletePlayerAction(PlayerAction action, bool succeeded) {
    g_ActivePlayerAction = PlayerAction::None;
    g_ActivePlayerCheckpoint.clear();
    g_PlayerActionSucceeded.store(succeeded, std::memory_order_release);
    g_PlayerActionCompleted.fetch_add(1, std::memory_order_acq_rel);
    if (g_Logger) {
        g_Logger->Info("UI automation: player_action=%d succeeded=%s", static_cast<int>(action),
                       succeeded ? "true" : "false");
    }
}

const char *PlayerActionCheckpoint(PlayerAction action) {
    switch (action) {
    case PlayerAction::StartToLevelOne:
        return "input-start-to-level-1";
    case PlayerAction::CaptureModMenu:
        return "capture-mod-menu";
    case PlayerAction::CaptureHud:
        return "capture-hud";
    case PlayerAction::CaptureCustomMaps:
        return "capture-custom-maps";
    case PlayerAction::CaptureConsole:
        return "capture-console";
    case PlayerAction::CaptureScriptTools:
        return "capture-script-tools";
    default:
        return nullptr;
    }
}

bool IsCaptureAction(PlayerAction action) {
    return action == PlayerAction::CaptureModMenu || action == PlayerAction::CaptureHud ||
           action == PlayerAction::CaptureCustomMaps || action == PlayerAction::CaptureConsole ||
           action == PlayerAction::CaptureScriptTools;
}

bool IsMenuAction(PlayerAction action) {
    return action == PlayerAction::OptionsToMain ||
        action == PlayerAction::MainToStart;
}

const char *SurfaceName(PlayerAction action) {
    switch (action) {
    case PlayerAction::CaptureModMenu:
        return "mod-menu";
    case PlayerAction::CaptureHud:
        return "hud";
    case PlayerAction::CaptureCustomMaps:
        return "custom-maps";
    case PlayerAction::CaptureConsole:
        return "console";
    case PlayerAction::CaptureScriptTools:
        return "script-tools";
    default:
        return nullptr;
    }
}

void ProcessPlayerAction() {
    if (g_ActivePlayerAction == PlayerAction::None) {
        const PlayerAction action = static_cast<PlayerAction>(g_PlayerAction.exchange(
            static_cast<int>(PlayerAction::None), std::memory_order_acq_rel));
        if (action == PlayerAction::None)
            return;

        bool succeeded = false;
        switch (action) {
        case PlayerAction::OptionsToMain:
            if (IsEntityVisible("M_Options_But_Back")) {
                const std::optional<int> row = MenuRow("Menu_Options", "Options Menu");
                succeeded = row.has_value();
            }
            break;
        case PlayerAction::MainToStart:
            if (IsEntityVisible("M_Main_But_1") && !IsEntityVisible("M_Options_But_4") &&
                !IsEntityVisible("M_Start_But_01")) {
                const std::optional<int> row = MenuRow("Menu_Main", "Main Menu");
                succeeded = row.has_value();
            }
            break;
        case PlayerAction::StartToLevelOne:
            if (IsEntityVisible("M_Start_But_01")) {
                g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
                if (g_Logger) {
                    g_Logger->Info("UI automation: native_transition=start-to-level-1 "
                                   "requested=true input=keyboard");
                }
                succeeded = true;
            }
            break;
        case PlayerAction::CaptureModMenu:
            g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case PlayerAction::CaptureHud:
            g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case PlayerAction::CaptureCustomMaps:
            g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case PlayerAction::CaptureConsole:
            g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case PlayerAction::CaptureScriptTools:
            g_ActivePlayerCheckpoint = PlayerActionCheckpoint(action);
            succeeded = HasExpectedNativeVisibility(action);
            break;
        case PlayerAction::None:
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
        if ((!IsMenuAction(action) && g_ActivePlayerCheckpoint.empty()) || !succeeded) {
            CompletePlayerAction(action, succeeded);
            return;
        }
        g_ActivePlayerAction = action;
    }

    CheckpointProgress progress = CheckpointProgress::Failed;
    if (g_ActivePlayerAction == PlayerAction::OptionsToMain) {
        progress = AdvanceMenuInput(OptionsToMainRoute);
    } else if (g_ActivePlayerAction == PlayerAction::MainToStart) {
        progress = AdvanceMenuInput(MainToStartRoute);
    } else {
        const UiAutomationSession::CheckpointKind kind =
            IsCaptureAction(g_ActivePlayerAction)
                ? UiAutomationSession::CheckpointKind::Capture
                : UiAutomationSession::CheckpointKind::Input;
        progress = AdvanceCheckpoint(kind, g_ActivePlayerCheckpoint.c_str());
    }
    if (progress != CheckpointProgress::Waiting)
        CompletePlayerAction(g_ActivePlayerAction, progress == CheckpointProgress::Succeeded);
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
        const CheckpointProgress progress = AdvanceMenuInput(MainToOptionsRoute);
        if (progress == CheckpointProgress::Succeeded) {
            g_NativeEntryPhase = NativeEntryPhase::WaitForOptionsMenu;
            g_NativeFrameCount = 0;
        }
        return false;
    }
    case NativeEntryPhase::WaitForOptionsMenu:
        if (IsEntityVisible("M_Options_But_4")) {
            float rowStep = 0.0f;
            float minimumGap = 0.0f;
            g_NativeOptionsLayoutValid =
                OptionsMenuRowsAreSeparated(rowStep, minimumGap);
            if (g_Logger) {
                g_Logger->Info("UI automation: native_transition=main-to-options "
                               "observed=true rows_separated=%s row_step=%.4f "
                               "minimum_gap=%.4f",
                               g_NativeOptionsLayoutValid ? "true" : "false",
                               rowStep, minimumGap);
            }
            g_NativeEntryPhase = NativeEntryPhase::RequestOptionsToImGui;
            g_NativeFrameCount = 0;
        }
        return false;
    case NativeEntryPhase::RequestOptionsToImGui: {
        const CheckpointProgress progress = AdvanceMenuInput(OptionsToImGuiRoute);
        if (progress == CheckpointProgress::Succeeded) {
            g_NativeEntryPhase = NativeEntryPhase::WaitForImGuiMenu;
            g_NativeFrameCount = 0;
        }
        return false;
    }
    case NativeEntryPhase::WaitForImGuiMenu: {
        const bool hidden = g_NativeFrameCount >= 2 &&
            !IsEntityVisible("M_Options_But_4") &&
            !IsEntityVisible("M_Main_But_1") &&
            !IsEntityVisible("M_Start_But_01");
        if (hidden && !g_ImGuiNativeSurfacesHidden) {
            g_ImGuiNativeSurfacesHidden = true;
            if (g_Logger) {
                g_Logger->Info("UI automation: native_transition=options-to-imgui "
                               "observed=true native_surfaces=hidden");
            }
        }
        return hidden;
    }
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
    if (!g_LevelStarted.load(std::memory_order_acquire) ||
        g_LevelObserved.load(std::memory_order_relaxed) || !g_Session ||
        g_ActivePlayerAction != PlayerAction::None ||
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
    g_LevelSucceeded.store(progress == CheckpointProgress::Succeeded, std::memory_order_release);
    g_LevelObserved.store(true, std::memory_order_release);
}

} // namespace

namespace UiAutomation::Test {

namespace {

std::vector<ScenarioDefinition> &ScenarioRegistry() {
    static std::vector<ScenarioDefinition> scenarios;
    return scenarios;
}

ImGuiID FindVisibleMenuItem(ImGuiTestContext *ctx, const char *label) {
    if (!WaitForItem(ctx, "**/Back"))
        return 0;
    const ImGuiTestItemInfo back = ctx->ItemInfo("**/Back");
    if (!back.Window)
        return 0;

    ImGuiTestItemList items;
    ctx->GatherItems(&items, back.Window->ID, -1);
    for (const ImGuiTestItemInfo &item : items) {
        if (item.DebugLabel && std::strcmp(item.DebugLabel, label) == 0)
            return item.ID;
    }
    return 0;
}

bool CaptureVisibleMenuItems(ImGuiTestContext *ctx, std::vector<ImGuiID> &ids) {
    const ImGuiTestItemInfo back = ctx->ItemInfo("**/Back");
    if (!back.Window)
        return false;

    ImGuiTestItemList items;
    ctx->GatherItems(&items, back.Window->ID, -1);
    ids.clear();
    ids.reserve(items.size());
    for (const ImGuiTestItemInfo &item : items)
        ids.push_back(item.ID);
    std::sort(ids.begin(), ids.end());
    return true;
}

bool ChangeMenuPage(ImGuiTestContext *ctx, const char *control) {
    std::vector<ImGuiID> before;
    std::vector<ImGuiID> after;
    if (!CaptureVisibleMenuItems(ctx, before))
        return false;
    ctx->ItemClick(control);
    ctx->Yield();
    return CaptureVisibleMenuItems(ctx, after) && before != after;
}

bool RewindMenuPages(ImGuiTestContext *ctx) {
    while (ctx->ItemExists("**/PrevPage")) {
        if (!ChangeMenuPage(ctx, "**/PrevPage"))
            return false;
    }
    return true;
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

void WaitForDuration(ImGuiTestContext *ctx, std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline)
        ctx->Yield();
}

static bool RunPlayerAction(ImGuiTestContext *ctx, PlayerAction action,
                            std::chrono::milliseconds timeout) {
    if (g_PlayerAction.load(std::memory_order_acquire) != static_cast<int>(PlayerAction::None)) {
        return false;
    }

    const unsigned int completed = g_PlayerActionCompleted.load(std::memory_order_acquire);
    g_PlayerAction.store(static_cast<int>(action), std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (g_PlayerActionCompleted.load(std::memory_order_acquire) != completed)
            return g_PlayerActionSucceeded.load(std::memory_order_acquire);
        ctx->Yield();
    }
    return false;
}

bool RunNativeMenuTransition(ImGuiTestContext *ctx, NativeMenuTransition transition,
                             std::chrono::milliseconds timeout) {
    switch (transition) {
    case NativeMenuTransition::OptionsToMain:
        return RunPlayerAction(ctx, PlayerAction::OptionsToMain, timeout);
    case NativeMenuTransition::MainToStart:
        return RunPlayerAction(ctx, PlayerAction::MainToStart, timeout);
    case NativeMenuTransition::StartToLevelOne:
        return RunPlayerAction(ctx, PlayerAction::StartToLevelOne, timeout);
    }
    return false;
}

bool CaptureSurface(ImGuiTestContext *ctx, SurfaceCapture surface,
                    std::chrono::milliseconds timeout) {
    switch (surface) {
    case SurfaceCapture::ModMenu:
        return RunPlayerAction(ctx, PlayerAction::CaptureModMenu, timeout);
    case SurfaceCapture::Hud:
        return RunPlayerAction(ctx, PlayerAction::CaptureHud, timeout);
    case SurfaceCapture::CustomMaps:
        return RunPlayerAction(ctx, PlayerAction::CaptureCustomMaps, timeout);
    case SurfaceCapture::Console:
        return RunPlayerAction(ctx, PlayerAction::CaptureConsole, timeout);
    case SurfaceCapture::ScriptTools:
        return RunPlayerAction(ctx, PlayerAction::CaptureScriptTools, timeout);
    }
    return false;
}

bool ObserveModList(ImGuiTestContext *ctx) {
    if (!WaitForItem(ctx, "**/Ballance Mod Loader"))
        return false;
    g_ImGuiEntryObserved.store(true, std::memory_order_release);
    return true;
}

bool OpenConfigCategory(ImGuiTestContext *ctx, const char *category, const char *firstProperty) {
    if (!WaitForItem(ctx, "**/Back"))
        return false;
    if (!RewindMenuPages(ctx))
        return false;

    while (true) {
        const ImGuiID categoryId = FindVisibleMenuItem(ctx, category);
        if (categoryId != 0) {
            ctx->ItemClick(categoryId);
            return WaitForItem(ctx, firstProperty);
        }
        if (!ctx->ItemExists("**/NextPage"))
            return false;
        if (!ChangeMenuPage(ctx, "**/NextPage"))
            return false;
    }
}

bool OpenModConfigCategory(ImGuiTestContext *ctx, const char *modName, const char *category,
                           const char *firstProperty) {
    if (!ObserveModList(ctx))
        return false;
    const std::string modPath = std::string("**/") + modName;
    if (!WaitForItem(ctx, modPath.c_str()))
        return false;
    ctx->ItemClick(modPath.c_str());
    ctx->Yield();
    return OpenConfigCategory(ctx, category, firstProperty);
}

bool MenuPagesContain(ImGuiTestContext *ctx,
                      std::initializer_list<const char *> itemLabels) {
    if (!WaitForItem(ctx, "**/Back") || itemLabels.size() == 0)
        return false;
    if (!RewindMenuPages(ctx))
        return false;

    const std::vector<const char *> labels(itemLabels);
    std::vector<bool> seen(labels.size(), false);
    while (true) {
        const ImGuiTestItemInfo back = ctx->ItemInfo("**/Back");
        if (!back.Window)
            return false;

        ImGuiTestItemList items;
        ctx->GatherItems(&items, back.Window->ID, -1);
        for (const ImGuiTestItemInfo &item : items) {
            if (!item.DebugLabel)
                continue;
            for (std::size_t label = 0; label < labels.size(); ++label) {
                if (std::strcmp(item.DebugLabel, labels[label]) != 0)
                    continue;
                if (seen[label])
                    return false;
                seen[label] = true;
                break;
            }
        }

        if (!ctx->ItemExists("**/NextPage"))
            break;
        if (!ChangeMenuPage(ctx, "**/NextPage"))
            return false;
    }
    for (bool value : seen) {
        if (!value)
            return false;
    }
    return true;
}

bool ToggleConfigBoolean(ImGuiTestContext *ctx, const char *path) {
    if (!WaitForItem(ctx, path))
        return false;
    const bool previous = ctx->ItemIsChecked(path);
    const ImGuiTestItemInfo item = ctx->ItemInfo(path);
    ctx->MouseMoveToPos(ImVec2(item.RectFull.Max.x - 4.0f, item.RectFull.GetCenter().y));
    ctx->MouseClick();
    ctx->Yield();
    return ctx->ItemIsChecked(path) != previous;
}

bool SubmitConsoleCommand(ImGuiTestContext *ctx, const char *command) {
    ctx->KeyPress(ImGuiKey_Slash);
    if (!WaitForItem(ctx, "**/##CmdBar"))
        return false;
    ctx->ItemClick("**/##CmdBar");
    ctx->KeyChars(command);
    ctx->KeyPress(ImGuiKey_Enter);
    return WaitForItemToDisappear(ctx, "**/##CmdBar");
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
    if (!LeaveModListForOptions(ctx) ||
        !RunNativeMenuTransition(ctx, NativeMenuTransition::OptionsToMain)) {
        return false;
    }
    WaitForDuration(ctx, std::chrono::milliseconds(1200));
    if (!RunNativeMenuTransition(ctx, NativeMenuTransition::MainToStart))
        return false;
    return WaitForItem(ctx, "**/Enter_Custom_Maps");
}

bool WaitForLevelStart(ImGuiTestContext *ctx) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline &&
           !g_LevelObserved.load(std::memory_order_acquire)) {
        ctx->Yield();
    }
    if (!g_LevelObserved.load(std::memory_order_acquire) ||
        !g_LevelSucceeded.load(std::memory_order_acquire))
        return false;
    WaitForDuration(ctx, std::chrono::milliseconds(1600));
    return true;
}

bool EnterLevelOneFromStartMenu(ImGuiTestContext *ctx) {
    return RunNativeMenuTransition(ctx, NativeMenuTransition::StartToLevelOne) &&
           WaitForLevelStart(ctx);
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
    const bool nativeUiValid =
        g_NativeOptionsLayoutValid && g_ImGuiNativeSurfacesHidden;
    const bool passed = summary.CountTested == 1 && summary.CountSuccess == 1 &&
        summary.CountInQueue == 0 && nativeUiValid;
    int failures = summary.CountTested - summary.CountSuccess + summary.CountInQueue;
    if (!nativeUiValid)
        ++failures;
    if (!passed && failures < 1)
        failures = 1;
    if (!passed && g_Logger) {
        for (ImGuiTest *test : g_Engine->TestsAll) {
            if (!test || std::strcmp(test->Category,
                                     UiAutomation::Test::ScenarioCategory) != 0)
                continue;
            g_Logger->Error("UI automation test log:\n%s",
                            test->Output.Log.GetText());
        }
    }
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
    g_NativeOptionsLayoutValid = false;
    g_ImGuiNativeSurfacesHidden = false;
    g_FrameCount = 0;
    g_NativeFrameCount = 0;
    g_ExitFrameCount = 0;
    g_NativeEntryReadyAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    g_NativeEntryPhase = NativeEntryPhase::SettleMainMenu;
    g_ImGuiEntryObserved.store(false, std::memory_order_relaxed);
    g_ImGuiCloseRequested.store(false, std::memory_order_relaxed);
    g_NativeReturnObserved.store(false, std::memory_order_relaxed);
    g_NativeReturnSucceeded.store(false, std::memory_order_relaxed);
    g_LevelStarted.store(false, std::memory_order_relaxed);
    g_LevelObserved.store(false, std::memory_order_relaxed);
    g_LevelSucceeded.store(false, std::memory_order_relaxed);
    g_PlayerAction.store(static_cast<int>(PlayerAction::None), std::memory_order_relaxed);
    g_PlayerActionCompleted.store(0, std::memory_order_relaxed);
    g_PlayerActionSucceeded.store(false, std::memory_order_relaxed);
    g_ActivePlayerAction = PlayerAction::None;
    g_ActivePlayerCheckpoint.clear();
    if (g_Logger)
        g_Logger->Info("UI automation: gate=game-menu state=armed");
}

void OnStartLevel() {
    if (!g_Engine && !g_Armed)
        return;
    g_LevelStarted.store(true, std::memory_order_release);
    if (g_Logger)
        g_Logger->Info("UI automation: native_transition=level-entry observed=true");
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

    ProcessPlayerAction();
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
    if (g_Mod)
        g_Mod->SetHUD(g_InitialHudMode);

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
    g_LevelStarted.store(false, std::memory_order_relaxed);
    g_LevelObserved.store(false, std::memory_order_relaxed);
    g_LevelSucceeded.store(false, std::memory_order_relaxed);
    g_PlayerAction.store(static_cast<int>(PlayerAction::None), std::memory_order_relaxed);
    g_PlayerActionCompleted.store(0, std::memory_order_relaxed);
    g_PlayerActionSucceeded.store(false, std::memory_order_relaxed);
    g_ActivePlayerAction = PlayerAction::None;
}

} // namespace UiAutomation
