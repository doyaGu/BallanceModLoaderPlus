#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <Windows.h>
#include <dinput.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "CKAll.h"
#include "imgui.h"

#include "bml_config.hpp"
#include "bml_console.h"
#include "bml_engine_events.h"
#include "bml_extension.h"
#include "bml_input.h"
#include "bml_ui.h"
#include "bml_virtools_payloads.h"
#include "bml_module.h"
#include "bml_topics.h"
#include "BML/Guids/Logics.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include "ScriptHelper.h"

namespace {
namespace fs = std::filesystem;
using namespace BML_MapMenu::ScriptHelper;

constexpr size_t kSearchBufferSize = 512;
constexpr int kDefaultMaxDepth = 8;
constexpr int kDefaultOpenKey = DIK_F2;

enum class EntryType {
    Directory,
    File,
};

struct MapEntry {
    EntryType type = EntryType::Directory;
    std::string name;
    std::wstring path;
    std::vector<std::unique_ptr<MapEntry>> children;
};

struct MapMenuSettings {
    bool enabled = true;
    bool showTooltip = false;
    int maxDepth = kDefaultMaxDepth;
    int levelNumber = 0;
    int openKey = kDefaultOpenKey;
};

BML_Mod g_ModHandle = nullptr;
BML_Subscription g_DrawSub = nullptr;
BML_Subscription g_KeyDownSub = nullptr;
BML_Subscription g_EngineInitSub = nullptr;
BML_Subscription g_ScriptLoadSub = nullptr;

BML_TopicId g_TopicDraw = 0;
BML_TopicId g_TopicKeyDown = 0;
BML_TopicId g_TopicConsoleOutput = 0;
BML_TopicId g_TopicEngineInit = 0;
BML_TopicId g_TopicScriptLoad = 0;

static BML_UI_ApiTable *g_UiApi = nullptr;
PFN_bmlUIGetImGuiContext g_GetImGuiContext = nullptr;
BML_InputExtension *g_InputExtension = nullptr;
bool g_InputExtensionLoaded = false;
BML_ConsoleExtension *g_ConsoleExtension = nullptr;
bool g_ConsoleExtensionLoaded = false;

CKContext *g_Context = nullptr;
CKBehavior *g_ExitStart = nullptr;
CKParameter *g_LoadCustom = nullptr;
CKParameter *g_MapFile = nullptr;
CKParameter *g_LevelRow = nullptr;
CKDataArray *g_CurrentLevelArray = nullptr;

MapMenuSettings g_Settings;
bool g_Visible = false;
bool g_FocusSearch = false;
std::array<char, kSearchBufferSize> g_SearchBuffer = {};
std::unique_ptr<MapEntry> g_MapRoot;
const MapEntry *g_SelectedEntry = nullptr;
std::vector<const MapEntry *> g_SearchResults;

void SetVisible(bool visible);

std::string TrimCopy(const std::string &text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(start, end - start);
}

std::vector<std::string> SplitWords(const std::string &text) {
    std::vector<std::string> words;
    size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        size_t start = index;
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) == 0) {
            ++index;
        }
        if (index > start) {
            words.emplace_back(text.substr(start, index - start));
        }
    }
    return words;
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void AddCompletionIfMatches(const char *candidate,
    const std::string &prefix,
    PFN_BML_ConsoleCompletionSink sink,
    void *sinkUserData) {
    if (!candidate || !sink) {
        return;
    }

    const std::string loweredCandidate = ToLowerAscii(candidate);
    if (prefix.empty() || loweredCandidate.rfind(prefix, 0) == 0) {
        sink(candidate, sinkUserData);
    }
}

