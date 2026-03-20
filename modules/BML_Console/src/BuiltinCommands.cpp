#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>

#include "bml_core.h"
#include "bml_console.h"
#include "bml_virtools.h"

#include "AnsiPalette.h"
#include "StringUtils.h"

#include "BuiltinCommands.h"

namespace BML::Console {
namespace {

// -- Utility helpers used only by builtins -------------------------------

std::string ExpandTabs(std::string_view text, int tabColumns) {
    std::string expanded;
    expanded.reserve(text.size());
    const int columns = (std::max)(1, tabColumns);
    int currentColumn = 0;
    for (char ch : text) {
        if (ch == '\t') {
            const int spaces = columns - (currentColumn % columns);
            expanded.append(static_cast<size_t>(spaces), ' ');
            currentColumn += spaces;
            continue;
        }
        expanded.push_back(ch);
        if (ch == '\n' || ch == '\r') {
            currentColumn = 0;
        } else {
            ++currentColumn;
        }
    }
    return expanded;
}

bool TryParsePositiveInt(const char *text, int &value) {
    if (!text || text[0] == '\0') return false;
    int parsed = 0;
    const char *end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed <= 0) return false;
    value = parsed;
    return true;
}

bool TryParseBooleanValue(std::string_view text, bool &value) {
    const std::string lowered = ToLowerAscii(text);
    if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes") {
        value = true;
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no") {
        value = false;
        return true;
    }
    return false;
}

std::string JoinArguments(const char *const *argv, uint32_t argc, uint32_t startIndex) {
    std::string result;
    if (!argv || startIndex >= argc) return result;
    for (uint32_t index = startIndex; index < argc; ++index) {
        if (!argv[index]) continue;
        if (!result.empty()) result.push_back(' ');
        result.append(argv[index]);
    }
    return result;
}

std::string DecodeEchoText(std::string_view text, bool &suppressNewline) {
    std::string decoded;
    decoded.reserve(text.size());
    suppressNewline = false;
    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch != '\\' || index + 1 >= text.size()) {
            decoded.push_back(ch);
            continue;
        }
        const char next = text[++index];
        switch (next) {
        case 'n': decoded.push_back('\n'); break;
        case 'r': decoded.push_back('\r'); break;
        case 't': decoded.push_back('\t'); break;
        case '\\': decoded.push_back('\\'); break;
        case '"': decoded.push_back('"'); break;
        case 'c':
            suppressNewline = true;
            return decoded;
        default:
            decoded.push_back('\\');
            decoded.push_back(next);
            break;
        }
    }
    return decoded;
}

// -- Message helper ------------------------------------------------------

void AddMsg(BuiltinCommandContext *ctx, const std::string &text,
    uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE, float ttl = -1.0f) {
    if (!ctx || !ctx->messageBoard) return;
    if (ttl < 0.0f) ttl = kPinnedMessageLifetime;
    ctx->messageBoard->Add(text, flags, ttl, ctx->consoleVisible, ctx->defaultDuration);
}

void AddErr(BuiltinCommandContext *ctx, const std::string &text) {
    AddMsg(ctx, text, BML_CONSOLE_OUTPUT_FLAG_ERROR, 8.0f);
}

void AddSys(BuiltinCommandContext *ctx, const std::string &text, float ttl = 8.0f) {
    AddMsg(ctx, text, BML_CONSOLE_OUTPUT_FLAG_SYSTEM, ttl);
}

// -- Builtin command table -----------------------------------------------

struct BuiltinDef {
    const char *name;
    const char *alias;
    const char *desc;
    PFN_BML_ConsoleCommandExecute execute;
    PFN_BML_ConsoleCommandComplete complete;
};

