// Runs one command line of the console shell: parse, then for every simple
// command expand its words and hand them to the dispatcher. Lists honour ;
// && and ||, pipelines capture each stage's output for the next one, and
// $(...) runs with a capture sink of its own. The executor keeps the status of
// the last pipeline so a later line can read $?.
#ifndef BML_SHELL_EXECUTOR_H
#define BML_SHELL_EXECUTOR_H

#include <cstddef>
#include <string>
#include <string_view>

#include "Console/Shell/ShellDispatcher.h"
#include "Console/Shell/ShellExpander.h"
#include "Console/Shell/ShellParser.h"
#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    // Collects ingame messages as lines. A message without a trailing newline
    // gets one, since one SendIngameMessage call is one row on the board.
    class CaptureSink final : public OutputSink {
    public:
        void Write(std::string_view message) override;

        const std::string &Text() const { return m_Text; }
        std::string TakeText();
        bool Truncated() const { return m_Truncated; }

    private:
        std::string m_Text;
        bool m_Truncated = false;
    };

    // Renders a diagnostic in the red the shell uses for its own errors.
    std::string FormatError(std::string_view message);

    class Executor {
    public:
        Executor(IDispatcher &dispatcher, const VariableResolver *variables, const AliasResolver *aliases);

        // Runs a whole line and returns the status of its last pipeline. A parse
        // error or incomplete input prints a diagnostic and yields Status::Syntax.
        // A blank line yields Status::Ok and leaves LastStatus alone.
        int Execute(std::string_view line);

        int LastStatus() const { return m_LastStatus; }
        void SetLastStatus(int status) { m_LastStatus = status; }

    private:
        int RunList(const List &list, std::size_t depth);
        int RunAndOr(const AndOr &andOr, std::size_t depth);
        int RunPipeline(const Pipeline &pipeline, std::size_t depth);
        int RunCommand(const SimpleCommand &command, const std::string *input, std::size_t depth);
        bool RunSubstitution(std::string_view source, std::size_t depth, std::string &out, std::string &error);
        void ReportSyntaxError(std::string_view source, const ParseResult &parsed);

        IDispatcher &m_Dispatcher;
        const VariableResolver *m_Variables;
        const AliasResolver *m_Aliases;
        int m_LastStatus = Status::Ok;
    };
}

#endif // BML_SHELL_EXECUTOR_H