std::wstring Utf8ToWide(const std::string &value) {
    if (value.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring &value) {
    if (value.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string WideToAnsi(const std::wstring &value) {
    if (value.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    const std::string loweredHaystack = ToLowerAscii(std::string(haystack));
    const std::string loweredNeedle = ToLowerAscii(std::string(needle));
    return loweredHaystack.find(loweredNeedle) != std::string::npos;
}

std::wstring GetLoaderDir() {
    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"ModLoader";
    }
    fs::path exePath(modulePath);
    return (exePath.parent_path() / L"ModLoader").wstring();
}

std::wstring GetMapsDir() {
    return (fs::path(GetLoaderDir()) / L"Maps").wstring();
}

std::wstring GetTempMapsDir() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD length = GetTempPathW(MAX_PATH, tempPath);
    fs::path base = (length > 0 && length < MAX_PATH) ? fs::path(tempPath) : fs::temp_directory_path();
    return (base / L"BML" / L"Maps").wstring();
}

std::string FormatPathForUi(const std::wstring &path) {
    std::string utf8 = WideToUtf8(path);
    return utf8.empty() ? "<invalid path>" : utf8;
}

void PublishConsoleMessage(const std::string &message, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_SYSTEM) {
    if (g_TopicConsoleOutput == 0 || message.empty()) {
        return;
    }
    BML_ConsoleOutputEvent event = BML_CONSOLE_OUTPUT_EVENT_INIT;
    event.message_utf8 = message.c_str();
    event.flags = flags;
    bmlImcPublish(g_TopicConsoleOutput, &event, sizeof(event));
}

void EnsureDefaultConfig() {
    bml::Config config(g_ModHandle);
    if (!config.GetBool("General", "Enabled").has_value()) {
        config.SetBool("General", "Enabled", g_Settings.enabled);
    }
    if (!config.GetBool("General", "ShowTooltip").has_value()) {
        config.SetBool("General", "ShowTooltip", g_Settings.showTooltip);
    }
    if (!config.GetInt("General", "MaxDepth").has_value()) {
        config.SetInt("General", "MaxDepth", g_Settings.maxDepth);
    }
    if (!config.GetInt("General", "LevelNumber").has_value()) {
        config.SetInt("General", "LevelNumber", g_Settings.levelNumber);
    }
    if (!config.GetInt("General", "OpenKey").has_value()) {
        config.SetInt("General", "OpenKey", g_Settings.openKey);
    }
}

void RefreshConfig() {
    bml::Config config(g_ModHandle);
    g_Settings.enabled = config.GetBool("General", "Enabled").value_or(true);
    g_Settings.showTooltip = config.GetBool("General", "ShowTooltip").value_or(false);
    g_Settings.maxDepth = std::clamp(config.GetInt("General", "MaxDepth").value_or(kDefaultMaxDepth), 1, 64);
    g_Settings.levelNumber = config.GetInt("General", "LevelNumber").value_or(0);
    g_Settings.openKey = config.GetInt("General", "OpenKey").value_or(kDefaultOpenKey);
}

bool IsSupportedFileType(const std::wstring &path) {
    std::wstring extension = fs::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
    return extension == L".nmo" || extension == L".cmo";
}

std::unique_ptr<MapEntry> BuildMapTree(const fs::path &directory, int depth, bool &foundAny) {
    auto root = std::make_unique<MapEntry>();
    root->type = EntryType::Directory;
    root->name = directory.filename().empty() ? "Maps" : WideToUtf8(directory.filename().wstring());
    if (root->name.empty()) {
        root->name = "Maps";
    }
    root->path = directory.wstring();

    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec) || depth <= 0) {
        return root;
    }

    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator iterator(directory, ec); !ec && iterator != fs::directory_iterator(); iterator.increment(ec)) {
        entries.push_back(*iterator);
    }
    if (ec) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_MapMenu", "Failed to enumerate custom maps directory");
        return root;
    }

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &lhs, const fs::directory_entry &rhs) {
        const bool lhsDir = lhs.is_directory();
        const bool rhsDir = rhs.is_directory();
        if (lhsDir != rhsDir) {
            return lhsDir > rhsDir;
        }
        return WideToUtf8(lhs.path().filename().wstring()) < WideToUtf8(rhs.path().filename().wstring());
    });

    for (const fs::directory_entry &entry : entries) {
        const fs::path fullPath = entry.path();
        if (entry.is_directory()) {
            std::unique_ptr<MapEntry> child = BuildMapTree(fullPath, depth - 1, foundAny);
            child->type = EntryType::Directory;
            child->name = WideToUtf8(fullPath.filename().wstring());
            child->path = fullPath.wstring();
            if (!child->children.empty()) {
                foundAny = true;
            }
            root->children.push_back(std::move(child));
            continue;
        }

        if (!entry.is_regular_file() || !IsSupportedFileType(fullPath.wstring())) {
            continue;
        }

        auto child = std::make_unique<MapEntry>();
        child->type = EntryType::File;
        child->name = WideToUtf8(fullPath.stem().wstring());
        child->path = fullPath.wstring();
        root->children.push_back(std::move(child));
        foundAny = true;
    }

    return root;
}

