#ifndef BML_COMMANDCONTEXT_H
#define BML_COMMANDCONTEXT_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "Console/CommandRecord.h"

namespace BML {
    typedef void (*CommandOutputCallback)(const char *line, void *userdata);

    class CommandContext {
    public:
        using CommandInfo = BML::CommandInfo;

        enum class UnregisterResult {
            Success,
            InvalidName,
            NotFound,
            AccessDenied,
            Busy,
            InternalError,
        };

        struct CommandCall {
            CommandRecord *Command = nullptr;
            CommandInfo Info;

        private:
            friend class CommandContext;
            std::shared_ptr<CommandRecord> Lifetime;
        };

        CommandContext();

        CommandContext(const CommandContext &rhs) = delete;
        CommandContext(CommandContext &&rhs) noexcept = delete;

        ~CommandContext();

        CommandContext &operator=(const CommandContext &rhs) = delete;
        CommandContext &operator=(CommandContext &&rhs) noexcept = delete;

        bool RegisterCommand(const void *registrar, std::shared_ptr<CommandRecord> command);

        template <class T> bool RegisterCommand(const void *registrar, T *command) {
            return RegisterCommand(registrar, MakeCommandRecord(command));
        }

        template <class T>
        bool RegisterCommand(const void *registrar, T *command, CommandInfo info,
                             std::shared_ptr<void> lifetime = {}) {
            return RegisterCommand(
                registrar, MakeCommandRecord(command, std::move(info), std::move(lifetime)));
        }
        UnregisterResult UnregisterCommand(const void *registrar, const char *name);
        void UnregisterCommands(const void *registrar);

        std::size_t GetCommandCount() const;
        CommandRecord *GetCommandByIndex(std::size_t index) const;
        CommandRecord *GetCommandByName(const char *name) const;
        bool AcquireCommand(const char *name, CommandCall &call) const;
        std::vector<CommandInfo> GetCommandSnapshot() const;
        bool GetCommandInfoByIndex(std::size_t index, CommandInfo &info) const;
        bool GetCommandInfoByName(const char *name, CommandInfo &info) const;
        bool SetCommandEnabled(const void *implementation, bool enabled);

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

        static bool IsValidCommandAlias(const char *alias);
        static bool IsValidCommandAlias(std::string_view alias);
        static bool IsValidCommandName(const char *name);
        static bool IsValidCommandName(std::string_view name);
        static std::string NormalizeCommandName(const char *name);

    private:
        struct Entry {
            std::shared_ptr<CommandRecord> Command;
            const void *Registrar = nullptr;
            std::string NameKey;
            std::string AliasKey;
        };

        struct CommandKeyHash {
            using is_transparent = void;

            std::size_t operator()(const std::string &key) const noexcept;
            std::size_t operator()(const char *key) const noexcept;
        };

        struct CommandKeyEqual {
            using is_transparent = void;

            bool operator()(const std::string &lhs, const std::string &rhs) const noexcept;
            bool operator()(const std::string &lhs, const char *rhs) const noexcept;
            bool operator()(const char *lhs, const std::string &rhs) const noexcept;
        };

        std::vector<Entry> m_Commands;
        typedef std::unordered_map<std::string, CommandRecord *, CommandKeyHash, CommandKeyEqual>
            CommandMap;
        CommandMap m_CommandMap;
        bool m_CheatEnabled = false;
        CommandOutputCallback m_OutputCallback = nullptr;
        void *m_OutputCallbackData = nullptr;
    };
}

#endif // BML_COMMANDCONTEXT_H
