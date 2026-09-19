#ifndef BML_COMMANDCOMPLETION_H
#define BML_COMMANDCOMPLETION_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace CommandCompletion {
    struct TokenRange {
        std::size_t begin = 0;
        std::size_t end = 0;
        bool followedByWhitespace = false;
    };

    std::size_t CommonPrefixLength(const std::vector<std::string> &candidates) noexcept;
    TokenRange FindTokenRange(std::string_view text, std::size_t cursor) noexcept;
}

#endif // BML_COMMANDCOMPLETION_H