void RebuildSearchResults(const MapEntry *entry, std::string_view query) {
    if (!entry) {
        return;
    }
    for (const std::unique_ptr<MapEntry> &child : entry->children) {
        if (child->type == EntryType::File) {
            const std::string fullPath = FormatPathForUi(child->path);
            if (ContainsCaseInsensitive(child->name, query) || ContainsCaseInsensitive(fullPath, query)) {
                g_SearchResults.push_back(child.get());
            }
        }
        if (!child->children.empty()) {
            RebuildSearchResults(child.get(), query);
        }
    }
}

void RefreshMapTree() {
    bool foundAny = false;
    g_MapRoot = BuildMapTree(fs::path(GetMapsDir()), g_Settings.maxDepth, foundAny);
    g_SelectedEntry = nullptr;
    g_SearchResults.clear();
    if (!foundAny) {
        PublishConsoleMessage("[mapmenu] no custom maps found under ModLoader/Maps");
    }
}

void RefreshSearchResults() {
    g_SearchResults.clear();
    if (!g_MapRoot) {
        return;
    }
    const std::string query = TrimCopy(g_SearchBuffer.data());
    if (query.empty()) {
        return;
    }
    RebuildSearchResults(g_MapRoot.get(), query);
}

CKDataArray *GetCurrentLevelArray() {
    if (!g_Context) {
        return nullptr;
    }
    g_CurrentLevelArray = static_cast<CKDataArray *>(g_Context->GetObjectByNameAndClass((CKSTRING) "CurrentLevel", CKCID_DATAARRAY));
    return g_CurrentLevelArray;
}

CK2dEntity *GetNamed2dEntity(const char *name) {
    if (!g_Context || !name) {
        return nullptr;
    }
    return static_cast<CK2dEntity *>(g_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_2DENTITY));
}

CKGroup *GetNamedGroup(const char *name) {
    if (!g_Context || !name) {
        return nullptr;
    }
    return static_cast<CKGroup *>(g_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_GROUP));
}

uint32_t HashStem(const std::wstring &value) {
    uint32_t hash = 2166136261u;
    for (wchar_t ch : value) {
        uint32_t code = static_cast<uint32_t>(towlower(ch));
        hash ^= code & 0xFFu;
        hash *= 16777619u;
        hash ^= (code >> 8) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

std::string CreateTempMapFile(const std::wstring &path) {
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) {
        return {};
    }

    const fs::path source(path);
    const std::wstring stem = source.stem().wstring();
    const std::wstring extension = source.extension().wstring();
    const uint32_t hash = HashStem(stem);

    wchar_t hashBuffer[9] = {};
    swprintf_s(hashBuffer, L"%08X", hash);

    const fs::path tempDir(GetTempMapsDir());
    fs::create_directories(tempDir, ec);
    if (ec) {
        return {};
    }

    const fs::path destination = tempDir / (std::wstring(hashBuffer) + extension);
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return {};
    }

    return WideToAnsi(destination.wstring());
}

bool HookLevelInitBuild(CKBehavior *script) {
    CKBehavior *loadLevel = FindFirstBB(script, "Load LevelXX");
    if (!loadLevel) {
        return false;
    }

    CKBehaviorLink *inLink = FindNextLink(loadLevel, loadLevel->GetInput(0));
    if (!inLink) {
        return false;
    }

    CKBehavior *operation = FindNextBB(loadLevel, inLink->GetOutBehaviorIO()->GetOwner());
    if (!operation || operation->GetOutputParameterCount() <= 0 || operation->GetOutputParameter(0)->GetDestinationCount() <= 0) {
        return false;
    }

    CKBehavior *objectLoad = FindFirstBB(loadLevel, "Object Load");
    if (!objectLoad || objectLoad->GetInputParameterCount() <= 0) {
        return false;
    }

    CKBehavior *binarySwitch = CreateBB(loadLevel, VT_LOGICS_BINARYSWITCH);
    if (!binarySwitch) {
        return false;
    }

    CreateLink(loadLevel, loadLevel->GetInput(0), binarySwitch, 0);
    g_LoadCustom = CreateLocalParameter(loadLevel, "Custom Level", CKPGUID_BOOL);
    if (!g_LoadCustom) {
        return false;
    }

    binarySwitch->GetInputParameter(0)->SetDirectSource(g_LoadCustom);
    inLink->SetInBehaviorIO(binarySwitch->GetOutput(1));
    CreateLink(loadLevel, binarySwitch, objectLoad);

    g_MapFile = objectLoad->GetInputParameter(0)->GetDirectSource();
    g_LevelRow = operation->GetOutputParameter(0)->GetDestination(0);
    return g_MapFile != nullptr && g_LevelRow != nullptr;
}

