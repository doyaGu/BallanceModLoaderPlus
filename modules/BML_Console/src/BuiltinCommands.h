#ifndef BML_CONSOLE_BUILTIN_COMMANDS_H
#define BML_CONSOLE_BUILTIN_COMMANDS_H

#include "bml_console.h"
#include "bml_errors.h"

#include "CommandRegistry.h"
#include "CommandHistory.h"
#include "MessageBoard.h"

namespace BML::Console {

struct BuiltinCommandContext {
    MessageBoard *messageBoard = nullptr;
    CommandRegistry *registry = nullptr;
    CommandHistory *history = nullptr;
    bool consoleVisible = false;
    float defaultDuration = kDefaultMessageLifetime;

    // Callbacks for operations that require ConsoleMod state
    using GetCheatStateFn = bool(*)();
    using SetCheatStateFn = BML_Result(*)(bool);
    using FindModByIdFn = BML_Mod(*)(std::string_view);
    using SetBoolConfigFn = bool(*)(BML_Mod, const char *, const char *, bool);
    using SetStringConfigFn = bool(*)(BML_Mod, const char *, const char *, const std::string &);
    using ReloadPaletteFn = bool(*)();
    using GetUserDataFn = void *(*)(const char *);
    using GetRuntimeVersionFn = const BML_Version *(*)();
    using GetLoadedModuleCountFn = uint32_t(*)();
    using GetLoadedModuleAtFn = BML_Mod(*)(uint32_t);
    using GetModIdFn = bool(*)(BML_Mod, const char **);
    using GetModVersionFn = bool(*)(BML_Mod, BML_Version *);
    using ExecuteCommandFn = void(*)(std::string);
    using SaveHistoryFn = void(*)();

    GetCheatStateFn getCheatState = nullptr;
    SetCheatStateFn setCheatState = nullptr;
    FindModByIdFn findModById = nullptr;
    SetBoolConfigFn setBoolConfig = nullptr;
    SetStringConfigFn setStringConfig = nullptr;
    ReloadPaletteFn reloadPalette = nullptr;
    GetUserDataFn getUserData = nullptr;
    GetRuntimeVersionFn getRuntimeVersion = nullptr;
    GetLoadedModuleCountFn getLoadedModuleCount = nullptr;
    GetLoadedModuleAtFn getLoadedModuleAt = nullptr;
    GetModIdFn getModId = nullptr;
    GetModVersionFn getModVersion = nullptr;
    ExecuteCommandFn executeCommand = nullptr;
    SaveHistoryFn saveHistory = nullptr;
};

void RegisterBuiltins(CommandRegistry &registry, BuiltinCommandContext *ctx);

} // namespace BML::Console

#endif // BML_CONSOLE_BUILTIN_COMMANDS_H
