// Tokenizer of the console shell. It turns one line, possibly holding embedded
// newlines from continuation rows, into operator tokens and words, where a word
// is a run of typed parts: bare text with escapes resolved, '...' text, "..."
// with its own children, $'...' text, a $variable, or a $(substitution).
//
// Outside quotes a backslash escapes only a metacharacter (see IsMetachar); in
// front of any other character it is kept literally, so Windows paths such as
// C:\Maps\x.nmo survive unquoted. Inside "..." only \" \\ \$ \` and \! are
// escapes. A backslash right before a newline joins the two rows.
//
// Input that stops inside a quote or a substitution, or right after a line
// continuation, is reported as incomplete rather than as an error so the
// command bar can ask for another row.
#ifndef BML_SHELL_LEXER_H
#define BML_SHELL_LEXER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace BML::Shell {
    struct WordPart {
        enum class Kind {
            Literal,      // bare text, escapes already resolved
            SingleQuoted, // text between '...'
            DoubleQuoted, // children carry the content of "..."
            AnsiC,        // text of $'...' after escape processing
            Variable,     // text is the name, or "?" for $?
            Substitution, // text is the source between $( and ), or between backticks
        };

        Kind kind = Kind::Literal;
        std::size_t begin = 0; // source range, delimiters included
        std::size_t end = 0;
        std::string text;
        std::vector<WordPart> children; // DoubleQuoted only
        bool braced = false;            // Variable written as ${name}
        bool backtick = false;          // Substitution written with backticks
    };

    struct Token {
        enum class Kind {
            Word,
            Semicolon,
            AndAnd,
            OrOr,
            Pipe,
            Newline,
            Comment,
        };

        Kind kind = Kind::Word;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::vector<WordPart> parts; // Word only
    };

    struct LexResult {
        std::vector<Token> tokens;
        bool incomplete = false;
        bool error = false;
        std::size_t errorPos = 0;
        std::string message;

        bool Ok() const { return !incomplete && !error; }
    };

    LexResult Lex(std::string_view text);

    // The text a word stands for before any expansion: quotes removed, escapes
    // resolved, variables and substitutions written back as source. This is what
    // completion hands to ICommand::GetTabCompletion.
    std::string LiteralText(const std::vector<WordPart> &parts);

    // Source text for one part written back from its fields.
    std::string PartSource(const WordPart &part);

    const char *TokenKindName(Token::Kind kind) noexcept;
}

#endif // BML_SHELL_LEXER_H
