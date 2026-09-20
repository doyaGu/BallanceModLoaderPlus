#include "Console/CommandInput.h"

#include <utility>

namespace CommandInput {
    std::string Continuations::Join(std::string_view current) const {
        std::string logical;
        logical.reserve(m_Bytes + current.size());
        for (const std::string &row : m_Rows) {
            logical += row;
            logical.push_back('\n');
        }
        logical.append(current.data(), current.size());
        return logical;
    }

    void Continuations::Push(std::string row) {
        const std::size_t bytes = row.size() + 1;
        m_Rows.push_back(std::move(row));
        m_Bytes += bytes;
    }

    void Continuations::Clear() noexcept {
        m_Rows.clear();
        m_Bytes = 0;
    }

    std::string Continuations::Replace(std::string_view logical) {
        Continuations replacement;
        std::size_t begin = 0;
        while (true) {
            const std::size_t newline = logical.find('\n', begin);
            if (newline == std::string_view::npos)
                break;
            replacement.Push(std::string(logical.substr(begin, newline - begin)));
            begin = newline + 1;
        }
        *this = std::move(replacement);
        return std::string(logical.substr(begin));
    }

    std::string SingleLinePreview(std::string_view text) {
        std::string preview;
        preview.reserve(text.size());
        for (char c : text) {
            if (c == '\r') {
                if (preview.empty() || preview.back() != ' ')
                    preview.push_back(' ');
            } else if (c == '\n') {
                preview += " \\n ";
            } else {
                preview.push_back(c);
            }
        }
        return preview;
    }
}
