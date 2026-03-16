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
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_config.hpp"
#include "bml_console.h"
#include "bml_imgui.hpp"
#include "bml_input_control.h"
#include "bml_interface.hpp"
#include "bml_topics.h"
#include "bml_ui_host.h"
#include "bml_ui_helpers.hpp"
#include "bml_virtools.h"

#include "BML/Menu.h"

#include "AnsiPalette.h"
#include "AnsiText.h"
#include "StringUtils.h"
#include "PathUtils.h"

#include "CommandRegistry.h"
#include "CommandHistory.h"
#include "MessageBoard.h"
#include "BuiltinCommands.h"
#include "ConsoleCompletionState.h"

namespace {
using namespace BML::Console;

constexpr wchar_t kCommandHistoryFileName[] = L"CommandBar.history";
constexpr char kInternalCheatStateKey[] = "bml.internal.cheat_enabled";

float ClampUnit(float value) {
    return (std::clamp)(value, 0.0f, 1.0f);
}

std::filesystem::path ResolveLoaderFilePath(const wchar_t *filename) {
    const std::wstring exePath = utils::GetExecutablePathW();
    if (exePath.empty()) return std::filesystem::current_path() / filename;
    return std::filesystem::path(exePath).parent_path().parent_path() / L"ModLoader" / filename;
}

std::wstring GetLoaderDirectoryPath() {
    return ResolveLoaderFilePath(L".").parent_path().wstring();
}

std::wstring ConsolePaletteLoaderDirProvider() {
    return GetLoaderDirectoryPath();
}
} // namespace

namespace {
const bml::ModuleServices *g_ConsoleServices = nullptr;

void ConsolePaletteLogger(int level, const char *message) {
    if (!message || message[0] == '\0' || !g_ConsoleServices) {
        return;
    }

    if (level >= 2) {
        g_ConsoleServices->Log().Error("%s", message);
    } else if (level == 1) {
        g_ConsoleServices->Log().Warn("%s", message);
    } else {
        g_ConsoleServices->Log().Info("%s", message);
    }
}
} // namespace

ImGuiContext *GImGui = nullptr;

extern BML_ConsoleCommandRegistry g_ConsoleRegistryService;

class ConsoleMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::ui::DrawRegistration m_DrawReg;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    bml::InterfaceLease<BML_HostRuntimeInterface> m_HostRuntime;
    bml::PublishedInterface m_PublishedService;
    BML_InputCaptureToken m_InputCaptureToken = nullptr;
    BML_TopicId m_TopicCommand = 0;

    CommandRegistry m_Registry;
    CommandHistory m_History;
    MessageBoard m_MessageBoard;
    ConsoleSettings m_Settings;
    BuiltinCommandContext m_BuiltinCtx;

    bool m_Visible = false;
    bool m_FocusInput = false;
    bool m_PendingKeyboardUnblock = false;
    bool m_VisiblePrev = false;
    std::vector<char> m_InputBuffer = std::vector<char>(65536, '\0');
    int m_CursorPos = 0;
    std::vector<std::string> m_Candidates;
    int m_CandidateSelected = -1;

    static void DrawCallback(const BML_UIDrawContext *ctx, void *user_data) {
        auto *self = static_cast<ConsoleMod *>(user_data);
        if (self && ctx) {
            ImGuiContext *previousContext = GImGui;
            if (ctx->imgui && ctx->imgui->igGetCurrentContext) {
                GImGui = ctx->imgui->igGetCurrentContext();
            }
            BML_IMGUI_SCOPE_FROM_CONTEXT(ctx);
            self->RenderUiFrame();
            GImGui = previousContext;
        }
    }
    int m_CandidateIndex = 0;
    int m_CandidatePage = 0;
    std::vector<int> m_CandidatePages;
    bool m_ShowHints = false;
    float m_ScrollY = 0.0f;
    float m_MaxScrollY = 0.0f;
    bool m_ScrollToBottom = true;

    // ── Context accessors ───────────────────────────────────────────────

    BML_Mod FindLoadedModuleById(std::string_view id) const {
        const std::string loweredId = ToLowerAscii(id);
        const uint32_t count = Services().Builtins().Module->GetLoadedModuleCount();
        for (uint32_t index = 0; index < count; ++index) {
            BML_Mod mod = Services().Builtins().Module->GetLoadedModuleAt(index);
            const char *loadedId = nullptr;
            if (!mod || Services().Builtins().Module->GetModId(mod, &loadedId) != BML_RESULT_OK || !loadedId) continue;
            if (ToLowerAscii(loadedId) == loweredId) return mod;
        }
        return nullptr;
    }

    bool SetBoolConfigOnMod(BML_Mod mod, const char *category, const char *name, bool value) const {
        if (!mod || !category || !name) return false;
        const BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        const BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_BOOL(value ? BML_TRUE : BML_FALSE);
        return Services().Builtins().Config->Set(mod, &key, &configValue) == BML_RESULT_OK;
    }

    bool SetStringConfigOnMod(BML_Mod mod, const char *category, const char *name, const std::string &value) const {
        if (!mod || !category || !name) return false;
        const BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        const BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_STRING(value.c_str());
        return Services().Builtins().Config->Set(mod, &key, &configValue) == BML_RESULT_OK;
    }

    static void DestroyBoolState(void *data) { delete static_cast<bool *>(data); }

    bool GetCheatState() const {
        void *raw = nullptr;
        if (Services().Builtins().Context->GetUserData(Services().GlobalContext(), kInternalCheatStateKey, &raw) != BML_RESULT_OK || !raw) {
            return false;
        }
        return *static_cast<bool *>(raw);
    }

    BML_Result SetCheatState(bool enabled) {
        void *raw = nullptr;
        if (Services().Builtins().Context->GetUserData(Services().GlobalContext(), kInternalCheatStateKey, &raw) == BML_RESULT_OK && raw) {
            *static_cast<bool *>(raw) = enabled;
            return BML_RESULT_OK;
        }
        auto *state = new bool(enabled);
        const BML_Result result = Services().Builtins().Context->SetUserData(Services().GlobalContext(), kInternalCheatStateKey, state, DestroyBoolState);
        if (result != BML_RESULT_OK) delete state;
        return result;
    }

    bool ReloadGlobalPalette() const {
        AnsiPalette palette;
        palette.EnsureInitialized();
        return palette.ReloadFromFile();
    }

    // ── Config ──────────────────────────────────────────────────────────

    void EnsureDefaultConfig() {
        auto config = Services().Config();
        if (!config.GetFloat("CommandBar", "MessageDuration").has_value())
            config.SetFloat("CommandBar", "MessageDuration", m_Settings.messageDuration);
        if (!config.GetInt("CommandBar", "TabColumns").has_value())
            config.SetInt("CommandBar", "TabColumns", m_Settings.tabColumns);
        if (!config.GetFloat("CommandBar", "LineSpacing").has_value())
            config.SetFloat("CommandBar", "LineSpacing", m_Settings.lineSpacing);
        if (!config.GetFloat("CommandBar", "MessageBackgroundAlpha").has_value())
            config.SetFloat("CommandBar", "MessageBackgroundAlpha", m_Settings.messageBackgroundAlpha);
        if (!config.GetFloat("CommandBar", "WindowBackgroundAlpha").has_value())
            config.SetFloat("CommandBar", "WindowBackgroundAlpha", m_Settings.windowBackgroundAlpha);
        if (!config.GetFloat("CommandBar", "FadeMaxAlpha").has_value())
            config.SetFloat("CommandBar", "FadeMaxAlpha", m_Settings.fadeMaxAlpha);
    }

    void RefreshConfig() {
        auto config = Services().Config();
        m_Settings.messageDuration = (std::max)(0.5f, config.GetFloat("CommandBar", "MessageDuration").value_or(kDefaultMessageLifetime));
        m_Settings.tabColumns = (std::max)(1, config.GetInt("CommandBar", "TabColumns").value_or(4));
        m_Settings.lineSpacing = config.GetFloat("CommandBar", "LineSpacing").value_or(-1.0f);
        m_Settings.messageBackgroundAlpha = ClampUnit(config.GetFloat("CommandBar", "MessageBackgroundAlpha").value_or(0.80f));
        m_Settings.windowBackgroundAlpha = ClampUnit(config.GetFloat("CommandBar", "WindowBackgroundAlpha").value_or(1.0f));
        m_Settings.fadeMaxAlpha = ClampUnit(config.GetFloat("CommandBar", "FadeMaxAlpha").value_or(1.0f));
    }

    // ── History path ────────────────────────────────────────────────────

    std::filesystem::path GetHistoryPath() const {
        return ResolveLoaderFilePath(kCommandHistoryFileName);
    }

    // ── Message helpers ─────────────────────────────────────────────────

    void AddMessage(std::string text, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE, float ttl = -1.0f) {
        m_MessageBoard.Add(std::move(text), flags, ttl, m_Visible, m_Settings.messageDuration);
        if (m_Visible && (m_ScrollToBottom || m_MaxScrollY <= 0.0f)) {
            m_ScrollToBottom = true;
        }
    }

    void PrintToConsole(const char *message, uint32_t flags) {
        if (!message || message[0] == '\0') return;
        AddMessage(message, flags, m_Visible ? kPinnedMessageLifetime : m_Settings.messageDuration);
    }

    BML_Result PublishCommand(const std::string &command) {
        BML_ConsoleCommandEvent event = BML_CONSOLE_COMMAND_EVENT_INIT;
        event.command_utf8 = command.c_str();
        return Services().Builtins().ImcBus->Publish(m_TopicCommand, &event, sizeof(event));
    }

    void ClearPaletteProviders() const {
        AnsiPalette::SetLoggerProvider(nullptr);
        AnsiPalette::SetLoaderDirProvider(nullptr);
    }

    // ── Console visibility ──────────────────────────────────────────────

    void SetConsoleVisible(bool visible) {
        if (m_Visible == visible) return;
        m_Visible = visible;
        m_FocusInput = visible;
        m_VisiblePrev = !visible;
        if (m_InputCaptureService) {
            if (visible && !m_InputCaptureToken) {
                BML_InputCaptureDesc capture = BML_INPUT_CAPTURE_DESC_INIT;
                capture.flags = BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD;
                capture.cursor_visible = -1;
                capture.priority = 100;
                m_InputCaptureService->AcquireCapture(&capture, &m_InputCaptureToken);
            } else if (!visible) {
                m_PendingKeyboardUnblock = m_InputCaptureToken != nullptr;
            }
        }
        if (visible) {
            m_InputBuffer[0] = '\0';
            m_History.SetIndexToEnd();
            m_ScrollToBottom = true;
        } else {
            m_InputBuffer[0] = '\0';
            m_History.ResetIndex();
            InvalidateCandidates();
        }
    }

    void ReleaseInputCapture() {
        if (m_InputCaptureToken && m_InputCaptureService) {
            m_InputCaptureService->ReleaseCapture(m_InputCaptureToken);
            m_InputCaptureToken = nullptr;
        }
        m_PendingKeyboardUnblock = false;
    }

    void RenderUiFrame() {
        RefreshConfig();
        m_BuiltinCtx.consoleVisible = m_Visible;
        m_BuiltinCtx.defaultDuration = m_Settings.messageDuration;
        if (!m_Visible && ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
            SetConsoleVisible(true);
        }
        if (m_PendingKeyboardUnblock && !ImGui::IsKeyDown(ImGuiKey_Escape) && !ImGui::IsKeyDown(ImGuiKey_Enter)) {
            ReleaseInputCapture();
        }
        RenderMessages();
        RenderCommandBar();
    }

    // ── Command execution ───────────────────────────────────────────────

    BML_Result ConsoleExecuteCommand(const char *commandUtf8) {
        if (!commandUtf8) return BML_RESULT_INVALID_ARGUMENT;
        const std::string trimmed = TrimCopy(commandUtf8);
        if (trimmed.empty()) return BML_RESULT_INVALID_ARGUMENT;
        const BML_Result registeredResult = m_Registry.Execute(trimmed);
        if (registeredResult != BML_RESULT_NOT_FOUND) return registeredResult;
        return PublishCommand(trimmed);
    }

    void ExecuteCommand(std::string command) {
        command = TrimCopy(command);
        if (command.empty()) return;
        const std::vector<ParsedToken> tokens = ParseTokens(command);
        if (tokens.empty()) {
            AddMessage("Error: Empty command", BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
            return;
        }
        const BML_Result registeredResult = m_Registry.Execute(command);
        if (registeredResult == BML_RESULT_OK) return;
        if (registeredResult != BML_RESULT_NOT_FOUND) {
            AddMessage(std::string("Error: Can not execute command ") + tokens[0].value, BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
            return;
        }
        const BML_Result publishResult = PublishCommand(command);
        if (publishResult == BML_RESULT_IMC_NO_SUBSCRIBERS) {
            AddMessage(std::string("Error: Unknown Command ") + tokens[0].value, BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
        }
    }

    // ── Completion helpers ──────────────────────────────────────────────

    static void StripLine(const char *&lineStart, const char *&lineEnd) {
        if (lineStart == lineEnd) return;
        while (lineStart < lineEnd && std::isspace(static_cast<unsigned char>(*lineStart)) != 0) ++lineStart;
        while (lineEnd > lineStart && std::isspace(static_cast<unsigned char>(lineEnd[-1])) != 0) --lineEnd;
    }

    static int FirstToken(const char *tokenStart, const char *&tokenEnd) {
        if (tokenStart == tokenEnd) return 0;
        const char *lineEnd = tokenEnd;
        tokenEnd = tokenStart;
        while (tokenEnd < lineEnd && std::isspace(static_cast<unsigned char>(*tokenEnd)) == 0) ++tokenEnd;
        return static_cast<int>(tokenEnd - tokenStart);
    }

    static int LastToken(const char *&tokenStart, const char *tokenEnd) {
        if (tokenStart == tokenEnd) return 0;
        const char *lineStart = tokenStart;
        tokenStart = tokenEnd;
        while (tokenStart > lineStart && std::isspace(static_cast<unsigned char>(tokenStart[-1])) == 0) --tokenStart;
        return static_cast<int>(tokenEnd - tokenStart);
    }

    static std::vector<std::string> MakeArgsRange(const char *begin, const char *end) {
        std::vector<std::string> args;
        if (!begin || !end || begin >= end) return args;
        const std::string_view text(begin, static_cast<size_t>(end - begin));
        const auto tokens = ParseTokens(text);
        args.reserve(tokens.size());
        for (const ParsedToken &token : tokens) args.push_back(token.value);
        if (HasTrailingWhitespace(text)) args.emplace_back();
        return args;
    }

    size_t OnCompletion(const char *lineStart, const char *lineEnd) {
        if (!lineStart || !lineEnd) return 0;
        const char *wordStart = lineStart;
        const int wordCount = LastToken(wordStart, lineEnd);
        const char *rawLineEnd = lineEnd;
        StripLine(lineStart, lineEnd);

        bool completeCommand = true;
        const char *cmdEnd = lineEnd;
        const char *cmdStart = nullptr;
        if (wordStart == lineStart) {
            cmdStart = wordStart;
        } else {
            cmdStart = lineStart;
            completeCommand = false;
        }
        const int cmdLength = FirstToken(cmdStart, cmdEnd);

        if (completeCommand) {
            for (const CommandRecord *record : m_Registry.FindVisible()) {
                const std::string recordNameLower = ToLowerAscii(record->name);
                const std::string prefix = ToLowerAscii(std::string(cmdStart, static_cast<size_t>(cmdLength)));
                if (recordNameLower.rfind(prefix, 0) == 0) {
                    m_Candidates.emplace_back(record->name);
                }
                if (!record->alias.empty()) {
                    const std::string aliasLower = ToLowerAscii(record->alias);
                    if (aliasLower.rfind(prefix, 0) == 0) {
                        m_Candidates.emplace_back(record->alias);
                    }
                }
            }
        } else {
            const auto args = MakeArgsRange(cmdStart, rawLineEnd);
            if (!args.empty()) {
                const CommandRecord *record = m_Registry.Find(args[0]);
                if (record && record->complete) {
                    std::vector<const char *> argv;
                    argv.reserve(args.size());
                    for (const std::string &arg : args) argv.push_back(arg.c_str());
                    const std::string currentPrefix(wordStart, static_cast<size_t>(wordCount));
                    BML_ConsoleCompletionContext context = BML_CONSOLE_COMPLETION_CONTEXT_INIT;
                    context.input_utf8 = cmdStart;
                    context.name_utf8 = args[0].c_str();
                    context.current_token_utf8 = currentPrefix.c_str();
                    context.argv_utf8 = argv.data();
                    context.argc = static_cast<uint32_t>(argv.size());
                    context.token_index = static_cast<uint32_t>(argv.empty() ? 0 : argv.size() - 1);
                    if (HasTrailingWhitespace(std::string_view(cmdStart, static_cast<size_t>(rawLineEnd - cmdStart)))) {
                        context.flags |= BML_CONSOLE_COMPLETION_FLAG_TRAILING_SPACE;
                    }
                    record->complete(&context, CompletionCollector, &m_Candidates, record->userData);
                }
            }
        }

        GenerateCandidatePages(ImGui::GetMainViewport()->Size.x * 0.96f);
        return m_Candidates.size();
    }

    int HandleTextEdit(ImGuiInputTextCallbackData *data) {
        if (!data) return 0;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            switch (BML::Console::GetCompletionTriggerAction(!m_Candidates.empty(), ImGui::GetIO().KeyShift)) {
            case BML::Console::CompletionTriggerAction::RebuildCandidates:
                OnCompletion(data->Buf, data->Buf + data->CursorPos);
                break;
            case BML::Console::CompletionTriggerAction::SelectPreviousCandidate:
                PrevCandidate();
                break;
            case BML::Console::CompletionTriggerAction::SelectNextCandidate:
                NextCandidate();
                break;
            }

            if (m_Candidates.size() == 1) {
                ReplaceWordAtCursor(data, m_Candidates[0].c_str());
            } else if (m_Candidates.size() > 1) {
                const std::string common = LongestCommonPrefix(m_Candidates);
                if (!common.empty()) {
                    const char *cursor = data->Buf + data->CursorPos;
                    const char *wordStart = cursor;
                    while (wordStart > data->Buf && std::isspace(static_cast<unsigned char>(wordStart[-1])) == 0) --wordStart;
                    const int leftCount = static_cast<int>(cursor - wordStart);
                    const char *textEnd = data->Buf + data->BufTextLen;
                    const char *p = cursor;
                    while (p < textEnd && std::isspace(static_cast<unsigned char>(*p)) == 0) ++p;
                    const int rightCount = static_cast<int>(p - cursor);
                    const int totalDelete = leftCount + rightCount;
                    const int deletePos = static_cast<int>(wordStart - data->Buf);
                    data->DeleteChars(deletePos, totalDelete);
                    data->InsertChars(deletePos, common.c_str(), common.c_str() + common.size());
                }
            }
            return 0;
        }
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (!m_Candidates.empty()) InvalidateCandidates();
            const int prevHistoryPos = m_History.Index();
            if (data->EventKey == ImGuiKey_UpArrow) {
                m_History.NavigateUp();
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                m_History.NavigateDown();
            }
            if (prevHistoryPos != m_History.Index()) {
                const std::string &historyStr = m_History.Current();
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr.c_str());
            }
            return 0;
        }
        if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
            if (!m_Candidates.empty() && m_CandidateSelected != -1) {
                ReplaceWordAtCursor(data, m_Candidates[static_cast<size_t>(m_CandidateSelected)].c_str());
                InvalidateCandidates();
            }
            if (m_CursorPos != data->CursorPos) InvalidateCandidates();
            m_CursorPos = data->CursorPos;
            return 0;
        }
        if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
            if (!m_Candidates.empty()) InvalidateCandidates();
            return 0;
        }
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            IM_ASSERT(data->Buf == m_InputBuffer.data());
            m_InputBuffer.resize(static_cast<size_t>(data->BufTextLen) + 1);
            data->Buf = m_InputBuffer.data();
            return 0;
        }
        return 0;
    }

    void InvalidateCandidates() {
        m_CandidateSelected = -1;
        m_CandidateIndex = 0;
        m_CandidatePage = 0;
        m_CandidatePages.clear();
        m_Candidates.clear();
        m_ShowHints = false;
    }

    void GenerateCandidatePages(float windowWidth) {
        if (m_Candidates.empty()) { m_CandidatePages.clear(); return; }
        const float separatorWidth = ImGui::CalcTextSize(" | ").x;
        const float pagerWidth = ImGui::CalcTextSize("< ").x;
        float width = -separatorWidth;
        m_CandidatePages.clear();
        m_CandidatePages.push_back(0);
        for (int index = 0; index < static_cast<int>(m_Candidates.size()); ++index) {
            const ImVec2 size = ImGui::CalcTextSize(m_Candidates[static_cast<size_t>(index)].c_str());
            width += size.x + separatorWidth;
            if (width > windowWidth) {
                m_CandidatePages.push_back(index);
                width = size.x + pagerWidth * 2.0f;
            }
        }
    }

    void NextCandidate() {
        BML::Console::NextCandidate(static_cast<int>(m_Candidates.size()),
            m_CandidatePages, m_CandidateIndex, m_CandidatePage);
    }

    void PrevCandidate() {
        BML::Console::PrevCandidate(static_cast<int>(m_Candidates.size()),
            m_CandidatePages, m_CandidateIndex, m_CandidatePage);
    }

    void NextPageOfCandidates() {
        BML::Console::NextCandidatePage(static_cast<int>(m_Candidates.size()),
            m_CandidatePages, m_CandidateIndex, m_CandidatePage);
    }

    void PrevPageOfCandidates() {
        BML::Console::PrevCandidatePage(static_cast<int>(m_Candidates.size()),
            m_CandidatePages, m_CandidateIndex, m_CandidatePage);
    }

    // ── Rendering ───────────────────────────────────────────────────────

    float ComputeMessageAlpha(const ConsoleMessage &message) const {
        const float maxAlpha = ClampUnit(m_Settings.fadeMaxAlpha);
        if (m_Visible) return maxAlpha;
        if (message.ttl <= 0.0f) return 0.0f;
        const float maxAlpha255 = maxAlpha * 255.0f;
        return (std::min)(maxAlpha255, (message.ttl * 1000.0f) / 20.0f) / 255.0f;
    }

    bool ShouldShowMessage(const ConsoleMessage &message) const {
        return m_Visible || message.ttl > 0.0f;
    }

    void SetScrollYClamped(float y) {
        m_ScrollY = (std::clamp)(y, 0.0f, m_MaxScrollY);
        m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - 0.5f);
    }

    std::string FormatScrollPercent(float contentHeight, float visibleHeight) const {
        const float maxScroll = (std::max)(0.0f, contentHeight - visibleHeight);
        const float scrollY = (std::clamp)(m_ScrollY, 0.0f, maxScroll);
        const float percent = maxScroll > 0.0f ? (scrollY / maxScroll) * 100.0f : 0.0f;
        char buffer[32] = {};
        std::snprintf(buffer, sizeof(buffer), "%.0f%%", percent);
        return std::string(buffer);
    }

    void DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize,
        float contentHeight, float visibleHeight, float padX, float padY, float scrollbarW, float scrollbarPad) const {
        if (!drawList || m_MaxScrollY <= 0.0f) return;
        const ImVec2 scrollbarStart(
            contentPos.x + contentSize.x - scrollbarW - scrollbarPad,
            contentPos.y + padY + scrollbarPad);
        const ImVec2 scrollbarEnd(
            contentPos.x + contentSize.x - scrollbarPad,
            contentPos.y + contentSize.y - padY - scrollbarPad);
        drawList->AddRectFilled(scrollbarStart, scrollbarEnd, IM_COL32(60, 60, 60, 100));
        const float scrollbarHeight = scrollbarEnd.y - scrollbarStart.y;
        const float maxScroll = (std::max)(0.0f, contentHeight - visibleHeight);
        const float scrollY = (std::clamp)(m_ScrollY, 0.0f, maxScroll);
        const float visibleRatio = contentHeight > 0.0f ? (visibleHeight / contentHeight) : 1.0f;
        const float handleHeight = (std::max)(ImGui::GetStyle().GrabMinSize, scrollbarHeight * visibleRatio);
        const float handlePos = maxScroll > 0.0f ? (scrollY / maxScroll) * (scrollbarHeight - handleHeight) : 0.0f;
        const ImVec2 handleStart(scrollbarStart.x + 1.0f, scrollbarStart.y + handlePos);
        const ImVec2 handleEnd(scrollbarEnd.x - 1.0f, handleStart.y + handleHeight);
        drawList->AddRectFilled(handleStart, handleEnd, IM_COL32(150, 150, 150, 200));
        if (m_ScrollY > 0.0f || !m_ScrollToBottom) {
            const std::string scrollText = FormatScrollPercent(contentHeight, visibleHeight);
            const ImVec2 textSize = ImGui::CalcTextSize(scrollText.c_str());
            const ImVec2 textPos(
                contentPos.x + contentSize.x - textSize.x - scrollbarW - scrollbarPad - padX,
                contentPos.y + padY * 0.5f);
            drawList->AddRectFilled(
                ImVec2(textPos.x - padX * 0.25f, textPos.y - padY * 0.25f),
                ImVec2(textPos.x + textSize.x + padX * 0.25f, textPos.y + textSize.y + padY * 0.25f),
                IM_COL32(0, 0, 0, 150));
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), scrollText.c_str());
        }
    }

    void RenderMessages() {
        auto &messages = m_MessageBoard.Messages();
        if (messages.empty()) return;
        ImGuiIO &io = ImGui::GetIO();
        const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);
        m_MessageBoard.Tick(dt, m_Visible);
        if (messages.empty()) return;
        if (!m_MessageBoard.HasVisible(m_Visible)) return;
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        const ImVec2 vpSize = viewport->Size;
        const float windowWidth = vpSize.x * 0.96f;
        const float padX = 8.0f;
        const float padY = 8.0f;
        const float lineSpacing = m_Settings.lineSpacing >= 0.0f ? m_Settings.lineSpacing : ImGui::GetStyle().ItemSpacing.y;
        const float wrapWidth = windowWidth - padX * 2.0f;
        const float maxDisplayHeight = vpSize.y * 0.8f;

        AnsiPalette palette;
        palette.EnsureInitialized();
        const AnsiPalette *palettePtr = palette.IsActive() ? &palette : nullptr;

        const float contentHeight = m_MessageBoard.CalculateContentHeight(wrapWidth, lineSpacing, m_Settings.tabColumns, palettePtr, m_Visible);
        const float displayHeight = contentHeight + padY * 2.0f;
        const float windowHeight = (std::min)(displayHeight, maxDisplayHeight);
        const float bottomAnchor = vpSize.y * 0.9f;
        const float posX = viewport->Pos.x + vpSize.x * 0.02f;
        const float posY = viewport->Pos.y + bottomAnchor - windowHeight;

        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImVec4 winBg = Menu::GetMenuColor();
        winBg.w = ClampUnit(winBg.w * m_Settings.windowBackgroundAlpha);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, winBg);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
        if (!m_Visible) flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("##BMLConsoleMessages", nullptr, flags)) {
            if (!m_Visible) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

            const ImVec2 contentPos = ImGui::GetCursorScreenPos();
            const ImVec2 contentSize = ImGui::GetContentRegionAvail();
            const float availableHeight = contentSize.y - padY * 2.0f;
            const float scrollbarW = 8.0f;
            const float scrollbarPad = 2.0f;
            bool needsScrollbar = m_Visible && contentHeight > availableHeight;
            float clippedWrapWidth = wrapWidth;
            float clippedContentHeight = contentHeight;
            if (needsScrollbar) {
                clippedWrapWidth = (std::max)(0.0f, wrapWidth - (scrollbarW + scrollbarPad * 2.0f));
                clippedContentHeight = m_MessageBoard.CalculateContentHeight(clippedWrapWidth, lineSpacing, m_Settings.tabColumns, palettePtr, m_Visible);
            }

            if (m_Visible) {
                m_MaxScrollY = clippedContentHeight > availableHeight ? (clippedContentHeight - availableHeight) : 0.0f;
                if (m_ScrollToBottom) m_ScrollY = m_MaxScrollY;
                if (needsScrollbar && ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
                    const float scrollSpeed = (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.y) * 3.0f;
                    SetScrollYClamped(m_ScrollY - io.MouseWheel * scrollSpeed);
                } else {
                    SetScrollYClamped(m_ScrollY);
                }
            } else {
                m_ScrollY = 0.0f;
                m_MaxScrollY = 0.0f;
                m_ScrollToBottom = true;
            }

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImVec2 startPos(contentPos.x + padX, contentPos.y + padY - m_ScrollY);
            const ImVec2 clipMin(contentPos.x + padX, contentPos.y + padY);
            const ImVec2 clipMax(
                contentPos.x + contentSize.x - padX - (needsScrollbar ? (scrollbarW + scrollbarPad * 2.0f) : 0.0f),
                contentPos.y + contentSize.y - padY);
            ImGui::PushClipRect(clipMin, clipMax, true);

            std::vector<size_t> indices;
            std::vector<float> heights;
            indices.reserve(messages.size());
            heights.reserve(messages.size());
            for (size_t reverse = messages.size(); reverse > 0; --reverse) {
                const size_t index = reverse - 1;
                const ConsoleMessage &message = messages[index];
                if (!ShouldShowMessage(message)) continue;
                indices.push_back(index);
                heights.push_back(message.GetTextHeight(
                    clippedWrapWidth,
                    lineSpacing,
                    m_Settings.tabColumns,
                    palettePtr));
            }

            std::vector<float> offsets(indices.size());
            std::vector<float> bottoms(indices.size());
            float accumulated = 0.0f;
            for (size_t index = 0; index < indices.size(); ++index) {
                offsets[index] = accumulated;
                bottoms[index] = accumulated + heights[index];
                accumulated += heights[index] + lineSpacing;
            }

            const float clipMinRel = clipMin.y - startPos.y;
            const float clipMaxRel = clipMax.y - startPos.y;
            const auto lb = std::lower_bound(bottoms.begin(), bottoms.end(), clipMinRel);
            int begin = static_cast<int>(std::distance(bottoms.begin(), lb));
            const auto ub = std::upper_bound(offsets.begin(), offsets.end(), clipMaxRel);
            int end = static_cast<int>(std::distance(offsets.begin(), ub));
            begin = (std::clamp)(begin, 0, static_cast<int>(indices.size()));
            end = (std::clamp)(end, begin, static_cast<int>(indices.size()));

            ImVec4 bgBase = Menu::GetMenuColor();
            ImGui::SetCursorScreenPos(startPos);
            const BML_ImGuiApi *imguiApi = bml::imgui::GetCurrentApi();
            ImGuiListClipper *clipper = imguiApi && imguiApi->ImGuiListClipper_ImGuiListClipper
                ? imguiApi->ImGuiListClipper_ImGuiListClipper()
                : nullptr;
            if (clipper && imguiApi->ImGuiListClipper_Begin && imguiApi->ImGuiListClipper_IncludeItemsByIndex &&
                imguiApi->ImGuiListClipper_Step) {
                imguiApi->ImGuiListClipper_Begin(clipper, static_cast<int>(indices.size()), 1.0f);
                imguiApi->ImGuiListClipper_IncludeItemsByIndex(clipper, begin, end);
            }

            while (clipper && imguiApi->ImGuiListClipper_Step && imguiApi->ImGuiListClipper_Step(clipper)) {
                const int displayStart = (std::max)(clipper->DisplayStart, begin);
                const int displayEnd = (std::min)(clipper->DisplayEnd, end);
                for (int index = displayStart; index < displayEnd; ++index) {
                    const ConsoleMessage &message = messages[indices[static_cast<size_t>(index)]];
                    const float messageHeight = heights[static_cast<size_t>(index)];
                    const ImVec2 pos(startPos.x, startPos.y + offsets[static_cast<size_t>(index)]);
                    ImGui::SetCursorScreenPos(pos);
                    const float alpha = ComputeMessageAlpha(message);
                    if (alpha > 0.0f) {
                        const float bgAlpha = ClampUnit(bgBase.w * m_Settings.messageBackgroundAlpha * alpha);
                        if (bgAlpha > 0.0f) {
                            const ImVec4 bg(bgBase.x, bgBase.y, bgBase.z, bgAlpha);
                            drawList->AddRectFilled(
                                ImVec2(pos.x - padX * 0.5f, pos.y - padY * 0.25f),
                                ImVec2(pos.x + clippedWrapWidth + padX * 0.5f, pos.y + messageHeight + padY * 0.25f),
                                ImGui::GetColorU32(bg));
                        }
                        AnsiText::TextOptions options;
                        options.font = ImGui::GetFont();
                        options.wrapWidth = clippedWrapWidth;
                        options.alpha = alpha;
                        options.lineSpacing = lineSpacing;
                        options.tabColumns = m_Settings.tabColumns;
                        options.palette = palettePtr;
                        AnsiText::RenderText(drawList, message.text, pos, options);
                    }
                    ImGui::ItemSize(ImVec2(0.0f, messageHeight + lineSpacing));
                }
            }

            if (clipper) {
                if (imguiApi->ImGuiListClipper_End) {
                    imguiApi->ImGuiListClipper_End(clipper);
                }
                if (imguiApi->ImGuiListClipper_destroy) {
                    imguiApi->ImGuiListClipper_destroy(clipper);
                }
            }

            ImGui::PopClipRect();
            if (m_Visible && needsScrollbar && m_MaxScrollY > 0.0f) {
                DrawScrollIndicators(drawList, contentPos, contentSize, clippedContentHeight, availableHeight,
                    padX, padY, scrollbarW, scrollbarPad);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void RenderCommandBar() {
        if (!m_Visible) return;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 1.0f));
        ImVec4 menuColor = Menu::GetMenuColor();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, menuColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, menuColor);

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        const ImVec2 vpSize = viewport->Size;
        const ImVec2 windowPos(viewport->Pos.x + vpSize.x * 0.02f, viewport->Pos.y + vpSize.y * 0.93f);
        const ImVec2 windowSize(vpSize.x * 0.96f, 0.0f);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        if (!m_VisiblePrev) ImGui::SetNextWindowFocus();

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##BMLConsoleBar", nullptr, flags)) {
            constexpr ImU32 buttonColor = IM_COL32(99, 99, 99, 255);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);
            ImGui::Button(">");
            ImGui::PopStyleColor(3);
            ImGui::SameLine();

            const ImVec2 buttonSize = ImGui::GetItemRectSize();
            ImGui::SetNextItemWidth(windowSize.x * ((windowSize.x - buttonSize.x) / windowSize.x));
            if (m_FocusInput) {
                ImGui::SetKeyboardFocusHere();
                m_FocusInput = false;
            }

            const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_EscapeClearsAll |
                ImGuiInputTextFlags_CallbackCompletion |
                ImGuiInputTextFlags_CallbackHistory |
                ImGuiInputTextFlags_CallbackAlways |
                ImGuiInputTextFlags_CallbackResize |
                ImGuiInputTextFlags_CallbackEdit;
            if (ImGui::InputText("##BMLConsoleInput", m_InputBuffer.data(), m_InputBuffer.size(), inputFlags, ImGuiTextEditCallback)) {
                if (m_InputBuffer[0] != '\0') {
                    m_History.Append(m_InputBuffer.data());
                    m_History.Save(GetHistoryPath());
                    ExecuteCommand(m_InputBuffer.data());
                }
                SetConsoleVisible(false);
            }

            if (m_ShowHints) {
                if (ImGui::BeginChild("##CmdHints")) {
                    constexpr ImVec4 selectedColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                    if (m_CandidatePage != 0) {
                        ImGui::TextUnformatted("< ");
                        ImGui::SameLine(0, 0);
                    }
                    const int pageEnd = m_CandidatePage != static_cast<int>(m_CandidatePages.size()) - 1
                        ? m_CandidatePages[static_cast<size_t>(m_CandidatePage + 1)]
                        : static_cast<int>(m_Candidates.size());
                    for (int i = m_CandidatePages[static_cast<size_t>(m_CandidatePage)]; i < pageEnd; ++i) {
                        if (i != m_CandidatePages[static_cast<size_t>(m_CandidatePage)]) {
                            ImGui::SameLine(0, 0);
                            ImGui::TextUnformatted(" | ");
                            ImGui::SameLine(0, 0);
                        }
                        if (i != m_CandidateIndex) {
                            ImGui::Text("%s", m_Candidates[static_cast<size_t>(i)].c_str());
                        } else {
                            const char *text = m_Candidates[static_cast<size_t>(i)].c_str();
                            ImDrawList *dl = ImGui::GetWindowDrawList();
                            ImVec2 p = ImGui::GetCursorScreenPos();
                            const ImVec2 size = ImGui::CalcTextSize(text);
                            dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32_WHITE);
                            ImGui::TextColored(selectedColor, "%s", text);
                        }
                    }
                    if (pageEnd != static_cast<int>(m_Candidates.size())) {
                        ImGui::SameLine(0, 0);
                        ImGui::TextUnformatted(" >");
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) PrevCandidate();
                    else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) NextCandidate();
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) PrevPageOfCandidates();
                    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) NextPageOfCandidates();
                    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && !m_Candidates.empty()) {
                        m_CandidateSelected = m_CandidateIndex;
                        m_ShowHints = false;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) InvalidateCandidates();
                }
                ImGui::EndChild();
            } else {
                if (!m_Candidates.empty()) m_ShowHints = true;
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) SetConsoleVisible(false);
            }

            ImGui::SetItemDefaultFocus();
            if (!m_VisiblePrev) ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        m_VisiblePrev = true;
    }

    // ── Builtin context wiring (static trampolines) ────────────────────

    void SetupBuiltinContext() {
        m_BuiltinCtx.messageBoard = &m_MessageBoard;
        m_BuiltinCtx.registry = &m_Registry;
        m_BuiltinCtx.history = &m_History;
        m_BuiltinCtx.consoleVisible = m_Visible;
        m_BuiltinCtx.defaultDuration = m_Settings.messageDuration;

        m_BuiltinCtx.getCheatState = []() -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->GetCheatState() : false;
        };
        m_BuiltinCtx.setCheatState = [](bool enabled) -> BML_Result {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->SetCheatState(enabled) : BML_RESULT_FAIL;
        };
        m_BuiltinCtx.findModById = [](std::string_view id) -> BML_Mod {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->FindLoadedModuleById(id) : nullptr;
        };
        m_BuiltinCtx.setBoolConfig = [](BML_Mod mod, const char *cat, const char *name, bool val) -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->SetBoolConfigOnMod(mod, cat, name, val) : false;
        };
        m_BuiltinCtx.setStringConfig = [](BML_Mod mod, const char *cat, const char *name, const std::string &val) -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->SetStringConfigOnMod(mod, cat, name, val) : false;
        };
        m_BuiltinCtx.reloadPalette = []() -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->ReloadGlobalPalette() : false;
        };
        m_BuiltinCtx.getUserData = [](const char *key) -> void * {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            if (!self || !key) return nullptr;
            void *value = nullptr;
            if (self->Services().Builtins().Context->GetUserData(self->Services().GlobalContext(), key, &value) != BML_RESULT_OK) {
                return nullptr;
            }
            return value;
        };
        m_BuiltinCtx.getRuntimeVersion = []() -> const BML_Version * {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->Services().Builtins().Context->GetRuntimeVersion() : nullptr;
        };
        m_BuiltinCtx.getLoadedModuleCount = []() -> uint32_t {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->Services().Builtins().Module->GetLoadedModuleCount() : 0;
        };
        m_BuiltinCtx.getLoadedModuleAt = [](uint32_t index) -> BML_Mod {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self ? self->Services().Builtins().Module->GetLoadedModuleAt(index) : nullptr;
        };
        m_BuiltinCtx.getModId = [](BML_Mod mod, const char **outId) -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self && self->Services().Builtins().Module->GetModId(mod, outId) == BML_RESULT_OK;
        };
        m_BuiltinCtx.getModVersion = [](BML_Mod mod, BML_Version *outVersion) -> bool {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            return self && self->Services().Builtins().Module->GetModVersion(mod, outVersion) == BML_RESULT_OK;
        };
        m_BuiltinCtx.executeCommand = [](std::string cmd) {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            if (self) self->ExecuteCommand(std::move(cmd));
        };
        m_BuiltinCtx.saveHistory = []() {
            auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
            if (self) self->m_History.Save(self->GetHistoryPath());
        };
    }

    BML_Result RegisterCommand(const BML_ConsoleCommandDesc *desc) {
        BML_InterfaceRegistration registration = nullptr;
        if (m_HostRuntime) {
            BML_Result registrationResult = m_HostRuntime->RegisterContribution(
                m_Handle, BML_CONSOLE_COMMAND_REGISTRY_INTERFACE_ID, &registration);
            if (registrationResult != BML_RESULT_OK) {
                return registrationResult;
            }
        }

        BML_Result result = m_Registry.Register(desc, registration);
        if (result != BML_RESULT_OK && registration && m_HostRuntime) {
            (void) m_HostRuntime->UnregisterContribution(registration);
        }
        return result;
    }

    BML_Result UnregisterCommand(const char *name) {
        CommandRecord *record = m_Registry.Find(name);
        if (!record) {
            return BML_RESULT_NOT_FOUND;
        }

        if (record->registration && m_HostRuntime) {
            BML_Result releaseResult = m_HostRuntime->UnregisterContribution(record->registration);
            if (releaseResult != BML_RESULT_OK) {
                return releaseResult;
            }
        }

        return m_Registry.Unregister(name);
    }

