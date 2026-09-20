#include "Console/Shell/ShellLexer.h"

#include "Console/Shell/ShellQuoting.h"
#include "StringUtils.h"

namespace BML::Shell {
    namespace {
        class Lexer {
        public:
            explicit Lexer(std::string_view text) : m_Text(text) {}

            LexResult Run() {
                LexResult result;
                std::vector<Token> tokens;
                if (LexSequence(tokens, false) && m_Pos < m_Text.size()) {
                    // Only a stray ')' can stop the top-level sequence early.
                    SetError(m_Pos, "unexpected ')'");
                }
                result.tokens = std::move(tokens);
                result.incomplete = m_Incomplete;
                result.error = m_Error;
                result.errorPos = m_ErrorPos;
                result.message = m_Message;
                return result;
            }

        private:
            bool Failed() const { return m_Error || m_Incomplete; }

            void SetError(std::size_t pos, std::string message) {
                if (Failed())
                    return;
                m_Error = true;
                m_ErrorPos = pos;
                m_Message = std::move(message);
            }

            void SetIncomplete(std::string message) {
                if (Failed())
                    return;
                m_Incomplete = true;
                m_ErrorPos = m_Text.size();
                m_Message = std::move(message);
            }

            char Peek(std::size_t offset = 0) const {
                const std::size_t index = m_Pos + offset;
                return index < m_Text.size() ? m_Text[index] : '\0';
            }

            bool AtEnd() const { return m_Pos >= m_Text.size(); }

            // Lexes operators and words until the end of the input or, inside a
            // substitution, until the ')' that closes it (left unconsumed).
            bool LexSequence(std::vector<Token> &tokens, bool inSubstitution) {
                while (!AtEnd() && !Failed()) {
                    const char c = m_Text[m_Pos];
                    if (IsWhitespace(c)) {
                        ++m_Pos;
                        continue;
                    }

                    Token token;
                    token.begin = m_Pos;
                    switch (c) {
                        case '\n':
                            token.kind = Token::Kind::Newline;
                            ++m_Pos;
                            break;
                        case '#':
                            token.kind = Token::Kind::Comment;
                            while (!AtEnd() && m_Text[m_Pos] != '\n')
                                ++m_Pos;
                            break;
                        case ';':
                            token.kind = Token::Kind::Semicolon;
                            ++m_Pos;
                            break;
                        case '|':
                            if (Peek(1) == '|') {
                                token.kind = Token::Kind::OrOr;
                                m_Pos += 2;
                            } else {
                                token.kind = Token::Kind::Pipe;
                                ++m_Pos;
                            }
                            break;
                        case '&':
                            if (Peek(1) == '&') {
                                token.kind = Token::Kind::AndAnd;
                                m_Pos += 2;
                            } else {
                                SetError(m_Pos, "unexpected '&'");
                                return false;
                            }
                            break;
                        case ')':
                            if (inSubstitution)
                                return true;
                            SetError(m_Pos, "unexpected ')'");
                            return false;
                        case '(':
                            SetError(m_Pos, "unexpected '('");
                            return false;
                        default:
                            token.kind = Token::Kind::Word;
                            if (!LexWord(token, inSubstitution)) {
                                // Keep the partial word of incomplete input so
                                // completion can still see what is being typed.
                                if (m_Incomplete && !token.parts.empty()) {
                                    token.end = m_Pos;
                                    tokens.push_back(std::move(token));
                                }
                                return false;
                            }
                            break;
                    }
                    token.end = m_Pos;
                    tokens.push_back(std::move(token));
                }
                return !Failed();
            }

            static bool EndsBareWord(char c, bool inSubstitution) {
                switch (c) {
                    case ' ':
                    case '\t':
                    case '\r':
                    case '\n':
                    case ';':
                    case '|':
                    case '&':
                    case '(':
                        return true;
                    case ')':
                        return inSubstitution;
                    default:
                        return false;
                }
            }

            void FlushLiteral(Token &token, std::string &literal, std::size_t &literalBegin) {
                if (literal.empty() && literalBegin == m_Pos)
                    return;
                WordPart part;
                part.kind = WordPart::Kind::Literal;
                part.begin = literalBegin;
                part.end = m_Pos;
                part.text = std::move(literal);
                literal.clear();
                token.parts.push_back(std::move(part));
                literalBegin = m_Pos;
            }

