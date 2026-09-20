#include "Console/Shell/ShellCompletion.h"

#include <algorithm>

#include <utf8.h>

#include "Console/Shell/ShellLexer.h"
#include "Console/Shell/ShellQuoting.h"

namespace BML::Shell {
    ArgumentSplit SplitArguments(std::string_view textUpToCaret) {
        ArgumentSplit split;
        const LexResult lexed = Lex(textUpToCaret);
        for (const Token &token : lexed.tokens) {
            switch (token.kind) {
                case Token::Kind::Word:
                    split.args.push_back(LiteralText(token.parts));
                    break;
                case Token::Kind::Comment:
                    break;
                default:
                    split.args.clear();
                    break;
            }
        }
        // Inside an unterminated quote the trailing whitespace belongs to the word.
        const bool insideWord = lexed.incomplete;
        split.trailingSeparator = !insideWord && !textUpToCaret.empty() &&
                                  (IsWhitespace(textUpToCaret.back()) || textUpToCaret.back() == '\n');
        return split;
    }

    namespace {
        bool HasPrefixIgnoreCase(const std::string &candidate, const std::string &prefix) {
            if (prefix.empty())
                return true;
            if (candidate.size() < prefix.size())
                return false;
            return utf8ncasecmp(candidate.c_str(), prefix.c_str(), prefix.size()) == 0;
        }

        void AddFiltered(std::vector<std::string> &out, const std::vector<std::string> &names,
                         const std::string &prefix) {
            for (const std::string &name : names) {
                if (name.empty() || !HasPrefixIgnoreCase(name, prefix))
                    continue;
                if (std::find(out.begin(), out.end(), name) == out.end())
                    out.push_back(name);
            }
        }

        QuoteContext ContextOfPart(const WordPart &part) {
            switch (part.kind) {
                case WordPart::Kind::SingleQuoted: return QuoteContext::Single;
                case WordPart::Kind::DoubleQuoted: return QuoteContext::Double;
                case WordPart::Kind::AnsiC: return QuoteContext::AnsiC;
                default: return QuoteContext::Bare;
            }
        }

        // The words of an alias body that form the command it expands to, so that
        // argument completion asks the right command.
        std::vector<std::string> AliasWords(const std::string &body) {
            std::vector<std::string> words;
            const LexResult lexed = Lex(body);
            if (!lexed.Ok())
                return words;
            for (const Token &token : lexed.tokens) {
                if (token.kind == Token::Kind::Word)
                    words.push_back(LiteralText(token.parts));
                else if (token.kind != Token::Kind::Comment)
                    words.clear();
            }
            return words;
        }

        CompletionPlan BuildInner(std::string_view text, std::size_t cursor, std::size_t offset,
                                  const CompletionProviders &providers, const AliasResolver *aliases);

        // Completion inside an unterminated $( ... ) or `...` works on the inner
        // text alone, with positions shifted back into the full line.
        bool TryNestedSubstitution(std::string_view textUpToCaret, std::size_t offset, const LexResult &lexed,
                                   const CompletionProviders &providers, const AliasResolver *aliases,
                                   CompletionPlan &plan) {
            if (!lexed.incomplete)
                return false;
            std::size_t innerStart = std::string_view::npos;
            if (lexed.message == "unterminated $(") {
                const std::size_t at = textUpToCaret.rfind("$(");
                if (at != std::string_view::npos)
                    innerStart = at + 2;
            } else if (lexed.message == "unterminated backtick") {
                const std::size_t at = textUpToCaret.rfind('`');
                if (at != std::string_view::npos)
                    innerStart = at + 1;
            }
            if (innerStart == std::string_view::npos)
                return false;
            plan = BuildInner(textUpToCaret.substr(innerStart), textUpToCaret.size() - innerStart,
                              offset + innerStart, providers, aliases);
            return true;
        }

