#include "BuiltinCustomMaps.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <random>

#include "BML/Bui.h"
#include "BML/DataShare.h"
#include "BML/Guids/Logics.h"
#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"
#include "BML/ScriptHelper.h"

#include "PathUtils.h"
#include "StringUtils.h"

using namespace ScriptHelper;

namespace {
constexpr const char *CUSTOM_MAP_NAME_KEY = "CustomMapName";
}

BuiltinCustomMaps::BuiltinCustomMaps()
    : m_Menu([this](const std::wstring &path) { return LoadMap(path); }) {}

void BuiltinCustomMaps::InitConfig(IConfig &config) {
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

void BuiltinCustomMaps::ApplyConfig() {
    if (m_ShowTooltip)
        m_Menu.SetShowTooltip(m_ShowTooltip->GetBoolean());
    if (m_MaxDepth)
        m_Menu.SetMaxDepth(m_MaxDepth->GetInteger());
}

bool BuiltinCustomMaps::OnModifyConfig(const char *category, const char *key, IProperty *property) {
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

void BuiltinCustomMaps::OnLoad(IBML &bml, ILogger &logger,
                               const std::wstring &loaderDirectory, const std::wstring &tempDirectory) {
    m_BML = &bml;
    m_CKContext = bml.GetCKContext();
    m_Logger = &logger;
    m_TempDirectory = tempDirectory;
    m_DataShare = BML_GetDataShare(nullptr);
    if (!m_DataShare)
        m_Logger->Error("Failed to acquire the loader data share for custom maps");

    m_Menu.Init(utils::CombinePathW(loaderDirectory, L"Maps"), logger);
}

void BuiltinCustomMaps::OnUnload() {
    m_Menu.Shutdown();
    ClearLoadMetadata();
    if (m_DataShare) {
        BML_DataShare_Release(m_DataShare);
        m_DataShare = nullptr;
    }

    ResetScriptBindings();
    m_TempDirectory.clear();
    m_Logger = nullptr;
    m_CKContext = nullptr;
    m_BML = nullptr;
}

void BuiltinCustomMaps::OnLoadObject(const char *filename) {
    if (!filename || std::strcmp(filename, "3D Entities\\Menu.nmo") != 0 || !m_BML)
        return;

    m_LevelButton = m_BML->Get2dEntityByName("M_Start_But_01");
    CKBehavior *menuStart = m_BML->GetScriptByName("Menu_Start");
    m_ExitStart = menuStart ? FindFirstBB(menuStart, "Exit") : nullptr;

    if ((!m_LevelButton || !m_ExitStart) && m_Logger)
        m_Logger->Warn("Custom map menu entry is unavailable in the current Menu.nmo");
}

void BuiltinCustomMaps::OnLoadScript(CKBehavior *script) {
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

void BuiltinCustomMaps::OnProcess() {
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

void BuiltinCustomMaps::OnStartLevel() {
    if (m_LoadCustom)
        SetParamValue(m_LoadCustom, FALSE);
    ClearLoadMetadata();
}

void BuiltinCustomMaps::OnExitGame() {
    ClearLoadMetadata();
    ResetScriptBindings();
}

bool BuiltinCustomMaps::Open() {
    return m_Menu.Open("Custom Maps");
}

bool BuiltinCustomMaps::Close() {
    return m_Menu.Close();
}

bool BuiltinCustomMaps::LoadMap(const std::wstring &path) {
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
        if (!BML_DataShare_Set(m_DataShare, CUSTOM_MAP_NAME_KEY,
                               mapPath.c_str(), mapPath.size() + 1)) {
            if (m_Logger)
                m_Logger->Error("Failed to publish custom map load metadata");
            ClearLoadMetadata();
            return false;
        }
        int level = m_LevelNumber->GetInteger();
        if (level < 1 || level > 13) {
            static std::mt19937 rng(std::random_device{}());
            level = std::uniform_int_distribution<int>(2, 11)(rng);
        }

        SetParamString(m_MapFile, filename.c_str());
        SetParamValue(m_LoadCustom, TRUE);
        m_CurrentLevel->SetElementValue(0, 0, &level);
        SetParamValue(m_LevelRow, level - 1);

        const CKMessageType loadLevel = messageManager->AddMessageType((CKSTRING) "Load Level");
        const CKMessageType loadMenu = messageManager->AddMessageType((CKSTRING) "Menu_Load");
        messageManager->SendMessageSingle(loadLevel, currentScene);
        messageManager->SendMessageSingle(loadMenu, allSound);
        blackScreen->Show(CKHIDE);
        m_ExitStart->ActivateInput(0);
        m_ExitStart->Activate();
        return true;
    } catch (const std::exception &exception) {
        if (m_Logger)
            m_Logger->Error("Exception loading custom map: %s", exception.what());
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Unknown exception loading custom map");
    }

    ClearLoadMetadata();
    return false;
}

std::string BuiltinCustomMaps::CreateTempMapFile(const std::wstring &path) const {
    if (path.empty() || !utils::FileExistsW(path) || m_TempDirectory.empty())
        return {};

    const std::wstring fileName = utils::GetFileNameW(path);
    const std::wstring fileNameWithoutExtension = utils::RemoveExtensionW(fileName);
    const std::wstring extension = utils::GetExtensionW(path);
    const size_t hash = utils::HashString(fileNameWithoutExtension);
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

void BuiltinCustomMaps::PatchLevelLoader(CKBehavior *script) {
    CKBehavior *loadLevel = FindFirstBB(script, "Load LevelXX");
    CKBehaviorLink *inputLink = loadLevel && loadLevel->GetInputCount() > 0
                                    ? FindNextLink(loadLevel, loadLevel->GetInput(0))
                                    : nullptr;
    CKBehaviorIO *target = inputLink ? inputLink->GetOutBehaviorIO() : nullptr;
    CKBehavior *operation = target ? FindNextBB(loadLevel, target->GetOwner()) : nullptr;
    auto *levelRowSource = operation && operation->GetOutputParameterCount() > 0
                               ? operation->GetOutputParameter(0)
                               : nullptr;
    CKParameter *levelRow = levelRowSource ? levelRowSource->GetDestination(0) : nullptr;
    CKBehavior *objectLoad = loadLevel ? FindFirstBB(loadLevel, "Object Load") : nullptr;
    auto *mapFileInput = objectLoad && objectLoad->GetInputParameterCount() > 0
                             ? objectLoad->GetInputParameter(0)
                             : nullptr;
    CKParameter *mapFile = mapFileInput ? mapFileInput->GetDirectSource() : nullptr;

    if (!loadLevel || !inputLink || !levelRow || !objectLoad || !mapFile) {
        if (m_Logger)
            m_Logger->Error("Custom map script patch is unavailable in the current Levelinit_build graph");
        m_LoadCustom = nullptr;
        m_MapFile = nullptr;
        m_LevelRow = nullptr;
        return;
    }

    CKBehavior *binarySwitch = CreateBB(loadLevel, VT_LOGICS_BINARYSWITCH);
    CKParameter *loadCustom = CreateLocalParameter(loadLevel, "Custom Level", CKPGUID_BOOL);
    if (!binarySwitch || !loadCustom) {
        if (m_Logger)
            m_Logger->Error("Failed to allocate the custom map script patch");
        m_LoadCustom = nullptr;
        m_MapFile = nullptr;
        m_LevelRow = nullptr;
        return;
    }

    CreateLink(loadLevel, loadLevel->GetInput(0), binarySwitch, 0);
    binarySwitch->GetInputParameter(0)->SetDirectSource(loadCustom);
    inputLink->SetInBehaviorIO(binarySwitch->GetOutput(1));
    CreateLink(loadLevel, binarySwitch, objectLoad);

    m_LoadCustom = loadCustom;
    m_MapFile = mapFile;
    m_LevelRow = levelRow;
}

void BuiltinCustomMaps::ClearLoadMetadata() {
    if (m_DataShare)
        BML_DataShare_Remove(m_DataShare, CUSTOM_MAP_NAME_KEY);
}

void BuiltinCustomMaps::ResetScriptBindings() {
    m_LevelButton = nullptr;
    m_ExitStart = nullptr;
    m_LoadCustom = nullptr;
    m_MapFile = nullptr;
    m_LevelRow = nullptr;
    m_CurrentLevel = nullptr;
}