// Forward declarations
BML_Result BuiltinBml(const BML_ConsoleCommandInvocation *, void *);
BML_Result BuiltinHelp(const BML_ConsoleCommandInvocation *, void *);
void BuiltinHelpComplete(const BML_ConsoleCompletionContext *, PFN_BML_ConsoleCompletionSink, void *, void *);
BML_Result BuiltinCheat(const BML_ConsoleCommandInvocation *, void *);
void BuiltinCheatComplete(const BML_ConsoleCompletionContext *, PFN_BML_ConsoleCompletionSink, void *, void *);
BML_Result BuiltinEcho(const BML_ConsoleCommandInvocation *, void *);
BML_Result BuiltinClear(const BML_ConsoleCommandInvocation *, void *);
BML_Result BuiltinHistory(const BML_ConsoleCommandInvocation *, void *);
void BuiltinHistoryComplete(const BML_ConsoleCompletionContext *, PFN_BML_ConsoleCompletionSink, void *, void *);
BML_Result BuiltinExit(const BML_ConsoleCommandInvocation *, void *);
BML_Result BuiltinHud(const BML_ConsoleCommandInvocation *, void *);
void BuiltinHudComplete(const BML_ConsoleCompletionContext *, PFN_BML_ConsoleCompletionSink, void *, void *);
BML_Result BuiltinPalette(const BML_ConsoleCommandInvocation *, void *);
void BuiltinPaletteComplete(const BML_ConsoleCompletionContext *, PFN_BML_ConsoleCompletionSink, void *, void *);

static const BuiltinDef kBuiltins[] = {
    {"bml",     nullptr, "Show runtime version and loaded modules",                    BuiltinBml,     nullptr},
    {"help",    "?",     "List registered console commands or show help for one command", BuiltinHelp,    BuiltinHelpComplete},
    {"cheat",   nullptr, "Toggle or set cheat mode",                                   BuiltinCheat,   BuiltinCheatComplete},
    {"echo",    nullptr, "Print text to the console",                                  BuiltinEcho,    nullptr},
    {"clear",   nullptr, "Clear the console message history",                          BuiltinClear,   nullptr},
    {"history", nullptr, "Show, clear, or replay command history",                     BuiltinHistory, BuiltinHistoryComplete},
    {"exit",    nullptr, "Exit the game",                                              BuiltinExit,    nullptr},
    {"hud",     nullptr, "Control the built-in HUD",                                   BuiltinHud,     BuiltinHudComplete},
    {"palette", "pal",   "Manage ANSI palette config and themes",                      BuiltinPalette, BuiltinPaletteComplete},
};

// -- Palette option table (DRY replacement for if-chains) ----------------

struct PaletteOption {
    const char *name;
    const char *altName;
    std::string (*getter)(const AnsiPalette &);
};

static const PaletteOption kPaletteGetOptions[] = {
    {"theme", "base", [](const AnsiPalette &p) -> std::string {
        const std::string t = p.GetActiveThemeName();
        return "theme = " + (t.empty() ? std::string("none") : t);
    }},
    {"cube", nullptr, [](const AnsiPalette &p) -> std::string {
        return std::string("cube = ") + (p.GetCubeMixFromTheme() ? "theme" : "standard");
    }},
    {"gray", "grey", [](const AnsiPalette &p) -> std::string {
        return std::string("gray = ") + (p.GetGrayMixFromTheme() ? "theme" : "standard");
    }},
    {"mix_strength", "mix", [](const AnsiPalette &p) -> std::string {
        return "mix_strength = " + std::to_string(p.GetMixStrength());
    }},
    {"mix_space", nullptr, [](const AnsiPalette &p) -> std::string {
        return std::string("mix_space = ") + (p.GetLinearMix() ? "linear" : "srgb");
    }},
    {"toning", nullptr, [](const AnsiPalette &p) -> std::string {
        return std::string("toning = ") + (p.GetToningEnabled() ? "on" : "off");
    }},
    {"tone_brightness", nullptr, [](const AnsiPalette &p) -> std::string {
        return "tone_brightness = " + std::to_string(p.GetToneBrightness());
    }},
    {"tone_saturation", nullptr, [](const AnsiPalette &p) -> std::string {
        return "tone_saturation = " + std::to_string(p.GetToneSaturation());
    }},
};

static constexpr const char *kPaletteSetKeys[] = {
    "cube", "gray", "mix_strength", "mix_space", "toning", "tone_brightness", "tone_saturation",
};

static constexpr const char *kPaletteActions[] = {
    "reload", "sample", "list", "show", "theme", "info", "set", "get", "reset",
};

