// Editing-oriented shell analysis. It accepts incomplete source, resolves the
// command at the caret with the same alias rules as execution, and reports
// typed command-head ranges for presentation without exposing lexer tokens or
// parser ASTs to callers.
#ifndef BML_SHELL_EDITING_H
#define BML_SHELL_EDITING_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    class AliasResolver;

    enum class CursorTarget {
        None,
        Command,
        Argument,
        Variable,
    };

    struct CursorAnalysis {
        CursorTarget target = CursorTarget::None;
        std::size_t replaceBegin = 0;
        std::size_t replaceEnd = 0;
        std::string prefix;
        QuoteContext context = QuoteContext::Bare;
        bool followedByWhitespace = false;
        // Alias-expanded words of the active simple command, including prefix.
        // Populated only for Argument targets.
        std::vector<std::string> arguments;
    };

    struct SourceRange {
        std::size_t begin = 0;
        std::size_t end = 0;

        bool operator==(const SourceRange &) const = default;
    };

    enum class SourceKind {
        Literal,
        String,
        Expansion,
        Operator,
        Comment,
    };

    struct SourcePiece {
        SourceRange source;
        SourceKind kind = SourceKind::Literal;
    };

    struct CommandHead {
        SourceRange source;
        std::string literal;
        // True only when the typed word is one bare literal naming an alias.
        bool namesAlias = false;
    };

    struct LineAnalysis {
        // Non-whitespace source pieces, ordered and non-overlapping.
        std::vector<SourcePiece> pieces;
        // Typed command words after combining tolerant lexical roles with
        // Parser alias semantics. Alias-generated words never leak as ranges.
        std::vector<CommandHead> commandHeads;
        std::optional<std::size_t> errorBegin;
    };

    CursorAnalysis AnalyzeCursor(std::string_view text, std::size_t cursor,
                                 const AliasResolver *aliases = nullptr);

    // Neutral line structure for presentation. This is deliberately independent
    // of renderer colours and does not expose lexer tokens or parser ASTs.
    LineAnalysis AnalyzeLine(std::string_view text,
                             const AliasResolver *aliases = nullptr);
}

#endif // BML_SHELL_EDITING_H
