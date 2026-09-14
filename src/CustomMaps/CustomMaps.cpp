#include "CustomMaps/CustomMaps.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <random>
#include <vector>

#include "BML/Bui.h"
#include "BML/DataShare.h"
#include "BML/Guids/Logics.h"
#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"

#include "Api/ObjectRefs.h"
#include "Loader/ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace Behavior = BML::Behavior;

namespace {
constexpr char CustomMapNameKey[] = "CustomMapName";

template <class T>
T *Resolve(ModContext *context, Behavior::ObjectRef reference) {
    CKObject *object = context ? context->ObjectRefs().Resolve(reference) : nullptr;
    return object ? T::Cast(object) : nullptr;
}

template <class T>
bool WriteParameter(CKParameter *parameter, const T &value) {
    T copy = value;
    return parameter && parameter->SetValue(&copy, sizeof(copy)) == CK_OK;
}

bool WriteString(CKParameter *parameter, const char *value) {
    std::string copy = value ? value : "";
    return parameter && parameter->SetStringValue(copy.data()) == CK_OK;
}

template <class T>
bool ReadParameter(CKParameter *parameter, T &value) {
    return parameter && parameter->GetValue(&value, FALSE) == CK_OK;
}

bool ReadString(CKParameter *parameter, std::string &value) {
    if (!parameter)
        return false;
    const int size = parameter->GetStringValue(nullptr, FALSE);
    if (size < 0)
        return false;
    std::vector<char> buffer(static_cast<std::size_t>(size) + 1u, '\0');
    if (size > 0 && parameter->GetStringValue(buffer.data(), FALSE) < 0)
        return false;
    value.assign(buffer.data());
    return true;
}

struct LoadStateSnapshot {
    std::string MapFile;
    int CurrentLevel = 0;
    int LevelRow = 0;
    CKBOOL LoadCustom = FALSE;
};

bool CaptureLoadState(CKParameter *mapFile, CKDataArray *currentLevel,
                      CKParameter *levelRow, CKParameter *loadCustom,
                      LoadStateSnapshot &snapshot) {
    return ReadString(mapFile, snapshot.MapFile) && currentLevel &&
        currentLevel->GetElementValue(0, 0, &snapshot.CurrentLevel) != 0 &&
        ReadParameter(levelRow, snapshot.LevelRow) &&
        ReadParameter(loadCustom, snapshot.LoadCustom);
}

bool RestoreLoadState(CKParameter *mapFile, CKDataArray *currentLevel,
                      CKParameter *levelRow, CKParameter *loadCustom,
                      const LoadStateSnapshot &snapshot) {
    int currentLevelValue = snapshot.CurrentLevel;
    bool restored = true;
    restored = WriteParameter(loadCustom, snapshot.LoadCustom) && restored;
    restored = WriteParameter(levelRow, snapshot.LevelRow) && restored;
    restored = currentLevel &&
        currentLevel->SetElementValue(0, 0, &currentLevelValue) != 0 &&
        restored;
    restored = WriteString(mapFile, snapshot.MapFile.c_str()) && restored;
    return restored;
}
}

CustomMaps::CustomMaps()
    : m_Menu([this](const std::wstring &path) { return LoadMap(path); }) {}

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
                        const std::wstring &loaderDirectory, const std::wstring &tempDirectory) {
    m_BML = &bml;
    m_CKContext = bml.GetCKContext();
    m_Logger = &logger;
    m_TempDirectory = tempDirectory;
    auto behavior = Behavior::Session::Open("BML");
    if (behavior)
        m_Behavior = behavior.Take();
    else
        m_Logger->Warn("Custom maps cannot use Behavior authoring: %s",
                       behavior.GetStatus().Message.empty()
                           ? "could not open the BML session"
                           : behavior.GetStatus().Message.c_str());
    ReleaseDataShare();
    m_DataShare = BML_GetDataShare(nullptr);
    if (!m_DataShare)
        m_Logger->Error("Failed to acquire the loader data share for custom maps");

    m_Menu.Init(utils::CombinePathW(loaderDirectory, L"Maps"), logger);
}

