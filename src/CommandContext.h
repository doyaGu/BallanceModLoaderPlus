#ifndef BML_COMMANDCONTEXT_H
#define BML_COMMANDCONTEXT_H

#include <cstdarg>
#include <string>
#include <vector>
#include <unordered_map>

#include "BML/ICommand.h"

namespace BML {
    typedef void (*CommandOutputCallback)(const char *line, void *userdata);

    class CommandContext {
    public:
        struct CommandInfo {
            std::string Name;
            std::string Alias;
            std::string Description;
            bool Cheat = false;
        };

        enum class UnregisterResult {
            Success,
            InvalidName,
            NotFound,
            AccessDenied,
            Busy,
            InternalError,
        };

        CommandContext();

        CommandContext(const CommandContext &rhs) = delete;
        CommandContext(CommandContext &&rhs) noexcept = delete;

        ~CommandContext();

        CommandContext &operator=(const CommandContext &rhs) = delete;
        CommandContext &operator=(CommandContext &&rhs) noexcept = delete;

        bool RegisterCommand(const void *registrar, ICommand *cmd);
        bool RegisterCommand(const void *registrar, ICommand *cmd, CommandInfo info);
        UnregisterResult UnregisterCommand(const void *registrar, const char *name);
        void UnregisterCommands(const void *registrar);

        size_t GetCommandCount() const;
        ICommand *GetCommandByIndex(size_t index) const;
        ICommand *GetCommandByName(const char *name) const;
        std::vector<CommandInfo> GetCommandSnapshot() const;
        bool GetCommandInfoByIndex(size_t index, CommandInfo &info) const;
        bool GetCommandInfoByName(const char *name, CommandInfo &info) const;

        bool IsCheatEnabled() const noexcept { return m_CheatEnabled; }
        bool SetCheatEnabled(bool enabled) noexcept;

        void ClearCommands();

        bool SetOutputCallback(CommandOutputCallback callback, void *userdata);
        void ClearOutputCallback();

        void Output(const char *message);
        void OutputV(const char *format, va_list args);
        void OutputF(const char *format, ...) {
            va_list args;
            va_start(args, format);
            OutputV(format, args);
            va_end(args);
        }

        static char *AllocPrintfV(const char *format, va_list args);

        static std::vector<std::string> ParseCommandLine(const char *cmd);
        static bool IsValidCommandAlias(const char *alias);
        static bool IsValidCommandName(const char *name);
        static std::string NormalizeCommandName(const char *name);

    private:
        struct Entry {
            ICommand *Command = nullptr;
            const void *Registrar = nullptr;
            CommandInfo Info;
            std::string NameKey;
            std::string AliasKey;
        };

        struct CommandKeyHash {
            using is_transparent = void;

            size_t operator()(const std::string &key) const noexcept;
            size_t operator()(const char *key) const noexcept;
        };

        struct CommandKeyEqual {
            using is_transparent = void;

            bool operator()(const std::string &lhs, const std::string &rhs) const noexcept;
            bool operator()(const std::string &lhs, const char *rhs) const noexcept;
            bool operator()(const char *lhs, const std::string &rhs) const noexcept;
        };

        std::vector<Entry> m_Commands;
        typedef std::unordered_map<std::string, ICommand *, CommandKeyHash, CommandKeyEqual>
            CommandMap;
        CommandMap m_CommandMap;
        bool m_CheatEnabled = false;
        CommandOutputCallback m_OutputCallback = nullptr;
        void *m_OutputCallbackData = nullptr;
    };
}

#endif // BML_COMMANDCONTEXT_H