            bool LexWord(Token &token, bool inSubstitution) {
                std::string literal;
                std::size_t literalBegin = m_Pos;
                bool sawContent = false;

                while (!AtEnd() && !Failed()) {
                    const char c = m_Text[m_Pos];
                    if (EndsBareWord(c, inSubstitution))
                        break;
                    if (c == ')') {
                        SetError(m_Pos, "unexpected ')'");
                        return false;
                    }

                    switch (c) {
                        case '\\': {
                            if (m_Pos + 1 >= m_Text.size()) {
                                FlushLiteral(token, literal, literalBegin);
                                SetIncomplete("line continuation");
                                return false;
                            }
                            const char next = m_Text[m_Pos + 1];
                            if (next == '\n') {
                                m_Pos += 2;
                                continue;
                            }
                            if (!IsMetachar(next))
                                literal.push_back('\\');
                            literal.push_back(next);
                            m_Pos += 2;
                            sawContent = true;
                            break;
                        }
                        case '\'': {
                            FlushLiteral(token, literal, literalBegin);
                            WordPart part;
                            const bool ok = LexSingle(part);
                            token.parts.push_back(std::move(part));
                            if (!ok)
                                return false;
                            literalBegin = m_Pos;
                            sawContent = true;
                            break;
                        }
                        case '"': {
                            FlushLiteral(token, literal, literalBegin);
                            WordPart part;
                            const bool ok = LexDouble(part);
                            token.parts.push_back(std::move(part));
                            if (!ok)
                                return false;
                            literalBegin = m_Pos;
                            sawContent = true;
                            break;
                        }
                        case '$': {
                            if (Peek(1) == '\'') {
                                FlushLiteral(token, literal, literalBegin);
                                WordPart part;
                                const bool ok = LexAnsiC(part);
                                token.parts.push_back(std::move(part));
                                if (!ok)
                                    return false;
                                literalBegin = m_Pos;
                                sawContent = true;
                                break;
                            }
                            WordPart part;
                            const std::size_t before = m_Pos;
                            if (!LexDollar(part))
                                return false;
                            if (part.kind == WordPart::Kind::Literal) {
                                literal.push_back('$');
                            } else {
                                m_Pos = before;
                                FlushLiteral(token, literal, literalBegin);
                                m_Pos = part.end;
                                token.parts.push_back(std::move(part));
                                literalBegin = m_Pos;
                            }
                            sawContent = true;
                            break;
                        }
                        case '`': {
                            FlushLiteral(token, literal, literalBegin);
                            WordPart part;
                            if (!LexBacktick(part))
                                return false;
                            token.parts.push_back(std::move(part));
                            literalBegin = m_Pos;
                            sawContent = true;
                            break;
                        }
                        default:
                            literal.push_back(c);
                            ++m_Pos;
                            sawContent = true;
                            break;
                    }
                }

                if (Failed())
                    return false;
                if (!literal.empty() || (!sawContent && token.parts.empty()))
                    FlushLiteral(token, literal, literalBegin);
                if (token.parts.empty()) {
                    // Only line continuations were consumed: not a word.
                    WordPart part;
                    part.kind = WordPart::Kind::Literal;
                    part.begin = token.begin;
                    part.end = m_Pos;
                    token.parts.push_back(std::move(part));
                }
                return true;
            }

            bool LexSingle(WordPart &part) {
                part.kind = WordPart::Kind::SingleQuoted;
                part.begin = m_Pos;
                ++m_Pos; // opening quote
                const std::size_t contentBegin = m_Pos;
                while (!AtEnd() && m_Text[m_Pos] != '\'')
                    ++m_Pos;
                if (AtEnd()) {
                    part.text.assign(m_Text.substr(contentBegin));
                    part.end = m_Pos;
                    SetIncomplete("unterminated single quote");
                    return false;
                }
                part.text.assign(m_Text.substr(contentBegin, m_Pos - contentBegin));
                ++m_Pos; // closing quote
                part.end = m_Pos;
                return true;
            }