        CompletionPlan BuildInner(std::string_view text, std::size_t cursor, std::size_t offset,
                                  const CompletionProviders &providers, const AliasResolver *aliases) {
            CompletionPlan plan;
            cursor = std::min(cursor, text.size());
            const std::string_view upToCaret = text.substr(0, cursor);
            const LexResult lexed = Lex(upToCaret);
            if (TryNestedSubstitution(upToCaret, offset, lexed, providers, aliases, plan))
                return plan;

            // Words of the current simple command, and the word under the caret.
            std::vector<const Token *> commandWords;
            const Token *current = nullptr;
            for (const Token &token : lexed.tokens) {
                switch (token.kind) {
                    case Token::Kind::Word:
                        commandWords.push_back(&token);
                        break;
                    case Token::Kind::Comment:
                        break;
                    default:
                        commandWords.clear();
                        break;
                }
            }
            if (!commandWords.empty() && commandWords.back()->end == cursor) {
                current = commandWords.back();
                commandWords.pop_back();
            }
            if (!lexed.tokens.empty() && lexed.tokens.back().kind == Token::Kind::Comment)
                return plan;

            plan.replaceBegin = offset + (current ? current->begin : cursor);
            plan.replaceEnd = offset + cursor;
            plan.followedByWhitespace = cursor < text.size() && (IsWhitespace(text[cursor]) || text[cursor] == '\n');

            if (current) {
                const WordPart &last = current->parts.back();
                plan.context = ContextOfPart(last);
                // A closed quote right before the caret means the word is complete and
                // anything appended is bare again.
                if (plan.context != QuoteContext::Bare && last.end == cursor) {
                    const std::string_view source = text.substr(last.begin, last.end - last.begin);
                    const bool closed = source.size() >= 2 && source.back() == '\'' &&
                                        (last.kind != WordPart::Kind::DoubleQuoted);
                    const bool closedDouble = last.kind == WordPart::Kind::DoubleQuoted &&
                                              source.size() >= 2 && source.back() == '"' && !lexed.incomplete;
                    if ((closed && !lexed.incomplete) || closedDouble)
                        plan.context = QuoteContext::Bare;
                }

                // $NAME under the caret, bare or inside double quotes.
                const WordPart *variable = nullptr;
                if (last.kind == WordPart::Kind::Variable && !last.braced && last.end == cursor) {
                    variable = &last;
                } else if (last.kind == WordPart::Kind::DoubleQuoted && !last.children.empty()) {
                    const WordPart &child = last.children.back();
                    if (child.kind == WordPart::Kind::Variable && !child.braced && child.end == cursor)
                        variable = &child;
                }
                if (variable) {
                    plan.kind = CompletionPlan::Kind::Variable;
                    plan.replaceBegin = offset + variable->begin;
                    plan.prefix = variable->text;
                    if (providers.variableNames) {
                        std::vector<std::string> names = providers.variableNames();
                        names.push_back("?");
                        AddFiltered(plan.candidates, names, plan.prefix);
                    }
                    return plan;
                }
                plan.prefix = LiteralText(current->parts);
            }

            if (commandWords.empty()) {
                plan.kind = CompletionPlan::Kind::Command;
                if (providers.commandNames)
                    AddFiltered(plan.candidates, providers.commandNames(), plan.prefix);
                if (providers.aliasNames)
                    AddFiltered(plan.candidates, providers.aliasNames(), plan.prefix);
                return plan;
            }

            plan.kind = CompletionPlan::Kind::Argument;
            std::vector<std::string> args;
            args.reserve(commandWords.size() + 1);
            for (const Token *word : commandWords)
                args.push_back(LiteralText(word->parts));
            if (aliases && commandWords[0]->parts.size() == 1 &&
                commandWords[0]->parts[0].kind == WordPart::Kind::Literal) {
                std::string body;
                if (aliases->LookupAlias(args[0], body)) {
                    std::vector<std::string> expanded = AliasWords(body);
                    if (!expanded.empty()) {
                        expanded.insert(expanded.end(), args.begin() + 1, args.end());
                        args = std::move(expanded);
                    }
                }
            }
            args.push_back(plan.prefix);
            if (providers.argumentCandidates)
                AddFiltered(plan.candidates, providers.argumentCandidates(args), plan.prefix);
            return plan;
        }
    }

    CompletionPlan BuildCompletion(std::string_view text, std::size_t cursor, const CompletionProviders &providers,
                                   const AliasResolver *aliases) {
        return BuildInner(text, cursor, 0, providers, aliases);
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