// -- Builtin implementations ---------------------------------------------

BML_Result BuiltinBml(const BML_ConsoleCommandInvocation *, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    const BML_Version *runtime = ctx && ctx->getRuntimeVersion ? ctx->getRuntimeVersion() : nullptr;
    if (runtime) {
        AddSys(ctx,
            "Ballance Mod Loader Plus " +
            std::to_string(runtime->major) + "." +
            std::to_string(runtime->minor) + "." +
            std::to_string(runtime->patch), kPinnedMessageLifetime);
    } else {
        AddSys(ctx, "Ballance Mod Loader Plus " BML_VERSION_STRING, kPinnedMessageLifetime);
    }

    if (!ctx || !ctx->getLoadedModuleCount || !ctx->getLoadedModuleAt || !ctx->getModId) {
        AddErr(ctx, "[bml] loaded module enumeration is unavailable");
        return BML_RESULT_NOT_SUPPORTED;
    }

    const uint32_t count = ctx->getLoadedModuleCount();
    AddSys(ctx, std::to_string(count) + " loaded modules:", kPinnedMessageLifetime);
    for (uint32_t index = 0; index < count; ++index) {
        BML_Mod mod = ctx->getLoadedModuleAt(index);
        const char *id = nullptr;
        BML_Version version = BML_VERSION_INIT(0, 0, 0);
        if (!mod || !ctx->getModId(mod, &id) || !id) continue;
        if (ctx->getModVersion) {
            ctx->getModVersion(mod, &version);
        }
        AddSys(ctx,
            "  " + std::string(id) + " v" +
            std::to_string(version.major) + "." +
            std::to_string(version.minor) + "." +
            std::to_string(version.patch), kPinnedMessageLifetime);
    }
    return BML_RESULT_OK;
}

BML_Result BuiltinHelp(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation || !ctx->registry) return BML_RESULT_INVALID_ARGUMENT;
    if (invocation->argc >= 2 && invocation->argv_utf8 && invocation->argv_utf8[1]) {
        const CommandRecord *record = ctx->registry->Find(invocation->argv_utf8[1]);
        if (!record) {
            AddErr(ctx, std::string("Unknown command: ") + invocation->argv_utf8[1]);
            return BML_RESULT_NOT_FOUND;
        }
        AddSys(ctx, FormatCommandSummary(*record), kPinnedMessageLifetime);
        return BML_RESULT_OK;
    }
    AddSys(ctx, "Available commands:", kPinnedMessageLifetime);
    for (const CommandRecord *record : ctx->registry->FindVisible())
        AddSys(ctx, FormatCommandSummary(*record), kPinnedMessageLifetime);
    AddSys(ctx, "Unregistered commands still fall back to BML/Console/Command for compatibility.", kPinnedMessageLifetime);
    return BML_RESULT_OK;
}

void BuiltinHelpComplete(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *ud) {
    if (!context || !sink || context->token_index != 1) return;
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!ctx->registry) return;
    const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
    for (const CommandRecord *record : ctx->registry->FindVisible()) {
        if (prefix.empty() || ToLowerAscii(record->name).rfind(prefix, 0) == 0)
            sink(record->name.c_str(), sinkUserData);
    }
}

BML_Result BuiltinCheat(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation || !ctx->getCheatState || !ctx->setCheatState) return BML_RESULT_INVALID_ARGUMENT;

    bool enabled = !ctx->getCheatState();
    if (invocation->argc >= 2 && invocation->argv_utf8 && invocation->argv_utf8[1]) {
        if (!TryParseBooleanValue(invocation->argv_utf8[1], enabled)) {
            AddErr(ctx, "[cheat] usage: cheat [on|off]");
            return BML_RESULT_INVALID_ARGUMENT;
        }
    }

    const BML_Result result = ctx->setCheatState(enabled);
    if (result != BML_RESULT_OK) {
        AddErr(ctx, "[cheat] failed to update cheat state");
        return result;
    }

    AddSys(ctx, enabled ? "Cheat Mode On" : "Cheat Mode Off");
    return BML_RESULT_OK;
}

