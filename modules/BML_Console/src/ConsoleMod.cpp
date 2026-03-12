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
#include <cstring>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

#include "bml_console.h"
#include "bml_extension.h"
#include "bml_input.h"
#include "bml_module.h"
#include "bml_topics.h"
#include "bml_ui.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

namespace {
constexpr size_t kInputBufferSize = 512;
constexpr size_t kMaxMessages = 200;
constexpr float kDefaultMessageLifetime = 6.0f;

struct ConsoleMessage {
    std::string text;
    float ttl = kDefaultMessageLifetime;
    uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE;
};

struct CommandRecord {
    std::string name;
    std::string alias;
    std::string description;
    uint32_t flags = BML_CONSOLE_COMMAND_FLAG_NONE;
    PFN_BML_ConsoleCommandExecute execute = nullptr;
    PFN_BML_ConsoleCommandComplete complete = nullptr;
    void *userData = nullptr;
};

struct ParsedToken {
    std::string value;
    size_t start = 0;
    size_t end = 0;
};

struct CompletionEdit {
    size_t replaceStart = 0;
    size_t replaceEnd = 0;
    size_t tokenIndex = 0;
    bool trailingWhitespace = false;
    std::string input;
    std::vector<ParsedToken> tokens;
};

BML_Mod g_ModHandle = nullptr;
BML_Subscription g_DrawSub = nullptr;
BML_Subscription g_KeyDownSub = nullptr;
BML_Subscription g_OutputSub = nullptr;

BML_TopicId g_TopicDraw = 0;
BML_TopicId g_TopicKeyDown = 0;
BML_TopicId g_TopicCommand = 0;
BML_TopicId g_TopicOutput = 0;

static BML_UI_ApiTable *g_UiApi = nullptr;
PFN_bmlUIGetImGuiContext g_GetImGuiContext = nullptr;
BML_InputExtension *g_InputExtension = nullptr;
bool g_InputExtensionLoaded = false;

std::vector<CommandRecord> g_Commands;
std::unordered_map<std::string, size_t> g_CommandLookup;

void ConsoleRegisterBuiltins();
BML_Result PublishCommand(const std::string &command);
void AddMessage(std::string text, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE, float ttl = kDefaultMessageLifetime);

bool g_Visible = false;
bool g_FocusInput = false;
std::array<char, kInputBufferSize> g_InputBuffer = {};
std::vector<std::string> g_History;
int g_HistoryIndex = -1;
std::vector<ConsoleMessage> g_Messages;

std::string ToLowerAscii(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    return normalized;
}

std::string TrimCopy(std::string_view text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

bool HasTrailingWhitespace(std::string_view text) {
    return !text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0;
}

bool ValidateCommandName(std::string_view name) {
    if (name.empty() || !std::isalpha(static_cast<unsigned char>(name.front()))) {
        return false;
    }

    for (size_t i = 1; i < name.size(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(name[i]))) {
            return false;
        }
    }
    return true;
}

void RebuildCommandLookup() {
    g_CommandLookup.clear();
    for (size_t index = 0; index < g_Commands.size(); ++index) {
        const CommandRecord &record = g_Commands[index];
        g_CommandLookup[ToLowerAscii(record.name)] = index;
        if (!record.alias.empty()) {
            g_CommandLookup[ToLowerAscii(record.alias)] = index;
        }
    }
}

CommandRecord *FindCommandRecord(std::string_view name) {
    const auto it = g_CommandLookup.find(ToLowerAscii(name));
    if (it == g_CommandLookup.end() || it->second >= g_Commands.size()) {
        return nullptr;
    }
    return &g_Commands[it->second];
}

std::vector<ParsedToken> ParseTokens(std::string_view text) {
    std::vector<ParsedToken> tokens;
    size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        const size_t start = index;
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) == 0) {
            ++index;
        }
        if (index > start) {
            tokens.push_back({std::string(text.substr(start, index - start)), start, index});
        }
    }
    return tokens;
}

