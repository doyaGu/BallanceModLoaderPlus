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
#include <cstring>
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
#include "bml_config_bind.hpp"
#include "bml_console.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_imgui.hpp"
#include "bml_imc.h"
#include "bml_input.h"
#include "bml_input_control.h"
#include "bml_input_capture.hpp"
#include "bml_interface.hpp"
#include "bml_virtools_payloads.h"
#include "bml_topics.h"
#include "bml_ui.hpp"
#include "BML/Guids/Logics.h"

#include "BML/ScriptGraph.h"
#include "StringUtils.h"

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

void AddCompletionIfMatches(const char *candidate, const std::string &prefix,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData) {
    if (!candidate || !sink) return;
    const std::string loweredCandidate = utils::ToLower(std::string(candidate));
    if (prefix.empty() || loweredCandidate.rfind(prefix, 0) == 0) sink(candidate, sinkUserData);
}

bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    return utils::Contains(std::string(haystack), std::string(needle), false);
}

std::wstring GetLoaderDir() {
    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = ::GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
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
    std::string utf8 = utils::ToString(path);
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
    root->name = directory.filename().empty() ? "Maps" : utils::ToString(directory.filename().wstring());
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
        return utils::ToString(lhs.path().filename().wstring()) < utils::ToString(rhs.path().filename().wstring());
    });

    for (const fs::directory_entry &entry : entries) {
        const fs::path fullPath = entry.path();
        if (entry.is_directory()) {
            std::unique_ptr<MapEntry> child = BuildMapTree(fullPath, depth - 1, foundAny, hadEnumerationError);
            child->type = EntryType::Directory;
            child->name = utils::ToString(fullPath.filename().wstring());
            child->path = fullPath.wstring();
            if (!child->children.empty()) foundAny = true;
            root->children.push_back(std::move(child));
            continue;
        }
        if (!entry.is_regular_file() || !IsSupportedFileType(fullPath.wstring())) continue;
        auto child = std::make_unique<MapEntry>();
        child->type = EntryType::File;
        child->name = utils::ToString(fullPath.stem().wstring());
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
    return utils::ToString(destination.wstring(), false);
}
} // namespace

class MapMenuMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::ui::DrawRegistration m_DrawReg;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    bml::InputCaptureGuard m_InputCapture;
    bml::InterfaceLease<BML_ConsoleCommandRegistry> m_ConsoleRegistry;
    BML_TopicId m_TopicConsoleOutput = 0;
    BML_TopicId m_TopicCustomMapName = 0;

    CKContext *m_Context = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CK_ID m_StartButtonId = 0;
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
        auto *imcBus = Services().Builtins().ImcBus;
        if (!imcBus || !imcBus->PublishBuffer || m_TopicConsoleOutput == 0 || message.empty()) return;

        const size_t textSize = message.size() + 1;
        const size_t totalSize = sizeof(BML_ConsoleOutputEvent) + textSize;
        auto *storage = static_cast<char *>(std::malloc(totalSize));
        if (!storage) return;

        auto *event = reinterpret_cast<BML_ConsoleOutputEvent *>(storage);
        *event = BML_CONSOLE_OUTPUT_EVENT_INIT;
        event->message_utf8 = storage + sizeof(BML_ConsoleOutputEvent);
        event->flags = flags;
        std::memcpy(const_cast<char *>(event->message_utf8), message.c_str(), textSize);

        BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
        buffer.data = event;
        buffer.size = totalSize;
        buffer.cleanup = [](const void *, size_t, void *user_data) {
            std::free(user_data);
        };
        buffer.cleanup_user_data = storage;
        imcBus->PublishBuffer(m_TopicConsoleOutput, &buffer);
    }

    bml::ConfigBindings m_Cfg;

    void InitConfigBindings() {
        m_Cfg.Bind("General", "Enabled",     m_Settings.enabled,     true);
        m_Cfg.Bind("General", "ShowTooltip", m_Settings.showTooltip, false);
        m_Cfg.Bind("General", "MaxDepth",    m_Settings.maxDepth,    kDefaultMaxDepth);
        m_Cfg.Bind("General", "LevelNumber", m_Settings.levelNumber, 0);
        m_Cfg.Bind("General", "OpenKey",     m_Settings.openKey,     kDefaultOpenKey);
    }

    void RefreshConfig() {
        m_Cfg.Refresh(Services().Config());
        m_Settings.maxDepth = std::clamp(m_Settings.maxDepth, 1, 64);
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
        const std::string query = utils::TrimStringCopy(std::string(m_SearchBuffer.data()));
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
        const std::string originalMapPath = utils::ToString(path);
        const auto *imcBus = Services().Builtins().ImcBus;
        if (m_TopicCustomMapName != 0 && imcBus && imcBus->PublishState && !originalMapPath.empty()) {
            BML_ImcMessage state_message = BML_IMC_MESSAGE_INIT;
            state_message.data = originalMapPath.c_str();
            state_message.size = originalMapPath.size() + 1;
            imcBus->PublishState(m_TopicCustomMapName, &state_message);
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
        if (visible) {
            m_InputCapture.Acquire(
                BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD | BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE,
                1, 100);
        } else {
            m_InputCapture.Release();
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
            ImGui::TextDisabled("%s", Services().Locale()["window.no_matching"]);
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
        auto *startButton = m_StartButtonId
            ? static_cast<CK2dEntity *>(m_Context->GetObject(m_StartButtonId))
            : nullptr;
        if (!startButton || !startButton->IsVisible()) return;
        const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.6238f, viewportSize.y * 0.4f), ImGuiCond_Always);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("BML_MapMenu_Button", nullptr, flags)) {
            if (ImGui::Button(Services().Locale()["window.title"])) SetVisible(true);
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
        const auto &loc = Services().Locale();
        if (ImGui::Begin(loc["window.title"], nullptr, flags)) {
            if (m_FocusSearch) {
                ImGui::SetKeyboardFocusHere();
                m_FocusSearch = false;
            }
            if (ImGui::InputTextWithHint("##map-search", loc["window.search_hint"], m_SearchBuffer.data(), m_SearchBuffer.size()))
                RefreshSearchResults();
            ImGui::SameLine();
            if (ImGui::Button(loc["window.refresh"])) {
                RefreshMapTree();
                RefreshSearchResults();
            }
            ImGui::SameLine();
            if (ImGui::Button(loc["window.close"]) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                SetVisible(false);
            }
            ImGui::Separator();
            if (!m_MapRoot) {
                ImGui::TextDisabled("%s", loc["window.no_maps_dir"]);
            } else if (utils::TrimStringCopy(std::string(m_SearchBuffer.data())).empty()) {
                if (!m_MapRoot->children.empty()) {
                    for (const std::unique_ptr<MapEntry> &child : m_MapRoot->children) {
                        if (RenderMapEntry(child.get())) break;
                    }
                } else {
                    ImGui::TextDisabled("%s %s", loc["window.no_maps_found"], FormatPathForUi(m_MapRoot->path).c_str());
                }
            } else {
                RenderSearchResults();
            }
            ImGui::Separator();
            if (m_SelectedEntry) {
                ImGui::TextWrapped("Selected: %s", FormatPathForUi(m_SelectedEntry->path).c_str());
                if (ImGui::Button(loc["window.load"])) LoadMap(m_SelectedEntry->path);
            } else {
                ImGui::TextDisabled("%s", loc["window.select_prompt"]);
            }
        }
        ImGui::End();
    }

    // Console command handlers (static, use user_data = this)
    static BML_Result ExecuteMapMenuCommand(const BML_ConsoleCommandInvocation *invocation, void *ud) {
        auto *self = static_cast<MapMenuMod *>(ud);
        if (!invocation || invocation->argc == 0 || !invocation->argv_utf8 || !invocation->argv_utf8[0])
            return BML_RESULT_INVALID_ARGUMENT;
        if (invocation->argc == 1 || (invocation->argc >= 2 && utils::ToLower(std::string(invocation->argv_utf8[1])) == "open")) {
            self->SetVisible(true);
            return BML_RESULT_OK;
        }
        if (utils::ToLower(std::string(invocation->argv_utf8[1])) == "close") {
            self->SetVisible(false);
            return BML_RESULT_OK;
        }
        if (utils::ToLower(std::string(invocation->argv_utf8[1])) == "refresh") {
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
        const std::string prefix = context->current_token_utf8 ? utils::ToLower(std::string(context->current_token_utf8)) : std::string();
        AddCompletionIfMatches("open", prefix, sink, sinkUserData);
        AddCompletionIfMatches("close", prefix, sink, sinkUserData);
        AddCompletionIfMatches("refresh", prefix, sink, sinkUserData);
    }

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Locale().Load(nullptr);

        m_DrawReg = bml::ui::RegisterDraw("bml.mapmenu.window", 0, DrawCallback, this);
        if (!m_DrawReg) return BML_RESULT_NOT_FOUND;

        InitConfigBindings();
        m_Cfg.Sync(Services().Config());

        m_InputCaptureService = Services().Acquire<BML_InputCaptureInterface>();
        if (m_InputCaptureService) {
            m_InputCapture.SetService(m_InputCaptureService.Get());
        } else {
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
            auto *payload = msg.As<BML_ScriptLoadEvent>();
            if (!payload || !payload->script) return;
            const char *scriptName = payload->script->GetName();
            if (!scriptName) return;
            if (strcmp(scriptName, "Levelinit_build") == 0) {
                if (HookLevelInitBuild(payload->script))
                    PublishConsoleMessage("[mapmenu] custom map load hook armed");
                return;
            }
            if (strcmp(scriptName, "Menu_Start") == 0) {
                m_ExitStart = bml::Graph(payload->script).Find("Exit");
                CK2dEntity *btn = GetNamed2dEntity("M_Start_But_01");
                m_StartButtonId = btn ? btn->GetID() : 0;
            }
        });

        if (m_Subs.Count() < 4) return BML_RESULT_FAIL;

        m_ConsoleRegistry = Services().Acquire<BML_ConsoleCommandRegistry>();
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
        m_InputCapture.Release();
        m_InputCaptureService.Reset();
        m_DrawReg.Reset();
        m_Context = nullptr;
        m_ExitStart = nullptr;
        m_StartButtonId = 0;
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