void BuiltinCheatComplete(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *) {
    if (!context || !sink || context->token_index != 1) return;
    const std::string prefix = context->current_token_utf8 ? context->current_token_utf8 : "";
    AddCompletionIfMatches("on", prefix, sink, sinkUserData);
    AddCompletionIfMatches("off", prefix, sink, sinkUserData);
}

BML_Result BuiltinEcho(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation) return BML_RESULT_INVALID_ARGUMENT;

    bool noNewline = false;
    bool interpretEscapes = false;
    uint32_t index = 1;
    while (index < invocation->argc && invocation->argv_utf8 && invocation->argv_utf8[index]) {
        const std::string_view token(invocation->argv_utf8[index]);
        if (token == "--") { ++index; break; }
        if (token.empty() || token.front() != '-' || token == "-") break;
        bool recognized = true;
        for (size_t i = 1; i < token.size(); ++i) {
            switch (token[i]) {
            case 'n': noNewline = true; break;
            case 'e': interpretEscapes = true; break;
            case 'E': interpretEscapes = false; break;
            default: recognized = false; break;
            }
            if (!recognized) break;
        }
        if (!recognized) break;
        ++index;
    }

    std::string text = JoinArguments(invocation->argv_utf8, invocation->argc, index);
    bool suppressNewline = false;
    if (interpretEscapes) text = DecodeEchoText(text, suppressNewline);
    if (!noNewline && !suppressNewline) text.push_back('\n');
    AddMsg(ctx, text, BML_CONSOLE_OUTPUT_FLAG_NONE, kPinnedMessageLifetime);
    return BML_RESULT_OK;
}

BML_Result BuiltinClear(const BML_ConsoleCommandInvocation *, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (ctx && ctx->messageBoard) ctx->messageBoard->Clear();
    return BML_RESULT_OK;
}

BML_Result BuiltinHistory(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation || !ctx->history) return BML_RESULT_INVALID_ARGUMENT;

    if (invocation->argc <= 1) {
        const auto &entries = ctx->history->Entries();
        const int count = static_cast<int>(entries.size());
        for (int i = 0; i < count; ++i) {
            AddSys(ctx, "[" + std::to_string(i + 1) + "] " + entries[(count - 1) - i], kPinnedMessageLifetime);
        }
        return BML_RESULT_OK;
    }

    if (!invocation->argv_utf8 || !invocation->argv_utf8[1]) return BML_RESULT_INVALID_ARGUMENT;

    if (std::strcmp(invocation->argv_utf8[1], "clear") == 0) {
        ctx->history->Clear();
        if (ctx->saveHistory) ctx->saveHistory();
        return BML_RESULT_OK;
    }

    int index = 0;
    if (!TryParsePositiveInt(invocation->argv_utf8[1], index)) {
        AddErr(ctx, "[history] usage: history [clear|index]");
        return BML_RESULT_INVALID_ARGUMENT;
    }

    const auto &entries = ctx->history->Entries();
    if (index < 1 || index > static_cast<int>(entries.size())) return BML_RESULT_OK;
    const size_t historyIndex = entries.size() - static_cast<size_t>(index);
    if (ctx->executeCommand) ctx->executeCommand(entries[historyIndex]);
    return BML_RESULT_OK;
}

void BuiltinHistoryComplete(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *) {
    if (!context || !sink || context->token_index != 1) return;
    const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
    if (prefix.empty() || std::string("clear").rfind(prefix, 0) == 0)
        sink("clear", sinkUserData);
}

BML_Result BuiltinExit(const BML_ConsoleCommandInvocation *, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!ctx || !ctx->getUserData) {
        AddErr(ctx, "[exit] main window is unavailable");
        return BML_RESULT_NOT_FOUND;
    }
    HWND hwnd = static_cast<HWND>(ctx->getUserData(BML_VIRTOOLS_KEY_MAINHWND));
    if (!hwnd) {
        AddErr(ctx, "[exit] main window is unavailable");
        return BML_RESULT_NOT_FOUND;
    }
    ::PostMessage(hwnd, 0x5FA, 0, 0);
    AddSys(ctx, "[exit] shutting down game");
    return BML_RESULT_OK;
}

