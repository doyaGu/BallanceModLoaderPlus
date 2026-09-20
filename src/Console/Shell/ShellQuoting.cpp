#include "Console/Shell/ShellQuoting.h"

namespace BML::Shell {
    bool IsWhitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\r';
    }

    bool IsMetachar(char c) noexcept {
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\'':
            case '"':
            case '$':
            case '|':
            case ';':
            case '&':
            case '#':
            case '\\':
            case '(':
            case ')':
            case '!':
            case '`':
                return true;
            default:
                return false;
        }
    }

    bool IsNameStart(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }

    bool IsNameChar(char c) noexcept {
        return IsNameStart(c) || (c >= '0' && c <= '9');
    }

    bool IsDoubleQuoteEscapable(char c) noexcept {
        return c == '"' || c == '\\' || c == '$' || c == '`' || c == '!';
    }

    std::string QuoteForContext(std::string_view value, QuoteContext ctx, bool closeQuote) {
        std::string out;
        out.reserve(value.size() + 8);
        switch (ctx) {
            case QuoteContext::Bare:
                if (value.empty())
                    return "''";
                for (std::size_t i = 0; i < value.size(); ++i) {
                    const char c = value[i];
                    if (c == '\n') {
                        // A bare newline cannot be escaped; fall back to quoting it.
                        out += "$'\\n'";
                        continue;
                    }
                    if (c == '\\') {
                        // The lexer keeps a backslash that precedes an ordinary
                        // character, so only one before a metacharacter, or a
                        // trailing one, needs doubling.
                        const bool keep = i + 1 < value.size() && !IsMetachar(value[i + 1]);
                        if (!keep)
                            out.push_back('\\');
                        out.push_back('\\');
                        continue;
                    }
                    if (IsMetachar(c))
                        out.push_back('\\');
                    out.push_back(c);
                }
                break;
            case QuoteContext::Single:
                for (char c : value) {
                    if (c == '\'')
                        out += "'\\''";
                    else
                        out.push_back(c);
                }
                if (closeQuote)
                    out.push_back('\'');
                break;
            case QuoteContext::Double:
                for (char c : value) {
                    if (IsDoubleQuoteEscapable(c))
                        out.push_back('\\');
                    out.push_back(c);
                }
                if (closeQuote)
                    out.push_back('"');
                break;
            case QuoteContext::AnsiC:
                for (char c : value) {
                    switch (c) {
                        case '\'': out += "\\'"; break;
                        case '\\': out += "\\\\"; break;
                        case '\n': out += "\\n"; break;
                        case '\t': out += "\\t"; break;
                        case '\r': out += "\\r"; break;
                        case '\x1b': out += "\\e"; break;
                        default: out.push_back(c); break;
                    }
                }
                if (closeQuote)
                    out.push_back('\'');
                break;
        }
        return out;
    }

    std::string SingleQuoted(std::string_view value) {
        std::string out = "'";
        out += QuoteForContext(value, QuoteContext::Single, true);
        return out;
    }
}
