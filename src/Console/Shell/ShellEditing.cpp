#include "Console/Shell/ShellEditing.h"

#include <algorithm>
#include <utility>

#include "Console/Shell/ShellLexer.h"
#include "Console/Shell/ShellParser.h"
#include "Console/Shell/ShellQuoting.h"

namespace BML::Shell {
    namespace {
        QuoteContext ContextOfPart(const WordPart &part) {
            switch (part.kind) {
                case WordPart::Kind::SingleQuoted: return QuoteContext::Single;
                case WordPart::Kind::DoubleQuoted: return QuoteContext::Double;
                case WordPart::Kind::AnsiC: return QuoteContext::AnsiC;
                default: return QuoteContext::Bare;
            }
        }

        bool IsBareAlias(const Token &token, const AliasResolver *aliases) {
            return aliases && token.parts.size() == 1 &&
                token.parts[0].kind == WordPart::Kind::Literal &&
                aliases->HasAlias(token.parts[0].text);
        }

        const SimpleCommand *LastCommand(const ParseResult &parsed) {
            if (!parsed.Ok() || parsed.list.items.empty())
                return nullptr;
            const AndOr &item = parsed.list.items.back();
            if (item.pipelines.empty() || item.pipelines.back().commands.empty())
                return nullptr;
            return &item.pipelines.back().commands.back();
        }

        bool AnalyzeAliasProbe(std::string_view text, std::size_t cursor,
                               const std::vector<const Token *> &commandWords,
                               const Token *current, const AliasResolver &aliases,
                               CursorAnalysis &analysis) {
            if (commandWords.empty())
                return false;
            const Token &head = *commandWords.front();
            if (!IsBareAlias(head, &aliases)) {
                return false;
            }

            const std::size_t probeEnd = current ? current->begin : cursor;
            if (head.begin > probeEnd || probeEnd > text.size())
                return false;
            std::string probe(text.substr(head.begin, probeEnd - head.begin));
            probe += SingleQuoted(analysis.prefix);

            const ParseResult parsed = Parse(probe, &aliases);
            const SimpleCommand *command = LastCommand(parsed);
            if (!command)
                return false;
            if (command->words.size() == 1) {
                analysis.target = CursorTarget::Command;
                return true;
            }

            analysis.arguments.reserve(command->words.size());
            for (const Word &word : command->words)
                analysis.arguments.push_back(word.Literal());
            return true;
        }

        CursorAnalysis AnalyzeCursorInner(std::string_view text, std::size_t cursor,
                                          std::size_t offset, const AliasResolver *aliases) {
            CursorAnalysis analysis;
            cursor = std::min(cursor, text.size());
            const std::string_view upToCaret = text.substr(0, cursor);
            const LexResult lexed = Lex(upToCaret);

            if (lexed.incomplete && lexed.activeCommandBegin != std::string_view::npos &&
                lexed.activeCommandBegin <= upToCaret.size()) {
                const std::size_t innerStart = lexed.activeCommandBegin;
                return AnalyzeCursorInner(text.substr(innerStart), cursor - innerStart,
                                          offset + innerStart, aliases);
            }

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
                return analysis;

            analysis.replaceBegin = offset + (current ? current->begin : cursor);
            analysis.replaceEnd = offset + cursor;
            analysis.followedByWhitespace = cursor < text.size() &&
                (IsWhitespace(text[cursor]) || text[cursor] == '\n');

            if (current) {
                const WordPart &last = current->parts.back();
                analysis.context = ContextOfPart(last);
                if (analysis.context != QuoteContext::Bare && last.end == cursor) {
                    const std::string_view source = text.substr(last.begin, last.end - last.begin);
                    const bool closed = source.size() >= 2 && source.back() == '\'' &&
                        last.kind != WordPart::Kind::DoubleQuoted;
                    const bool closedDouble = last.kind == WordPart::Kind::DoubleQuoted &&
                        source.size() >= 2 && source.back() == '"' && !lexed.incomplete;
                    if ((closed && !lexed.incomplete) || closedDouble)
                        analysis.context = QuoteContext::Bare;
                }

                const WordPart *variable = nullptr;
                if (last.kind == WordPart::Kind::Variable && !last.braced && last.end == cursor) {
                    variable = &last;
                } else if (last.kind == WordPart::Kind::DoubleQuoted && !last.children.empty()) {
                    const WordPart &child = last.children.back();
                    if (child.kind == WordPart::Kind::Variable && !child.braced && child.end == cursor)
                        variable = &child;
                }
                if (variable) {
                    analysis.target = CursorTarget::Variable;
                    analysis.replaceBegin = offset + variable->begin;
                    analysis.prefix = variable->text;
                    return analysis;
                }
                analysis.prefix = LiteralText(current->parts);
            }

            if (commandWords.empty()) {
                analysis.target = CursorTarget::Command;
                return analysis;
            }

            analysis.target = CursorTarget::Argument;
            if (aliases && AnalyzeAliasProbe(text, cursor, commandWords, current, *aliases, analysis))
                return analysis;

            analysis.arguments.reserve(commandWords.size() + 1);
            for (const Token *word : commandWords)
                analysis.arguments.push_back(LiteralText(word->parts));
            analysis.arguments.push_back(analysis.prefix);
            return analysis;
        }