BML_Result BuiltinHud(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation || !ctx->findModById) return BML_RESULT_INVALID_ARGUMENT;

    BML_Mod hudMod = ctx->findModById("com.bml.hud");
    if (!hudMod) {
        AddErr(ctx, "[hud] com.bml.hud is not loaded");
        return BML_RESULT_NOT_FOUND;
    }

    if (invocation->argc == 2 && invocation->argv_utf8 && invocation->argv_utf8[1]) {
        bool enabled = false;
        if (TryParseBooleanValue(invocation->argv_utf8[1], enabled)) {
            if (!ctx->setBoolConfig || !ctx->setBoolConfig(hudMod, "General", "Enabled", enabled)) {
                AddErr(ctx, "[hud] failed to update HUD enabled state");
                return BML_RESULT_FAIL;
            }
            AddSys(ctx, enabled ? "[hud] enabled" : "[hud] disabled");
            return BML_RESULT_OK;
        }
    }

    if (invocation->argc == 3 && invocation->argv_utf8 && invocation->argv_utf8[1] && invocation->argv_utf8[2]) {
        bool enabled = false;
        if (!TryParseBooleanValue(invocation->argv_utf8[2], enabled)) {
            AddErr(ctx, "[hud] usage: hud <title|fps|sr> <on|off>");
            return BML_RESULT_INVALID_ARGUMENT;
        }
        std::string key;
        const std::string target = ToLowerAscii(invocation->argv_utf8[1]);
        if (target == "title") key = "ShowTitle";
        else if (target == "fps") key = "ShowFPS";
        else if (target == "sr") key = "ShowSRTimer";
        if (!key.empty()) {
            if (!ctx->setBoolConfig || !ctx->setBoolConfig(hudMod, "HUD", key.c_str(), enabled)) {
                AddErr(ctx, "[hud] failed to update HUD option");
                return BML_RESULT_FAIL;
            }
            AddSys(ctx, "[hud] " + target + (enabled ? " enabled" : " disabled"));
            return BML_RESULT_OK;
        }
    }

    if (invocation->argc >= 3 && invocation->argv_utf8 && invocation->argv_utf8[1] &&
        ToLowerAscii(invocation->argv_utf8[1]) == "text") {
        const std::string text = JoinArguments(invocation->argv_utf8, invocation->argc, 2);
        if (text.empty()) {
            AddErr(ctx, "[hud] usage: hud text <value>");
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!ctx->setStringConfig || !ctx->setStringConfig(hudMod, "General", "TitleText", text)) {
            AddErr(ctx, "[hud] failed to update title text");
            return BML_RESULT_FAIL;
        }
        AddSys(ctx, "[hud] title text updated");
        return BML_RESULT_OK;
    }

    AddErr(ctx, "[hud] usage: hud <on|off> | hud <title|fps|sr> <on|off> | hud text <value>");
    return BML_RESULT_INVALID_ARGUMENT;
}

void BuiltinHudComplete(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *) {
    if (!context || !sink) return;
    const std::string prefix = context->current_token_utf8 ? context->current_token_utf8 : "";
    if (context->token_index == 1) {
        for (const char *opt : {"on", "off", "title", "fps", "sr", "text"})
            AddCompletionIfMatches(opt, prefix, sink, sinkUserData);
        return;
    }
    if (context->token_index == 2 && context->argv_utf8 && context->argc >= 2 && context->argv_utf8[1]) {
        const std::string subcommand = ToLowerAscii(context->argv_utf8[1]);
        if (subcommand == "title" || subcommand == "fps" || subcommand == "sr") {
            AddCompletionIfMatches("on", prefix, sink, sinkUserData);
            AddCompletionIfMatches("off", prefix, sink, sinkUserData);
        }
    }
}

