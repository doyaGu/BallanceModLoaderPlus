#include "Console/CommandCompletion.h"

#include <algorithm>
#include <cctype>

#include <utf8.h>

#include "StringUtils.h"

namespace CommandCompletion {
    namespace {
        std::size_t CommonPrefixLength(const std::string &first,
                                       const std::string &candidate) noexcept {
            const auto *firstBegin = reinterpret_cast<const utf8_int8_t *>(first.c_str());
            const auto *candidateBegin = reinterpret_cast<const utf8_int8_t *>(candidate.c_str());
            const utf8_int8_t *firstCursor = firstBegin;
            const utf8_int8_t *candidateCursor = candidateBegin;
            std::size_t matchedBytes = 0;

            while (*firstCursor != '\0' && *candidateCursor != '\0') {
                utf8_int32_t firstCodepoint = 0;
                utf8_int32_t candidateCodepoint = 0;
                const utf8_int8_t *firstNext = utf8codepoint(firstCursor, &firstCodepoint);
                const utf8_int8_t *candidateNext = utf8codepoint(candidateCursor, &candidateCodepoint);
                if (utf8lwrcodepoint(firstCodepoint) != utf8lwrcodepoint(candidateCodepoint))
                    break;

                firstCursor = firstNext;
                candidateCursor = candidateNext;
                matchedBytes = static_cast<std::size_t>(firstCursor - firstBegin);
            }

            return matchedBytes;
        }
    }

    std::size_t CommonPrefixLength(const std::vector<std::string> &candidates) noexcept {
        if (candidates.empty() || !utils::IsValidUtf8(candidates.front()))
            return 0;

        std::size_t prefixLength = candidates.front().size();
        for (std::size_t index = 1; index < candidates.size(); ++index) {
            if (!utils::IsValidUtf8(candidates[index]))
                return 0;
            prefixLength = std::min(prefixLength,
                                    CommonPrefixLength(candidates.front(), candidates[index]));
            if (prefixLength == 0)
                break;
        }
        return prefixLength;
    }

    TokenRange FindTokenRange(std::string_view text, std::size_t cursor) noexcept {
        cursor = std::min(cursor, text.size());

        std::size_t begin = cursor;
        while (begin > 0 && !std::isspace(static_cast<unsigned char>(text[begin - 1])))
            --begin;

        std::size_t end = cursor;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])))
            ++end;

        return {begin, end,
                end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) != 0};
    }
}
