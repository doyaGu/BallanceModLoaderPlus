// Syntax colouring of the command line as it is typed. The result partitions
// the whole text into spans, so a renderer can walk it front to back and pick a
// colour per span; whitespace and ordinary words come back as Plain.
#ifndef BML_SHELL_HIGHLIGHTER_H
#define BML_SHELL_HIGHLIGHTER_H

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace BML::Shell {
    struct HighlightSpan {
        enum class Kind {
            Plain,
            CommandValid,   // first word of a command that names a known command or alias
            CommandInvalid, // first word of a command that names nothing
            String,         // quoted text including its quotes
            Variable,       // $NAME, ${NAME}, $?, and command substitutions
            Operator,       // ; && || | and newlines
            Comment,
            Error,          // from the position of a syntax error to the end
        };

        Kind kind = Kind::Plain;
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    // isCommand answers whether a bare first word names something runnable.
    std::vector<HighlightSpan> Highlight(std::string_view text,
                                         const std::function<bool(std::string_view)> &isCommand);
}

#endif // BML_SHELL_HIGHLIGHTER_H
