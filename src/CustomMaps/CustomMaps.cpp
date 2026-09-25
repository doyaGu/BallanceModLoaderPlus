#include "CustomMaps/CustomMaps.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "BML/Bui.h"
#include "BML/DataShare.h"
#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"

#include "Api/ObjectRefs.h"
#include "CustomMaps/CustomMapLoad.h"
#include "CustomMaps/LevelLoader.h"
#include "CustomMaps/MapCatalog.h"
#include "CustomMaps/MapCommand.h"
#include "Loader/ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace Behavior = BML::Behavior;
using CustomMap::LevelLoader;

namespace {
constexpr auto LoadTimeout = std::chrono::seconds(60);

template <class T>
T *Resolve(ModContext *context, Behavior::ObjectRef reference) {
    CKObject *object = context ? context->ObjectRefs().Resolve(reference) : nullptr;
    return object ? T::Cast(object) : nullptr;
}

}

struct CustomMaps::LoadAttempt {
    enum class PollState {
        Waiting,
        ObjectLoaded,
        Failed,
    };

    struct PollResult {
        PollState State = PollState::Waiting;
        const char *Reason = nullptr;
    };

    std::uint64_t Id = 0;
    int Level = 0;
    bool ObjectLoaded = false;
    bool LevelStarted = false;
    LoadOrigin Origin = LoadOrigin::Menu;
    std::wstring SourcePath;
    LevelLoader::Transaction Runtime;
    std::chrono::steady_clock::time_point Started;

    Behavior::Result<void> StartLevel() {
        auto result = Runtime.ClearRoute();
        if (result)
            LevelStarted = true;
        return result;
    }

    PollResult Poll(BML_DataShare *share, std::chrono::steady_clock::time_point now) {
        const auto elapsed = now - Started;
        if (ObjectLoaded) {
            if (!LevelStarted && elapsed > LoadTimeout)
                return {PollState::Failed, "timed out waiting for StartLevel"};
            return {};
        }

        CustomMapLoad::Result result;
        if (!CustomMapLoad::ReadResult(share, result)) {
            if (BML_DataShare_Has(share, CustomMapLoad::ResultKey))
                return {PollState::Failed, "the loader returned malformed completion data"};
            if (elapsed > LoadTimeout)
                return {PollState::Failed, "timed out waiting for Virtools Object Load"};
            return {};
        }

        BML_DataShare_Remove(share, CustomMapLoad::ResultKey);
        if (result.Attempt != Id)
            return {PollState::Failed, "the loader returned a mismatched completion id"};
        if (result.Value == CustomMapLoad::Outcome::Failed)
            return {PollState::Failed, "Virtools Object Load failed"};
        if (result.Value != CustomMapLoad::Outcome::Loaded)
            return {PollState::Failed, "the loader returned an unknown completion state"};

        ObjectLoaded = true;
        return {PollState::ObjectLoaded};
    }

    bool IsComplete() const {
        return ObjectLoaded && LevelStarted;
    }

    Behavior::Result<void> Rollback(CKDataArray *currentLevel) const {
        Behavior::Result<void> restored;
        try {
            restored = Runtime.Rollback(currentLevel);
        } catch (...) {
            Behavior::Status status;
            status.Error = Behavior::Error::NativeError;
            status.Phase = Behavior::Phase::Binding;
            status.Message = "An exception interrupted the custom map runtime rollback.";
            restored = Behavior::Result<void>::Failure(BML_ERROR_FAIL, std::move(status));
        }

        return restored;
    }
};

CustomMaps::CustomMaps()
    : m_Menu([this](const std::wstring &path) { return LoadMap(path); }),
      m_LevelLoader(std::make_unique<LevelLoader>()) {}

CustomMaps::~CustomMaps() {
    ReleaseDataShare();
}

