// Word expansion of the console shell: variables ($NAME, ${NAME}, $?) and
// command substitutions ($(...) and backticks), then quote removal. There is
// deliberately no word splitting, globbing, tilde, or brace expansion: one word
// in the source is one argument, whatever a variable holds.
#ifndef BML_SHELL_EXPANDER_H
#define BML_SHELL_EXPANDER_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Console/Shell/ShellParser.h"

namespace BML::Shell {
    class VariableResolver {
    public:
        virtual ~VariableResolver() = default;
        virtual bool LookupVariable(std::string_view name, std::string &value) const = 0;
    };

    struct ExpandContext {
        const VariableResolver *variables = nullptr;
        int lastStatus = 0;
        // Runs a substitution body and yields its captured output with trailing
        // newlines removed. Absent when substitutions are not allowed here.
        std::function<bool(std::string_view source, std::string &out, std::string &error)> runSubstitution;
    };

    bool ExpandWord(const Word &word, const ExpandContext &context, std::string &out, std::string &error);
    bool ExpandWords(const std::vector<Word> &words, const ExpandContext &context,
                     std::vector<std::string> &out, std::string &error);
}

#endif // BML_SHELL_EXPANDER_H
