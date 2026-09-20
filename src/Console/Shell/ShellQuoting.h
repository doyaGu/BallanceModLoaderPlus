// Character classes of the console shell and the inverse of the lexer: turning a
// plain value back into source text that lexes to exactly that value inside a
// given quoting context. Completion and the alias listing use this.
#ifndef BML_SHELL_QUOTING_H
#define BML_SHELL_QUOTING_H

#include <string>
#include <string_view>

#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    // Characters that end or alter a bare word: whitespace, both quotes, and
    // $ | ; & # \ ( ) ! `. A backslash outside quotes only escapes these.
    bool IsMetachar(char c) noexcept;

    // Horizontal whitespace that separates words: space, tab, carriage return.
    bool IsWhitespace(char c) noexcept;

    // Whether c may start, or continue, a variable name.
    bool IsNameStart(char c) noexcept;
    bool IsNameChar(char c) noexcept;

    // Inside double quotes only these characters take a backslash.
    bool IsDoubleQuoteEscapable(char c) noexcept;

    // Renders value so that inserting it at a caret sitting in ctx reproduces
    // value after quote removal. With closeQuote the closing delimiter is
    // appended for the quoted contexts. An empty bare value becomes ''.
    std::string QuoteForContext(std::string_view value, QuoteContext ctx, bool closeQuote);

    // value wrapped in single quotes, with embedded single quotes written as
    // '\''. This is how listings print alias bodies and variable values.
    std::string SingleQuoted(std::string_view value);
}

#endif // BML_SHELL_QUOTING_H