void CustomMaps::InitConfig(IConfig &config) {
    m_LevelNumber = config.GetProperty("CustomMap", "LevelNumber");
    m_ShowTooltip = config.GetProperty("CustomMap", "ShowTooltip");
    m_MaxDepth = config.GetProperty("CustomMap", "MaxDepth");

    config.SetCategoryComment("CustomMap", "Custom Map Settings");

    m_LevelNumber->SetComment(
        "Level number to use for custom maps (affects level bonus and sky textures)."
        " Must be in the range of 1~13; 0 to randomly select one between 2 and 11");
    m_LevelNumber->SetDefaultInteger(0);

    m_ShowTooltip->SetComment("Show custom map's full name in tooltip");
    m_ShowTooltip->SetDefaultBoolean(false);

    m_MaxDepth->SetComment("The max depth of the nested subdirectories.");
    m_MaxDepth->SetDefaultInteger(8);
}

void CustomMaps::ApplyConfig() {
    if (m_ShowTooltip)
        m_Menu.SetShowTooltip(m_ShowTooltip->GetBoolean());
    if (m_MaxDepth)
        m_Menu.SetMaxDepth(m_MaxDepth->GetInteger());
}

bool CustomMaps::OnModifyConfig(const char *category, const char *key, IProperty *property) {
    if (!property || std::strcmp(category ? category : "", "CustomMap") != 0)
        return false;

    if (property == m_ShowTooltip && std::strcmp(key ? key : "", "ShowTooltip") == 0) {
        m_Menu.SetShowTooltip(property->GetBoolean());
        return true;
    }
    if (property == m_MaxDepth && std::strcmp(key ? key : "", "MaxDepth") == 0) {
        m_Menu.SetMaxDepth(property->GetInteger());
        return true;
    }
    return property == m_LevelNumber && std::strcmp(key ? key : "", "LevelNumber") == 0;
}

void CustomMaps::OnLoad(IBML &bml, ILogger &logger,
                        const std::wstring &gameDirectory, const std::wstring &loaderDirectory,
                        const std::wstring &tempDirectory) {
    m_BML = &bml;
    m_CKContext = bml.GetCKContext();
    m_Logger = &logger;
    m_Staging.Initialize(gameDirectory, tempDirectory);
    m_MapsDirectory = utils::CombinePathW(loaderDirectory, L"Maps");
    auto behavior = Behavior::Session::Open("BML");
    if (behavior) {
        m_Behavior = behavior.Take();
        InstallLevelLoader();
    } else {
        m_Logger->Warn("Custom maps cannot use Behavior authoring: %s",
                       behavior.GetStatus().Message.empty()
                           ? "could not open the BML session"
                           : behavior.GetStatus().Message.c_str());
    }
    ReleaseDataShare();
    m_DataShare = BML_GetDataShare(nullptr);
    if (!m_DataShare)
        m_Logger->Error("Failed to acquire the loader data share for custom maps");

    m_Menu.Init(m_MapsDirectory, logger);
    if (!m_Command)
        m_Command = std::make_unique<CustomMap::MapCommand>(*this);
    bml.RegisterCommand(m_Command.get());
}

void CustomMaps::OnUnload() {
    auto restored = RollbackLoad();
    if (!restored && m_Logger) {
        m_Logger->Error(
            "Failed to restore the custom map runtime state during unload: %s",
            restored.GetStatus().Message.empty()
                ? "the Behavior transaction rollback failed"
                : restored.GetStatus().Message.c_str());
    }
    m_Menu.Shutdown();
    ReleaseDataShare();

    ResetScriptBindings();
    m_Behavior.Reset();
    m_Staging.Shutdown();
    m_MapsDirectory.clear();
    m_Logger = nullptr;
    m_CKContext = nullptr;
    m_BML = nullptr;
}

void CustomMaps::OnLoadObject(const char *filename) {
    if (!filename || std::strcmp(filename, "3D Entities\\Menu.nmo") != 0 || !m_BML)
        return;

    BindMenuEntry();
}