void LoadMap(const std::wstring &path) {
    if (!g_Context || !g_Settings.enabled) {
        return;
    }

    if (!g_LoadCustom || !g_MapFile || !g_LevelRow) {
        PublishConsoleMessage("[mapmenu] Levelinit_build hook is not ready yet", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        return;
    }

    CKDataArray *currentLevel = GetCurrentLevelArray();
    if (!currentLevel) {
        PublishConsoleMessage("[mapmenu] CurrentLevel array is unavailable", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        return;
    }

    const std::string tempFile = CreateTempMapFile(path);
    if (tempFile.empty()) {
        PublishConsoleMessage("[mapmenu] failed to stage selected map", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        return;
    }

    SetParamString(g_MapFile, tempFile.c_str());
    SetParamValue(g_LoadCustom, TRUE);

    int levelNumber = g_Settings.levelNumber;
    if (levelNumber < 1 || levelNumber > 13) {
        levelNumber = std::rand() % 10 + 2;
    }
    currentLevel->SetElementValue(0, 0, &levelNumber);

    int levelRow = levelNumber - 1;
    SetParamValue(g_LevelRow, levelRow);

    CKMessageManager *messageManager = g_Context->GetMessageManager();
    CKGroup *allSound = GetNamedGroup("All_Sound");
    CK2dEntity *blackScreen = GetNamed2dEntity("M_BlackScreen");
    if (!g_ExitStart || !messageManager || !allSound || !blackScreen || !g_Context->GetCurrentLevel()) {
        PublishConsoleMessage("[mapmenu] menu transition objects are unavailable", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        return;
    }

    CKMessageType loadLevelMessage = messageManager->AddMessageType((CKSTRING) "Load Level");
    CKMessageType loadMenuMessage = messageManager->AddMessageType((CKSTRING) "Menu_Load");
    messageManager->SendMessageSingle(loadLevelMessage, g_Context->GetCurrentLevel());
    messageManager->SendMessageSingle(loadMenuMessage, allSound);
    blackScreen->Show(CKHIDE);
    g_ExitStart->ActivateInput(0);
    g_ExitStart->Activate();

    PublishConsoleMessage("[mapmenu] loading " + FormatPathForUi(path));
    SetVisible(false);
}

void SetVisible(bool visible) {
    if (g_Visible == visible) {
        return;
    }
    g_Visible = visible;
    g_FocusSearch = visible;

    if (g_InputExtension) {
        if (visible) {
            g_InputExtension->BlockDevice(BML_INPUT_DEVICE_KEYBOARD);
            g_InputExtension->BlockDevice(BML_INPUT_DEVICE_MOUSE);
            g_InputExtension->ShowCursor(true);
        } else {
            g_InputExtension->UnblockDevice(BML_INPUT_DEVICE_MOUSE);
            g_InputExtension->UnblockDevice(BML_INPUT_DEVICE_KEYBOARD);
            g_InputExtension->ShowCursor(false);
        }
    }

    if (visible) {
        if (!g_MapRoot) {
            RefreshMapTree();
        }
        RefreshSearchResults();
    }
}

void ToggleVisible() {
    SetVisible(!g_Visible);
}

bool RenderMapEntry(const MapEntry *entry) {
    if (!entry) {
        return false;
    }

    if (entry->type == EntryType::Directory) {
        const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        const bool opened = ImGui::TreeNodeEx(entry, flags, "%s", entry->name.c_str());
        if (g_Settings.showTooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
        }
        bool loaded = false;
        if (opened) {
            for (const std::unique_ptr<MapEntry> &child : entry->children) {
                loaded = RenderMapEntry(child.get()) || loaded;
            }
            ImGui::TreePop();
        }
        return loaded;
    }

    const bool selected = (g_SelectedEntry == entry);
    if (ImGui::Selectable(entry->name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
        g_SelectedEntry = entry;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            LoadMap(entry->path);
            return true;
        }
    }
    if (g_Settings.showTooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
    }
    return false;
}

void RenderSearchResults() {
    if (g_SearchResults.empty()) {
        ImGui::TextDisabled("No matching maps.");
        return;
    }

    for (const MapEntry *entry : g_SearchResults) {
        const std::string label = entry->name + "##" + FormatPathForUi(entry->path);
        const bool selected = (g_SelectedEntry == entry);
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            g_SelectedEntry = entry;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                LoadMap(entry->path);
                return;
            }
        }
        if (g_Settings.showTooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
        }
    }
}

void RenderMainMenuButton() {
    if (g_Visible || !g_Context || !g_Settings.enabled) {
        return;
    }

    CK2dEntity *startButton = GetNamed2dEntity("M_Start_But_01");
    if (!startButton || !startButton->IsVisible()) {
        return;
    }

    const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.6238f, viewportSize.y * 0.4f), ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("BML_MapMenu_Button", nullptr, flags)) {
        if (ImGui::Button("Custom Maps")) {
            SetVisible(true);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void RenderWindow() {
    if (!g_Visible || !g_Settings.enabled) {
        RenderMainMenuButton();
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.08f, viewport->Pos.y + viewport->Size.y * 0.08f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 0.78f, viewport->Size.y * 0.78f), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Custom Maps", nullptr, flags)) {
        if (g_FocusSearch) {
            ImGui::SetKeyboardFocusHere();
            g_FocusSearch = false;
        }

        if (ImGui::InputTextWithHint("##map-search", "Search maps", g_SearchBuffer.data(), g_SearchBuffer.size())) {
            RefreshSearchResults();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            RefreshMapTree();
            RefreshSearchResults();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            SetVisible(false);
        }

        ImGui::Separator();

        if (!g_MapRoot) {
            ImGui::TextDisabled("Maps directory is not available.");
        } else if (TrimCopy(g_SearchBuffer.data()).empty()) {
            if (!g_MapRoot->children.empty()) {
                for (const std::unique_ptr<MapEntry> &child : g_MapRoot->children) {
                    if (RenderMapEntry(child.get())) {
                        break;
                    }
                }
            } else {
                ImGui::TextDisabled("No maps found under %s", FormatPathForUi(g_MapRoot->path).c_str());
            }
        } else {
            RenderSearchResults();
        }

        ImGui::Separator();
        if (g_SelectedEntry) {
            ImGui::TextWrapped("Selected: %s", FormatPathForUi(g_SelectedEntry->path).c_str());
            if (ImGui::Button("Load Selected")) {
                LoadMap(g_SelectedEntry->path);
            }
        } else {
            ImGui::TextDisabled("Select a map to load.");
        }
    }
    ImGui::End();
}

void OnDraw(BML_Context, BML_TopicId, const BML_ImcMessage *, void *) {
    if (!g_GetImGuiContext) {
        return;
    }
    ImGuiContext *context = static_cast<ImGuiContext *>(g_GetImGuiContext());
    if (!context) {
        return;
    }
    ImGui::SetCurrentContext(context);
    RenderWindow();
}

void OnKeyDown(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_KeyDownEvent)) {
        return;
    }
    const auto *event = static_cast<const BML_KeyDownEvent *>(msg->data);
    if (event->repeat || !g_Settings.enabled) {
        return;
    }
    if (static_cast<int>(event->key_code) == g_Settings.openKey) {
        ToggleVisible();
    }
}