        void AddPiece(std::vector<SourcePiece> &pieces, SourceKind kind,
                      std::size_t begin, std::size_t end) {
            if (end <= begin)
                return;
            if (!pieces.empty() && pieces.back().kind == kind && pieces.back().source.end == begin) {
                pieces.back().source.end = end;
                return;
            }
            pieces.push_back(SourcePiece{SourceRange{begin, end}, kind});
        }

        void AddPartPieces(std::vector<SourcePiece> &pieces, const WordPart &part) {
            switch (part.kind) {
                case WordPart::Kind::Literal:
                    AddPiece(pieces, SourceKind::Literal, part.begin, part.end);
                    break;
                case WordPart::Kind::SingleQuoted:
                case WordPart::Kind::AnsiC:
                    AddPiece(pieces, SourceKind::String, part.begin, part.end);
                    break;
                case WordPart::Kind::Variable:
                case WordPart::Kind::Substitution:
                    AddPiece(pieces, SourceKind::Expansion, part.begin, part.end);
                    break;
                case WordPart::Kind::DoubleQuoted: {
                    std::size_t cursor = part.begin;
                    for (const WordPart &child : part.children) {
                        if (child.kind == WordPart::Kind::Literal)
                            continue;
                        AddPiece(pieces, SourceKind::String, cursor, child.begin);
                        AddPiece(pieces, SourceKind::Expansion, child.begin, child.end);
                        cursor = child.end;
                    }
                    AddPiece(pieces, SourceKind::String, cursor, part.end);
                    break;
                }
            }
        }

        struct TypedWord {
            SourceRange source;
            std::string literal;
            bool namesAlias = false;
        };

        void AddCommandHead(LineAnalysis &analysis, const std::vector<TypedWord> &words,
                            SourceRange source) {
            const auto word = std::find_if(words.begin(), words.end(), [&](const TypedWord &candidate) {
                return candidate.source == source;
            });
            if (word == words.end())
                return;
            const auto existing = std::find_if(
                analysis.commandHeads.begin(), analysis.commandHeads.end(), [&](const CommandHead &head) {
                    return head.source == source;
                });
            if (existing == analysis.commandHeads.end())
                analysis.commandHeads.push_back(CommandHead{source, word->literal, word->namesAlias});
        }
    }

    CursorAnalysis AnalyzeCursor(std::string_view text, std::size_t cursor,
                                 const AliasResolver *aliases) {
        return AnalyzeCursorInner(text, cursor, 0, aliases);
    }

    LineAnalysis AnalyzeLine(std::string_view text, const AliasResolver *aliases) {
        LineAnalysis analysis;
        LexResult lexed = Lex(text);
        if (lexed.error)
            analysis.errorBegin = std::min(lexed.errorPos, text.size());
        const bool incomplete = lexed.incomplete;

        std::vector<TypedWord> words;
        words.reserve(lexed.tokens.size());
        bool commandPosition = true;
        for (const Token &token : lexed.tokens) {
            switch (token.kind) {
                case Token::Kind::Word: {
                    const TypedWord word{
                        SourceRange{token.begin, token.end},
                        LiteralText(token.parts),
                        IsBareAlias(token, aliases),
                    };
                    words.push_back(word);
                    if (commandPosition)
                        AddCommandHead(analysis, words, word.source);
                    for (const WordPart &part : token.parts)
                        AddPartPieces(analysis.pieces, part);
                    commandPosition = false;
                    break;
                }
                case Token::Kind::Comment:
                    AddPiece(analysis.pieces, SourceKind::Comment, token.begin, token.end);
                    break;
                default:
                    AddPiece(analysis.pieces, SourceKind::Operator, token.begin, token.end);
                    commandPosition = true;
                    break;
            }
        }

        if (aliases) {
            const ParseResult parsed = ParseLexed(std::move(lexed), aliases);
            if (parsed.Ok()) {
                for (const AndOr &item : parsed.list.items) {
                    for (const Pipeline &pipeline : item.pipelines) {
                        for (const SimpleCommand &command : pipeline.commands) {
                            if (!command.words.empty()) {
                                AddCommandHead(analysis, words, SourceRange{
                                    command.words.front().begin,
                                    command.words.front().end,
                                });
                            }
                        }
                    }
                }
            } else if (incomplete) {
                // A partial quoted word can make strict parsing fail even when
                // an alias injected an operator before it. Cursor analysis
                // repairs that final word and preserves the semantic role.
                const CursorAnalysis cursor = AnalyzeCursor(text, text.size(), aliases);
                if (cursor.target == CursorTarget::Command) {
                    AddCommandHead(analysis, words,
                                   SourceRange{cursor.replaceBegin, cursor.replaceEnd});
                }
            }
        }

        std::sort(analysis.commandHeads.begin(), analysis.commandHeads.end(),
                  [](const CommandHead &left, const CommandHead &right) {
            return left.source.begin < right.source.begin ||
                (left.source.begin == right.source.begin && left.source.end < right.source.end);
        });
        return analysis;
    }
}