void CustomMaps::BindMenuEntry() {
    if (!m_BML)
        return;

    m_LevelButton = m_BML->Get2dEntityByName("M_Start_But_01");
    CKBehavior *menuStart = m_BML->GetScriptByName("Menu_Start");
    m_ExitStart = nullptr;
    if (menuStart && m_Behavior) {
        auto menu = m_Behavior.Inspect(menuStart, Behavior::View::Logical);
        if (menu) {
            auto exit = menu->Find(Behavior::Named("Exit", 0));
            if (exit) {
                m_ExitStart = Resolve<CKBehavior>(
                    dynamic_cast<ModContext *>(m_BML), exit->Object());
            }
        }
    }

    if ((!m_LevelButton || !m_ExitStart) && m_Logger)
        m_Logger->Warn("Custom map menu entry is unavailable in the current Menu.nmo");
}

void CustomMaps::OnLoadScript(CKBehavior *script) {
    if (!script || !script->GetName())
        return;

    if (std::strcmp(script->GetName(), "Gameplay_Ingame") == 0) {
        m_CurrentLevel = m_BML ? m_BML->GetArrayByName("CurrentLevel") : nullptr;
        if (!m_CurrentLevel && m_Logger)
            m_Logger->Warn("Custom map level selection is unavailable: CurrentLevel was not found");
        return;
    }

    if (std::strcmp(script->GetName(), "Levelinit_build") == 0 &&
        m_LevelLoader) {
        m_LevelLoader->Invalidate();
    }
}

void CustomMaps::OnProcess() {
    RefreshLevelLoader();
    PollLoadResult();
    m_Menu.Render();

    if (!m_Menu.IsOpen() && IsRuntimeReady() && m_LevelButton->IsVisible()) {
        const ImVec2 &viewportSize = ImGui::GetMainViewport()->Size;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.6238f, viewportSize.y * 0.4f));

        constexpr ImGuiWindowFlags buttonFlags = ImGuiWindowFlags_NoDecoration |
                                                  ImGuiWindowFlags_NoBackground |
                                                  ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoNav |
                                                  ImGuiWindowFlags_AlwaysAutoResize |
                                                  ImGuiWindowFlags_NoFocusOnAppearing |
                                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                  ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Button_Custom_Maps", nullptr, buttonFlags)) {
            ImGui::SetNextItemShortcut(ImGuiKey_RightArrow, ImGuiInputFlags_RouteGlobal);
            if (Bui::RightButton("Enter_Custom_Maps")) {
                m_ExitStart->ActivateInput(0);
                m_ExitStart->Activate();
                Open();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }
}

void CustomMaps::OnPostStartMenu() {
    BindMenuEntry();
}

void CustomMaps::OnStartLevel() {
    if (m_LoadAttempt) {
        auto finished = m_LoadAttempt->StartLevel();
        if (!finished) {
            const std::string reason = finished.GetStatus().Message.empty()
                ? "the custom-level route could not be cleared"
                : finished.GetStatus().Message;
            CompleteLoadFailure(reason.c_str());
        } else {
            PollLoadResult();
            TryCompleteLoad();
        }
    }
    m_LevelButton = nullptr;
    m_ExitStart = nullptr;
}

void CustomMaps::OnExitGame() {
    auto restored = RollbackLoad();
    if (!restored && m_Logger) {
        m_Logger->Error(
            "Failed to restore the custom map runtime state during world teardown: %s",
            restored.GetStatus().Message.empty()
                ? "the Behavior transaction rollback failed"
                : restored.GetStatus().Message.c_str());
    }
    m_Staging.ReleaseLoaded();
    m_Menu.ResetLoad();
    ResetScriptBindings();
}

bool CustomMaps::Open() {
    if (m_LoadAttempt) {
        if (m_Logger)
            m_Logger->Warn("A custom map load is already in progress");
        return false;
    }
    if (!IsRuntimeReady()) {
        if (m_Logger)
            m_Logger->Error("Custom map loading is unavailable because its runtime bindings are incomplete");
        return false;
    }
    return m_Menu.Open("Custom Maps");
}

bool CustomMaps::Close() {
    if (m_LoadAttempt)
        return false;
    return m_Menu.Close();
}

