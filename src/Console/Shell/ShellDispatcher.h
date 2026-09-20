// The narrow seam between the shell executor and the loader. The executor never
// touches the command registry or the message board directly: it hands one
// expanded command at a time to IDispatcher::Invoke and routes captured output
// through a stack of sinks. Tests drive the executor with a fake dispatcher.
#ifndef BML_SHELL_DISPATCHER_H
#define BML_SHELL_DISPATCHER_H

#include <string>
#include <string_view>
#include <vector>

namespace BML::Shell {
    class OutputSink {
    public:
        virtual ~OutputSink() = default;
        // One call is one message, the unit IBML::SendIngameMessage works in.
        virtual void Write(std::string_view message) = 0;
    };

    class IDispatcher {
    public:
        virtual ~IDispatcher() = default;

        // Runs one command whose words are already expanded. args[0] names the
        // command; input is the text piped into it or null. The dispatcher looks
        // the command up, applies the cheat gate, runs the Pre/Post callbacks,
        // catches exceptions, and prints its own diagnostics. Returns the exit
        // status (see Status in ShellTypes.h).
        virtual int Invoke(const std::vector<std::string> &args, const std::string *input) = 0;

        // Prints a diagnostic to the player even while output is being captured.
        virtual void WriteError(std::string_view message) = 0;

        // Redirects everything the loader would show as an ingame message into
        // sink until the matching PopSink. Nested pushes stack.
        virtual void PushSink(OutputSink *sink) = 0;
        virtual void PopSink() = 0;

        // Records the raw line about to run.
        virtual void LogExecute(std::string_view line) = 0;
    };
}

#endif // BML_SHELL_DISPATCHER_H
