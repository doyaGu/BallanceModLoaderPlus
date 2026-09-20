#include "Console/Shell/ShellHighlighter.h"

#include <algorithm>

#include "Console/Shell/ShellLexer.h"

namespace BML::Shell {
    namespace {
        using Kind = HighlightSpan::Kind;

        void Push(std::vector<HighlightSpan> &spans, Kind kind, std::size_t begin, std::size_t end) {
            if (end <= begin)
                return;
            if (!spans.empty() && spans.back().kind == kind && spans.back().end == begin) {
                spans.back().end = end;
                return;
            }
            spans.push_back(HighlightSpan{kind, begin, end});
        }

        void PushPart(std::vector<HighlightSpan> &spans, const WordPart &part, Kind literalKind) {
            switch (part.kind) {
                case WordPart::Kind::Literal:
                    Push(spans, literalKind, part.begin, part.end);
                    break;
                case WordPart::Kind::SingleQuoted:
                case WordPart::Kind::AnsiC:
                    Push(spans, Kind::String, part.begin, part.end);
                    break;
                case WordPart::Kind::Variable:
                case WordPart::Kind::Substitution:
                    Push(spans, Kind::Variable, part.begin, part.end);
                    break;
                case WordPart::Kind::DoubleQuoted: {
                    std::size_t cursor = part.begin;
                    for (const WordPart &child : part.children) {
                        if (child.kind == WordPart::Kind::Literal)
                            continue;
                        Push(spans, Kind::String, cursor, child.begin);
                        Push(spans, Kind::Variable, child.begin, child.end);
                        cursor = child.end;
                    }
                    Push(spans, Kind::String, cursor, part.end);
                    break;
                }
            }
        }
    }

    std::vector<HighlightSpan> Highlight(std::string_view text,
                                         const std::function<bool(std::string_view)> &isCommand) {
        std::vector<HighlightSpan> spans;
        if (text.empty())
            return spans;

        const LexResult lexed = Lex(text);
        bool commandPosition = true;
        for (const Token &token : lexed.tokens) {
            switch (token.kind) {
                case Token::Kind::Word: {
                    Kind literalKind = Kind::Plain;
                    if (commandPosition) {
                        const std::string name = LiteralText(token.parts);
                        literalKind = isCommand && isCommand(name) ? Kind::CommandValid : Kind::CommandInvalid;
                    }
                    for (const WordPart &part : token.parts)
                        PushPart(spans, part, literalKind);
                    commandPosition = false;
                    break;
                }
                case Token::Kind::Comment:
                    Push(spans, Kind::Comment, token.begin, token.end);
                    break;
                default:
                    Push(spans, Kind::Operator, token.begin, token.end);
                    commandPosition = true;
                    break;
            }
        }

        std::size_t limit = text.size();
        if (lexed.error) {
            const std::size_t errorPos = std::min(lexed.errorPos, text.size());
            // Drop anything the lexer produced past the error and paint the rest red.
            while (!spans.empty() && spans.back().begin >= errorPos)
                spans.pop_back();
            if (!spans.empty() && spans.back().end > errorPos)
                spans.back().end = errorPos;
            limit = errorPos;
        }

        // Fill the gaps so the spans partition the whole text.
        std::vector<HighlightSpan> full;
        full.reserve(spans.size() * 2 + 2);
        std::size_t cursor = 0;
        for (const HighlightSpan &span : spans) {
            if (span.begin > cursor)
                full.push_back(HighlightSpan{Kind::Plain, cursor, span.begin});
            full.push_back(span);
            cursor = std::max(cursor, span.end);
        }
        if (cursor < limit)
            full.push_back(HighlightSpan{Kind::Plain, cursor, limit});
        if (lexed.error && limit < text.size())
            full.push_back(HighlightSpan{Kind::Error, limit, text.size()});
        return full;
    }
}