bool CustomMaps::LoadFromCommand(std::string_view relativePath, std::string &error) {
    error.clear();
    if (m_Menu.IsOpen()) {
        error = "close the Custom Maps menu before loading from a command";
        return false;
    }
    if (m_LoadAttempt) {
        error = "another custom map load is already in progress";
        return false;
    }
    if (!IsRuntimeReady()) {
        error = "custom maps can only be loaded from the main menu";
        return false;
    }

    std::wstring path;
    if (!MapCatalog::ResolveFile(m_MapsDirectory, relativePath, path, error))
        return false;
    return BeginLoad(path, LoadOrigin::Command, error);
}

std::vector<std::string> CustomMaps::ListMaps(std::string_view fragment,
                                               std::string &error) const {
    error.clear();
    if (m_MapsDirectory.empty()) {
        error = "the maps directory is unavailable";
        return {};
    }

    MapCatalog catalog;
    const int maxDepth = m_MaxDepth ? m_MaxDepth->GetInteger() : 8;
    if (!catalog.Refresh(m_MapsDirectory, (std::max)(1, maxDepth), m_Logger)) {
        error = "could not scan ModLoader/Maps";
        return {};
    }
    return catalog.ListFiles(fragment, 21);
}

bool CustomMaps::LoadMap(const std::wstring &requestedPath) {
    std::wstring path;
    std::string error;
    if (!MapCatalog::ValidateFile(m_MapsDirectory, requestedPath, path, error)) {
        if (m_Logger) {
            m_Logger->Error("Custom map is unavailable: %s: %s",
                            utils::Utf16ToUtf8(requestedPath).c_str(), error.c_str());
        }
        return false;
    }
    return BeginLoad(path, LoadOrigin::Menu, error);
}

