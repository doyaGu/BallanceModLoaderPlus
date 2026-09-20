// Grammar of the console shell over the lexer's tokens:
//
//   list     ::= and_or ( (';' | newline) and_or )* (';' | newline)?
//   and_or   ::= pipeline ( ('&&' | '||') pipeline )*
//   pipeline ::= command ( '|' command )*
//   command  ::= word+
//
// Newlines may follow '&&', '||', and '|'. The first word of a command is
// replaced by its alias body when the word is a single bare literal naming an
// alias; the body is lexed and spliced in, so it may hold operators. An alias
// is not expanded again inside its own expansion chain.
#ifndef BML_SHELL_PARSER_H
#define BML_SHELL_PARSER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Console/Shell/ShellLexer.h"

namespace BML::Shell {
    struct Word {
        std::vector<WordPart> parts;
        std::size_t begin = 0;
        std::size_t end = 0;

        // A word made of one unquoted literal, the only shape an alias name can take.
        bool IsBareLiteral() const {
            return parts.size() == 1 && parts[0].kind == WordPart::Kind::Literal;
        }

        std::string Literal() const { return LiteralText(parts); }
    };

    struct SimpleCommand {
        std::vector<Word> words;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::string aliasName; // the typed word an alias replaced, empty otherwise
    };

    struct Pipeline {
        std::vector<SimpleCommand> commands;
    };

    enum class ListOp {
        And,
        Or,
    };

    struct AndOr {
        std::vector<Pipeline> pipelines;
        std::vector<ListOp> ops; // ops[i] joins pipelines[i] and pipelines[i + 1]
    };

    struct List {
        std::vector<AndOr> items;

        bool Empty() const { return items.empty(); }
    };

    class AliasResolver {
    public:
        virtual ~AliasResolver() = default;
        virtual bool LookupAlias(std::string_view name, std::string &body) const = 0;
        virtual bool HasAlias(std::string_view name) const {
            std::string body;
            return LookupAlias(name, body);
        }
    };

    struct ParseResult {
        List list;
        bool incomplete = false;
        bool error = false;
        std::size_t errorPos = 0;
        std::string message;

        bool Ok() const { return !incomplete && !error; }
    };

    // Parses an existing lexer result. Editing analysis uses this overload to
    // derive syntax roles without tokenizing the same line a second time.
    ParseResult ParseLexed(LexResult lexed, const AliasResolver *aliases = nullptr);
    ParseResult Parse(std::string_view text, const AliasResolver *aliases = nullptr);
}

#endif // BML_SHELL_PARSER_H