BML_Result ExecuteMapMenuConsoleCommand(const BML_ConsoleCommandInvocation *invocation, void *) {
    if (!invocation || invocation->argc == 0 || !invocation->argv_utf8 || !invocation->argv_utf8[0]) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    if (invocation->argc == 1 || (invocation->argc >= 2 && ToLowerAscii(invocation->argv_utf8[1]) == "open")) {
        SetVisible(true);
        return BML_RESULT_OK;
    }
    if (ToLowerAscii(invocation->argv_utf8[1]) == "close") {
        SetVisible(false);
        return BML_RESULT_OK;
    }
    if (ToLowerAscii(invocation->argv_utf8[1]) == "refresh") {
        RefreshMapTree();
        RefreshSearchResults();
        PublishConsoleMessage("[mapmenu] refreshed custom map list");
        return BML_RESULT_OK;
    }

    PublishConsoleMessage("[mapmenu] usage: mapmenu [open|close|refresh]", BML_CONSOLE_OUTPUT_FLAG_ERROR);
    return BML_RESULT_INVALID_ARGUMENT;
}

void CompleteMapMenuConsoleCommand(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink,
    void *sinkUserData,
    void *) {
    if (!context || !sink || context->token_index != 1) {
        return;
    }

    const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
    AddCompletionIfMatches("open", prefix, sink, sinkUserData);
    AddCompletionIfMatches("close", prefix, sink, sinkUserData);
    AddCompletionIfMatches("refresh", prefix, sink, sinkUserData);
}

