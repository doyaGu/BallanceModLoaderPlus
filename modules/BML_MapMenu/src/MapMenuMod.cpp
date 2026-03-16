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

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_console.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_imgui.hpp"
#include "bml_imc_state.h"
#include "bml_input.h"
#include "bml_input_control.h"
#include "bml_interface.hpp"
#include "bml_virtools_payloads.h"
#include "bml_topics.h"
#include "bml_ui_host.h"
#include "bml_ui_helpers.hpp"
#include "BML/Guids/Logics.h"

#include "BML/ScriptGraph.h"

namespace {
namespace fs = std::filesystem;

constexpr size_t kSearchBufferSize = 512;
constexpr int kDefaultMaxDepth = 8;
constexpr int kDefaultOpenKey = DIK_F2;

enum class EntryType { Directory, File };

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

std::string TrimCopy(const std::string &text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) ++start;
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) --end;
    return text.substr(start, end - start);
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void AddCompletionIfMatches(const char *candidate, const std::string &prefix,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData) {
    if (!candidate || !sink) return;
    const std::string loweredCandidate = ToLowerAscii(candidate);
    if (prefix.empty() || loweredCandidate.rfind(prefix, 0) == 0) sink(candidate, sinkUserData);
}

std::wstring Utf8ToWide(const std::string &value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring &value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string WideToAnsi(const std::wstring &value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    const std::string loweredHaystack = ToLowerAscii(std::string(haystack));
    const std::string loweredNeedle = ToLowerAscii(std::string(needle));
    return loweredHaystack.find(loweredNeedle) != std::string::npos;
}

std::wstring GetLoaderDir() {
    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L"ModLoader";
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

bool IsSupportedFileType(const std::wstring &path) {
    std::wstring extension = fs::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
    return extension == L".nmo" || extension == L".cmo";
}

std::unique_ptr<MapEntry> BuildMapTree(const fs::path &directory,
    int depth,
    bool &foundAny,
    bool &hadEnumerationError) {
    auto root = std::make_unique<MapEntry>();
    root->type = EntryType::Directory;
    root->name = directory.filename().empty() ? "Maps" : WideToUtf8(directory.filename().wstring());
    if (root->name.empty()) root->name = "Maps";
    root->path = directory.wstring();

    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec) || depth <= 0) return root;

    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator iterator(directory, ec); !ec && iterator != fs::directory_iterator(); iterator.increment(ec))
        entries.push_back(*iterator);
    if (ec) {
        hadEnumerationError = true;
        return root;
    }

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &lhs, const fs::directory_entry &rhs) {
        const bool lhsDir = lhs.is_directory();
        const bool rhsDir = rhs.is_directory();
        if (lhsDir != rhsDir) return lhsDir > rhsDir;
        return WideToUtf8(lhs.path().filename().wstring()) < WideToUtf8(rhs.path().filename().wstring());
    });

    for (const fs::directory_entry &entry : entries) {
        const fs::path fullPath = entry.path();
        if (entry.is_directory()) {
            std::unique_ptr<MapEntry> child = BuildMapTree(fullPath, depth - 1, foundAny, hadEnumerationError);
            child->type = EntryType::Directory;
            child->name = WideToUtf8(fullPath.filename().wstring());
            child->path = fullPath.wstring();
            if (!child->children.empty()) foundAny = true;
            root->children.push_back(std::move(child));
            continue;
        }
        if (!entry.is_regular_file() || !IsSupportedFileType(fullPath.wstring())) continue;
        auto child = std::make_unique<MapEntry>();
        child->type = EntryType::File;
        child->name = WideToUtf8(fullPath.stem().wstring());
        child->path = fullPath.wstring();
        root->children.push_back(std::move(child));
        foundAny = true;
    }
    return root;
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
    if (path.empty() || !fs::exists(path, ec)) return {};
    const fs::path source(path);
    const std::wstring stem = source.stem().wstring();
    const std::wstring extension = source.extension().wstring();
    const uint32_t hash = HashStem(stem);
    wchar_t hashBuffer[9] = {};
    swprintf_s(hashBuffer, L"%08X", hash);
    const fs::path tempDir(GetTempMapsDir());
    fs::create_directories(tempDir, ec);
    if (ec) return {};
    const fs::path destination = tempDir / (std::wstring(hashBuffer) + extension);
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) return {};
    return WideToAnsi(destination.wstring());
}
} // namespace

class MapMenuMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::ui::DrawRegistration m_DrawReg;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    BML_InputCaptureToken m_InputCaptureToken = nullptr;
    bml::InterfaceLease<BML_ConsoleCommandRegistry> m_ConsoleRegistry;
    BML_TopicId m_TopicConsoleOutput = 0;
    BML_TopicId m_TopicCustomMapName = 0;

    CKContext *m_Context = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKParameter *m_LoadCustom = nullptr;
    CKParameter *m_MapFile = nullptr;
    CKParameter *m_LevelRow = nullptr;
    CKDataArray *m_CurrentLevelArray = nullptr;

    MapMenuSettings m_Settings;
    bool m_Visible = false;
    bool m_FocusSearch = false;
    bool m_MapLoaded = false;
    bool m_MapTreeDirty = true;
    std::array<char, kSearchBufferSize> m_SearchBuffer = {};
    std::unique_ptr<MapEntry> m_MapRoot;
    const MapEntry *m_SelectedEntry = nullptr;
    std::vector<const MapEntry *> m_SearchResults;

    static void DrawCallback(const BML_UIDrawContext *ctx, void *user_data) {
        auto *self = static_cast<MapMenuMod *>(user_data);
        if (self && ctx) {
            BML_IMGUI_SCOPE_FROM_CONTEXT(ctx);
            self->RenderWindow();
        }
    }

    void PublishConsoleMessage(const std::string &message, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_SYSTEM) {
        if (m_TopicConsoleOutput == 0 || message.empty()) return;
        BML_ConsoleOutputEvent event = BML_CONSOLE_OUTPUT_EVENT_INIT;
        event.message_utf8 = message.c_str();
        event.flags = flags;
        Services().Builtins().ImcBus->Publish(m_TopicConsoleOutput, &event, sizeof(event));
    }

    void EnsureDefaultConfig() {
        auto config = Services().Config();
        if (!config.GetBool("General", "Enabled").has_value()) config.SetBool("General", "Enabled", m_Settings.enabled);
        if (!config.GetBool("General", "ShowTooltip").has_value()) config.SetBool("General", "ShowTooltip", m_Settings.showTooltip);
        if (!config.GetInt("General", "MaxDepth").has_value()) config.SetInt("General", "MaxDepth", m_Settings.maxDepth);
        if (!config.GetInt("General", "LevelNumber").has_value()) config.SetInt("General", "LevelNumber", m_Settings.levelNumber);
        if (!config.GetInt("General", "OpenKey").has_value()) config.SetInt("General", "OpenKey", m_Settings.openKey);
    }

    void RefreshConfig() {
        auto config = Services().Config();
        m_Settings.enabled = config.GetBool("General", "Enabled").value_or(true);
        m_Settings.showTooltip = config.GetBool("General", "ShowTooltip").value_or(false);
        m_Settings.maxDepth = std::clamp(config.GetInt("General", "MaxDepth").value_or(kDefaultMaxDepth), 1, 64);
        m_Settings.levelNumber = config.GetInt("General", "LevelNumber").value_or(0);
        m_Settings.openKey = config.GetInt("General", "OpenKey").value_or(kDefaultOpenKey);
    }

    void RefreshMapTree() {
        bool foundAny = false;
        bool hadEnumerationError = false;
        m_MapRoot = BuildMapTree(fs::path(GetMapsDir()), m_Settings.maxDepth, foundAny, hadEnumerationError);
        m_SelectedEntry = nullptr;
        m_SearchResults.clear();
        m_MapTreeDirty = false;
        if (hadEnumerationError) {
            Services().Log().Warn("Failed to enumerate custom maps directory");
        }
        if (!foundAny) {
            m_MapRoot.reset();
        }
    }

    void RebuildSearchResults(const MapEntry *entry, std::string_view query) {
        if (!entry) return;
        for (const std::unique_ptr<MapEntry> &child : entry->children) {
            if (child->type == EntryType::File) {
                const std::string fullPath = FormatPathForUi(child->path);
                if (ContainsCaseInsensitive(child->name, query) || ContainsCaseInsensitive(fullPath, query))
                    m_SearchResults.push_back(child.get());
            }
            if (!child->children.empty()) RebuildSearchResults(child.get(), query);
        }
    }

    void RefreshSearchResults() {
        m_SearchResults.clear();
        if (!m_MapRoot) return;
        const std::string query = TrimCopy(m_SearchBuffer.data());
        if (query.empty()) return;
        RebuildSearchResults(m_MapRoot.get(), query);
    }

    CKDataArray *GetCurrentLevelArray() {
        if (!m_Context) return nullptr;
        m_CurrentLevelArray = static_cast<CKDataArray *>(m_Context->GetObjectByNameAndClass((CKSTRING)"CurrentLevel", CKCID_DATAARRAY));
        return m_CurrentLevelArray;
    }

    CK2dEntity *GetNamed2dEntity(const char *name) {
        if (!m_Context || !name) return nullptr;
        return static_cast<CK2dEntity *>(m_Context->GetObjectByNameAndClass((CKSTRING)name, CKCID_2DENTITY));
    }

    CKGroup *GetNamedGroup(const char *name) {
        if (!m_Context || !name) return nullptr;
        return static_cast<CKGroup *>(m_Context->GetObjectByNameAndClass((CKSTRING)name, CKCID_GROUP));
    }

    bool HookLevelInitBuild(CKBehavior *script) {
        bml::Graph graph(script);
        auto loadLevelNode = graph.Find("Load LevelXX");
        if (!loadLevelNode) return false;
        CKBehavior *loadLevel = loadLevelNode;

        bml::Graph ll(loadLevel);
        CKBehaviorLink *inLink = ll.From(loadLevel->GetInput(0)).NextLink();
        if (!inLink) return false;
        auto operation = ll.From(inLink->GetOutBehaviorIO()->GetOwner()).Next();
        if (!operation || operation->GetOutputParameterCount() <= 0 ||
            operation->GetOutputParameter(0)->GetDestinationCount() <= 0) return false;
        auto objectLoad = loadLevelNode.Find("Object Load");
        if (!objectLoad || objectLoad->GetInputParameterCount() <= 0) return false;
        CKBehavior *binarySwitch = ll.CreateBehavior(VT_LOGICS_BINARYSWITCH);
        if (!binarySwitch) return false;
        ll.Link(loadLevel->GetInput(0), binarySwitch, 0);
        m_LoadCustom = ll.Param("Custom Level", CKPGUID_BOOL);
        if (!m_LoadCustom) return false;
        binarySwitch->GetInputParameter(0)->SetDirectSource(m_LoadCustom);
        inLink->SetInBehaviorIO(binarySwitch->GetOutput(1));
        ll.Link(binarySwitch, objectLoad);
        m_MapFile = objectLoad->GetInputParameter(0)->GetDirectSource();
        m_LevelRow = operation->GetOutputParameter(0)->GetDestination(0);
        return m_MapFile != nullptr && m_LevelRow != nullptr;
    }

    void LoadMap(const std::wstring &path) {
        if (!m_Context || !m_Settings.enabled) return;
        if (!m_LoadCustom || !m_MapFile || !m_LevelRow) {
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
        bml::SetParam(m_MapFile, tempFile.c_str());
        bml::SetParam(m_LoadCustom, TRUE);
        int levelNumber = m_Settings.levelNumber;
        if (levelNumber < 1 || levelNumber > 13) levelNumber = std::rand() % 10 + 2;
        currentLevel->SetElementValue(0, 0, &levelNumber);
        int levelRow = levelNumber - 1;
        bml::SetParam(m_LevelRow, levelRow);
        CKMessageManager *messageManager = m_Context->GetMessageManager();
        CKGroup *allSound = GetNamedGroup("All_Sound");
        CK2dEntity *blackScreen = GetNamed2dEntity("M_BlackScreen");
        if (!m_ExitStart || !messageManager || !allSound || !blackScreen || !m_Context->GetCurrentLevel()) {
            PublishConsoleMessage("[mapmenu] menu transition objects are unavailable", BML_CONSOLE_OUTPUT_FLAG_ERROR);
            return;
        }
        CKMessageType loadLevelMessage = messageManager->AddMessageType((CKSTRING)"Load Level");
        CKMessageType loadMenuMessage = messageManager->AddMessageType((CKSTRING)"Menu_Load");
        const std::string originalMapPath = WideToUtf8(path);
        const auto *imcState = Services().Builtins().ImcState;
        if (m_TopicCustomMapName != 0 && imcState && imcState->PublishState && !originalMapPath.empty()) {
            BML_ImcMessage state_message = BML_IMC_MESSAGE_INIT;
            state_message.data = originalMapPath.c_str();
            state_message.size = originalMapPath.size() + 1;
            imcState->PublishState(m_TopicCustomMapName, &state_message);
        }
        messageManager->SendMessageSingle(loadLevelMessage, m_Context->GetCurrentLevel());
        messageManager->SendMessageSingle(loadMenuMessage, allSound);
        blackScreen->Show(CKHIDE);
        m_ExitStart->ActivateInput(0);
        m_ExitStart->Activate();
        PublishConsoleMessage("[mapmenu] loading " + FormatPathForUi(path));
        m_MapLoaded = true;
        SetVisible(false);
    }

    void SetVisible(bool visible) {
        if (m_Visible == visible) return;
        m_Visible = visible;
        m_FocusSearch = visible;
        if (visible) {
            m_MapLoaded = false;
        }
        if (m_InputCaptureService) {
            if (visible && !m_InputCaptureToken) {
                BML_InputCaptureDesc capture = BML_INPUT_CAPTURE_DESC_INIT;
                capture.flags = BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD | BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE;
                capture.cursor_visible = 1;
                capture.priority = 100;
                m_InputCaptureService->AcquireCapture(&capture, &m_InputCaptureToken);
            } else if (!visible && m_InputCaptureToken) {
                m_InputCaptureService->ReleaseCapture(m_InputCaptureToken);
                m_InputCaptureToken = nullptr;
            }
        }
        if (visible) {
            if (m_MapTreeDirty || !m_MapRoot) {
                RefreshMapTree();
            }
            RefreshSearchResults();
        }
    }

    bool RenderMapEntry(const MapEntry *entry) {
        if (!entry) return false;
        if (entry->type == EntryType::Directory) {
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            const bool opened = ImGui::TreeNodeEx(entry, flags, "%s", entry->name.c_str());
            if (m_Settings.showTooltip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
            bool loaded = false;
            if (opened) {
                for (const std::unique_ptr<MapEntry> &child : entry->children)
                    loaded = RenderMapEntry(child.get()) || loaded;
                ImGui::TreePop();
            }
            return loaded;
        }
        const bool selected = (m_SelectedEntry == entry);
        if (ImGui::Selectable(entry->name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            m_SelectedEntry = entry;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                LoadMap(entry->path);
                return true;
            }
        }
        if (m_Settings.showTooltip && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
        return false;
    }

    void RenderSearchResults() {
        if (m_SearchResults.empty()) {
            ImGui::TextDisabled("No matching maps.");
            return;
        }
        for (const MapEntry *entry : m_SearchResults) {
            const std::string label = entry->name + "##" + FormatPathForUi(entry->path);
            const bool selected = (m_SelectedEntry == entry);
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                m_SelectedEntry = entry;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    LoadMap(entry->path);
                    return;
                }
            }
            if (m_Settings.showTooltip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", FormatPathForUi(entry->path).c_str());
        }
    }

    void RenderMainMenuButton() {
        if (m_Visible || !m_Context || !m_Settings.enabled) return;
        CK2dEntity *startButton = GetNamed2dEntity("M_Start_But_01");
        if (!startButton || !startButton->IsVisible()) return;
        const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.6238f, viewportSize.y * 0.4f), ImGuiCond_Always);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("BML_MapMenu_Button", nullptr, flags)) {
            if (ImGui::Button("Custom Maps")) SetVisible(true);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void RenderWindow() {
        if (!m_Visible || !m_Settings.enabled) {
            RenderMainMenuButton();
            return;
        }
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->Pos.x + viewport->Size.x * 0.08f, viewport->Pos.y + viewport->Size.y * 0.08f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 0.78f, viewport->Size.y * 0.78f), ImGuiCond_Always);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("Custom Maps", nullptr, flags)) {
            if (m_FocusSearch) {
                ImGui::SetKeyboardFocusHere();
                m_FocusSearch = false;
            }
            if (ImGui::InputTextWithHint("##map-search", "Search maps", m_SearchBuffer.data(), m_SearchBuffer.size()))
                RefreshSearchResults();
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
            if (!m_MapRoot) {
                ImGui::TextDisabled("Maps directory is not available.");
            } else if (TrimCopy(m_SearchBuffer.data()).empty()) {
                if (!m_MapRoot->children.empty()) {
                    for (const std::unique_ptr<MapEntry> &child : m_MapRoot->children) {
                        if (RenderMapEntry(child.get())) break;
                    }
                } else {
                    ImGui::TextDisabled("No maps found under %s", FormatPathForUi(m_MapRoot->path).c_str());
                }
            } else {
                RenderSearchResults();
            }
            ImGui::Separator();
            if (m_SelectedEntry) {
                ImGui::TextWrapped("Selected: %s", FormatPathForUi(m_SelectedEntry->path).c_str());
                if (ImGui::Button("Load Selected")) LoadMap(m_SelectedEntry->path);
            } else {
                ImGui::TextDisabled("Select a map to load.");
            }
        }
        ImGui::End();
    }

    // Console command handlers (static, use user_data = this)
    static BML_Result ExecuteMapMenuCommand(const BML_ConsoleCommandInvocation *invocation, void *ud) {
        auto *self = static_cast<MapMenuMod *>(ud);
        if (!invocation || invocation->argc == 0 || !invocation->argv_utf8 || !invocation->argv_utf8[0])
            return BML_RESULT_INVALID_ARGUMENT;
        if (invocation->argc == 1 || (invocation->argc >= 2 && ToLowerAscii(invocation->argv_utf8[1]) == "open")) {
            self->SetVisible(true);
            return BML_RESULT_OK;
        }
        if (ToLowerAscii(invocation->argv_utf8[1]) == "close") {
            self->SetVisible(false);
            return BML_RESULT_OK;
        }
        if (ToLowerAscii(invocation->argv_utf8[1]) == "refresh") {
            self->RefreshMapTree();
            self->RefreshSearchResults();
            self->PublishConsoleMessage("[mapmenu] refreshed custom map list");
            return BML_RESULT_OK;
        }
        self->PublishConsoleMessage("[mapmenu] usage: mapmenu [open|close|refresh]", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        return BML_RESULT_INVALID_ARGUMENT;
    }

    static void CompleteMapMenuCommand(const BML_ConsoleCompletionContext *context,
        PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *) {
        if (!context || !sink || context->token_index != 1) return;
        const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
        AddCompletionIfMatches("open", prefix, sink, sinkUserData);
        AddCompletionIfMatches("close", prefix, sink, sinkUserData);
        AddCompletionIfMatches("refresh", prefix, sink, sinkUserData);
    }

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        m_DrawReg = bml::ui::RegisterWindowDraw("bml.mapmenu.window", 0, DrawCallback, this);
        if (!m_DrawReg) return BML_RESULT_NOT_FOUND;

        EnsureDefaultConfig();
        RefreshConfig();

        m_InputCaptureService = bml::AcquireInterface<BML_InputCaptureInterface>(BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0, 0);
        if (!m_InputCaptureService) {
            Services().Log().Warn("Failed to acquire input capture service; map menu will not capture input");
        }

        Services().Builtins().ImcBus->GetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &m_TopicConsoleOutput);
        Services().Builtins().ImcBus->GetTopicId(BML_TOPIC_STATE_CUSTOM_MAP_NAME, &m_TopicCustomMapName);

        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            auto *event = msg.As<BML_KeyDownEvent>();
            if (!event || event->repeat || !m_Settings.enabled) return;
            if (static_cast<int>(event->key_code) == m_Settings.openKey) SetVisible(!m_Visible);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            auto *event = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!event) return;
            m_Context = event->context;
            GetCurrentLevelArray();
        });

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_LegacyScriptLoadPayload>();
            if (!payload || !payload->script) return;
            const char *scriptName = payload->script->GetName();
            if (!scriptName) return;
            if (strcmp(scriptName, "Levelinit_build") == 0) {
                if (HookLevelInitBuild(payload->script))
                    PublishConsoleMessage("[mapmenu] custom map load hook armed");
                return;
            }
            if (strcmp(scriptName, "Menu_Start") == 0)
                m_ExitStart = bml::Graph(payload->script).Find("Exit");
        });

        if (m_Subs.Count() < 4) return BML_RESULT_FAIL;

        m_ConsoleRegistry = bml::AcquireInterface<BML_ConsoleCommandRegistry>(
            BML_CONSOLE_COMMAND_REGISTRY_INTERFACE_ID, 1, 0, 0);
        if (m_ConsoleRegistry) {
            BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
            desc.name_utf8 = "mapmenu";
            desc.description_utf8 = "Open, close, or refresh the custom map browser";
            desc.execute = ExecuteMapMenuCommand;
            desc.complete = CompleteMapMenuCommand;
            desc.user_data = this;
            if (m_ConsoleRegistry->RegisterCommand(&desc) != BML_RESULT_OK)
                Services().Log().Warn("Failed to register mapmenu console command");
        } else {
            Services().Log().Warn("Failed to acquire console command registry service; mapmenu console command will be unavailable");
        }

        Services().Log().Info("Map menu module initialized");
        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        SetVisible(false);
        if (m_ConsoleRegistry) {
            m_ConsoleRegistry->UnregisterCommand("mapmenu");
            m_ConsoleRegistry.Reset();
        }
        m_DrawReg.Reset();
        m_InputCaptureService.Reset();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        SetVisible(false);
        if (m_ConsoleRegistry) {
            m_ConsoleRegistry->UnregisterCommand("mapmenu");
            m_ConsoleRegistry.Reset();
        }
        if (m_InputCaptureToken && m_InputCaptureService) {
            m_InputCaptureService->ReleaseCapture(m_InputCaptureToken);
            m_InputCaptureToken = nullptr;
        }
        m_InputCaptureService.Reset();
        m_DrawReg.Reset();
        m_Context = nullptr;
        m_ExitStart = nullptr;
        m_LoadCustom = nullptr;
        m_MapFile = nullptr;
        m_LevelRow = nullptr;
        m_CurrentLevelArray = nullptr;
        m_MapLoaded = false;
        m_MapRoot.reset();
        m_SearchResults.clear();
        m_SelectedEntry = nullptr;
        m_SearchBuffer.fill('\0');
        m_MapTreeDirty = true;
    }
};

BML_DEFINE_MODULE(MapMenuMod)