void CustomMaps::OnUnload() {
    m_Menu.Shutdown();
    ReleaseDataShare();

    ResetScriptBindings();
    m_Behavior.Reset();
    m_TempDirectory.clear();
    m_Logger = nullptr;
    m_CKContext = nullptr;
    m_BML = nullptr;
}

void CustomMaps::OnLoadObject(const char *filename) {
    if (!filename || std::strcmp(filename, "3D Entities\\Menu.nmo") != 0 || !m_BML)
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

    if (std::strcmp(script->GetName(), "Levelinit_build") == 0)
        PatchLevelLoader(script);
}

void CustomMaps::OnProcess() {
    ResolveLevelLoaderBindings();

    if (m_LevelButton && m_ExitStart && m_LevelButton->IsVisible()) {
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
            if (Bui::RightButton("Enter_Custom_Maps")) {
                m_ExitStart->ActivateInput(0);
                m_ExitStart->Activate();
                Open();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

    m_Menu.Render();
}

void CustomMaps::OnStartLevel() {
    if (m_LoadCustom) {
        const CKBOOL disabled = FALSE;
        (void) WriteParameter(m_LoadCustom, disabled);
    }
    ClearLoadMetadata();
}

void CustomMaps::OnExitGame() {
    ClearLoadMetadata();
    ResetScriptBindings();
}

bool CustomMaps::Open() {
    return m_Menu.Open("Custom Maps");
}

bool CustomMaps::Close() {
    return m_Menu.Close();
}

bool CustomMaps::LoadMap(const std::wstring &path) {
    LoadStateSnapshot previousState;
    bool stateChanged = false;
    try {
        if (path.empty()) {
            if (m_Logger)
                m_Logger->Error("Attempted to load an empty map path");
            return false;
        }
        if (!utils::FileExistsW(path)) {
            if (m_Logger) {
                m_Logger->Error("Map file does not exist: %s",
                                utils::Utf16ToUtf8(path).c_str());
            }
            return false;
        }

        if (!m_BML || !m_CKContext || !m_DataShare || !m_LevelNumber ||
            !m_LoadCustom || !m_MapFile || !m_LevelRow || !m_CurrentLevel || !m_ExitStart) {
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
            if (m_Logger)
                m_Logger->Error("Custom map loading is unavailable because the menu scene is incomplete");
            ClearLoadMetadata();
            return false;
        }

        const std::string filename = CreateTempMapFile(path);
        if (filename.empty()) {
            if (m_Logger) {
                m_Logger->Error("Failed to prepare custom map: %s",
                                utils::Utf16ToUtf8(path).c_str());
            }
            ClearLoadMetadata();
            return false;
        }

        const std::string mapPath = utils::ToString(path);
        if (!BML_DataShare_Set(m_DataShare, CustomMapNameKey,
                               mapPath.c_str(), mapPath.size() + 1)) {
            if (m_Logger)
                m_Logger->Error("Failed to publish custom map load metadata");
            ClearLoadMetadata();
            return false;
        }
        m_MetadataPublished = true;

        int level = m_LevelNumber->GetInteger();
        if (level < 1 || level > 13) {
            static std::mt19937 rng(std::random_device{}());
            level = std::uniform_int_distribution<int>(2, 11)(rng);
        }

        if (!CaptureLoadState(m_MapFile, m_CurrentLevel, m_LevelRow,
                              m_LoadCustom, previousState)) {
            if (m_Logger)
                m_Logger->Error("Failed to capture the custom map runtime state");
            ClearLoadMetadata();
            return false;
        }

        stateChanged = true;
        const CKBOOL enabled = TRUE;
        const int row = level - 1;
        const bool stateReady =
            WriteString(m_MapFile, filename.c_str()) &&
            m_CurrentLevel->SetElementValue(0, 0, &level) != 0 &&
            WriteParameter(m_LevelRow, row) &&
            WriteParameter(m_LoadCustom, enabled);
        if (!stateReady) {
            if (m_Logger)
                m_Logger->Error("Failed to prepare the custom map runtime state");
            const bool restored = RestoreLoadState(
                m_MapFile, m_CurrentLevel, m_LevelRow, m_LoadCustom,
                previousState);
            stateChanged = !restored;
            if (!restored && m_Logger)
                m_Logger->Error("Failed to restore the custom map runtime state");
            ClearLoadMetadata();
            return false;
        }

        const CKMessageType loadLevel = messageManager->AddMessageType((CKSTRING) "Load Level");
        const CKMessageType loadMenu = messageManager->AddMessageType((CKSTRING) "Menu_Load");
        messageManager->SendMessageSingle(loadLevel, currentScene);
        messageManager->SendMessageSingle(loadMenu, allSound);
        blackScreen->Show(CKHIDE);
        m_ExitStart->ActivateInput(0);
        m_ExitStart->Activate();
        stateChanged = false;
        return true;
    } catch (const std::exception &exception) {
        if (m_Logger)
            m_Logger->Error("Exception loading custom map: %s", exception.what());
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Unknown exception loading custom map");
    }

    if (stateChanged) {
        try {
            if (!RestoreLoadState(m_MapFile, m_CurrentLevel, m_LevelRow,
                                  m_LoadCustom, previousState) && m_Logger) {
                m_Logger->Error("Failed to restore the custom map runtime state");
            }
        } catch (...) {
        }
    }
    ClearLoadMetadata();
    return false;
}

std::string CustomMaps::CreateTempMapFile(const std::wstring &path) const {
    if (path.empty() || !utils::FileExistsW(path) || m_TempDirectory.empty())
        return {};

    const std::wstring fileName = utils::GetFileNameW(path);
    const std::wstring fileNameWithoutExtension = utils::RemoveExtensionW(fileName);
    const std::wstring extension = utils::GetExtensionW(path);
    const std::size_t hash = utils::HashString(fileNameWithoutExtension);
    const std::wstring mapsDirectory = utils::CombinePathW(m_TempDirectory, L"Maps");

    if (!utils::DirectoryExistsW(mapsDirectory) && !utils::CreateDirectoryW(mapsDirectory))
        return {};

    wchar_t hashString[9];
    swprintf(hashString, sizeof(hashString) / sizeof(wchar_t),
             L"%08X", static_cast<unsigned int>(hash));
    const std::wstring destination =
        utils::CombinePathW(mapsDirectory, std::wstring(hashString) + extension);

    if (!utils::CopyFileW(path, destination))
        return {};
    return utils::Utf16ToAnsi(destination);
}

void CustomMaps::PatchLevelLoader(CKBehavior *script) {
    ResetLevelLoaderPatch();

    if (!m_Behavior) {
        if (m_Logger)
            m_Logger->Error("Custom map script patch requires Behavior authoring");
        return;
    }

    auto outer = m_Behavior.Inspect(script, Behavior::View::Logical);
    if (!outer) {
        if (m_Logger)
            m_Logger->Error("Cannot inspect Levelinit_build: %s",
                            outer.GetStatus().Message.c_str());
        return;
    }
    auto loadLevel = outer->Find(Behavior::Named("Load LevelXX", 0));
    if (!loadLevel) {
        if (m_Logger)
            m_Logger->Error("Cannot find Levelinit_build/Load LevelXX: %s",
                            loadLevel.GetStatus().Message.c_str());
        return;
    }
    auto graph = outer->Inspect(loadLevel.Value());
    if (!graph) {
        if (m_Logger)
            m_Logger->Error("Cannot inspect Levelinit_build/Load LevelXX: %s",
                            graph.GetStatus().Message.c_str());
        return;
    }

    auto inputLink = graph->Leaving(graph->Root().In(0));
    auto first = graph->Next(graph->Root().In(0));
    if (!inputLink || !first) {
        if (m_Logger)
            m_Logger->Error("Cannot identify the Levelinit_build entry path");
        return;
    }

    auto operation = graph->Next(first.Value());
    auto objectLoad = graph->Find(Behavior::Named("Object Load", 0));
    if (!operation || !objectLoad) {
        if (m_Logger)
            m_Logger->Error("Cannot identify the Levelinit_build loader path");
        return;
    }

    ModContext *context = dynamic_cast<ModContext *>(m_BML);
    CKBehavior *operationBlock = Resolve<CKBehavior>(context, operation->Object());
    CKParameterOut *levelRowSource = operationBlock &&
        operationBlock->GetOutputParameterCount() > 0
            ? operationBlock->GetOutputParameter(0) : nullptr;
    CKParameter *levelRow = levelRowSource &&
        levelRowSource->GetDestinationCount() > 0
            ? levelRowSource->GetDestination(0) : nullptr;
    CKBehavior *objectLoadBlock = Resolve<CKBehavior>(context, objectLoad->Object());
    CKParameterIn *mapFileInput = objectLoadBlock &&
        objectLoadBlock->GetInputParameterCount() > 0
            ? objectLoadBlock->GetInputParameter(0) : nullptr;
    CKParameter *mapFile = mapFileInput ? mapFileInput->GetDirectSource() : nullptr;

    if (!levelRow || !mapFile) {
        if (m_Logger)
            m_Logger->Error("Custom map script patch is unavailable in the current Levelinit_build graph");
        return;
    }

    Behavior::Edit edit;
    auto level = edit.Root().Require(loadLevel.Value()).Graph();
    const auto entry = level.Require(inputLink.Value());
    const auto originalFirst = level.Require(first.Value());
    const auto loader = level.Require(objectLoad.Value());
    const auto selector = level.Add(m_Behavior.Use(VT_LOGICS_BINARYSWITCH));
    const auto custom = level.AppendLocal("Custom Level", CKPGUID_BOOL);
    level.Bind(selector.Pin(0, CKPGUID_BOOL), custom);
    level.Flow(level.Root().In(0), selector.In(0));
    level.Reconnect(entry, selector.Out(1),
                    originalFirst.In(inputLink->Target().Index()));
    level.Flow(selector.Out(0), loader.In(0));

    auto applied = outer->Apply("Custom map level loader", edit);
    if (!applied) {
        if (m_Logger)
            m_Logger->Error("Failed to apply the custom map script patch: %s",
                            applied.GetStatus().Message.empty()
                                ? "Behavior Patch creation failed"
                                : applied.GetStatus().Message.c_str());
        return;
    }

    m_LevelLoaderPatch = applied.Take();
    m_LevelSwitch = selector;
    m_MapFile = mapFile;
    m_LevelRow = levelRow;
    ResolveLevelLoaderBindings();
}

void CustomMaps::ResolveLevelLoaderBindings() {
    if (m_LoadCustom || !m_LevelLoaderPatch)
        return;

    auto resolved = m_LevelLoaderPatch.Resolve(m_LevelSwitch);
    if (!resolved) {
        if (resolved.Code() == BML_ERROR_BUSY)
            return;
        if (m_Logger) {
            m_Logger->Error(
                "The custom map selector could not be resolved: %s",
                resolved.GetStatus().Message.empty()
                    ? "the Behavior Patch did not become active"
                    : resolved.GetStatus().Message.c_str());
        }
        ResetLevelLoaderPatch();
        return;
    }
    CKBehavior *selector = Resolve<CKBehavior>(
        dynamic_cast<ModContext *>(m_BML), resolved.Value());
    CKParameterIn *flag = selector && selector->GetInputParameterCount() > 0
        ? selector->GetInputParameter(0) : nullptr;
    m_LoadCustom = flag ? flag->GetDirectSource() : nullptr;
    if (!m_LoadCustom) {
        if (m_Logger)
            m_Logger->Error("The custom map selector did not retain its Custom Level parameter");
        ResetLevelLoaderPatch();
    }
}

void CustomMaps::ResetLevelLoaderPatch() {
    (void) m_LevelLoaderPatch.Close();
    m_LevelLoaderPatch = {};
    m_LevelSwitch = {};
    m_LoadCustom = nullptr;
    m_MapFile = nullptr;
    m_LevelRow = nullptr;
}

void CustomMaps::ClearLoadMetadata() {
    if (!m_MetadataPublished)
        return;

    if (m_DataShare && BML_DataShare_Has(m_DataShare, CustomMapNameKey))
        BML_DataShare_Remove(m_DataShare, CustomMapNameKey);
    m_MetadataPublished = false;
}

void CustomMaps::ReleaseDataShare() {
    ClearLoadMetadata();
    if (!m_DataShare)
        return;

    BML_DataShare_Release(m_DataShare);
    m_DataShare = nullptr;
}

void CustomMaps::ResetScriptBindings() {
    ResetLevelLoaderPatch();
    m_LevelButton = nullptr;
    m_ExitStart = nullptr;
    m_CurrentLevel = nullptr;
}