bool CustomMaps::BeginLoad(const std::wstring &path, LoadOrigin origin,
                           std::string &error) {
    error.clear();

    LevelLoader::Transaction transaction;
    bool stateChanged = false;
    std::uint64_t stagedAttempt = 0;
    try {
        if (m_LoadAttempt) {
            error = "another custom map load is already in progress";
            if (m_Logger)
                m_Logger->Warn("A custom map load is already in progress");
            return false;
        }
        if (!IsRuntimeReady()) {
            error = "custom maps can only be loaded from the main menu";
            if (m_Logger)
                m_Logger->Error("Custom map loading is unavailable because its runtime bindings are incomplete");
            ClearLoadMetadata();
            return false;
        }
        CKLevel *currentScene = m_CKContext->GetCurrentLevel();
        CKGroup *allSound = m_BML->GetGroupByName("All_Sound");
        CK2dEntity *blackScreen = m_BML->Get2dEntityByName("M_BlackScreen");
        CKMessageManager *messageManager = m_CKContext->GetMessageManager();
        if (!currentScene || !allSound || !blackScreen || !messageManager) {
            error = "the main menu scene is incomplete";
            if (m_Logger)
                m_Logger->Error("Custom map loading is unavailable because the menu scene is incomplete");
            ClearLoadMetadata();
            return false;
        }

        std::uint64_t attemptId = m_NextLoadAttempt++;
        if (attemptId == 0)
            attemptId = m_NextLoadAttempt++;

        CustomMap::StagedMap stagedMap;
        if (!m_Staging.Prepare(path, attemptId, m_CKContext->GetPathManager(),
                               stagedMap, error)) {
            if (error.empty())
                error = "could not prepare a private copy of the map";
            if (m_Logger) {
                m_Logger->Error("Failed to prepare custom map %s: %s",
                                utils::Utf16ToUtf8(path).c_str(), error.c_str());
            }
            ClearLoadMetadata();
            return false;
        }
        stagedAttempt = stagedMap.Attempt;
        std::string filename = std::move(stagedMap.LoadPath);

        int level = m_LevelNumber->GetInteger();
        if (level < 1 || level > 13) {
            static std::mt19937 rng(std::random_device{}());
            level = std::uniform_int_distribution<int>(2, 11)(rng);
        }

        auto begun = m_LevelLoader->Begin(m_CurrentLevel);
        if (!begun) {
            error = begun.GetStatus().Message.empty()
                ? "the level-loader state is unavailable"
                : begun.GetStatus().Message;
            if (m_Logger) {
                m_Logger->Error(
                    "Failed to begin the custom map load transaction: %s",
                    begun.GetStatus().Message.empty()
                        ? "the level-loader state is unavailable"
                        : begun.GetStatus().Message.c_str());
            }
            m_LevelLoader->Invalidate();
            ClearLoadMetadata();
            m_Staging.Complete(stagedAttempt, CustomMap::StagedMapCompletion::Discarded);
            return false;
        }
        transaction = begun.Take();

        stateChanged = true;
        auto staged = transaction.Stage(filename, level, m_CurrentLevel);
        if (!staged) {
            error = staged.GetStatus().Message.empty()
                ? "the level-loader transaction was rejected"
                : staged.GetStatus().Message;
            if (m_Logger) {
                m_Logger->Error(
                    "Failed to prepare the custom map runtime state: %s",
                    staged.GetStatus().Message.empty()
                        ? "the level-loader transaction was rejected"
                        : staged.GetStatus().Message.c_str());
            }
        } else if (!PublishLoadMetadata(path, attemptId)) {
            error = "could not publish custom map load metadata";
            if (m_Logger)
                m_Logger->Error("Failed to publish custom map load metadata");
        }

        if (!error.empty()) {
            auto restored = transaction.Rollback(m_CurrentLevel);
            stateChanged = !restored;
            if (!restored && m_Logger) {
                m_Logger->Error(
                    "Failed to restore the custom map runtime state: %s",
                    restored.GetStatus().Message.empty()
                        ? "the rollback was rejected"
                        : restored.GetStatus().Message.c_str());
            }
            ClearLoadMetadata();
            m_Staging.Complete(
                stagedAttempt, stateChanged ? CustomMap::StagedMapCompletion::Retained :
                                              CustomMap::StagedMapCompletion::Discarded);
            return false;
        }

        auto attempt = std::make_unique<LoadAttempt>();
        attempt->Id = attemptId;
        attempt->Level = level;
        attempt->Origin = origin;
        attempt->SourcePath = path;
        attempt->Runtime = std::move(transaction);
        attempt->Started = std::chrono::steady_clock::now();
        m_LoadAttempt = std::move(attempt);
        stateChanged = false;

        if (m_Logger) {
            m_Logger->Info(
                "Dispatch custom map load #%llu: %s (level %d)",
                static_cast<unsigned long long>(attemptId),
                utils::Utf16ToUtf8(path).c_str(), level);
        }

        const CKMessageType loadLevel = messageManager->AddMessageType((CKSTRING) "Load Level");
        const CKMessageType loadMenu = messageManager->AddMessageType((CKSTRING) "Menu_Load");
        messageManager->SendMessageSingle(loadLevel, currentScene);
        messageManager->SendMessageSingle(loadMenu, allSound);
        blackScreen->Show(CKHIDE);
        m_ExitStart->ActivateInput(0);
        m_ExitStart->Activate();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        if (m_Logger)
            m_Logger->Error("Exception loading custom map: %s", exception.what());
    } catch (...) {
        error = "an unexpected error interrupted the map load";
        if (m_Logger)
            m_Logger->Error("Unknown exception loading custom map");
    }

    if (m_LoadAttempt) {
        m_LoadAttempt->Origin = LoadOrigin::Menu;
        CompleteLoadFailure("an exception interrupted the load dispatch");
    } else if (stateChanged) {
        try {
            auto restored = transaction.Rollback(m_CurrentLevel);
            stateChanged = !restored;
            if (!restored && m_Logger) {
                m_Logger->Error(
                    "Failed to restore the custom map runtime state: %s",
                    restored.GetStatus().Message.empty()
                        ? "the rollback was rejected"
                        : restored.GetStatus().Message.c_str());
            }
        } catch (...) {
        }
    }
    ClearLoadMetadata();
    if (stagedAttempt != 0) {
        m_Staging.Complete(
            stagedAttempt, stateChanged ? CustomMap::StagedMapCompletion::Retained :
                                          CustomMap::StagedMapCompletion::Discarded);
    }
    if (error.empty())
        error = "the map load could not be started";
    return false;
}

