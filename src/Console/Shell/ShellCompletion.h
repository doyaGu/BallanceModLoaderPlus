// Completion support that understands the shell's syntax: which simple command
// the caret sits in, how its words read once quotes are removed, whether the
// caret follows a separator so an empty argument is being started, and how a
// chosen candidate has to be written back inside the surrounding quotes.
#ifndef BML_SHELL_COMPLETION_H
#define BML_SHELL_COMPLETION_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    class AliasResolver;

    // Where candidates come from. Every list is unfiltered; Build keeps the ones
    // that start with what has been typed, ignoring case.
    struct CompletionProviders {
        std::function<std::vector<std::string>()> commandNames;
        std::function<std::vector<std::string>()> aliasNames;
        std::function<std::vector<std::string>()> variableNames;
        // args as ICommand::GetTabCompletion expects them: args[0] the command,
        // an empty last element when a new argument is being started.
        std::function<std::vector<std::string>(const std::vector<std::string> &args)> argumentCandidates;
    };

    struct CompletionPlan {
        enum class Kind {
            None,
            Command,
            Argument,
            Variable,
        };

        Kind kind = Kind::None;
        std::size_t replaceBegin = 0; // start of the word being completed
        std::size_t replaceEnd = 0;   // the caret
        std::string prefix;           // what has been typed, quotes removed
        QuoteContext context = QuoteContext::Bare;
        bool followedByWhitespace = false;
        // Exact duplicates removed while preserving provider order.
        std::vector<std::string> candidates;
        // UTF-8 byte length shared by every candidate, compared without case.
        std::size_t commonPrefixLength = 0;
    };

    CompletionPlan BuildCompletion(std::string_view text, std::size_t cursor, const CompletionProviders &providers,
                                   const AliasResolver *aliases);

    // Source text that replaces [replaceBegin, replaceEnd) so that the word reads
    // as candidate: quoted for the plan's context, with the closing quote and a
    // trailing space only when final (a single candidate was accepted).
    std::string RenderReplacement(const CompletionPlan &plan, std::string_view candidate, bool final);
}

#endif // BML_SHELL_COMPLETION_H