public:
    static BML_Result Service_RegisterCommand(const BML_ConsoleCommandDesc *desc) {
        auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
        return self ? self->RegisterCommand(desc) : BML_RESULT_FAIL;
    }
    static BML_Result Service_UnregisterCommand(const char *name) {
        auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
        return self ? self->UnregisterCommand(name) : BML_RESULT_FAIL;
    }
    static BML_Result Service_ExecuteCommand(const char *command) {
        auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
        return self ? self->ConsoleExecuteCommand(command) : BML_RESULT_FAIL;
    }
    static void Service_Print(const char *msg, uint32_t flags) {
        auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
        if (self) self->PrintToConsole(msg, flags);
    }
    static int ImGuiTextEditCallback(ImGuiInputTextCallbackData *data) {
        auto *self = bml::detail::ModuleEntryHelper<ConsoleMod>::GetInstance();
        return (self && data) ? self->HandleTextEdit(data) : 0;
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        g_ConsoleServices = &services;
        m_Subs = services.CreateSubscriptions();
        m_HostRuntime = Acquire<BML_HostRuntimeInterface>(BML_CORE_HOST_RUNTIME_INTERFACE_ID, 1);
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_FOUND;
        }

        AnsiPalette::SetLoaderDirProvider(ConsolePaletteLoaderDirProvider);
        AnsiPalette::SetLoggerProvider(ConsolePaletteLogger);

        m_DrawReg = bml::ui::RegisterWindowDraw("bml.console.window", 50, DrawCallback, this);
        if (!m_DrawReg) {
            ClearPaletteProviders();
            return BML_RESULT_NOT_FOUND;
        }

        m_InputCaptureService = bml::AcquireInterface<BML_InputCaptureInterface>(BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0, 0);
        if (!m_InputCaptureService) {
            Services().Log().Warn("Failed to acquire input capture service; console input capture will be limited");
        }

        EnsureDefaultConfig();
        RefreshConfig();
        m_History.Load(GetHistoryPath());
        SetCheatState(false);

        if (Services().Builtins().ImcBus->GetTopicId(BML_TOPIC_CONSOLE_COMMAND, &m_TopicCommand) != BML_RESULT_OK) {
            m_DrawReg.Reset();
            m_InputCaptureService.Reset();
            ClearPaletteProviders();
            return BML_RESULT_FAIL;
        }

        SetupBuiltinContext();
        RegisterBuiltins(m_Registry, &m_BuiltinCtx);

        m_Subs.Add(BML_TOPIC_CONSOLE_OUTPUT, [this](const bml::imc::Message &msg) {
            auto *event = msg.As<BML_ConsoleOutputEvent>();
            if (!event || !event->message_utf8 || event->struct_size < sizeof(BML_ConsoleOutputEvent)) return;
            AddMessage(event->message_utf8, event->flags, m_Visible ? kPinnedMessageLifetime : m_Settings.messageDuration);
        });

        if (m_Subs.Count() < 1) {
            m_DrawReg.Reset();
            m_InputCaptureService.Reset();
            ClearPaletteProviders();
            return BML_RESULT_FAIL;
        }

        m_PublishedService = Publish(
            BML_CONSOLE_COMMAND_REGISTRY_INTERFACE_ID,
            &g_ConsoleRegistryService,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_IMMUTABLE);
        if (!m_PublishedService) {
            m_DrawReg.Reset();
            m_InputCaptureService.Reset();
            m_HostRuntime.Reset();
            Services().Log().Error( "Failed to register console command registry service");
            ClearPaletteProviders();
            return BML_RESULT_FAIL;
        }

        AddMessage("BML Console ready. Press / to open.", BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 12.0f);
        Services().Log().Info( "Console module initialized");
        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        m_History.Save(GetHistoryPath());
        SetConsoleVisible(false);
        ReleaseInputCapture();
        m_DrawReg.Reset();
        m_InputCaptureService.Reset();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_History.Save(GetHistoryPath());
        SetConsoleVisible(false);
        ReleaseInputCapture();
        (void) Services().Builtins().Context->SetUserData(Services().GlobalContext(), kInternalCheatStateKey, nullptr, nullptr);
        (void) m_PublishedService.Reset();
        m_DrawReg.Reset();
        m_InputCaptureService.Reset();
        m_HostRuntime.Reset();
        m_History.Clear();
        m_MessageBoard.Clear();
        m_Registry.Clear();
        m_InputBuffer.assign(65536, '\0');
        ClearPaletteProviders();
        g_ConsoleServices = nullptr;
    }
};

BML_ConsoleCommandRegistry g_ConsoleRegistryService = {
    1, 0,
    ConsoleMod::Service_RegisterCommand,
    ConsoleMod::Service_UnregisterCommand,
    ConsoleMod::Service_ExecuteCommand,
    ConsoleMod::Service_Print,
};

BML_DEFINE_MODULE(ConsoleMod)

