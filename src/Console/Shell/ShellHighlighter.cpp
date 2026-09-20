#include "Console/Shell/ShellHighlighter.h"

#include <algorithm>

#include "Console/Shell/ShellEditing.h"

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

        Kind PieceKind(const SourcePiece &piece, const LineAnalysis &analysis,
                       const std::function<bool(std::string_view)> &isCommand) {
            switch (piece.kind) {
                case SourceKind::String: return Kind::String;
                case SourceKind::Expansion: return Kind::Variable;
                case SourceKind::Operator: return Kind::Operator;
                case SourceKind::Comment: return Kind::Comment;
                case SourceKind::Literal: break;
            }

            const auto head = std::find_if(
                analysis.commandHeads.begin(), analysis.commandHeads.end(), [&](const CommandHead &candidate) {
                    return candidate.source.begin <= piece.source.begin &&
                        piece.source.end <= candidate.source.end;
                });
            if (head == analysis.commandHeads.end())
                return Kind::Plain;
            const bool valid = (isCommand && isCommand(head->literal)) || head->namesAlias;
            return valid ? Kind::CommandValid : Kind::CommandInvalid;
        }
    }

    std::vector<HighlightSpan> Highlight(std::string_view text,
                                         const std::function<bool(std::string_view)> &isCommand,
                                         const AliasResolver *aliases) {
        std::vector<HighlightSpan> spans;
        if (text.empty())
            return spans;

        const LineAnalysis analysis = AnalyzeLine(text, aliases);
        for (const SourcePiece &piece : analysis.pieces) {
            Push(spans, PieceKind(piece, analysis, isCommand),
                 piece.source.begin, piece.source.end);
        }

        const std::size_t limit = analysis.errorBegin.value_or(text.size());
        if (analysis.errorBegin) {
            // Drop anything the lexer produced past the error and paint the rest red.
            while (!spans.empty() && spans.back().begin >= limit)
                spans.pop_back();
            if (!spans.empty() && spans.back().end > limit)
                spans.back().end = limit;
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
        if (analysis.errorBegin && limit < text.size())
            full.push_back(HighlightSpan{Kind::Error, limit, text.size()});
        return full;
    }
}