std::string FormatCommandSummary(const CommandRecord &record) {
    std::string line = record.name;
    if (!record.alias.empty()) {
        line += " (alias: ";
        line += record.alias;
        line += ')';
    }
    if (!record.description.empty()) {
        line += " - ";
        line += record.description;
    }
    return line;
}

std::vector<const CommandRecord *> GetVisibleCommands() {
    std::vector<const CommandRecord *> commands;
    commands.reserve(g_Commands.size());
    for (const CommandRecord &record : g_Commands) {
        if ((record.flags & BML_CONSOLE_COMMAND_FLAG_HIDDEN) == 0) {
            commands.push_back(&record);
        }
    }

    std::sort(commands.begin(), commands.end(), [](const CommandRecord *lhs, const CommandRecord *rhs) {
        return lhs->name < rhs->name;
    });
    return commands;
}

void PrintToConsole(const char *message, uint32_t flags) {
    if (!message || message[0] == '\0') {
        return;
    }
    AddMessage(message, flags, g_Visible ? 3600.0f : kDefaultMessageLifetime);
}

void AddCompletionCandidate(std::vector<std::string> &candidates, std::string candidate) {
    if (candidate.empty()) {
        return;
    }

    const std::string normalized = ToLowerAscii(candidate);
    const bool exists = std::any_of(candidates.begin(), candidates.end(), [&](const std::string &existing) {
        return ToLowerAscii(existing) == normalized;
    });
    if (!exists) {
        candidates.push_back(std::move(candidate));
    }
}

void CompletionCollector(const char *candidateUtf8, void *userData) {
    if (!userData || !candidateUtf8 || candidateUtf8[0] == '\0') {
        return;
    }
    auto *candidates = static_cast<std::vector<std::string> *>(userData);
    AddCompletionCandidate(*candidates, candidateUtf8);
}

std::string LongestCommonPrefix(const std::vector<std::string> &values) {
    if (values.empty()) {
        return {};
    }

    std::string prefix = values.front();
    for (size_t i = 1; i < values.size() && !prefix.empty(); ++i) {
        const std::string &value = values[i];
        size_t length = 0;
        const size_t bound = (std::min)(prefix.size(), value.size());
        while (length < bound && std::tolower(static_cast<unsigned char>(prefix[length])) == std::tolower(static_cast<unsigned char>(value[length]))) {
            ++length;
        }
        prefix.resize(length);
    }
    return prefix;
}

bool ReplaceInputText(ImGuiInputTextCallbackData *data, const std::string &value) {
    if (!data) {
        return false;
    }
    if (value.size() + 1 >= static_cast<size_t>(data->BufSize)) {
        AddMessage("[console] completion result is too long for the input buffer", BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
        return false;
    }

    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, value.c_str());
    return true;
}

void ShowCompletionSuggestions(const std::vector<std::string> &candidates) {
    if (candidates.empty()) {
        return;
    }

    std::string line = "Suggestions: ";
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i != 0) {
            line += ", ";
        }
        line += candidates[i];
    }
    AddMessage(std::move(line), BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 8.0f);
}

CompletionEdit BuildCompletionEdit(std::string_view input) {
    CompletionEdit edit;
    edit.input = std::string(input);
    edit.tokens = ParseTokens(edit.input);
    edit.trailingWhitespace = HasTrailingWhitespace(edit.input);

    if (edit.tokens.empty() || edit.trailingWhitespace) {
        edit.tokenIndex = edit.tokens.size();
        edit.replaceStart = edit.input.size();
        edit.replaceEnd = edit.input.size();
        return edit;
    }

    edit.tokenIndex = edit.tokens.size() - 1;
    edit.replaceStart = edit.tokens.back().start;
    edit.replaceEnd = edit.tokens.back().end;
    return edit;
}