            bool LexAnsiC(WordPart &part) {
                part.kind = WordPart::Kind::AnsiC;
                part.begin = m_Pos;
                m_Pos += 2; // $'
                const std::size_t contentBegin = m_Pos;
                while (!AtEnd()) {
                    const char c = m_Text[m_Pos];
                    if (c == '\\') {
                        if (m_Pos + 1 >= m_Text.size()) {
                            SetIncomplete("unterminated $'...'");
                            return false;
                        }
                        m_Pos += 2;
                        continue;
                    }
                    if (c == '\'')
                        break;
                    ++m_Pos;
                }
                if (AtEnd()) {
                    SetIncomplete("unterminated $'...'");
                    return false;
                }
                const std::string raw(m_Text.substr(contentBegin, m_Pos - contentBegin));
                part.text = utils::UnescapeString(raw.c_str());
                ++m_Pos; // closing quote
                part.end = m_Pos;
                return true;
            }

            bool LexDouble(WordPart &part) {
                part.kind = WordPart::Kind::DoubleQuoted;
                part.begin = m_Pos;
                ++m_Pos; // opening quote

                std::string literal;
                std::size_t literalBegin = m_Pos;
                auto flush = [&]() {
                    if (literal.empty())
                        return;
                    WordPart child;
                    child.kind = WordPart::Kind::Literal;
                    child.begin = literalBegin;
                    child.end = m_Pos;
                    child.text = std::move(literal);
                    literal.clear();
                    part.children.push_back(std::move(child));
                };

                while (!AtEnd() && !Failed()) {
                    const char c = m_Text[m_Pos];
                    if (c == '"') {
                        flush();
                        ++m_Pos;
                        part.end = m_Pos;
                        return true;
                    }
                    if (c == '\\') {
                        if (m_Pos + 1 >= m_Text.size()) {
                            flush();
                            part.end = m_Pos;
                            SetIncomplete("unterminated double quote");
                            return false;
                        }
                        const char next = m_Text[m_Pos + 1];
                        if (next == '\n') {
                            m_Pos += 2;
                            continue;
                        }
                        if (!IsDoubleQuoteEscapable(next))
                            literal.push_back('\\');
                        literal.push_back(next);
                        m_Pos += 2;
                        continue;
                    }
                    if (c == '$') {
                        WordPart child;
                        const std::size_t before = m_Pos;
                        if (!LexDollar(child))
                            return false;
                        if (child.kind == WordPart::Kind::Literal) {
                            literal.push_back('$');
                            m_Pos = before + 1;
                        } else {
                            m_Pos = before;
                            flush();
                            m_Pos = child.end;
                            part.children.push_back(std::move(child));
                            literalBegin = m_Pos;
                        }
                        continue;
                    }
                    if (c == '`') {
                        flush();
                        WordPart child;
                        if (!LexBacktick(child))
                            return false;
                        part.children.push_back(std::move(child));
                        literalBegin = m_Pos;
                        continue;
                    }
                    literal.push_back(c);
                    ++m_Pos;
                }
                flush();
                part.end = m_Pos;
                if (!Failed())
                    SetIncomplete("unterminated double quote");
                return false;
            }

            // At '$'. Produces a Variable or Substitution part, or a Literal part
            // (with m_Pos advanced past the '$') when the '$' stands for itself.
            bool LexDollar(WordPart &part) {
                part.begin = m_Pos;
                const char next = Peek(1);
                if (next == '(') {
                    part.kind = WordPart::Kind::Substitution;
                    m_Pos += 2;
                    const std::size_t contentBegin = m_Pos;
                    std::vector<Token> inner;
                    if (!LexSequence(inner, true))
                        return false;
                    if (AtEnd()) {
                        SetIncomplete("unterminated $(");
                        return false;
                    }
                    part.text.assign(m_Text.substr(contentBegin, m_Pos - contentBegin));
                    ++m_Pos; // ')'
                    part.end = m_Pos;
                    return true;
                }
                if (next == '{') {
                    part.kind = WordPart::Kind::Variable;
                    part.braced = true;
                    m_Pos += 2;
                    const std::size_t nameBegin = m_Pos;
                    if (Peek() == '?') {
                        ++m_Pos;
                    } else {
                        while (!AtEnd() && IsNameChar(m_Text[m_Pos]))
                            ++m_Pos;
                    }
                    if (AtEnd()) {
                        SetIncomplete("unterminated ${");
                        return false;
                    }
                    if (m_Pos == nameBegin || m_Text[m_Pos] != '}' ||
                        (m_Text[nameBegin] != '?' && !IsNameStart(m_Text[nameBegin]))) {
                        SetError(part.begin, "bad substitution");
                        return false;
                    }
                    part.text.assign(m_Text.substr(nameBegin, m_Pos - nameBegin));
                    ++m_Pos; // '}'
                    part.end = m_Pos;
                    return true;
                }
                if (next == '?') {
                    part.kind = WordPart::Kind::Variable;
                    part.text = "?";
                    m_Pos += 2;
                    part.end = m_Pos;
                    return true;
                }
                if (IsNameStart(next)) {
                    part.kind = WordPart::Kind::Variable;
                    ++m_Pos;
                    const std::size_t nameBegin = m_Pos;
                    while (!AtEnd() && IsNameChar(m_Text[m_Pos]))
                        ++m_Pos;
                    part.text.assign(m_Text.substr(nameBegin, m_Pos - nameBegin));
                    part.end = m_Pos;
                    return true;
                }
                part.kind = WordPart::Kind::Literal;
                ++m_Pos;
                part.end = m_Pos;
                return true;
            }