void OnEngineInit(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_EngineInitEvent)) {
        return;
    }
    const auto *event = static_cast<const BML_EngineInitEvent *>(msg->data);
    if (event->struct_size < sizeof(BML_EngineInitEvent) || !event->context) {
        return;
    }

    g_Context = event->context;
    GetCurrentLevelArray();
}

void OnScriptLoad(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_LegacyScriptLoadPayload)) {
        return;
    }

    const auto *payload = static_cast<const BML_LegacyScriptLoadPayload *>(msg->data);
    if (!payload->script) {
        return;
    }

    const char *scriptName = payload->script->GetName();
    if (!scriptName) {
        return;
    }

    if (strcmp(scriptName, "Levelinit_build") == 0) {
        if (HookLevelInitBuild(payload->script)) {
            PublishConsoleMessage("[mapmenu] custom map load hook armed");
        }
        return;
    }

    if (strcmp(scriptName, "Menu_Start") == 0) {
        g_ExitStart = FindFirstBB(payload->script, "Exit");
    }
}

BML_Result ValidateAttachArgs(const BML_ModAttachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModAttachArgs) || args->mod == nullptr || args->get_proc == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return args->api_version == BML_MOD_ENTRYPOINT_API_VERSION ? BML_RESULT_OK : BML_RESULT_VERSION_MISMATCH;
}

BML_Result ValidateDetachArgs(const BML_ModDetachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return args->api_version == BML_MOD_ENTRYPOINT_API_VERSION ? BML_RESULT_OK : BML_RESULT_VERSION_MISMATCH;
}

void ReleaseSubscriptions() {
    if (g_DrawSub) {
        bmlImcUnsubscribe(g_DrawSub);
        g_DrawSub = nullptr;
    }
    if (g_KeyDownSub) {
        bmlImcUnsubscribe(g_KeyDownSub);
        g_KeyDownSub = nullptr;
    }
    if (g_EngineInitSub) {
        bmlImcUnsubscribe(g_EngineInitSub);
        g_EngineInitSub = nullptr;
    }
    if (g_ScriptLoadSub) {
        bmlImcUnsubscribe(g_ScriptLoadSub);
        g_ScriptLoadSub = nullptr;
    }
}