BML_Result RegisterCommandImpl(const BML_ConsoleCommandDesc *desc) {
    if (!desc || desc->struct_size < sizeof(BML_ConsoleCommandDesc) || !desc->name_utf8 || !desc->execute) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    const std::string name = desc->name_utf8;
    const std::string alias = desc->alias_utf8 ? desc->alias_utf8 : "";
    if (!ValidateCommandName(name) || (!alias.empty() && !ValidateCommandName(alias))) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    if (FindCommandRecord(name) != nullptr || (!alias.empty() && FindCommandRecord(alias) != nullptr)) {
        return BML_RESULT_ALREADY_EXISTS;
    }

    CommandRecord record;
    record.name = name;
    record.alias = alias;
    record.description = desc->description_utf8 ? desc->description_utf8 : "";
    record.flags = desc->flags;
    record.execute = desc->execute;
    record.complete = desc->complete;
    record.userData = desc->user_data;
    g_Commands.push_back(std::move(record));
    RebuildCommandLookup();
    return BML_RESULT_OK;
}

BML_Result UnregisterCommandImpl(const char *nameUtf8) {
    if (!nameUtf8 || nameUtf8[0] == '\0') {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    CommandRecord *record = FindCommandRecord(nameUtf8);
    if (!record) {
        return BML_RESULT_NOT_FOUND;
    }

    const std::string canonicalName = record->name;
    g_Commands.erase(std::remove_if(g_Commands.begin(), g_Commands.end(), [&](const CommandRecord &entry) {
        return entry.name == canonicalName;
    }), g_Commands.end());
    RebuildCommandLookup();
    return BML_RESULT_OK;
}

const char *ResolveArgsUtf8(const std::string &commandLine, const std::vector<ParsedToken> &tokens) {
    if (tokens.size() <= 1) {
        return commandLine.c_str() + commandLine.size();
    }
    return commandLine.c_str() + tokens[1].start;
}

BML_Result ExecuteRegisteredCommand(const std::string &commandLine) {
    const std::vector<ParsedToken> tokens = ParseTokens(commandLine);
    if (tokens.empty()) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    CommandRecord *record = FindCommandRecord(tokens[0].value);
    if (!record) {
        return BML_RESULT_NOT_FOUND;
    }

    std::vector<const char *> argv;
    argv.reserve(tokens.size());
    for (const ParsedToken &token : tokens) {
        argv.push_back(token.value.c_str());
    }

    BML_ConsoleCommandInvocation invocation = BML_CONSOLE_COMMAND_INVOCATION_INIT;
    invocation.command_line_utf8 = commandLine.c_str();
    invocation.args_utf8 = ResolveArgsUtf8(commandLine, tokens);
    invocation.name_utf8 = tokens[0].value.c_str();
    invocation.argv_utf8 = argv.empty() ? nullptr : argv.data();
    invocation.argc = static_cast<uint32_t>(argv.size());
    return record->execute(&invocation, record->userData);
}

BML_Result ConsoleExecuteCommand(const char *commandUtf8) {
    if (!commandUtf8) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    const std::string trimmed = TrimCopy(commandUtf8);
    if (trimmed.empty()) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    const BML_Result registeredResult = ExecuteRegisteredCommand(trimmed);
    if (registeredResult != BML_RESULT_NOT_FOUND) {
        return registeredResult;
    }
    return PublishCommand(trimmed);
}

void ClearCommandRegistry() {
    g_Commands.clear();
    g_CommandLookup.clear();
}

BML_Result ExecuteHelpCommand(const BML_ConsoleCommandInvocation *invocation, void *) {
    if (!invocation) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    if (invocation->argc >= 2 && invocation->argv_utf8 && invocation->argv_utf8[1]) {
        const CommandRecord *record = FindCommandRecord(invocation->argv_utf8[1]);
        if (!record) {
            AddMessage(std::string("Unknown command: ") + invocation->argv_utf8[1], BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
            return BML_RESULT_NOT_FOUND;
        }
        AddMessage(FormatCommandSummary(*record), BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 3600.0f);
        return BML_RESULT_OK;
    }

    AddMessage("Available commands:", BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 3600.0f);
    const auto commands = GetVisibleCommands();
    for (const CommandRecord *record : commands) {
        AddMessage(FormatCommandSummary(*record), BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 3600.0f);
    }
    AddMessage("Unregistered commands still fall back to BML/Console/Command for compatibility.", BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 3600.0f);
    return BML_RESULT_OK;
}

void CompleteHelpCommand(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink,
    void *sinkUserData,
    void *) {
    if (!context || !sink) {
        return;
    }

    if (context->token_index != 1) {
        return;
    }

    const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
    for (const CommandRecord *record : GetVisibleCommands()) {
        if (prefix.empty() || ToLowerAscii(record->name).rfind(prefix, 0) == 0) {
            sink(record->name.c_str(), sinkUserData);
        }
    }
}

BML_Result ExecuteClearCommand(const BML_ConsoleCommandInvocation *, void *) {
    g_Messages.clear();
    return BML_RESULT_OK;
}

BML_Result ExecuteHistoryCommand(const BML_ConsoleCommandInvocation *, void *) {
    for (const std::string &entry : g_History) {
        AddMessage(entry, BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 3600.0f);
    }
    return BML_RESULT_OK;
}

void ConsoleRegisterBuiltins() {
    ClearCommandRegistry();

    BML_ConsoleCommandDesc helpDesc = BML_CONSOLE_COMMAND_DESC_INIT;
    helpDesc.name_utf8 = "help";
    helpDesc.description_utf8 = "List registered console commands or show help for one command";
    helpDesc.execute = ExecuteHelpCommand;
    helpDesc.complete = CompleteHelpCommand;
    RegisterCommandImpl(&helpDesc);

    BML_ConsoleCommandDesc clearDesc = BML_CONSOLE_COMMAND_DESC_INIT;
    clearDesc.name_utf8 = "clear";
    clearDesc.description_utf8 = "Clear the console message history";
    clearDesc.execute = ExecuteClearCommand;
    RegisterCommandImpl(&clearDesc);

    BML_ConsoleCommandDesc historyDesc = BML_CONSOLE_COMMAND_DESC_INIT;
    historyDesc.name_utf8 = "history";
    historyDesc.description_utf8 = "Print previously entered commands";
    historyDesc.execute = ExecuteHistoryCommand;
    RegisterCommandImpl(&historyDesc);
}

void CollectCommandNameCompletions(const CompletionEdit &edit, std::vector<std::string> &candidates) {
    const std::string prefix = (edit.tokenIndex < edit.tokens.size()) ? ToLowerAscii(edit.tokens[edit.tokenIndex].value) : std::string();
    for (const CommandRecord *record : GetVisibleCommands()) {
        if (prefix.empty() || ToLowerAscii(record->name).rfind(prefix, 0) == 0) {
            AddCompletionCandidate(candidates, record->name);
        }
        if (!record->alias.empty() && (prefix.empty() || ToLowerAscii(record->alias).rfind(prefix, 0) == 0)) {
            AddCompletionCandidate(candidates, record->alias);
        }
    }
}

void CollectCommandArgumentCompletions(const CompletionEdit &edit, std::vector<std::string> &candidates) {
    if (edit.tokens.empty()) {
        return;
    }

    const CommandRecord *record = FindCommandRecord(edit.tokens.front().value);
    if (!record || !record->complete) {
        return;
    }

    std::vector<const char *> argv;
    argv.reserve(edit.tokens.size());
    for (const ParsedToken &token : edit.tokens) {
        argv.push_back(token.value.c_str());
    }

    const char *currentToken = "";
    if (!edit.trailingWhitespace && edit.tokenIndex < edit.tokens.size()) {
        currentToken = edit.tokens[edit.tokenIndex].value.c_str();
    }

    BML_ConsoleCompletionContext context = BML_CONSOLE_COMPLETION_CONTEXT_INIT;
    context.input_utf8 = edit.input.c_str();
    context.name_utf8 = edit.tokens.front().value.c_str();
    context.current_token_utf8 = currentToken;
    context.argv_utf8 = argv.empty() ? nullptr : argv.data();
    context.argc = static_cast<uint32_t>(argv.size());
    context.token_index = static_cast<uint32_t>(edit.tokenIndex);
    context.flags = edit.trailingWhitespace ? BML_CONSOLE_COMPLETION_FLAG_TRAILING_SPACE : BML_CONSOLE_COMPLETION_FLAG_NONE;
    record->complete(&context, CompletionCollector, &candidates, record->userData);
}

void ApplyCompletion(ImGuiInputTextCallbackData *data, std::vector<std::string> candidates, const CompletionEdit &edit) {
    if (candidates.empty()) {
        return;
    }

    std::sort(candidates.begin(), candidates.end());
    std::string replacement;
    bool appendSpace = false;
    const std::string currentToken = (!edit.trailingWhitespace && edit.tokenIndex < edit.tokens.size()) ? edit.tokens[edit.tokenIndex].value : std::string();

    if (candidates.size() == 1) {
        replacement = candidates.front();
        appendSpace = true;
    } else {
        replacement = LongestCommonPrefix(candidates);
        if (replacement.size() <= currentToken.size()) {
            replacement.clear();
        }
        ShowCompletionSuggestions(candidates);
    }

    if (replacement.empty()) {
        return;
    }

    std::string completed = edit.input;
    completed.replace(edit.replaceStart, edit.replaceEnd - edit.replaceStart, replacement);
    if (appendSpace && (completed.empty() || completed.back() != ' ')) {
        completed.push_back(' ');
    }
    ReplaceInputText(data, completed);
}

void HandleCompletion(ImGuiInputTextCallbackData *data) {
    if (!data) {
        return;
    }

    const CompletionEdit edit = BuildCompletionEdit(std::string_view(data->Buf, static_cast<size_t>(data->BufTextLen)));
    std::vector<std::string> candidates;
    if (edit.tokenIndex == 0) {
        CollectCommandNameCompletions(edit, candidates);
    } else {
        CollectCommandArgumentCompletions(edit, candidates);
    }
    ApplyCompletion(data, std::move(candidates), edit);
}

static BML_ConsoleExtension g_ConsoleExtension = {
    1,
    0,
    RegisterCommandImpl,
    UnregisterCommandImpl,
    ConsoleExecuteCommand,
    PrintToConsole,
};

void AddMessage(std::string text, uint32_t flags, float ttl) {
    if (text.empty()) {
        return;
    }

    if (g_Messages.size() >= kMaxMessages) {
        g_Messages.erase(g_Messages.begin());
    }

    g_Messages.push_back({std::move(text), ttl, flags});
}

void SetConsoleVisible(bool visible) {
    if (g_Visible == visible) {
        return;
    }

    g_Visible = visible;
    g_FocusInput = visible;
    g_HistoryIndex = -1;

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
}

void RenderMessages() {
    if (g_Messages.empty()) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);
    for (auto &message : g_Messages) {
        if (!g_Visible) {
            message.ttl = std::max(0.0f, message.ttl - dt);
        }
    }

    if (!g_Visible) {
        g_Messages.erase(std::remove_if(g_Messages.begin(), g_Messages.end(), [](const ConsoleMessage &message) {
            return message.ttl <= 0.0f;
        }), g_Messages.end());
        if (g_Messages.empty()) {
            return;
        }
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float width = viewport->Size.x * 0.62f;
    const float height = g_Visible ? viewport->Size.y * 0.30f : viewport->Size.y * 0.18f;
    const float x = viewport->Pos.x + viewport->Size.x * 0.02f;
    const float y = viewport->Pos.y + viewport->Size.y - height - (g_Visible ? 84.0f : 24.0f);

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(g_Visible ? 0.82f : 0.48f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav;
    if (!g_Visible) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    if (ImGui::Begin("##BMLConsoleMessages", nullptr, flags)) {
        const size_t start = g_Messages.size() > 64 ? g_Messages.size() - 64 : 0;
        for (size_t i = start; i < g_Messages.size(); ++i) {
            const ConsoleMessage &message = g_Messages[i];
            ImVec4 color = (message.flags & BML_CONSOLE_OUTPUT_FLAG_ERROR) != 0
                ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f)
                : ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", message.text.c_str());
            ImGui::PopStyleColor();
        }
        if (g_Visible) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::End();
}

int TextEditCallback(ImGuiInputTextCallbackData *data) {
    if (!data) {
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        HandleCompletion(data);
        return 0;
    }

    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory) {
        return 0;
    }

    if (g_History.empty()) {
        return 0;
    }

    if (data->EventKey == ImGuiKey_UpArrow) {
        if (g_HistoryIndex < 0) {
            g_HistoryIndex = static_cast<int>(g_History.size()) - 1;
        } else if (g_HistoryIndex > 0) {
            --g_HistoryIndex;
        }
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (g_HistoryIndex >= 0 && g_HistoryIndex < static_cast<int>(g_History.size()) - 1) {
            ++g_HistoryIndex;
        } else {
            g_HistoryIndex = -1;
            data->DeleteChars(0, data->BufTextLen);
            return 0;
        }
    }

    if (g_HistoryIndex >= 0) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, g_History[static_cast<size_t>(g_HistoryIndex)].c_str());
    }
    return 0;
}