bool CustomMaps::IsRuntimeReady() const {
    return m_BML && m_CKContext && m_DataShare && m_LevelNumber &&
           m_LevelLoader && m_LevelLoader->IsReady() && m_CurrentLevel &&
           m_LevelButton && m_ExitStart;
}

bool CustomMaps::PublishLoadMetadata(const std::wstring &path,
                                     std::uint64_t attempt) {
    if (!m_DataShare || attempt == 0)
        return false;

    ClearLoadMetadata();
    const std::string mapPath = utils::Utf16ToUtf8(path);
    if (!BML_DataShare_Set(m_DataShare, CustomMapLoad::NameKey,
                           mapPath.c_str(), mapPath.size() + 1)) {
        return false;
    }
    if (!CustomMapLoad::WriteRequest(m_DataShare, attempt)) {
        BML_DataShare_Remove(m_DataShare, CustomMapLoad::NameKey);
        return false;
    }
    return true;
}

void CustomMaps::PollLoadResult() {
    if (!m_LoadAttempt || !m_DataShare)
        return;

    const LoadAttempt::PollResult result = m_LoadAttempt->Poll(
        m_DataShare, std::chrono::steady_clock::now());
    if (result.State == LoadAttempt::PollState::Waiting)
        return;
    if (result.State == LoadAttempt::PollState::Failed) {
        CompleteLoadFailure(result.Reason);
        return;
    }

    if (m_Logger) {
        m_Logger->Info(
            "Custom map object load #%llu completed; waiting for StartLevel: %s",
            static_cast<unsigned long long>(m_LoadAttempt->Id),
            utils::Utf16ToUtf8(m_LoadAttempt->SourcePath).c_str());
    }
    ClearLoadMetadata();
    TryCompleteLoad();
}

void CustomMaps::TryCompleteLoad() {
    if (m_LoadAttempt && m_LoadAttempt->IsComplete())
        CompleteLoadSuccess();
}

void CustomMaps::CompleteLoadSuccess() {
    if (!m_LoadAttempt)
        return;

    if (m_Logger) {
        m_Logger->Info(
            "Custom map load #%llu completed: %s (level %d)",
            static_cast<unsigned long long>(m_LoadAttempt->Id),
            utils::Utf16ToUtf8(m_LoadAttempt->SourcePath).c_str(),
            m_LoadAttempt->Level);
    }
    const bool fromCommand = m_LoadAttempt->Origin == LoadOrigin::Command;
    const std::wstring sourcePath = m_LoadAttempt->SourcePath;
    const std::uint64_t attempt = m_LoadAttempt->Id;
    m_LoadAttempt.reset();
    if (!m_Staging.Complete(attempt, CustomMap::StagedMapCompletion::Loaded) && m_Logger)
        m_Logger->Error("Custom map staging state was lost for completed load #%llu",
                        static_cast<unsigned long long>(attempt));
    m_Menu.CompleteLoad(true);
    ClearLoadMetadata();
    if (fromCommand && m_BML) {
        const std::string message = "map load: loaded " +
                                    utils::Utf16ToUtf8(utils::GetFileNameW(sourcePath));
        m_BML->SendIngameMessage(message.c_str());
    }
}

void CustomMaps::CompleteLoadFailure(const char *reason) {
    if (!m_LoadAttempt)
        return;

    const std::uint64_t attempt = m_LoadAttempt->Id;
    const bool fromCommand = m_LoadAttempt->Origin == LoadOrigin::Command;
    const std::string sourcePath = utils::Utf16ToUtf8(m_LoadAttempt->SourcePath);
    auto restored = RollbackLoad();

    if (m_Logger) {
        m_Logger->Error(
            "Custom map load #%llu failed: %s: %s",
            static_cast<unsigned long long>(attempt), sourcePath.c_str(),
            reason ? reason : "unknown failure");
        if (!restored) {
            m_Logger->Error(
                "Failed to restore the custom map runtime state: %s",
                restored.GetStatus().Message.empty()
                    ? "the Behavior transaction rollback failed"
                    : restored.GetStatus().Message.c_str());
        }
    }

    ReactivateStartMenu();
    if (fromCommand && m_BML) {
        const std::string message = std::string("map load: failed: ") +
                                    (reason ? reason : "unknown failure");
        m_BML->SendIngameMessage(message.c_str());
    }
}

