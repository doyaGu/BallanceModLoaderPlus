#include "Console/Shell/ShellCompletion.h"

#include <algorithm>
#include <unordered_set>

#include <utf8.h>

#include "Console/Shell/ShellEditing.h"
#include "Console/Shell/ShellQuoting.h"
#include "StringUtils.h"

namespace BML::Shell {
    namespace {
        std::size_t CommonPrefixLength(const std::string &first,
                                       const std::string &candidate) noexcept {
            const auto *firstBegin = reinterpret_cast<const utf8_int8_t *>(first.c_str());
            const auto *candidateBegin = reinterpret_cast<const utf8_int8_t *>(candidate.c_str());
            const utf8_int8_t *firstCursor = firstBegin;
            const utf8_int8_t *candidateCursor = candidateBegin;
            std::size_t matchedBytes = 0;

            while (*firstCursor != '\0' && *candidateCursor != '\0') {
                utf8_int32_t firstCodepoint = 0;
                utf8_int32_t candidateCodepoint = 0;
                const utf8_int8_t *firstNext = utf8codepoint(firstCursor, &firstCodepoint);
                const utf8_int8_t *candidateNext = utf8codepoint(candidateCursor, &candidateCodepoint);
                if (utf8lwrcodepoint(firstCodepoint) != utf8lwrcodepoint(candidateCodepoint))
                    break;

                firstCursor = firstNext;
                candidateCursor = candidateNext;
                matchedBytes = static_cast<std::size_t>(firstCursor - firstBegin);
            }

            return matchedBytes;
        }

        std::size_t CommonPrefixLength(const std::vector<std::string> &candidates) noexcept {
            if (candidates.empty() || !utils::IsValidUtf8(candidates.front()))
                return 0;

            std::size_t prefixLength = candidates.front().size();
            for (std::size_t index = 1; index < candidates.size(); ++index) {
                if (!utils::IsValidUtf8(candidates[index]))
                    return 0;
                prefixLength = std::min(prefixLength,
                                        CommonPrefixLength(candidates.front(), candidates[index]));
                if (prefixLength == 0)
                    break;
            }
            return prefixLength;
        }

        bool HasPrefixIgnoreCase(const std::string &candidate, const std::string &prefix) {
            if (prefix.empty())
                return true;
            if (candidate.size() < prefix.size())
                return false;
            return utf8ncasecmp(candidate.c_str(), prefix.c_str(), prefix.size()) == 0;
        }

        void AddFiltered(std::vector<std::string> &out, const std::vector<std::string> &names,
                         const std::string &prefix, std::unordered_set<std::string> &seen) {
            for (const std::string &name : names) {
                if (name.empty() || !HasPrefixIgnoreCase(name, prefix))
                    continue;
                if (seen.insert(name).second)
                    out.push_back(name);
            }
        }

        void AddCommandCandidates(CompletionPlan &plan, const CompletionProviders &providers) {
            plan.kind = CompletionPlan::Kind::Command;
            const std::vector<std::string> commands = providers.commandNames
                ? providers.commandNames()
                : std::vector<std::string>{};
            const std::vector<std::string> aliases = providers.aliasNames
                ? providers.aliasNames()
                : std::vector<std::string>{};
            std::unordered_set<std::string> seen;
            seen.reserve(commands.size() + aliases.size());
            AddFiltered(plan.candidates, commands, plan.prefix, seen);
            AddFiltered(plan.candidates, aliases, plan.prefix, seen);
        }
    }

    CompletionPlan BuildCompletion(std::string_view text, std::size_t cursor, const CompletionProviders &providers,
                                   const AliasResolver *aliases) {
        const CursorAnalysis analysis = AnalyzeCursor(text, cursor, aliases);
        CompletionPlan plan;
        plan.replaceBegin = analysis.replaceBegin;
        plan.replaceEnd = analysis.replaceEnd;
        plan.prefix = analysis.prefix;
        plan.context = analysis.context;
        plan.followedByWhitespace = analysis.followedByWhitespace;

        switch (analysis.target) {
            case CursorTarget::Command:
                AddCommandCandidates(plan, providers);
                break;
            case CursorTarget::Argument:
                plan.kind = CompletionPlan::Kind::Argument;
                if (providers.argumentCandidates) {
                    const std::vector<std::string> candidates = providers.argumentCandidates(analysis.arguments);
                    std::unordered_set<std::string> seen;
                    seen.reserve(candidates.size());
                    AddFiltered(plan.candidates, candidates, plan.prefix, seen);
                }
                break;
            case CursorTarget::Variable:
                plan.kind = CompletionPlan::Kind::Variable;
                if (providers.variableNames) {
                    std::vector<std::string> names = providers.variableNames();
                    names.push_back("?");
                    std::unordered_set<std::string> seen;
                    seen.reserve(names.size());
                    AddFiltered(plan.candidates, names, plan.prefix, seen);
                }
                break;
            case CursorTarget::None:
                break;
        }
        plan.commonPrefixLength = CommonPrefixLength(plan.candidates);
        return plan;
    }

    std::string RenderReplacement(const CompletionPlan &plan, std::string_view candidate, bool final) {
        std::string out;
        if (plan.kind == CompletionPlan::Kind::Variable) {
            out = "$";
            out.append(candidate.data(), candidate.size());
            if (final && !plan.followedByWhitespace && plan.context == QuoteContext::Bare)
                out.push_back(' ');
            return out;
        }

        switch (plan.context) {
            case QuoteContext::Bare:
                out = QuoteForContext(candidate, QuoteContext::Bare, false);
                break;
            case QuoteContext::Single:
                out = "'" + QuoteForContext(candidate, QuoteContext::Single, final);
                break;
            case QuoteContext::Double:
                out = "\"" + QuoteForContext(candidate, QuoteContext::Double, final);
                break;
            case QuoteContext::AnsiC:
                out = "$'" + QuoteForContext(candidate, QuoteContext::AnsiC, final);
                break;
        }
        if (final && !plan.followedByWhitespace)
            out.push_back(' ');
        return out;
    }
}