BML_Result PublishCommand(const std::string &command) {
    BML_ConsoleCommandEvent event = BML_CONSOLE_COMMAND_EVENT_INIT;
    event.command_utf8 = command.c_str();
    return bmlImcPublish(g_TopicCommand, &event, sizeof(event));
}

void ExecuteCommand(std::string command) {
    command = TrimCopy(command);
    if (command.empty()) {
        return;
    }

    AddMessage(std::string("> ") + command, BML_CONSOLE_OUTPUT_FLAG_SYSTEM, g_Visible ? 3600.0f : kDefaultMessageLifetime);

    g_History.erase(std::remove(g_History.begin(), g_History.end(), command), g_History.end());
    g_History.push_back(command);
    g_HistoryIndex = -1;

    const BML_Result registeredResult = ExecuteRegisteredCommand(command);
    if (registeredResult == BML_RESULT_OK) {
        return;
    }
    if (registeredResult != BML_RESULT_NOT_FOUND) {
        AddMessage(std::string("[console] command failed with result ") + std::to_string(registeredResult), BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
        return;
    }

    const BML_Result publishResult = PublishCommand(command);
    if (publishResult == BML_RESULT_IMC_NO_SUBSCRIBERS) {
        AddMessage(std::string("Unknown command: ") + command, BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
    }
}

void RenderCommandBar() {
    if (!g_Visible) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float width = viewport->Size.x * 0.62f;
    const float x = viewport->Pos.x + viewport->Size.x * 0.02f;
    const float y = viewport->Pos.y + viewport->Size.y - 52.0f;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##BMLConsoleBar", nullptr, flags)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(">");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);

        if (g_FocusInput) {
            ImGui::SetKeyboardFocusHere();
            g_FocusInput = false;
        }

        const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory |
            ImGuiInputTextFlags_CallbackCompletion;
        if (ImGui::InputText("##BMLConsoleInput", g_InputBuffer.data(), g_InputBuffer.size(), inputFlags, TextEditCallback)) {
            ExecuteCommand(g_InputBuffer.data());
            g_InputBuffer.fill('\0');
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            SetConsoleVisible(false);
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
    RenderMessages();
    RenderCommandBar();
}

void OnKeyDown(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_KeyDownEvent)) {
        return;
    }

    const auto *event = static_cast<const BML_KeyDownEvent *>(msg->data);
    if (event->repeat) {
        return;
    }

    if (!g_Visible && event->key_code == DIK_SLASH) {
        SetConsoleVisible(true);
    }
}

void OnOutput(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_ConsoleOutputEvent)) {
        return;
    }

    const auto *event = static_cast<const BML_ConsoleOutputEvent *>(msg->data);
    if (!event->message_utf8 || event->struct_size < sizeof(BML_ConsoleOutputEvent)) {
        return;
    }

    AddMessage(event->message_utf8, event->flags, g_Visible ? 3600.0f : kDefaultMessageLifetime);
}

