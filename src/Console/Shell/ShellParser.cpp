#include "Console/Shell/ShellParser.h"

#include <set>
#include <utility>

#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    namespace {
        class Parser {
        public:
            Parser(LexResult lex, const AliasResolver *aliases) : m_Aliases(aliases) {
                m_Result.incomplete = lex.incomplete;
                m_Result.error = lex.error;
                m_Result.errorPos = lex.errorPos;
                m_Result.message = std::move(lex.message);
                m_Tokens.reserve(lex.tokens.size());
                for (Token &token : lex.tokens) {
                    if (token.kind != Token::Kind::Comment)
                        m_Tokens.push_back(std::move(token));
                }
            }

            ParseResult Run() {
                if (!m_Result.Ok())
                    return std::move(m_Result);
                ParseList(m_Result.list);
                if (!m_Result.Ok())
                    m_Result.list = List{};
                return std::move(m_Result);
            }

        private:
            bool Failed() const { return m_Result.error || m_Result.incomplete; }
            bool AtEnd() const { return m_Index >= m_Tokens.size(); }
            const Token *Cur() const { return AtEnd() ? nullptr : &m_Tokens[m_Index]; }

            void SetError(std::size_t pos, std::string message) {
                if (Failed())
                    return;
                m_Result.error = true;
                m_Result.errorPos = pos;
                m_Result.message = std::move(message);
            }

            void SetIncomplete(std::size_t pos, std::string message) {
                if (Failed())
                    return;
                m_Result.incomplete = true;
                m_Result.errorPos = pos;
                m_Result.message = std::move(message);
            }

            void SkipNewlines() {
                while (!AtEnd() && m_Tokens[m_Index].kind == Token::Kind::Newline)
                    ++m_Index;
            }

            static std::string Unexpected(const Token &token) {
                std::string message = "unexpected token '";
                message += TokenKindName(token.kind);
                message += "'";
                return message;
            }

            bool ParseList(List &list) {
                SkipNewlines();
                while (!AtEnd() && !Failed()) {
                    AndOr item;
                    if (!ParseAndOr(item))
                        return false;
                    list.items.push_back(std::move(item));
                    if (AtEnd())
                        break;

                    const Token &token = m_Tokens[m_Index];
                    if (token.kind == Token::Kind::Semicolon || token.kind == Token::Kind::Newline) {
                        ++m_Index;
                        SkipNewlines();
                        if (const Token *next = Cur(); next && next->kind == Token::Kind::Semicolon) {
                            SetError(next->begin, "unexpected token ';'");
                            return false;
                        }
                        continue;
                    }
                    SetError(token.begin, Unexpected(token));
                    return false;
                }
                return !Failed();
            }

            bool ParseAndOr(AndOr &andOr) {
                Pipeline first;
                if (!ParsePipeline(first))
                    return false;
                andOr.pipelines.push_back(std::move(first));

                while (const Token *token = Cur()) {
                    if (token->kind != Token::Kind::AndAnd && token->kind != Token::Kind::OrOr)
                        break;
                    const ListOp op = token->kind == Token::Kind::AndAnd ? ListOp::And : ListOp::Or;
                    const std::size_t opPos = token->begin;
                    ++m_Index;
                    SkipNewlines();
                    if (AtEnd()) {
                        SetIncomplete(opPos, op == ListOp::And ? "expected a command after '&&'"
                                                               : "expected a command after '||'");
                        return false;
                    }
                    Pipeline next;
                    if (!ParsePipeline(next))
                        return false;
                    andOr.ops.push_back(op);
                    andOr.pipelines.push_back(std::move(next));
                }
                return true;
            }

            bool ParsePipeline(Pipeline &pipeline) {
                SimpleCommand first;
                if (!ParseCommand(first))
                    return false;
                pipeline.commands.push_back(std::move(first));

                while (const Token *token = Cur()) {
                    if (token->kind != Token::Kind::Pipe)
                        break;
                    const std::size_t opPos = token->begin;
                    ++m_Index;
                    SkipNewlines();
                    if (AtEnd()) {
                        SetIncomplete(opPos, "expected a command after '|'");
                        return false;
                    }
                    if (pipeline.commands.size() >= Limits::MaxPipelineStages) {
                        SetError(opPos, "too many pipeline stages");
                        return false;
                    }
                    SimpleCommand next;
                    if (!ParseCommand(next))
                        return false;
                    pipeline.commands.push_back(std::move(next));
                }
                return true;
            }

            static Word WordFromToken(const Token &token) {
                Word word;
                word.parts = token.parts;
                word.begin = token.begin;
                word.end = token.end;
                return word;
            }

            bool ExpandAliases(SimpleCommand &command) {
                if (!m_Aliases)
                    return true;

                std::set<std::string> active;
                std::size_t depth = 0;
                while (const Token *token = Cur()) {
                    if (token->kind != Token::Kind::Word)
                        break;
                    const Word word = WordFromToken(*token);
                    if (!word.IsBareLiteral())
                        break;
                    const std::string name = word.parts[0].text;
                    if (name.empty() || active.count(name))
                        break;
                    std::string body;
                    if (!m_Aliases->LookupAlias(name, body))
                        break;
                    if (++depth > Limits::MaxAliasDepth) {
                        SetError(token->begin, "alias '" + name + "': nesting too deep");
                        return false;
                    }

                    LexResult lexed = Lex(body);
                    if (!lexed.Ok()) {
                        SetError(token->begin, "alias '" + name + "': " + lexed.message);
                        return false;
                    }

                    std::vector<Token> replacement;
                    replacement.reserve(lexed.tokens.size());
                    for (Token &bodyToken : lexed.tokens) {
                        if (bodyToken.kind == Token::Kind::Comment)
                            continue;
                        // Positions of spliced tokens point at the typed alias word.
                        bodyToken.begin = token->begin;
                        bodyToken.end = token->end;
                        for (WordPart &part : bodyToken.parts) {
                            part.begin = token->begin;
                            part.end = token->end;
                        }
                        replacement.push_back(std::move(bodyToken));
                    }

                    if (command.aliasName.empty())
                        command.aliasName = name;
                    active.insert(name);

                    const auto at = m_Tokens.begin() + static_cast<std::ptrdiff_t>(m_Index);
                    const auto after = m_Tokens.erase(at);
                    m_Tokens.insert(after, std::make_move_iterator(replacement.begin()),
                                    std::make_move_iterator(replacement.end()));
                }
                return true;
            }

            bool ParseCommand(SimpleCommand &command) {
                if (AtEnd()) {
                    SetIncomplete(0, "expected a command");
                    return false;
                }
                if (!ExpandAliases(command))
                    return false;

                const Token *token = Cur();
                if (!token) {
                    SetError(m_Tokens.empty() ? 0 : m_Tokens.back().end,
                             command.aliasName.empty() ? "expected a command"
                                                       : "alias '" + command.aliasName + "' expands to nothing");
                    return false;
                }
                if (token->kind != Token::Kind::Word) {
                    SetError(token->begin, Unexpected(*token));
                    return false;
                }

                if (++m_CommandCount > Limits::MaxCommandsPerLine) {
                    SetError(token->begin, "too many commands in one line");
                    return false;
                }

                command.begin = token->begin;
                while (const Token *word = Cur()) {
                    if (word->kind != Token::Kind::Word)
                        break;
                    command.words.push_back(WordFromToken(*word));
                    command.end = word->end;
                    ++m_Index;
                }
                return true;
            }

            std::vector<Token> m_Tokens;
            std::size_t m_Index = 0;
            std::size_t m_CommandCount = 0;
            const AliasResolver *m_Aliases = nullptr;
            ParseResult m_Result;
        };
    }

    ParseResult Parse(std::string_view text, const AliasResolver *aliases) {
        Parser parser(Lex(text), aliases);
        return parser.Run();
    }
}
