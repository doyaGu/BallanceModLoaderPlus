#include "Console/CommandInput.h"

namespace CommandInput {
    std::size_t PendingBytes(const std::vector<std::string> &pending) noexcept {
        std::size_t size = 0;
        for (const std::string &row : pending)
            size += row.size() + 1;
        return size;
    }

    std::string Join(const std::vector<std::string> &pending, std::string_view current) {
        std::string logical;
        logical.reserve(PendingBytes(pending) + current.size());
        for (const std::string &row : pending) {
            logical += row;
            logical.push_back('\n');
        }
        logical.append(current.data(), current.size());
        return logical;
    }

    bool Equals(const std::vector<std::string> &pending, std::string_view current,
                std::string_view logical) noexcept {
        if (PendingBytes(pending) + current.size() != logical.size())
            return false;
        std::size_t offset = 0;
        for (const std::string &row : pending) {
            if (logical.compare(offset, row.size(), row) != 0)
                return false;
            offset += row.size();
            if (logical[offset++] != '\n')
                return false;
        }
        return logical.compare(offset, current.size(), current) == 0;
    }

    Rows Split(std::string_view logical) {
        Rows rows;
        std::size_t begin = 0;
        while (true) {
            const std::size_t newline = logical.find('\n', begin);
            if (newline == std::string_view::npos)
                break;
            rows.pending.emplace_back(logical.substr(begin, newline - begin));
            begin = newline + 1;
        }
        rows.currentOffset = begin;
        rows.current.assign(logical.substr(begin));
        return rows;
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
