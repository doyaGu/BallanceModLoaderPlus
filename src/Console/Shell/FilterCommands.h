// Text filters for pipelines: grep, head, tail, wc, sort, uniq. Each has a pure
// core over a vector of lines, which the tests drive, and a thin ICommand that
// reads the piped text and prints the result one line per message.
#ifndef BML_SHELL_FILTER_COMMANDS_H
#define BML_SHELL_FILTER_COMMANDS_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "BML/ICommand.h"

namespace BML::Shell::Filters {
    // Splits captured text into lines; the final newline does not add a line.
    std::vector<std::string> SplitLines(std::string_view text);

    struct GrepOptions {
        bool ignoreCase = false;  // -i
        bool invert = false;      // -v
        bool lineNumbers = false; // -n
        bool countOnly = false;   // -c
        bool fixed = false;       // -F
    };

    // Matches against each line with its ANSI colour codes removed but prints the
    // original line. Returns true when any line was selected. error is set, and
    // false returned, for a pattern the regex engine rejects.
    bool Grep(const std::vector<std::string> &lines, std::string_view pattern, const GrepOptions &options,
              std::vector<std::string> &out, std::string &error);

    std::vector<std::string> Head(const std::vector<std::string> &lines, std::size_t count);
    std::vector<std::string> Tail(const std::vector<std::string> &lines, std::size_t count);

    struct WcOptions {
        bool lines = false;
        bool words = false;
        bool bytes = false;
    };

    // Counts on the raw text; with no option set all three are printed.
    std::string Wc(std::string_view text, const WcOptions &options);

    struct SortOptions {
        bool reverse = false; // -r
        bool unique = false;  // -u
        bool numeric = false; // -n
    };

    std::vector<std::string> Sort(std::vector<std::string> lines, const SortOptions &options);
    std::vector<std::string> Uniq(const std::vector<std::string> &lines, bool count);
}

namespace BML::Shell {
    class CommandGrep final : public ICommand {
    public:
        std::string GetName() override { return "grep"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Print piped lines matching a pattern."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };

    class CommandHead final : public ICommand {
    public:
        std::string GetName() override { return "head"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Print the first lines of piped text."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };

    class CommandTail final : public ICommand {
    public:
        std::string GetName() override { return "tail"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Print the last lines of piped text."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };

    class CommandWc final : public ICommand {
    public:
        std::string GetName() override { return "wc"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Count lines, words, and bytes of piped text."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };

    class CommandSort final : public ICommand {
    public:
        std::string GetName() override { return "sort"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Sort piped lines."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };

    class CommandUniq final : public ICommand {
    public:
        std::string GetName() override { return "uniq"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Drop repeated adjacent piped lines."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override;
    };
}

#endif // BML_SHELL_FILTER_COMMANDS_H
