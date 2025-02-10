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
        CommandContext();

        CommandContext(const CommandContext &rhs) = delete;
        CommandContext(CommandContext &&rhs) noexcept = delete;

        ~CommandContext();

        CommandContext &operator=(const CommandContext &rhs) = delete;
        CommandContext &operator=(CommandContext &&rhs) noexcept = delete;

        bool RegisterCommand(ICommand *cmd);
        bool UnregisterCommand(const char *name);

        size_t GetCommandCount() const;
        ICommand *GetCommandByIndex(size_t index) const;
        ICommand *GetCommandByName(const char *name) const;

        void SortCommands();
        void ClearCommands();

        const char *GetVariable(const char *key) const;
        bool AddVariable(const char *key, const char *value);
        bool RemoveVariable(const char *key);

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
        static char *AllocPrintf(const char *format, ...);

    private:
        static bool ValidateCommandName(const char *name);

        std::vector<ICommand *> m_Commands;
        typedef std::unordered_map<std::string, ICommand *> CommandMap;
        CommandMap m_CommandMap;
        typedef std::unordered_map<std::string, std::string> VariableMap;
        VariableMap m_Variables;

        CommandOutputCallback m_OutputCallback = nullptr;
        void *m_OutputCallbackData = nullptr;
    };
}

#endif // BML_COMMANDCONTEXT_H
