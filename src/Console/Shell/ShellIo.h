// Per-invocation state a command may reach while its Execute runs: the text
// piped into it and the exit status it wants to report. ICommand::Execute is
// frozen at returning void, so the loader wraps every call in an
// InvocationScope and commands talk to it through SetStatus and GetInput, the
// same functions the BML_SetCommandStatus and BML_GetCommandInput exports use.
#ifndef BML_SHELL_IO_H
#define BML_SHELL_IO_H

#include <string>
#include <string_view>

class IBML;

namespace BML::Shell {
    // Covers an entire command dispatch, including pre/post callbacks. Nested
    // dispatch is supported, but bounded so commands cannot recurse until the
    // game-thread stack is exhausted.
    class CommandDispatchScope {
    public:
        CommandDispatchScope();
        ~CommandDispatchScope();

        CommandDispatchScope(const CommandDispatchScope &) = delete;
        CommandDispatchScope &operator=(const CommandDispatchScope &) = delete;

        explicit operator bool() const { return m_Active; }

    private:
        bool m_Active = false;
    };

    class InvocationScope {
    public:
        explicit InvocationScope(const std::string *input);
        ~InvocationScope();

        InvocationScope(const InvocationScope &) = delete;
        InvocationScope &operator=(const InvocationScope &) = delete;

        int Status() const;

    private:
        std::size_t m_Depth;
    };

    // True while some command's Execute is running on this thread.
    bool HasInvocation();

    // Records the non-negative exit status of the innermost running command.
    // Negative API errors become the generic failure status. Returns false and
    // changes nothing outside an invocation.
    bool SetStatus(int status);

    // The text piped into the innermost running command, or null when there is
    // no pipe or no invocation.
    const std::string *GetInput();

    // Prints message in red through bml and marks the current command failed.
    void Fail(IBML *bml, std::string_view message);
}

#endif // BML_SHELL_IO_H