BML_Result BuiltinPalette(const BML_ConsoleCommandInvocation *invocation, void *ud) {
    auto *ctx = static_cast<BuiltinCommandContext *>(ud);
    if (!invocation || !invocation->argv_utf8) return BML_RESULT_INVALID_ARGUMENT;

    AnsiPalette palette;
    palette.EnsureInitialized();

    const std::string action = invocation->argc >= 2 && invocation->argv_utf8[1]
        ? ToLowerAscii(invocation->argv_utf8[1])
        : "reload";

    if (action == "reload") {
        const bool loaded = ctx->reloadPalette ? ctx->reloadPalette() : false;
        AddSys(ctx,
            loaded
                ? "[palette] reloaded from " + utils::ToString(palette.GetConfigPathW(), true)
                : "[palette] no config found, using default palette");
        return BML_RESULT_OK;
    }

    if (action == "sample") {
        const bool created = palette.SaveSampleIfMissing();
        if (ctx->reloadPalette) ctx->reloadPalette();
        AddSys(ctx,
            created
                ? "[palette] sample created: " + utils::ToString(palette.GetConfigPathW(), true)
                : "[palette] sample exists: " + utils::ToString(palette.GetConfigPathW(), true));
        return BML_RESULT_OK;
    }

    if (action == "list") {
        const auto names = palette.GetAvailableThemes();
        std::string line = "[palette] themes";
        const std::string active = palette.GetActiveThemeName();
        line += " (active: ";
        line += active.empty() ? "none" : active;
        line += "):";
        if (names.empty()) {
            line += " <none>";
        } else {
            for (const std::string &name : names) {
                line += " ";
                line += name;
                if (!active.empty() && ToLowerAscii(name) == ToLowerAscii(active)) line += "*";
            }
        }
        AddSys(ctx, line, kPinnedMessageLifetime);
        return BML_RESULT_OK;
    }

    if (action == "show") {
        const auto chain = palette.GetResolvedThemeChain();
        std::string line = "[palette] chain:";
        if (chain.empty()) {
            line += " none";
        } else {
            for (const auto &entry : chain) {
                line += " ";
                line += entry.name;
                if (!entry.exists) line += "(missing)";
            }
        }
        AddSys(ctx, line, kPinnedMessageLifetime);
        return BML_RESULT_OK;
    }

    if (action == "theme") {
        if (invocation->argc < 3 || !invocation->argv_utf8[2]) {
            AddErr(ctx, "[palette] usage: palette theme <name>");
            return BML_RESULT_INVALID_ARGUMENT;
        }
        palette.SaveSampleIfMissing();
        if (!palette.SetActiveThemeName(invocation->argv_utf8[2])) {
            AddErr(ctx, "[palette] failed to update palette theme");
            return BML_RESULT_FAIL;
        }
        if (ctx->reloadPalette) ctx->reloadPalette();
        AddSys(ctx,
            ToLowerAscii(invocation->argv_utf8[2]) == "none"
                ? "[palette] theme cleared"
                : "[palette] theme set to " + std::string(invocation->argv_utf8[2]));
        return BML_RESULT_OK;
    }

    if (action == "info") {
        palette.ReloadFromFile();
        char buffer[256] = {};
        std::snprintf(buffer, sizeof(buffer),
            "[palette] info: theme=%s cube=%s gray=%s mix=%.2f space=%s toning=%s tb=%.2f ts=%.2f",
            palette.GetActiveThemeName().empty() ? "none" : palette.GetActiveThemeName().c_str(),
            palette.GetCubeMixFromTheme() ? "theme" : "standard",
            palette.GetGrayMixFromTheme() ? "theme" : "standard",
            palette.GetMixStrength(),
            palette.GetLinearMix() ? "linear" : "srgb",
            palette.GetToningEnabled() ? "on" : "off",
            palette.GetToneBrightness(),
            palette.GetToneSaturation());
        AddSys(ctx, buffer, kPinnedMessageLifetime);
        return BML_RESULT_OK;
    }

    if (action == "set") {
        if (invocation->argc < 4 || !invocation->argv_utf8[2]) {
            AddErr(ctx, "[palette] usage: palette set <option> <value>");
            return BML_RESULT_INVALID_ARGUMENT;
        }
        palette.SaveSampleIfMissing();
        const std::string key = ToLowerAscii(invocation->argv_utf8[2]);
        const std::string value = JoinArguments(invocation->argv_utf8, invocation->argc, 3);
        if (!palette.SetThemeOption(key, value)) {
            AddErr(ctx, "[palette] failed to update palette option");
            return BML_RESULT_FAIL;
        }
        if (ctx->reloadPalette) ctx->reloadPalette();
        AddSys(ctx, "[palette] option updated");
        return BML_RESULT_OK;
    }

    if (action == "get") {
        if (invocation->argc < 3 || !invocation->argv_utf8[2]) {
            AddErr(ctx, "[palette] usage: palette get <option>");
            return BML_RESULT_INVALID_ARGUMENT;
        }
        palette.ReloadFromFile();
        const std::string key = ToLowerAscii(invocation->argv_utf8[2]);
        for (const auto &opt : kPaletteGetOptions) {
            if (key == opt.name || (opt.altName && key == opt.altName)) {
                AddSys(ctx, "[palette] " + opt.getter(palette), kPinnedMessageLifetime);
                return BML_RESULT_OK;
            }
        }
        AddErr(ctx, "[palette] unknown option");
        return BML_RESULT_INVALID_ARGUMENT;
    }

    if (action == "reset") {
        if (!palette.ResetThemeOptions()) {
            AddErr(ctx, "[palette] failed to reset theme options");
            return BML_RESULT_FAIL;
        }
        if (ctx->reloadPalette) ctx->reloadPalette();
        AddSys(ctx, "[palette] theme reset");
        return BML_RESULT_OK;
    }

    AddErr(ctx, "[palette] usage: palette [reload|sample|list|show|theme|info|set|get|reset]");
    return BML_RESULT_INVALID_ARGUMENT;
}