BML_Result ValidateAttachArgs(const BML_ModAttachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModAttachArgs) || args->mod == nullptr || args->get_proc == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
        return BML_RESULT_VERSION_MISMATCH;
    }
    return BML_RESULT_OK;
}

BML_Result ValidateDetachArgs(const BML_ModDetachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (args->api_version != BML_MOD_ENTRYPOINT_API_VERSION) {
        return BML_RESULT_VERSION_MISMATCH;
    }
    return BML_RESULT_OK;
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
    if (g_OutputSub) {
        bmlImcUnsubscribe(g_OutputSub);
        g_OutputSub = nullptr;
    }
}

BML_Result HandleAttach(const BML_ModAttachArgs *args) {
    BML_Result validation = ValidateAttachArgs(args);
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
            bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Console", "Failed to load BML_UI extension");
            bmlUnloadAPI();
            g_ModHandle = nullptr;
            return BML_RESULT_NOT_FOUND;
        }
        g_GetImGuiContext = g_UiApi->GetImGuiContext;
    }

    BML_Version requiredVersion = bmlMakeVersion(1, 0, 0);
    result = bmlExtensionLoad(BML_INPUT_EXTENSION_NAME, &requiredVersion, reinterpret_cast<void **>(&g_InputExtension), nullptr);
    if (result == BML_RESULT_OK) {
        g_InputExtensionLoaded = true;
    } else {
        g_InputExtension = nullptr;
        g_InputExtensionLoaded = false;
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_Console", "Failed to load BML_EXT_Input; console input capture will be limited");
    }

    bmlImcGetTopicId(BML_TOPIC_UI_DRAW, &g_TopicDraw);
    bmlImcGetTopicId(BML_TOPIC_INPUT_KEY_DOWN, &g_TopicKeyDown);
    bmlImcGetTopicId(BML_TOPIC_CONSOLE_COMMAND, &g_TopicCommand);
    bmlImcGetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &g_TopicOutput);

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
    if ((result = bmlImcSubscribe(g_TopicOutput, OnOutput, nullptr, &g_OutputSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }

    ConsoleRegisterBuiltins();

    BML_ExtensionDesc consoleDesc = BML_EXTENSION_DESC_INIT;
    consoleDesc.name = BML_CONSOLE_EXTENSION_NAME;
    consoleDesc.version = bmlMakeVersion(1, 0, 0);
    consoleDesc.api_table = &g_ConsoleExtension;
    consoleDesc.api_size = sizeof(g_ConsoleExtension);
    consoleDesc.description = "BML Console command registration and completion API";
    if ((result = bmlExtensionRegister(&consoleDesc)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        if (g_InputExtensionLoaded) {
            bmlExtensionUnload(BML_INPUT_EXTENSION_NAME);
            g_InputExtensionLoaded = false;
        }
        g_InputExtension = nullptr;
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        ClearCommandRegistry();
        return result;
    }

    AddMessage("BML Console ready. Press / to open.", BML_CONSOLE_OUTPUT_FLAG_SYSTEM, 12.0f);
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Console", "Console module initialized");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs *args) {
    BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    SetConsoleVisible(false);
    bmlExtensionUnregister(BML_CONSOLE_EXTENSION_NAME);
    ReleaseSubscriptions();
    if (g_InputExtensionLoaded) {
        bmlExtensionUnload(BML_INPUT_EXTENSION_NAME);
        g_InputExtensionLoaded = false;
    }
    g_InputExtension = nullptr;
    g_GetImGuiContext = nullptr;
    bmlExtensionUnload(BML_UI_EXTENSION_NAME);
    g_UiApi = nullptr;
    g_History.clear();
    g_Messages.clear();
    ClearCommandRegistry();
    g_InputBuffer.fill('\0');
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