BML_Result HandleAttach(const BML_ModAttachArgs *args) {
    const BML_Result validation = ValidateAttachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    g_ModHandle = args->mod;
    BML_Result result = bmlLoadAPI(args->get_proc, args->get_proc_by_id);
    if (result != BML_RESULT_OK) {
        return result;
    }

    {
        BML_Version uiReqVer = bmlMakeVersion(0, 4, 0);
        BML_Result uiResult = bmlExtensionLoad(BML_UI_EXTENSION_NAME, &uiReqVer, reinterpret_cast<void **>(&g_UiApi), nullptr);
        if (uiResult != BML_RESULT_OK || !g_UiApi) {
            bmlUnloadAPI();
            g_ModHandle = nullptr;
            return BML_RESULT_NOT_FOUND;
        }
    }
    g_GetImGuiContext = g_UiApi->GetImGuiContext;

    EnsureDefaultConfig();
    RefreshConfig();
    RefreshMapTree();

    BML_Version requiredVersion = bmlMakeVersion(1, 0, 0);
    result = bmlExtensionLoad(BML_INPUT_EXTENSION_NAME, &requiredVersion, reinterpret_cast<void **>(&g_InputExtension), nullptr);
    if (result == BML_RESULT_OK) {
        g_InputExtensionLoaded = true;
    } else {
        g_InputExtension = nullptr;
        g_InputExtensionLoaded = false;
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_MapMenu", "Failed to load BML_EXT_Input; map menu will not capture input");
    }

    bmlImcGetTopicId(BML_TOPIC_UI_DRAW, &g_TopicDraw);
    bmlImcGetTopicId(BML_TOPIC_INPUT_KEY_DOWN, &g_TopicKeyDown);
    bmlImcGetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &g_TopicConsoleOutput);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_INIT, &g_TopicEngineInit);
    bmlImcGetTopicId(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, &g_TopicScriptLoad);

    if ((result = bmlImcSubscribe(g_TopicDraw, OnDraw, nullptr, &g_DrawSub)) != BML_RESULT_OK) {
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }
    if ((result = bmlImcSubscribe(g_TopicKeyDown, OnKeyDown, nullptr, &g_KeyDownSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }
    if ((result = bmlImcSubscribe(g_TopicEngineInit, OnEngineInit, nullptr, &g_EngineInitSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }
    if ((result = bmlImcSubscribe(g_TopicScriptLoad, OnScriptLoad, nullptr, &g_ScriptLoadSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }

    requiredVersion = bmlMakeVersion(1, 0, 0);
    result = bmlExtensionLoad(BML_CONSOLE_EXTENSION_NAME, &requiredVersion, reinterpret_cast<void **>(&g_ConsoleExtension), nullptr);
    if (result == BML_RESULT_OK) {
        g_ConsoleExtensionLoaded = true;

        BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
        desc.name_utf8 = "mapmenu";
        desc.description_utf8 = "Open, close, or refresh the custom map browser";
        desc.execute = ExecuteMapMenuConsoleCommand;
        desc.complete = CompleteMapMenuConsoleCommand;
        result = g_ConsoleExtension->RegisterCommand(&desc);
        if (result != BML_RESULT_OK) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_MapMenu", "Failed to register mapmenu console command");
        }
    } else {
        g_ConsoleExtension = nullptr;
        g_ConsoleExtensionLoaded = false;
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_MapMenu", "Failed to load BML_EXT_Console; mapmenu console command will be unavailable");
    }

    PublishConsoleMessage("BML_MapMenu ready. Press F2 or run 'mapmenu'.");
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_MapMenu", "Map menu module initialized");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs *args) {
    const BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    SetVisible(false);
    if (g_ConsoleExtensionLoaded && g_ConsoleExtension) {
        g_ConsoleExtension->UnregisterCommand("mapmenu");
        bmlExtensionUnload(BML_CONSOLE_EXTENSION_NAME);
        g_ConsoleExtensionLoaded = false;
    }
    g_ConsoleExtension = nullptr;
    ReleaseSubscriptions();
    if (g_InputExtensionLoaded) {
        bmlExtensionUnload(BML_INPUT_EXTENSION_NAME);
        g_InputExtensionLoaded = false;
    }
    g_InputExtension = nullptr;
    g_GetImGuiContext = nullptr;
    bmlExtensionUnload(BML_UI_EXTENSION_NAME);
    g_UiApi = nullptr;
    g_Context = nullptr;
    g_ExitStart = nullptr;
    g_LoadCustom = nullptr;
    g_MapFile = nullptr;
    g_LevelRow = nullptr;
    g_CurrentLevelArray = nullptr;
    g_MapRoot.reset();
    g_SearchResults.clear();
    g_SelectedEntry = nullptr;
    g_SearchBuffer.fill('\0');
    bmlUnloadAPI();
    g_ModHandle = nullptr;
    return BML_RESULT_OK;
}

} // namespace

BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {
    switch (cmd) {
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach(static_cast<const BML_ModAttachArgs *>(data));
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach(static_cast<const BML_ModDetachArgs *>(data));
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}