void BuiltinPaletteComplete(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData, void *) {
    if (!context || !sink) return;
    const std::string prefix = context->current_token_utf8 ? context->current_token_utf8 : "";
    if (context->token_index == 1) {
        for (const char *action : kPaletteActions)
            AddCompletionIfMatches(action, prefix, sink, sinkUserData);
        return;
    }
    if (!context->argv_utf8 || context->argc < 2 || !context->argv_utf8[1]) return;
    const std::string action = ToLowerAscii(context->argv_utf8[1]);
    if (context->token_index == 2 && action == "theme") {
        AddCompletionIfMatches("none", prefix, sink, sinkUserData);
        AnsiPalette palette;
        palette.EnsureInitialized();
        for (const std::string &name : palette.GetAvailableThemes())
            AddCompletionIfMatches(name.c_str(), prefix, sink, sinkUserData);
        return;
    }
    if (context->token_index == 2 && (action == "set" || action == "get")) {
        if (action == "get") AddCompletionIfMatches("theme", prefix, sink, sinkUserData);
        for (const char *key : kPaletteSetKeys)
            AddCompletionIfMatches(key, prefix, sink, sinkUserData);
        return;
    }
    if (context->token_index == 3 && action == "set" && context->argc >= 3 && context->argv_utf8[2]) {
        const std::string option = ToLowerAscii(context->argv_utf8[2]);
        if (option == "cube" || option == "gray") {
            AddCompletionIfMatches("standard", prefix, sink, sinkUserData);
            AddCompletionIfMatches("theme", prefix, sink, sinkUserData);
        } else if (option == "mix_space") {
            AddCompletionIfMatches("linear", prefix, sink, sinkUserData);
            AddCompletionIfMatches("srgb", prefix, sink, sinkUserData);
        } else if (option == "toning") {
            AddCompletionIfMatches("on", prefix, sink, sinkUserData);
            AddCompletionIfMatches("off", prefix, sink, sinkUserData);
        }
    }
}

} // anonymous namespace

// -- Public registration function ----------------------------------------

void RegisterBuiltins(CommandRegistry &registry, BuiltinCommandContext *ctx) {
    registry.Clear();
    for (const auto &def : kBuiltins) {
        BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
        desc.name_utf8 = def.name;
        desc.alias_utf8 = def.alias;
        desc.description_utf8 = def.desc;
        desc.execute = def.execute;
        desc.complete = def.complete;
        desc.user_data = ctx;
        registry.Register(&desc);
    }
}

} // namespace BML::Console