            bool LexBacktick(WordPart &part) {
                part.kind = WordPart::Kind::Substitution;
                part.backtick = true;
                part.begin = m_Pos;
                ++m_Pos;
                const std::size_t contentBegin = m_Pos;
                while (!AtEnd() && m_Text[m_Pos] != '`')
                    ++m_Pos;
                if (AtEnd()) {
                    SetIncomplete("unterminated backtick");
                    return false;
                }
                part.text.assign(m_Text.substr(contentBegin, m_Pos - contentBegin));
                ++m_Pos;
                part.end = m_Pos;
                return true;
            }

            std::string_view m_Text;
            std::size_t m_Pos = 0;
            bool m_Incomplete = false;
            bool m_Error = false;
            std::size_t m_ErrorPos = 0;
            std::string m_Message;
        };
    }

    LexResult Lex(std::string_view text) {
        Lexer lexer(text);
        return lexer.Run();
    }

    std::string PartSource(const WordPart &part) {
        switch (part.kind) {
            case WordPart::Kind::Literal:
                return QuoteForContext(part.text, QuoteContext::Bare, false);
            case WordPart::Kind::SingleQuoted:
                return "'" + part.text + "'";
            case WordPart::Kind::AnsiC:
                return "$'" + QuoteForContext(part.text, QuoteContext::AnsiC, true);
            case WordPart::Kind::DoubleQuoted: {
                std::string out = "\"";
                for (const WordPart &child : part.children) {
                    if (child.kind == WordPart::Kind::Literal)
                        out += QuoteForContext(child.text, QuoteContext::Double, false);
                    else
                        out += PartSource(child);
                }
                out.push_back('"');
                return out;
            }
            case WordPart::Kind::Variable:
                return part.braced ? "${" + part.text + "}" : "$" + part.text;
            case WordPart::Kind::Substitution:
                return part.backtick ? "`" + part.text + "`" : "$(" + part.text + ")";
        }
        return {};
    }

    std::string LiteralText(const std::vector<WordPart> &parts) {
        std::string out;
        for (const WordPart &part : parts) {
            switch (part.kind) {
                case WordPart::Kind::Literal:
                case WordPart::Kind::SingleQuoted:
                case WordPart::Kind::AnsiC:
                    out += part.text;
                    break;
                case WordPart::Kind::DoubleQuoted:
                    for (const WordPart &child : part.children) {
                        if (child.kind == WordPart::Kind::Literal)
                            out += child.text;
                        else
                            out += PartSource(child);
                    }
                    break;
                case WordPart::Kind::Variable:
                case WordPart::Kind::Substitution:
                    out += PartSource(part);
                    break;
            }
        }
        return out;
    }

    const char *TokenKindName(Token::Kind kind) noexcept {
        switch (kind) {
            case Token::Kind::Word: return "word";
            case Token::Kind::Semicolon: return ";";
            case Token::Kind::AndAnd: return "&&";
            case Token::Kind::OrOr: return "||";
            case Token::Kind::Pipe: return "|";
            case Token::Kind::Newline: return "newline";
            case Token::Kind::Comment: return "comment";
        }
        return "?";
    }
}