Behavior::Result<void> CustomMaps::RollbackLoad() {
    if (!m_LoadAttempt)
        return Behavior::Result<void>::Success();

    const std::uint64_t attempt = m_LoadAttempt->Id;
    auto restored = m_LoadAttempt->Rollback(m_CurrentLevel);
    m_LoadAttempt.reset();
    if (!m_Staging.Complete(
            attempt, restored ? CustomMap::StagedMapCompletion::Discarded :
                                CustomMap::StagedMapCompletion::Retained) && m_Logger) {
        m_Logger->Error("Custom map staging state was lost for failed load #%llu",
                        static_cast<unsigned long long>(attempt));
    }
    m_Menu.CompleteLoad(false);
    ClearLoadMetadata();
    return restored;
}

void CustomMaps::ReactivateStartMenu() {
    CKBehavior *menuStart = m_BML ? m_BML->GetScriptByName("Menu_Start") : nullptr;
    CKScene *scene = m_CKContext ? m_CKContext->GetCurrentScene() : nullptr;
    if (menuStart && scene) {
        scene->Activate(menuStart, true);
    } else if (m_Logger) {
        m_Logger->Error("Failed to reactivate Menu_Start after a custom map load failure");
    }
}

void CustomMaps::InstallLevelLoader() {
    if (!m_Behavior || !m_LevelLoader) {
        if (m_Logger)
            m_Logger->Error("Custom map script patch requires Behavior authoring");
        return;
    }

    auto installed = m_LevelLoader->Install(m_Behavior);
    if (!installed) {
        if (m_Logger)
            m_Logger->Error("Failed to install the custom map level loader plan: %s",
                            installed.GetStatus().Message.empty()
                                ? "Behavior Plan creation failed"
                                : installed.GetStatus().Message.c_str());
    }
}

void CustomMaps::RefreshLevelLoader() {
    if (!m_LevelLoader || m_LevelLoader->IsReady())
        return;

    auto refreshed = m_LevelLoader->Refresh();
    if (m_LevelLoader->IsRetiring())
        return;
    if (refreshed || refreshed.Code() == BML_ERROR_BUSY)
        return;

    if (m_Logger) {
        m_Logger->Error(
            "The custom map level loader could not be bound: %s",
            refreshed.GetStatus().Message.empty()
                ? "the Behavior Plan instance is unavailable"
                : refreshed.GetStatus().Message.c_str());
    }
    ResetLevelLoader();
}

void CustomMaps::ResetLevelLoader() {
    if (!m_LevelLoader)
        return;

    auto closed = m_LevelLoader->Close();
    if (!closed && closed.Code() != BML_ERROR_BUSY && m_Logger) {
        m_Logger->Error(
            "Failed to restore the custom map level loader plan: %s",
            closed.GetStatus().Message.empty()
                ? "Behavior Plan close failed"
                : closed.GetStatus().Message.c_str());
    }
}

void CustomMaps::ClearLoadMetadata() {
    if (!m_DataShare)
        return;
    BML_DataShare_Remove(m_DataShare, CustomMapLoad::NameKey);
    BML_DataShare_Remove(m_DataShare, CustomMapLoad::RequestKey);
    BML_DataShare_Remove(m_DataShare, CustomMapLoad::ResultKey);
}

void CustomMaps::ReleaseDataShare() {
    ClearLoadMetadata();
    if (!m_DataShare)
        return;

    BML_DataShare_Release(m_DataShare);
    m_DataShare = nullptr;
}

void CustomMaps::ResetScriptBindings() {
    ResetLevelLoader();
    m_LevelButton = nullptr;
    m_ExitStart = nullptr;
    m_CurrentLevel = nullptr;
}
